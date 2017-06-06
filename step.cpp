/*
* File: onefunction.cpp
* Author: Matheus Coelho Ambrozio
* Date: May, 2017
*
* To compile , execute on terminal :
* clang++ -o step step.cpp `llvm-config --cxxflags --ldflags --libs all --system-libs`
*/

#include <memory>

// printing tests
#include <iostream>
//

// LLVM includes
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/DynamicLibrary.h>
//

#define SIZE_A          8
#define SIZE_B          9
#define SIZE_C          9

#define SIZE_OP         6

#define POS_OP          0
#define POS_A           (POS_OP + SIZE_OP)
#define POS_C           (POS_A + SIZE_A)
#define POS_B           (POS_C + SIZE_C)


typedef enum {
/*----------------------------------------------------------------------
name          code     args    description
------------------------------------------------------------------------*/
OP_MOVE       =  0, /*  A B     R(A) := R(B)                                    */
OP_LOADK      =  1, /*  A Bx    R(A) := Kst(Bx)                                 */
OP_LOADKX     =  2, /*  A       R(A) := Kst(extra arg)                          */
OP_LOADBOOL   =  3, /*  A B C   R(A) := (Bool)B; if (C) pc++                    */
OP_LOADNIL    =  4, /*  A B     R(A), R(A+1), ..., R(A+B) := nil                */
OP_GETUPVAL   =  5, /*  A B     R(A) := UpValue[B]                              */

OP_GETTABUP   =  6, /*  A B C   R(A) := UpValue[B][RK(C)]                       */
OP_GETTABLE   =  7, /*  A B C   R(A) := R(B)[RK(C)]                             */

OP_SETTABUP   =  8, /*  A B C   UpValue[A][RK(B)] := RK(C)                      */
OP_SETUPVAL   =  9, /*  A B     UpValue[B] := R(A)                              */
OP_SETTABLE   = 10, /*  A B C   R(A)[RK(B)] := RK(C)                            */

OP_NEWTABLE   = 11, /*  A B C   R(A) := {} (size = B,C)                         */

OP_SELF       = 12, /*  A B C   R(A+1) := R(B); R(A) := R(B)[RK(C)]   */

OP_ADD        = 13, /*  A B C   R(A) := RK(B) + RK(C)                           */
OP_SUB        = 14, /*  A B C   R(A) := RK(B) - RK(C)                           */
OP_MUL        = 15, /*  A B C   R(A) := RK(B) * RK(C)                           */
OP_MOD        = 16, /*  A B C   R(A) := RK(B) % RK(C)                           */
OP_POW        = 17, /*  A B C   R(A) := RK(B) ^ RK(C)                           */
OP_DIV        = 18, /*  A B C   R(A) := RK(B) / RK(C)                           */
OP_IDIV       = 19, /*  A B C   R(A) := RK(B) // RK(C)                          */
OP_BAND       = 20, /*  A B C   R(A) := RK(B) & RK(C)                           */
OP_BOR        = 21, /*  A B C   R(A) := RK(B) | RK(C)                           */
OP_BXOR       = 22, /*  A B C   R(A) := RK(B) ~ RK(C)                           */
OP_SHL        = 23, /*  A B C   R(A) := RK(B) << RK(C)                          */
OP_SHR        = 24, /*  A B C   R(A) := RK(B) >> RK(C)                          */
OP_UNM        = 25, /*  A B     R(A) := -R(B)                                   */
OP_BNOT       = 26, /*  A B     R(A) := ~R(B)                                   */
OP_NOT        = 27, /*  A B     R(A) := not R(B)                                */
OP_LEN        = 28, /*  A B     R(A) := length of R(B)                          */

OP_CONCAT     = 29, /*  A B C   R(A) := R(B).. ... ..R(C)                       */

OP_JMP        = 30, /*  A sBx   pc+=sBx; if (A) close all upvalues >= R(A - 1)  */
OP_EQ         = 31, /*  A B C   if ((RK(B) == RK(C)) ~= A) then pc++            */
OP_LT         = 32, /*  A B C   if ((RK(B) <  RK(C)) ~= A) then pc++            */
OP_LE         = 33, /*  A B C   if ((RK(B) <= RK(C)) ~= A) then pc++            */

OP_TEST       = 34, /*  A C     if not (R(A) <=> C) then pc++                   */
OP_TESTSET    = 35, /*  A B C   if (R(B) <=> C) then R(A) := R(B) else pc++     */

OP_CALL       = 36, /*  A B C   R(A), ... ,R(A+C-2) := R(A)(R(A+1), ... ,R(A+B-1)) */
OP_TAILCALL   = 37, /*  A B C   return R(A)(R(A+1), ... ,R(A+B-1))              */
OP_RETURN     = 38, /*  A B     return R(A), ... ,R(A+B-2)      (see note)      */

OP_FORLOOP    = 39, /*  A sBx   R(A)+=R(A+2); if R(A) <?= R(A+1) then { pc+=sBx; R(A+3)=R(A) }*/
OP_FORPREP    = 40, /*  A sBx   R(A)-=R(A+2); pc+=sBx                           */

OP_TFORCALL   = 41, /*  A C     R(A+3), ... ,R(A+2+C) := R(A)(R(A+1), R(A+2));  */
OP_TFORLOOP   = 42, /*  A sBx   if R(A+1) ~= nil then { R(A)=R(A+1); pc += sBx }*/

OP_SETLIST    = 43, /*  A B C   R(A)[(C-1)*FPF+i] := R(A+i), 1 <= i <= B        */

OP_CLOSURE    = 44, /*  A Bx    R(A) := closure(KPROTO[Bx])                     */

OP_VARARG     = 45, /*  A B     R(A), R(A+1), ..., R(A+B-2) = vararg            */

OP_EXTRAARG   = 46, /*  Ax      extra (larger) argument for previous opcode     */
} OpCode;



// --- Type definitions copied from c-minilua.c
typedef uint8_t Byte;
typedef int32_t Int;
typedef uint32_t Instruction;
typedef int64_t lua_integer;
typedef double lua_float;

typedef enum { SHORT_STRING, LONG_STRING } StringType;
typedef struct {
    StringType typ;
    const char *str;
} String;

typedef struct {
    int typ;
    union {
        int b;
        lua_integer i;
        lua_float n;
    } u;
} Value;

// Function prototype
typedef struct Proto {
    Byte numparams;  /* number of fixed parameters */
    Byte is_vararg;
    Byte maxstacksize;  /* number of registers needed by this function */
    Int sizeupvalues;  /* size of 'upvalues' */
    Int sizek;  /* size of 'k' */
    Int sizecode;
    //Int sizelineinfo;
    Int sizep;  /* size of 'p' */
    //Int sizelocvars;
    Int linedefined;  /* debug information  */
    Int lastlinedefined;  /* debug information  */
    Value *k;  /* constants used by the function */
    Instruction *code;  /* opcodes */
    //struct Proto **p;  /* functions defined inside the function */
    //int *lineinfo;  /* map from opcodes to source lines (debug information) */
    //LocVar *locvars;  /* information about local variables (debug information) */
    //Upvaldesc *upvalues;  /* upvalue information */
    //struct LClosure *cache;  /* last-created closure with this prototype */
    String  *source;  /* used for debug information */
    //GCObject *gclist;
} Proto;

//MiniLuaState Struct
typedef struct MiniLuaState {
    Proto *proto;
    Value *registers;
    size_t return_begin;
    size_t return_end;
} MiniLuaState;
// --- End of type definitions


// Globals
llvm::LLVMContext context;
std::unique_ptr<llvm::Module> Owner(new llvm::Module("step_module", context));
llvm::Module *module = Owner.get();
llvm::IRBuilder<> builder(context);

llvm::Function *step_func;
llvm::Value *_mls;
llvm::Value *_inst;
llvm::Value *_op;
llvm::Value *_constants;

llvm::Function *llvm_memcpy;

llvm::BasicBlock *entry_block;
llvm::BasicBlock *end_block;
llvm::BasicBlock *op_move_block;
llvm::BasicBlock *op_loadk_block;
llvm::BasicBlock *op_add_block;
llvm::BasicBlock *op_sub_block;
llvm::BasicBlock *op_mul_block;
llvm::BasicBlock *op_div_block;
llvm::BasicBlock *op_mod_block;
llvm::BasicBlock *op_idiv_block;
llvm::BasicBlock *op_pow_block;
llvm::BasicBlock *op_unm_block;
llvm::BasicBlock *op_not_block;
llvm::BasicBlock *op_jmp_block;
llvm::BasicBlock *op_eq_block;
llvm::BasicBlock *op_lt_block;
llvm::BasicBlock *op_le_block;
llvm::BasicBlock *op_forloop_block;
llvm::BasicBlock *op_forprep_block;
llvm::BasicBlock *default_block;


int main() {

    //necessary LLVM initializations
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    //create the context and the main module
//    llvm::LLVMContext context;
//    std::unique_ptr<llvm::Module> Owner(new llvm::Module("step_module", context));
//    llvm::Module *module = Owner.get();


    // --- Declarations
    //treating string_type enum as an int32

    //create String struct
    llvm::StructType *string_struct_type = llvm::StructType::create(context, "String");
    llvm::PointerType *p_string_struct_type = llvm::PointerType::get(string_struct_type, 0);
    std::vector<llvm::Type *> string_elements;
    string_elements.push_back(llvm::Type::getInt32Ty(context));   //typ
    string_elements.push_back(llvm::Type::getInt8PtrTy(context)); //*str
    string_struct_type->setBody(string_elements);
    // std::cout << "Struct String dump:\n";
    // string_struct_type->dump();
    // std::cout << "\n";

    //create Value struct
    llvm::StructType *value_struct_type = llvm::StructType::create(context, "Value");
    llvm::PointerType *p_value_struct_type = llvm::PointerType::get(value_struct_type, 0);
    std::vector<llvm::Type *> value_elements;
    value_elements.push_back(llvm::Type::getInt32Ty(context)); //typ
    value_elements.push_back(llvm::Type::getInt64Ty(context)); //u
    value_struct_type->setBody(value_elements);
    // std::cout << "Struct Value dump:\n";
    // value_struct_type->dump();
    // std::cout << "\n";

    //create Proto struct
    llvm::StructType *proto_struct_type = llvm::StructType::create(context, "Proto");
    llvm::PointerType *p_proto_struct_type = llvm::PointerType::get(proto_struct_type, 0);
    std::vector<llvm::Type *> proto_elements;
    proto_elements.push_back(llvm::Type::getInt8Ty(context));     //numparams
    proto_elements.push_back(llvm::Type::getInt8Ty(context));     //is_vararg
    proto_elements.push_back(llvm::Type::getInt8Ty(context));     //maxstacksize
    proto_elements.push_back(llvm::Type::getInt32Ty(context));    //sizeupvalues
    proto_elements.push_back(llvm::Type::getInt32Ty(context));    //sizek
    proto_elements.push_back(llvm::Type::getInt32Ty(context));    //sizecode
    proto_elements.push_back(llvm::Type::getInt32Ty(context));    //sizep
    proto_elements.push_back(llvm::Type::getInt32Ty(context));    //linedefined
    proto_elements.push_back(llvm::Type::getInt32Ty(context));    //lastlinedefined
    proto_elements.push_back(p_value_struct_type);                //*k
    proto_elements.push_back(llvm::Type::getInt32PtrTy(context)); //*code
    proto_elements.push_back(p_string_struct_type);               //*source
    proto_struct_type->setBody(proto_elements);
    // std::cout << "Struct Proto dump:\n";
    // proto_struct_type->dump();
    // std::cout << "\n";

    //create MiniLuaState struct
    llvm::StructType *miniluastate_struct_type = llvm::StructType::create(context, "MiniLuaState");
    llvm::PointerType *p_miniluastate_struct_type = llvm::PointerType::get(miniluastate_struct_type, 0);
    std::vector<llvm::Type *> miniluastate_elements;
    miniluastate_elements.push_back(p_proto_struct_type);             //*proto
    miniluastate_elements.push_back(p_value_struct_type);             //*value
    miniluastate_elements.push_back(llvm::Type::getInt64Ty(context)); //return_begin
    miniluastate_elements.push_back(llvm::Type::getInt64Ty(context)); //return_end
    miniluastate_struct_type->setBody(miniluastate_elements);
    // std::cout << "Struct MiniLuaState dump:\n";
    // miniluastate_struct_type->dump();
    // std::cout << "\n";

    //create declaration for step
    //int step(MiniLuaState *mls, Instruction inst, uint32_t op, Value *constants);
    std::vector<llvm::Type *> args;
    args.push_back(p_miniluastate_struct_type);      //*mls
    args.push_back(llvm::Type::getInt32Ty(context)); //inst
    args.push_back(llvm::Type::getInt32Ty(context)); //op
    args.push_back(p_value_struct_type);             //constants
    llvm::FunctionType *step_type = llvm::FunctionType::get(llvm::Type::getInt64Ty(context), args, false);
    step_func = llvm::Function::Create(step_type, llvm::Function::ExternalLinkage, "step", module);

    //get arguments
    auto argiter = step_func->arg_begin();
    _mls = &*argiter++;
    _inst = &*argiter++;
    _op = &*argiter++;
    _constants = &*argiter++;
    //_mls->setName("arg");

    std::cout << "Arguments received by interpret function dump:\n";
    _mls->dump();
    //std::cout << _mls;
    std::cout << "\n";


    //Creating llvm::memcpy function reference
    llvm::SmallVector<llvm::Type *, 3> vec;
    vec.push_back(llvm::Type::getInt8PtrTy(context));  /* i8 */
    vec.push_back(llvm::Type::getInt8PtrTy(context));  /* i8 */
    vec.push_back(llvm::Type::getInt64Ty(context));
    llvm_memcpy = llvm::Intrinsic::getDeclaration(module, llvm::Intrinsic::memcpy, vec);

    llvm_memcpy->dump();


    //create irbuilder
//    llvm::IRBuilder<> builder(context);


    // --- Creating Blocks
    //create the entry block
    entry_block = llvm::BasicBlock::Create(context, "entry", step_func);

    //create the end block
    end_block = llvm::BasicBlock::Create(context, "end", step_func);

    //create op's blocks
    op_move_block = llvm::BasicBlock::Create(context, "op_move", step_func);
    op_loadk_block = llvm::BasicBlock::Create(context, "op_loadk", step_func);
    op_add_block = llvm::BasicBlock::Create(context, "op_add", step_func);
    op_sub_block = llvm::BasicBlock::Create(context, "op_sub", step_func);
    op_mul_block = llvm::BasicBlock::Create(context, "op_mul", step_func);
    op_div_block = llvm::BasicBlock::Create(context, "op_div", step_func);
    op_mod_block = llvm::BasicBlock::Create(context, "op_mod", step_func);
    op_idiv_block = llvm::BasicBlock::Create(context, "op_idiv", step_func);
    op_pow_block = llvm::BasicBlock::Create(context, "op_pow", step_func);
    op_unm_block = llvm::BasicBlock::Create(context, "op_unm", step_func);
    op_not_block = llvm::BasicBlock::Create(context, "op_not", step_func);
    op_jmp_block = llvm::BasicBlock::Create(context, "op_jmp", step_func);
    op_eq_block = llvm::BasicBlock::Create(context, "op_eq", step_func);
    op_lt_block = llvm::BasicBlock::Create(context, "op_lt", step_func);
    op_le_block = llvm::BasicBlock::Create(context, "op_le", step_func);
    op_forloop_block = llvm::BasicBlock::Create(context, "op_forloop", step_func);
    op_forprep_block = llvm::BasicBlock::Create(context, "op_forprep", step_func);
    default_block = llvm::BasicBlock::Create(context, "op_default", step_func);

    // --- Create code for the entry block
    builder.SetInsertPoint(entry_block);

    llvm::SwitchInst *theSwitch = builder.CreateSwitch(_op, default_block, 18);

    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_MOVE, true)), op_move_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_LOADK, true)), op_loadk_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_ADD, true)), op_add_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_SUB, true)), op_sub_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_MUL, true)), op_mul_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_DIV, true)), op_div_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_MOD, true)), op_mod_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_IDIV, true)), op_idiv_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_POW, true)), op_pow_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_UNM, true)), op_unm_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_NOT, true)), op_not_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_JMP, true)), op_jmp_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_EQ, true)), op_eq_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_LT, true)), op_lt_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_LE, true)), op_le_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_FORLOOP, true)), op_forloop_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_FORPREP, true)), op_forprep_block);


    //create OP_MOVE
    builder.SetInsertPoint(op_move_block);

    std::vector<llvm::Value *> temp;
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true))); //registers offset
    llvm::Value *registers_GEP = builder.CreateInBoundsGEP(miniluastate_struct_type, _mls, temp);

    llvm::Value *registers_LD = builder.CreateLoad(registers_GEP);

    llvm::Value *temp_inst = builder.CreateLShr(_inst, llvm::ConstantInt::get(context, llvm::APInt(32, POS_A, true)));
    llvm::Value *a_inst = builder.CreateAnd(temp_inst, llvm::ConstantInt::get(context, llvm::APInt(32, 0xFF, true)));

    temp.clear();
    temp.push_back(a_inst);
    llvm::Value *registers_ath = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);

    temp_inst = NULL;
    temp_inst = builder.CreateLShr(_inst, llvm::ConstantInt::get(context, llvm::APInt(32, POS_B, true)));
    llvm::Value *b_inst = builder.CreateAnd(temp_inst, llvm::ConstantInt::get(context, llvm::APInt(32, 0x1FF, true)));

    temp.clear();
    temp.push_back(b_inst);
    llvm::Value *registers_bth = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);

    llvm::Value *ath_bitcast = builder.CreateBitCast(registers_ath, llvm::Type::getInt8PtrTy(context));
    llvm::Value *bth_bitcast = builder.CreateBitCast(registers_bth, llvm::Type::getInt8PtrTy(context));

    temp.clear();
    temp.push_back(ath_bitcast);
    temp.push_back(bth_bitcast);
    builder.CreateCall(llvm_memcpy, temp);

    builder.CreateBr(end_block);


    //create OP_LOADK
    builder.SetInsertPoint(op_loadk_block);


    //create OP_ADD
    builder.SetInsertPoint(op_add_block);

    //create OP_SUB
    builder.SetInsertPoint(op_sub_block);

    //create OP_MUL
    builder.SetInsertPoint(op_mul_block);

    //create OP_DIV
    builder.SetInsertPoint(op_div_block);

    //create OP_MOD
    builder.SetInsertPoint(op_mod_block);

    //create OP_IDIV
    builder.SetInsertPoint(op_idiv_block);

    //create OP_POW
    builder.SetInsertPoint(op_pow_block);

    //create OP_UNM
    builder.SetInsertPoint(op_unm_block);

    //create OP_NOT
    builder.SetInsertPoint(op_not_block);

    //create OP_JMP
    builder.SetInsertPoint(op_jmp_block);

    //create OP_EQ
    builder.SetInsertPoint(op_eq_block);

    //create OP_LT
    builder.SetInsertPoint(op_lt_block);

    //create OP_LE
    builder.SetInsertPoint(op_le_block);

    //create OP_FORLOOP
    builder.SetInsertPoint(op_forloop_block);

    //create OP_FORPREP
    builder.SetInsertPoint(op_forprep_block);

    //create DEFAULT
    builder.SetInsertPoint(default_block);

    //create END label
    builder.SetInsertPoint(end_block);

    builder.CreateRet(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));




    //dump module to check ir
    std::cout << "Module:\n";
    module->dump();
    std::cout << "\n";

    return 0;
}


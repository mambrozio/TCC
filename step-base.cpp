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
#include <cstdio>
#include <iostream>
#include <sstream>
//

// LLVM includes
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/DynamicLibrary.h>
//

//
#define DEBUG do { std::cout << "AQUI" << std::endl; } while (0);
//

#define SIZE_C          9
#define SIZE_B          9
#define SIZE_Bx         (SIZE_C + SIZE_B)
#define SIZE_A          8
#define SIZE_Ax         (SIZE_C + SIZE_B + SIZE_A)

#define SIZE_OP         6

#define POS_OP          0
#define POS_A           (POS_OP + SIZE_OP)
#define POS_C           (POS_A + SIZE_A)
#define POS_B           (POS_C + SIZE_C)
#define POS_Bx          POS_C
#define POS_Ax          POS_A


#define MAXARG_A        ((1<<SIZE_A)-1)
#define MAXARG_B        ((1<<SIZE_B)-1)
#define MAXARG_C        ((1<<SIZE_C)-1)

#define MAXARG_Ax       ((1<<SIZE_Ax)-1)
#define MAXARG_Bx       ((1<<SIZE_Bx)-1)
#define MAXARG_sBx      (MAXARG_Bx>>1)    // 'sBx' is signed

/* this bit 1 means constant (0 means register) */
#define BITRK           (1 << (SIZE_B - 1))

// Lua parameters
#define LUAI_MAXSHORTLEN 40

#define LUA_TNIL                0
#define LUA_TBOOLEAN            1
#define LUA_TLIGHTUSERDATA      2
#define LUA_TNUMBER             3
#define LUA_TSTRING             4
#define LUA_TTABLE              5
#define LUA_TFUNCTION           6
#define LUA_TUSERDATA           7
#define LUA_TTHREAD             8
#define LUA_NUMTAGS             9

#define LUA_TNUMFLT     (LUA_TNUMBER | (0 << 4))  /* float numbers */
#define LUA_TNUMINT     (LUA_TNUMBER | (1 << 4))  /* integer numbers */

#define LUA_TSHRSTR     (LUA_TSTRING | (0 << 4))  /* short strings */
#define LUA_TLNGSTR     (LUA_TSTRING | (1 << 4))  /* long strings */


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



//Function definitions
//TODO put this on a header file
llvm::Value* create_op_move_block();
llvm::Value* create_op_loadk_block();
llvm::Value* create_op_add_block();
llvm::Value* create_op_sub_block();
llvm::Value* create_op_mul_block();
llvm::Value* create_op_div_block();
llvm::Value* create_op_mod_block();
llvm::Value* create_op_idiv_block();
llvm::Value* create_op_pow_block();
llvm::Value* create_op_unm_block();
llvm::Value* create_op_not_block();
llvm::Value* create_op_jmp_block();

llvm::Value* create_op_eq_block();
llvm::Value* create_op_lt_block();
llvm::Value* create_op_le_block();
llvm::Value* create_op_forloop_block();
llvm::Value* create_op_forprep_block();



// Globals
llvm::LLVMContext context;
std::unique_ptr<llvm::Module> Owner(new llvm::Module("step_module", context));
llvm::Module *module = Owner.get();
llvm::IRBuilder<> builder(context);

llvm::StructType *string_struct_type;
llvm::PointerType *p_string_struct_type;

llvm::StructType *value_struct_type;
llvm::PointerType *p_value_struct_type;

llvm::StructType *proto_struct_type;
llvm::PointerType *p_proto_struct_type;

llvm::StructType *miniluastate_struct_type;
llvm::PointerType *p_miniluastate_struct_type;

llvm::FunctionType *step_type;
llvm::Function *step_func;
llvm::Value *_mls;
llvm::Value *_inst;
llvm::Value *_op;
llvm::Value *_constants;

llvm::Function *llvm_memcpy;
llvm::Function *llvm_floor;
llvm::Function *llvm_pow;

llvm::Function *error; //used

// debug errors
llvm::Function *error_1;
llvm::Function *error_2;
llvm::Function *error_3;
llvm::Function *error_4;
llvm::Function *error_5;
llvm::Function *error_6;

// llvm::BasicBlock *error_block_1;
// llvm::BasicBlock *error_block_2;
// llvm::BasicBlock *error_block_3;
// llvm::BasicBlock *error_block_4;
// llvm::BasicBlock *error_block_5;
// llvm::BasicBlock *error_block_6;
//

//
llvm::Function *step_in_C;
//

llvm::PHINode *return_phi_node;

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

llvm::BasicBlock *error_block;

//
llvm::BasicBlock *op_add_1_block;
llvm::BasicBlock *op_add_2_block;
llvm::BasicBlock *op_add_3_block;
llvm::BasicBlock *op_add_4_block;
llvm::BasicBlock *op_add_5_block;
llvm::BasicBlock *op_add_6_block;
llvm::BasicBlock *op_add_7_block;
llvm::BasicBlock *op_add_8_block;
llvm::BasicBlock *op_add_9_block;
llvm::BasicBlock *op_add_10_block;

llvm::BasicBlock *op_sub_1_block;
llvm::BasicBlock *op_sub_2_block;
llvm::BasicBlock *op_sub_3_block;
llvm::BasicBlock *op_sub_4_block;
llvm::BasicBlock *op_sub_5_block;
llvm::BasicBlock *op_sub_6_block;
llvm::BasicBlock *op_sub_7_block;
llvm::BasicBlock *op_sub_8_block;
llvm::BasicBlock *op_sub_9_block;
llvm::BasicBlock *op_sub_10_block;

llvm::BasicBlock *op_mul_1_block;
llvm::BasicBlock *op_mul_2_block;
llvm::BasicBlock *op_mul_3_block;
llvm::BasicBlock *op_mul_4_block;
llvm::BasicBlock *op_mul_5_block;
llvm::BasicBlock *op_mul_6_block;
llvm::BasicBlock *op_mul_7_block;
llvm::BasicBlock *op_mul_8_block;
llvm::BasicBlock *op_mul_9_block;
llvm::BasicBlock *op_mul_10_block;

llvm::BasicBlock *op_div_1_block;
llvm::BasicBlock *op_div_4_block;
llvm::BasicBlock *op_div_5_block;
llvm::BasicBlock *op_div_6_block;
llvm::BasicBlock *op_div_7_block;
llvm::BasicBlock *op_div_8_block;
llvm::BasicBlock *op_div_9_block;
llvm::BasicBlock *op_div_10_block;

llvm::BasicBlock *op_mod_1_block;
llvm::BasicBlock *op_mod_2_block;
llvm::BasicBlock *op_mod_3_block;
llvm::BasicBlock *op_mod_4_block;
llvm::BasicBlock *op_mod_5_block;
llvm::BasicBlock *op_mod_6_block;
llvm::BasicBlock *op_mod_7_block;
llvm::BasicBlock *op_mod_8_block;
llvm::BasicBlock *op_mod_9_block;
llvm::BasicBlock *op_mod_10_block;

llvm::BasicBlock *op_idiv_1_block;
llvm::BasicBlock *op_idiv_2_block;
llvm::BasicBlock *op_idiv_3_block;
llvm::BasicBlock *op_idiv_4_block;
llvm::BasicBlock *op_idiv_5_block;
llvm::BasicBlock *op_idiv_6_block;
llvm::BasicBlock *op_idiv_7_block;
llvm::BasicBlock *op_idiv_8_block;
llvm::BasicBlock *op_idiv_9_block;
llvm::BasicBlock *op_idiv_10_block;

llvm::BasicBlock *op_pow_1_block;
llvm::BasicBlock *op_pow_4_block;
llvm::BasicBlock *op_pow_5_block;
llvm::BasicBlock *op_pow_6_block;
llvm::BasicBlock *op_pow_7_block;
llvm::BasicBlock *op_pow_8_block;
llvm::BasicBlock *op_pow_9_block;
llvm::BasicBlock *op_pow_10_block;

llvm::BasicBlock *op_unm_1_block;
llvm::BasicBlock *op_unm_2_block;

llvm::BasicBlock *op_not_1_block;
llvm::BasicBlock *op_not_2_block;
llvm::BasicBlock *op_not_3_block;

llvm::BasicBlock *op_eq_1_block;
llvm::BasicBlock *op_eq_2_block;
llvm::BasicBlock *op_eq_3_block;
llvm::BasicBlock *op_eq_4_block;
llvm::BasicBlock *op_eq_5_block;
llvm::BasicBlock *op_eq_6_block;
llvm::BasicBlock *op_eq_7_block;
llvm::BasicBlock *op_eq_8_block;
llvm::BasicBlock *op_eq_9_block;
llvm::BasicBlock *op_eq_10_block;

llvm::BasicBlock *op_lt_1_block;
llvm::BasicBlock *op_lt_2_block;
llvm::BasicBlock *op_lt_3_block;
llvm::BasicBlock *op_lt_4_block;
llvm::BasicBlock *op_lt_5_block;
llvm::BasicBlock *op_lt_6_block;
llvm::BasicBlock *op_lt_7_block;
llvm::BasicBlock *op_lt_8_block;
llvm::BasicBlock *op_lt_9_block;
llvm::BasicBlock *op_lt_10_block;
llvm::BasicBlock *op_lt_11_block;

llvm::BasicBlock *op_le_1_block;
llvm::BasicBlock *op_le_2_block;
llvm::BasicBlock *op_le_3_block;
llvm::BasicBlock *op_le_4_block;
llvm::BasicBlock *op_le_5_block;
llvm::BasicBlock *op_le_6_block;
llvm::BasicBlock *op_le_7_block;
llvm::BasicBlock *op_le_8_block;
llvm::BasicBlock *op_le_9_block;
llvm::BasicBlock *op_le_10_block;
llvm::BasicBlock *op_le_11_block;

llvm::BasicBlock *op_forloop_1_block;
llvm::BasicBlock *op_forloop_2_block;
llvm::BasicBlock *op_forloop_3_block;
llvm::BasicBlock *op_forloop_4_block;
llvm::BasicBlock *op_forloop_5_block;
llvm::BasicBlock *op_forloop_6_block;
llvm::BasicBlock *op_forloop_7_block;
llvm::BasicBlock *op_forloop_8_block;
llvm::BasicBlock *op_forloop_9_block;
llvm::BasicBlock *op_forloop_10_block;
llvm::BasicBlock *op_forloop_11_block;
llvm::BasicBlock *op_forloop_12_block;
llvm::BasicBlock *op_forloop_13_block;
llvm::BasicBlock *op_forloop_14_block;
llvm::BasicBlock *op_forloop_15_block;
llvm::BasicBlock *op_forloop_16_block;
llvm::BasicBlock *op_forloop_17_block;
llvm::BasicBlock *op_forloop_18_block;

llvm::BasicBlock *op_forprep_1_block;
llvm::BasicBlock *op_forprep_2_block;
llvm::BasicBlock *op_forprep_3_block;
llvm::BasicBlock *op_forprep_4_block;
llvm::BasicBlock *op_forprep_5_block;
llvm::BasicBlock *op_forprep_6_block;
llvm::BasicBlock *op_forprep_7_block;
llvm::BasicBlock *op_forprep_8_block;
llvm::BasicBlock *op_forprep_9_block;
llvm::BasicBlock *op_forprep_10_block;
llvm::BasicBlock *op_forprep_11_block;
//

void create_string_struct() {
    string_struct_type = llvm::StructType::create(context, "String");
    p_string_struct_type = llvm::PointerType::get(string_struct_type, 0);
    std::vector<llvm::Type *> string_elements;
    string_elements.push_back(llvm::Type::getInt32Ty(context));   //typ
    string_elements.push_back(llvm::Type::getInt8PtrTy(context)); //*str
    string_struct_type->setBody(string_elements);
    // std::cout << "Struct String dump:\n";
    // string_struct_type->dump();
    // std::cout << "\n";
}

void create_value_struct() {
    value_struct_type = llvm::StructType::create(context, "Value");
    p_value_struct_type = llvm::PointerType::get(value_struct_type, 0);
    std::vector<llvm::Type *> value_elements;
    value_elements.push_back(llvm::Type::getInt32Ty(context)); //typ
    value_elements.push_back(llvm::Type::getInt64Ty(context)); //u
    value_struct_type->setBody(value_elements);
    // std::cout << "Struct Value dump:\n";
    // value_struct_type->dump();
    // std::cout << "\n";
}

void create_proto_struct() {
    proto_struct_type = llvm::StructType::create(context, "Proto");
    p_proto_struct_type = llvm::PointerType::get(proto_struct_type, 0);
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
}

void create_miniluastate_struct() {
    miniluastate_struct_type = llvm::StructType::create(context, "MiniLuaState");
    p_miniluastate_struct_type = llvm::PointerType::get(miniluastate_struct_type, 0);
    std::vector<llvm::Type *> miniluastate_elements;
    miniluastate_elements.push_back(p_proto_struct_type);             //*proto
    miniluastate_elements.push_back(p_value_struct_type);             //*registers
    miniluastate_elements.push_back(llvm::Type::getInt64Ty(context)); //return_begin
    miniluastate_elements.push_back(llvm::Type::getInt64Ty(context)); //return_end
    miniluastate_struct_type->setBody(miniluastate_elements);
    // std::cout << "Struct MiniLuaState dump:\n";
    // miniluastate_struct_type->dump();
    // std::cout << "\n";
}

void create_step_decl() {
    std::vector<llvm::Type *> args;
    args.push_back(p_miniluastate_struct_type);      //*mls
    args.push_back(llvm::Type::getInt32Ty(context)); //inst
    args.push_back(llvm::Type::getInt32Ty(context)); //op
    args.push_back(p_value_struct_type);             //constants
    step_type = llvm::FunctionType::get(llvm::Type::getInt64Ty(context), args, false);
    step_func = llvm::Function::Create(step_type, llvm::Function::ExternalLinkage, "step", module);
}

void init_step_args() {
    auto argiter = step_func->arg_begin();
    _mls = &*argiter++;
    _inst = &*argiter++;
    _op = &*argiter++;
    _constants = &*argiter++;
}

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
    create_string_struct();

    //create Value struct
    create_value_struct();

    //create Proto struct
    create_proto_struct();

    //create MiniLuaState struct
    create_miniluastate_struct();

    //create declaration for step
    //int step(MiniLuaState *mls, Instruction inst, uint32_t op, Value *constants);
    create_step_decl();

    //get arguments
    init_step_args();
    //_mls->setName("arg");

    // std::cout << "Arguments received by interpret function dump:\n";
    // _mls->dump();
    //std::cout << _mls;
    // std::cout << "\n";


    //
    step_in_C = llvm::Function::Create(step_type, llvm::Function::ExternalLinkage, "step_in_C", module);
    std::vector<llvm::Value *> step_args;
    step_args.push_back(_mls);
    step_args.push_back(_inst);
    step_args.push_back(_op);
    step_args.push_back(_constants);

    std::vector<llvm::Type *> error_args;
    llvm::FunctionType *error_type = llvm::FunctionType::get(llvm::Type::getVoidTy(context), error_args, false);
    error = llvm::Function::Create(error_type, llvm::Function::ExternalLinkage, "error_default", module);

    // debug errors
    // error_1 = llvm::Function::Create(error_type, llvm::Function::ExternalLinkage, "error_1", module);
    // error_2 = llvm::Function::Create(error_type, llvm::Function::ExternalLinkage, "error_2", module);
    // error_3 = llvm::Function::Create(error_type, llvm::Function::ExternalLinkage, "error_3", module);
    // error_4 = llvm::Function::Create(error_type, llvm::Function::ExternalLinkage, "error_4", module);
    // error_5 = llvm::Function::Create(error_type, llvm::Function::ExternalLinkage, "error_5", module);
    // error_6 = llvm::Function::Create(error_type, llvm::Function::ExternalLinkage, "error_6", module);
    //

    //Creating llvm::memcpy function reference
    llvm::SmallVector<llvm::Type *, 3> vec_memcpy;
    vec_memcpy.push_back(llvm::Type::getInt8PtrTy(context));  /* i8 */
    vec_memcpy.push_back(llvm::Type::getInt8PtrTy(context));  /* i8 */
    vec_memcpy.push_back(llvm::Type::getInt64Ty(context));
    llvm_memcpy = llvm::Intrinsic::getDeclaration(module, llvm::Intrinsic::memcpy, vec_memcpy);

    //Creating llvm::floor function reference
    llvm::SmallVector<llvm::Type *, 1> vec_floor;
    vec_floor.push_back(llvm::Type::getDoubleTy(context));  /* double */
    llvm_floor = llvm::Intrinsic::getDeclaration(module, llvm::Intrinsic::floor, vec_floor);

    //Creating llvm::floor function reference
    llvm::SmallVector<llvm::Type *, 2> vec_pow;
    vec_pow.push_back(llvm::Type::getDoubleTy(context));  /* double */
    vec_pow.push_back(llvm::Type::getDoubleTy(context));  /* double */
    llvm_pow = llvm::Intrinsic::getDeclaration(module, llvm::Intrinsic::pow, vec_pow);

    //create irbuilder
//    llvm::IRBuilder<> builder(context);


    // --- Creating Blocks
    //create the entry block
    entry_block = llvm::BasicBlock::Create(context, "entry", step_func);

    //create the end block
    end_block = llvm::BasicBlock::Create(context, "end", step_func);

    //create op's blocks
    // op_eq_block = llvm::BasicBlock::Create(context, "op_eq", step_func);
    // op_forloop_block = llvm::BasicBlock::Create(context, "op_forloop", step_func);
    // op_forprep_block = llvm::BasicBlock::Create(context, "op_forprep", step_func);
    default_block = llvm::BasicBlock::Create(context, "op_default", step_func);

    error_block = llvm::BasicBlock::Create(context, "error_block", step_func);

    //debug error
    // error_block_1 = llvm::BasicBlock::Create(context, "error_block_1", step_func);
    // error_block_2 = llvm::BasicBlock::Create(context, "error_block_2", step_func);
    // error_block_3 = llvm::BasicBlock::Create(context, "error_block_3", step_func);
    // error_block_4 = llvm::BasicBlock::Create(context, "error_block_4", step_func);
    // error_block_5 = llvm::BasicBlock::Create(context, "error_block_5", step_func);
    // error_block_6 = llvm::BasicBlock::Create(context, "error_block_6", step_func);
    //

    // --- Create code for the entry block
    builder.SetInsertPoint(entry_block);

    llvm::SwitchInst *theSwitch = builder.CreateSwitch(_op, default_block, 18);

    //create OP_MOVE
    llvm::Value *return_from_op_move = create_op_move_block();
    builder.CreateBr(end_block);

    //create OP_LOADK
    llvm::Value *return_from_op_loadk = create_op_loadk_block();
    builder.CreateBr(end_block);

    //create OP_ADD
    builder.SetInsertPoint(op_add_block);
    //call step_in_C
    // llvm::Value *return_from_op_add = builder.CreateCall(step_in_C, step_args);
    llvm::Value *return_from_op_add = create_op_add_block();

    //create OP_SUB
    builder.SetInsertPoint(op_sub_block);
    // llvm::Value *return_from_op_sub = builder.CreateCall(step_in_C, step_args);
    llvm::Value *return_from_op_sub = create_op_sub_block();

    //create OP_MUL
    builder.SetInsertPoint(op_mul_block);
    // llvm::Value *return_from_op_mul = builder.CreateCall(step_in_C, step_args);
    llvm::Value *return_from_op_mul = create_op_mul_block();

    //create OP_DIV
    builder.SetInsertPoint(op_div_block);
    // llvm::Value *return_from_op_div = builder.CreateCall(step_in_C, step_args);
    llvm::Value *return_from_op_div = create_op_div_block();

    //create OP_MOD
    builder.SetInsertPoint(op_mod_block);
    // llvm::Value *return_from_op_mod = builder.CreateCall(step_in_C, step_args);
    llvm::Value *return_from_op_mod = create_op_mod_block();

    //create OP_IDIV
    builder.SetInsertPoint(op_idiv_block);
    // llvm::Value *return_from_op_idiv = builder.CreateCall(step_in_C, step_args);
    llvm::Value *return_from_op_idiv = create_op_idiv_block();

    //create OP_POW
    builder.SetInsertPoint(op_pow_block);
    // llvm::Value *return_from_op_pow = builder.CreateCall(step_in_C, step_args);
    llvm::Value *return_from_op_pow = create_op_pow_block();

    //create OP_UNM
    builder.SetInsertPoint(op_unm_block);
    // llvm::Value *return_from_op_unm = builder.CreateCall(step_in_C, step_args);
    llvm::Value *return_from_op_unm = create_op_unm_block();

    //create OP_NOT
    builder.SetInsertPoint(op_not_block);
    // llvm::Value *return_from_op_not = builder.CreateCall(step_in_C, step_args);
    llvm::Value *return_from_op_not = create_op_not_block();

    //create OP_JMP
    builder.SetInsertPoint(op_jmp_block);
    // op_jmp_block = llvm::BasicBlock::Create(context, "op_jmp", step_func);
    llvm::Value *return_from_op_jmp = create_op_jmp_block();

    //create OP_EQ
    builder.SetInsertPoint(op_eq_block);
    // llvm::Value *return_from_op_eq = builder.CreateCall(step_in_C, step_args);
    llvm::Value *return_from_op_eq = create_op_eq_block();
    // builder.CreateBr(end_block);

    //create OP_LT
    builder.SetInsertPoint(op_lt_block);
    // llvm::Value *return_from_op_lt = builder.CreateCall(step_in_C, step_args);
    llvm::Value *return_from_op_lt = create_op_lt_block();

    //create OP_LE
    builder.SetInsertPoint(op_le_block);
    // llvm::Value *return_from_op_le = builder.CreateCall(step_in_C, step_args);
    llvm::Value *return_from_op_le = create_op_le_block();

    //create OP_FORLOOP
    builder.SetInsertPoint(op_forloop_block);
    // llvm::Value *return_from_op_forloop = builder.CreateCall(step_in_C, step_args);
    llvm::Value *return_from_op_forloop = create_op_forloop_block();

    //create OP_FORPREP
    builder.SetInsertPoint(op_forprep_block);
    // llvm::Value *return_from_op_forprep = builder.CreateCall(step_in_C, step_args);
    llvm::Value *return_from_op_forprep = create_op_forprep_block();

    //create DEFAULT
    builder.SetInsertPoint(default_block);
    llvm::Value *return_from_op_default = builder.CreateCall(step_in_C, step_args);
    builder.CreateBr(end_block);

    // ERROR BLOCK
    builder.SetInsertPoint(error_block);
    builder.CreateCall(error);
    builder.CreateUnreachable();

    //DEBUG error
    // builder.SetInsertPoint(error_block_1);
    // builder.CreateCall(error_1);
    // builder.CreateUnreachable();
    //
    // builder.SetInsertPoint(error_block_2);
    // builder.CreateCall(error_2);
    // builder.CreateUnreachable();
    //
    // builder.SetInsertPoint(error_block_3);
    // builder.CreateCall(error_3);
    // builder.CreateUnreachable();
    //
    // builder.SetInsertPoint(error_block_4);
    // builder.CreateCall(error_4);
    // builder.CreateUnreachable();
    //
    // builder.SetInsertPoint(error_block_5);
    // builder.CreateCall(error_5);
    // builder.CreateUnreachable();
    //
    // builder.SetInsertPoint(error_block_6);
    // builder.CreateCall(error_6);
    // builder.CreateUnreachable();
    //

    //create END label
    builder.SetInsertPoint(end_block);
    //create return_phi_node
    return_phi_node = builder.CreatePHI(llvm::Type::getInt64Ty(context), 19);
    return_phi_node->addIncoming(return_from_op_move, op_move_block);
    return_phi_node->addIncoming(return_from_op_loadk, op_loadk_block);
    return_phi_node->addIncoming(return_from_op_add, op_add_3_block);
    return_phi_node->addIncoming(return_from_op_add, op_add_10_block);
    return_phi_node->addIncoming(return_from_op_sub, op_sub_3_block);
    return_phi_node->addIncoming(return_from_op_sub, op_sub_10_block);
    return_phi_node->addIncoming(return_from_op_mul, op_mul_3_block);
    return_phi_node->addIncoming(return_from_op_mul, op_mul_10_block);
    return_phi_node->addIncoming(return_from_op_div, op_div_10_block);
    return_phi_node->addIncoming(return_from_op_mod, op_mod_3_block);
    return_phi_node->addIncoming(return_from_op_mod, op_mod_10_block);
    return_phi_node->addIncoming(return_from_op_idiv, op_idiv_3_block);
    return_phi_node->addIncoming(return_from_op_idiv, op_idiv_10_block);
    return_phi_node->addIncoming(return_from_op_pow, op_pow_10_block);
    return_phi_node->addIncoming(return_from_op_unm, op_unm_1_block);
    return_phi_node->addIncoming(return_from_op_unm, op_unm_2_block);
    return_phi_node->addIncoming(return_from_op_not, op_not_3_block);
    return_phi_node->addIncoming(return_from_op_jmp, op_jmp_block);
    return_phi_node->addIncoming(return_from_op_eq, op_eq_10_block);
    return_phi_node->addIncoming(return_from_op_lt, op_lt_11_block);
    return_phi_node->addIncoming(return_from_op_le, op_le_11_block);
    return_phi_node->addIncoming(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)), op_forloop_13_block);
    return_phi_node->addIncoming(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)), op_forloop_17_block);
    return_phi_node->addIncoming(return_from_op_forloop, op_forloop_18_block);
    return_phi_node->addIncoming(return_from_op_forprep, op_forprep_11_block);
    return_phi_node->addIncoming(return_from_op_default, default_block);
    builder.CreateRet(return_phi_node);


    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_MOVE,    true)), op_move_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_LOADK,   true)), op_loadk_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_ADD,     true)), op_add_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_SUB,     true)), op_sub_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_MUL,     true)), op_mul_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_DIV,     true)), op_div_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_MOD,     true)), op_mod_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_IDIV,    true)), op_idiv_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_POW,     true)), op_pow_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_UNM,     true)), op_unm_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_NOT,     true)), op_not_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_JMP,     true)), op_jmp_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_EQ,      true)), op_eq_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_LT,      true)), op_lt_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_LE,      true)), op_le_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_FORLOOP, true)), op_forloop_block);
    theSwitch->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, OP_FORPREP, true)), op_forprep_block);


    //dump module to check ir
    // std::cout << "Module:\n";
    // module->dump();
    // std::cout << "\n";

    //dump module to check ir
    freopen("step-base.ll", "w", stderr);
    module->dump();

    return 0;
}


llvm::Value* is_numerical(llvm::Value *v) {
    std::vector<llvm::Value *> temp;
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *v_type_ptr = builder.CreateInBoundsGEP(value_struct_type, v, temp);
    llvm::Value *v_type = builder.CreateLoad(v_type_ptr);

    llvm::Value *is_int = builder.CreateICmpEQ(v_type, llvm::ConstantInt::get(context, llvm::APInt(32, LUA_TNUMINT, true)));
    llvm::Value *is_float = builder.CreateICmpEQ(v_type, llvm::ConstantInt::get(context, llvm::APInt(32, LUA_TNUMFLT, true)));

    return builder.CreateOr(is_int, is_float);
}

llvm::Value* create_A() {
    llvm::Value *temp_inst = builder.CreateLShr(_inst, llvm::ConstantInt::get(context, llvm::APInt(32, POS_A, true)));
    llvm::Value *a_inst = builder.CreateAnd(temp_inst, llvm::ConstantInt::get(context, llvm::APInt(32, 0xFF, true)));

    return a_inst;
}

llvm::Value* create_B() {
    llvm::Value *temp_inst = builder.CreateLShr(_inst, llvm::ConstantInt::get(context, llvm::APInt(32, POS_B, true)));
    llvm::Value *b_inst = builder.CreateAnd(temp_inst, llvm::ConstantInt::get(context, llvm::APInt(32, 0x1FF, true)));

    return b_inst;
}

llvm::Value* create_C() {
    llvm::Value *temp_inst = builder.CreateLShr(_inst, llvm::ConstantInt::get(context, llvm::APInt(32, POS_C, true)));
    llvm::Value *c_inst = builder.CreateAnd(temp_inst, llvm::ConstantInt::get(context, llvm::APInt(32, 0x1FF, true)));

    return c_inst;
}

llvm::Value* create_bx() {
    llvm::Value *temp_inst = builder.CreateLShr(_inst, llvm::ConstantInt::get(context, llvm::APInt(32, POS_Bx, true)));
    llvm::Value *bx_inst = builder.CreateAnd(temp_inst, llvm::ConstantInt::get(context, llvm::APInt(32, 0x3ffff, true)));

    return bx_inst;
}


llvm::Value* create_isk(llvm::Value *n) {
    return builder.CreateAnd(n, llvm::ConstantInt::get(context, llvm::APInt(32, BITRK, true)));
}

llvm::Value* create_indexk(llvm::Value * x_inst) {
    return builder.CreateAnd(x_inst, llvm::ConstantInt::get(context, llvm::APInt(32, 255, false)));
}

std::vector<llvm::Value *> create_rk(llvm::Value * x_inst, llvm::Value *registers_LD) {
    std::vector<llvm::Value *> ret;

    llvm::Value *isk = create_isk(x_inst);

    llvm::Value *cmp_eq = builder.CreateICmpEQ(isk, llvm::ConstantInt::get(context, llvm::APInt(32, 0, true))); // isk == 0

    llvm::Value *indexk = create_indexk(x_inst);

    llvm::Value *select_arg = builder.CreateSelect(cmp_eq, x_inst, indexk);
    ret.push_back(select_arg);

    llvm::Value *select_ld = builder.CreateSelect(cmp_eq, registers_LD, _constants);
    ret.push_back(select_ld);

    std::vector<llvm::Value *> temp;
    temp.push_back(select_arg);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    ret.push_back(builder.CreateInBoundsGEP(value_struct_type, select_ld,temp));

    return ret;
}

// llvm::Value* create_k() {

// }

llvm::Value * create_bitcast(llvm::Value *x) {
    llvm::Value *xth_bitcast = builder.CreateBitCast(x, llvm::Type::getInt8PtrTy(context));

    return xth_bitcast;
}


/* R(A()) */
std::vector<llvm::Value *> create_ra(llvm::Value *registers_LD) {
    std::vector<llvm::Value *> ret;

    /* R(A()) */
    llvm::Value *a_inst = create_A();

    std::vector<llvm::Value *> temp;
    temp.push_back(a_inst);
    llvm::Value *registers_ath = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp); /* Value *a */

    ret.push_back(a_inst);
    ret.push_back(registers_ath);
    return ret;
}

/* R(B()) */
std::vector<llvm::Value *> create_rb(llvm::Value *registers_LD) {
    std::vector<llvm::Value *> ret;

    /* R(B()) */
    llvm::Value *b_inst = create_B();

    std::vector<llvm::Value *> temp;
    temp.push_back(b_inst);
    llvm::Value *registers_bth = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp); /* Value *b */

    ret.push_back(b_inst);
    ret.push_back(registers_bth);
    return ret;
}

/* K(Bx()) */
llvm::Value* create_krbx() {
    /* K(Bx()) */
    llvm::Value *bx_inst = create_bx();

    std::vector<llvm::Value *> temp;
    temp.push_back(bx_inst);
    llvm::Value *registers_bxth = builder.CreateInBoundsGEP(value_struct_type, _constants, temp); /* Value *b */

    return registers_bxth;
}

llvm::Value* create_sbx() {
    llvm::Value *lshr = builder.CreateLShr(_inst, llvm::ConstantInt::get(context, llvm::APInt(32, 14, true)));
    llvm::Value *add = builder.CreateAdd(lshr, llvm::ConstantInt::get(context, llvm::APInt(32, -131071, true)));
    llvm::Value *sext = builder.CreateSExt(add, llvm::Type::getInt64Ty(context));

    return sext;
}







/* OPCODES */
llvm::Value* create_op_move_block() {
    op_move_block = llvm::BasicBlock::Create(context, "op_move", step_func);

    builder.SetInsertPoint(op_move_block);

    std::vector<llvm::Value *> temp;
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true))); //registers offset
    llvm::Value *registers_GEP = builder.CreateInBoundsGEP(miniluastate_struct_type, _mls, temp);
    llvm::Value *registers_LD = builder.CreateLoad(registers_GEP);

    llvm::Value *ra = create_ra(registers_LD)[1];
    llvm::Value *ra_bitcast = create_bitcast(ra);
    llvm::Value *rb = create_rb(registers_LD)[1];
    llvm::Value *rb_bitcast = create_bitcast(rb);

    temp.clear();
    temp.push_back(ra_bitcast);
    temp.push_back(rb_bitcast);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 16, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 8, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(1, 0, true)));
    builder.CreateCall(llvm_memcpy, temp);

    return llvm::ConstantInt::get(context, llvm::APInt(64, 0, true));
}


llvm::Value* create_op_loadk_block() {
    op_loadk_block = llvm::BasicBlock::Create(context, "op_loadk", step_func);

    builder.SetInsertPoint(op_loadk_block);

    std::vector<llvm::Value *> temp;
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true))); //registers offset
    llvm::Value *registers_GEP = builder.CreateInBoundsGEP(miniluastate_struct_type, _mls, temp);
    llvm::Value *registers_LD = builder.CreateLoad(registers_GEP);

    llvm::Value *ra = create_ra(registers_LD)[1];
    llvm::Value *ra_bitcast = create_bitcast(ra);
    llvm::Value *rb = create_krbx();
    llvm::Value *rb_bitcast = create_bitcast(rb);

    temp.clear();
    temp.push_back(ra_bitcast);
    temp.push_back(rb_bitcast);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 16, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 8, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(1, 0, true)));
    builder.CreateCall(llvm_memcpy, temp);

    return llvm::ConstantInt::get(context, llvm::APInt(64, 0, true));
}

llvm::Value* create_op_add_block() {
    op_add_block = llvm::BasicBlock::Create(context, "op_add", step_func);

    op_add_1_block = llvm::BasicBlock::Create(context, "op_add_1", step_func);
    op_add_2_block = llvm::BasicBlock::Create(context, "op_add_2", step_func);
    op_add_3_block = llvm::BasicBlock::Create(context, "op_add_3", step_func);
    op_add_4_block = llvm::BasicBlock::Create(context, "op_add_4", step_func);
    op_add_5_block = llvm::BasicBlock::Create(context, "op_add_5", step_func);
    op_add_6_block = llvm::BasicBlock::Create(context, "op_add_6", step_func);
    op_add_7_block = llvm::BasicBlock::Create(context, "op_add_7", step_func);
    op_add_8_block = llvm::BasicBlock::Create(context, "op_add_8", step_func);
    op_add_9_block = llvm::BasicBlock::Create(context, "op_add_9", step_func);
    op_add_10_block = llvm::BasicBlock::Create(context, "op_add_10", step_func);

    builder.SetInsertPoint(op_add_block);

    //
    std::vector<llvm::Value *> temp;
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true))); //registers offset
    llvm::Value *registers_GEP = builder.CreateInBoundsGEP(miniluastate_struct_type, _mls, temp);
    llvm::Value *registers_LD = builder.CreateLoad(registers_GEP);

    std::vector<llvm::Value *> ra_ret = create_ra(registers_LD);
    llvm::Value *ra = ra_ret[1]; //Value *a = R(A(inst));

    llvm::Value *b_inst = create_B();
    std::vector<llvm::Value *> rkb_return = create_rk(b_inst, registers_LD);
    llvm::Value *rkb = rkb_return[2];
    llvm::Value *rkb_LD = builder.CreateLoad(rkb);
    llvm::Value *rkb_or = builder.CreateOr(llvm::ConstantInt::get(context, llvm::APInt(32, 16, true)), rkb_LD);
    llvm::Value *rkb_is_numerical_condition = builder.CreateICmpEQ(rkb_or, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    builder.CreateCondBr(rkb_is_numerical_condition, op_add_1_block, error_block);

    //op_add_1_block
    builder.SetInsertPoint(op_add_1_block);
    llvm::Value *c_inst = create_C();
    std::vector<llvm::Value *> rkc_return = create_rk(c_inst, registers_LD);
    llvm::Value *rkc = rkc_return[2];
    llvm::Value *rkc_LD = builder.CreateLoad(rkc);
    llvm::Value *rkc_or = builder.CreateOr(llvm::ConstantInt::get(context, llvm::APInt(32, 16, true)), rkc_LD);
    llvm::Value *rkc_is_numerical_condition = builder.CreateICmpEQ(rkc_or, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    builder.CreateCondBr(rkc_is_numerical_condition, op_add_2_block, error_block);

    builder.SetInsertPoint(op_add_2_block);
    llvm::Value *rkb_is_int = builder.CreateICmpEQ(rkb_LD, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    llvm::Value *rkc_is_int = builder.CreateICmpEQ(rkc_LD, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    llvm::Value *both_int = builder.CreateAnd(rkb_is_int, rkc_is_int);
    builder.CreateCondBr(both_int, op_add_3_block, op_add_4_block);

    // XXX TODO discover why it seg faults when adding the third argument (the 0 on the commented line. On rkb and rkc!!!
    // op_add_1_block true
    builder.SetInsertPoint(op_add_3_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_value_LD = builder.CreateLoad(b_value_GEP);

    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_value_LD = builder.CreateLoad(c_value_GEP);

    //ADD
    llvm::Value *add_b_c = builder.CreateAdd(b_value_LD, c_value_LD);

    temp.clear();
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_type = builder.CreateInBoundsGEP(value_struct_type, ra, temp);
    builder.CreateStore(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), a_type);

    temp.clear();
    temp.push_back(ra_ret[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_value = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    builder.CreateStore(add_b_c, a_value);
    builder.CreateBr(end_block);

    // op_add_1_block false
    builder.SetInsertPoint(op_add_4_block);
    llvm::SwitchInst *switch_type = builder.CreateSwitch(rkb_LD, error_block, 2);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_add_5_block);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_add_6_block);

    // op_add_5_block (3)
    builder.SetInsertPoint(op_add_5_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP_2 = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_bitcast = builder.CreateBitCast(b_value_GEP_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *b_load = builder.CreateLoad(llvm::Type::getDoubleTy(context), b_bitcast);
    builder.CreateBr(op_add_7_block);

    // op_add_6_block (19)
    builder.SetInsertPoint(op_add_6_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP_3 = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_load_2 = builder.CreateLoad(b_value_GEP_3);
    llvm::Value *b_sitofp = builder.CreateSIToFP(b_load_2, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_add_7_block);


    // op_add_7_block
    builder.SetInsertPoint(op_add_7_block);
    llvm::PHINode *phi_node = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node->addIncoming(b_load, op_add_5_block);
    phi_node->addIncoming(b_sitofp, op_add_6_block);
    llvm::SwitchInst *switch_type_2 = builder.CreateSwitch(rkc_LD, error_block, 2);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_add_8_block);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_add_9_block);

    // op_add_8_block (3)
    builder.SetInsertPoint(op_add_8_block);
    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP_2 = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_bitcast_2 = builder.CreateBitCast(c_value_GEP_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *c_load = builder.CreateLoad(llvm::Type::getDoubleTy(context), c_bitcast_2);
    builder.CreateBr(op_add_10_block);

    // op_add_9_block (19)
    builder.SetInsertPoint(op_add_9_block);
    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP_3 = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_load_2 = builder.CreateLoad(c_value_GEP_3);
    llvm::Value *c_sitofp = builder.CreateSIToFP(c_load_2, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_add_10_block);

    builder.SetInsertPoint(op_add_10_block);
    llvm::PHINode *phi_node_2 = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node_2->addIncoming(c_load, op_add_8_block);
    phi_node_2->addIncoming(c_sitofp, op_add_9_block);
    llvm::Value *add_b_c_2 = builder.CreateFAdd(phi_node, phi_node_2);
    temp.clear();
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_type_2 = builder.CreateInBoundsGEP(value_struct_type, ra, temp);
    builder.CreateStore(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), a_type_2);
    temp.clear();
    temp.push_back(ra_ret[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_value_2 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *a_bitcast_2 = builder.CreateBitCast(a_value_2, llvm::Type::getDoublePtrTy(context));
    builder.CreateStore(add_b_c_2, a_bitcast_2);
    builder.CreateBr(end_block);
    //


    return llvm::ConstantInt::get(context, llvm::APInt(64, 0, true));
}

llvm::Value* create_op_sub_block() {
    op_sub_block = llvm::BasicBlock::Create(context, "op_sub", step_func);

    op_sub_1_block = llvm::BasicBlock::Create(context, "op_sub_1", step_func);
    op_sub_2_block = llvm::BasicBlock::Create(context, "op_sub_2", step_func);
    op_sub_3_block = llvm::BasicBlock::Create(context, "op_sub_3", step_func);
    op_sub_4_block = llvm::BasicBlock::Create(context, "op_sub_4", step_func);
    op_sub_5_block = llvm::BasicBlock::Create(context, "op_sub_5", step_func);
    op_sub_6_block = llvm::BasicBlock::Create(context, "op_sub_6", step_func);
    op_sub_7_block = llvm::BasicBlock::Create(context, "op_sub_7", step_func);
    op_sub_8_block = llvm::BasicBlock::Create(context, "op_sub_8", step_func);
    op_sub_9_block = llvm::BasicBlock::Create(context, "op_sub_9", step_func);
    op_sub_10_block = llvm::BasicBlock::Create(context, "op_sub_10", step_func);

    builder.SetInsertPoint(op_sub_block);

    //
    std::vector<llvm::Value *> temp;
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true))); //registers offset
    llvm::Value *registers_GEP = builder.CreateInBoundsGEP(miniluastate_struct_type, _mls, temp);
    llvm::Value *registers_LD = builder.CreateLoad(registers_GEP);

    std::vector<llvm::Value *> ra_ret = create_ra(registers_LD);
    llvm::Value *ra = ra_ret[1]; //Value *a = R(A(inst));

    llvm::Value *b_inst = create_B();
    std::vector<llvm::Value *> rkb_return = create_rk(b_inst, registers_LD);
    llvm::Value *rkb = rkb_return[2];
    llvm::Value *rkb_LD = builder.CreateLoad(rkb);
    llvm::Value *rkb_or = builder.CreateOr(llvm::ConstantInt::get(context, llvm::APInt(32, 16, true)), rkb_LD);
    llvm::Value *rkb_is_numerical_condition = builder.CreateICmpEQ(rkb_or, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    builder.CreateCondBr(rkb_is_numerical_condition, op_sub_1_block, error_block);

    //op_sub_1_block
    builder.SetInsertPoint(op_sub_1_block);
    llvm::Value *c_inst = create_C();
    std::vector<llvm::Value *> rkc_return = create_rk(c_inst, registers_LD);
    llvm::Value *rkc = rkc_return[2];
    llvm::Value *rkc_LD = builder.CreateLoad(rkc);
    llvm::Value *rkc_or = builder.CreateOr(llvm::ConstantInt::get(context, llvm::APInt(32, 16, true)), rkc_LD);
    llvm::Value *rkc_is_numerical_condition = builder.CreateICmpEQ(rkc_or, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    builder.CreateCondBr(rkc_is_numerical_condition, op_sub_2_block, error_block);

    builder.SetInsertPoint(op_sub_2_block);
    llvm::Value *rkb_is_int = builder.CreateICmpEQ(rkb_LD, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    llvm::Value *rkc_is_int = builder.CreateICmpEQ(rkc_LD, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    llvm::Value *both_int = builder.CreateAnd(rkb_is_int, rkc_is_int);
    builder.CreateCondBr(both_int, op_sub_3_block, op_sub_4_block);

    // XXX TODO discover why it seg faults when adding the third argument (the 0 on the commented line. On rkb and rkc!!!
    // op_sub_1_block true
    builder.SetInsertPoint(op_sub_3_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_value_LD = builder.CreateLoad(b_value_GEP);

    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_value_LD = builder.CreateLoad(c_value_GEP);

    //SUB
    llvm::Value *sub_b_c = builder.CreateSub(b_value_LD, c_value_LD);

    temp.clear();
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_type = builder.CreateInBoundsGEP(value_struct_type, ra, temp);
    builder.CreateStore(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), a_type);

    temp.clear();
    temp.push_back(ra_ret[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_value = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    builder.CreateStore(sub_b_c, a_value);
    builder.CreateBr(end_block);

    // op_sub_1_block false
    builder.SetInsertPoint(op_sub_4_block);
    llvm::SwitchInst *switch_type = builder.CreateSwitch(rkb_LD, error_block, 2);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_sub_5_block);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_sub_6_block);

    // op_sub_5_block (3)
    builder.SetInsertPoint(op_sub_5_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP_2 = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_bitcast = builder.CreateBitCast(b_value_GEP_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *b_load = builder.CreateLoad(llvm::Type::getDoubleTy(context), b_bitcast);
    builder.CreateBr(op_sub_7_block);

    // op_sub_6_block (19)
    builder.SetInsertPoint(op_sub_6_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP_3 = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_load_2 = builder.CreateLoad(b_value_GEP_3);
    llvm::Value *b_sitofp = builder.CreateSIToFP(b_load_2, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_sub_7_block);


    // op_sub_7_block
    builder.SetInsertPoint(op_sub_7_block);
    llvm::PHINode *phi_node = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node->addIncoming(b_load, op_sub_5_block);
    phi_node->addIncoming(b_sitofp, op_sub_6_block);
    llvm::SwitchInst *switch_type_2 = builder.CreateSwitch(rkc_LD, error_block, 2);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_sub_8_block);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_sub_9_block);

    // op_sub_8_block (3)
    builder.SetInsertPoint(op_sub_8_block);
    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP_2 = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_bitcast_2 = builder.CreateBitCast(c_value_GEP_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *c_load = builder.CreateLoad(llvm::Type::getDoubleTy(context), c_bitcast_2);
    builder.CreateBr(op_sub_10_block);

    // op_sub_9_block (19)
    builder.SetInsertPoint(op_sub_9_block);
    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP_3 = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_load_2 = builder.CreateLoad(c_value_GEP_3);
    llvm::Value *c_sitofp = builder.CreateSIToFP(c_load_2, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_sub_10_block);

    builder.SetInsertPoint(op_sub_10_block);
    llvm::PHINode *phi_node_2 = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node_2->addIncoming(c_load, op_sub_8_block);
    phi_node_2->addIncoming(c_sitofp, op_sub_9_block);
    llvm::Value *sub_b_c_2 = builder.CreateFSub(phi_node, phi_node_2);
    temp.clear();
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_type_2 = builder.CreateInBoundsGEP(value_struct_type, ra, temp);
    builder.CreateStore(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), a_type_2);
    temp.clear();
    temp.push_back(ra_ret[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_value_2 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *a_bitcast_2 = builder.CreateBitCast(a_value_2, llvm::Type::getDoublePtrTy(context));
    builder.CreateStore(sub_b_c_2, a_bitcast_2);
    builder.CreateBr(end_block);
    //


    return llvm::ConstantInt::get(context, llvm::APInt(64, 0, true));
}

llvm::Value* create_op_mul_block() {
    op_mul_block = llvm::BasicBlock::Create(context, "op_mul", step_func);

    op_mul_1_block = llvm::BasicBlock::Create(context, "op_mul_1", step_func);
    op_mul_2_block = llvm::BasicBlock::Create(context, "op_mul_2", step_func);
    op_mul_3_block = llvm::BasicBlock::Create(context, "op_mul_3", step_func);
    op_mul_4_block = llvm::BasicBlock::Create(context, "op_mul_4", step_func);
    op_mul_5_block = llvm::BasicBlock::Create(context, "op_mul_5", step_func);
    op_mul_6_block = llvm::BasicBlock::Create(context, "op_mul_6", step_func);
    op_mul_7_block = llvm::BasicBlock::Create(context, "op_mul_7", step_func);
    op_mul_8_block = llvm::BasicBlock::Create(context, "op_mul_8", step_func);
    op_mul_9_block = llvm::BasicBlock::Create(context, "op_mul_9", step_func);
    op_mul_10_block = llvm::BasicBlock::Create(context, "op_mul_10", step_func);

    builder.SetInsertPoint(op_mul_block);

    //
    std::vector<llvm::Value *> temp;
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true))); //registers offset
    llvm::Value *registers_GEP = builder.CreateInBoundsGEP(miniluastate_struct_type, _mls, temp);
    llvm::Value *registers_LD = builder.CreateLoad(registers_GEP);

    std::vector<llvm::Value *> ra_ret = create_ra(registers_LD);
    llvm::Value *ra = ra_ret[1]; //Value *a = R(A(inst));

    llvm::Value *b_inst = create_B();
    std::vector<llvm::Value *> rkb_return = create_rk(b_inst, registers_LD);
    llvm::Value *rkb = rkb_return[2];
    llvm::Value *rkb_LD = builder.CreateLoad(rkb);
    llvm::Value *rkb_or = builder.CreateOr(llvm::ConstantInt::get(context, llvm::APInt(32, 16, true)), rkb_LD);
    llvm::Value *rkb_is_numerical_condition = builder.CreateICmpEQ(rkb_or, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    builder.CreateCondBr(rkb_is_numerical_condition, op_mul_1_block, error_block);

    //op_mul_1_block
    builder.SetInsertPoint(op_mul_1_block);
    llvm::Value *c_inst = create_C();
    std::vector<llvm::Value *> rkc_return = create_rk(c_inst, registers_LD);
    llvm::Value *rkc = rkc_return[2];
    llvm::Value *rkc_LD = builder.CreateLoad(rkc);
    llvm::Value *rkc_or = builder.CreateOr(llvm::ConstantInt::get(context, llvm::APInt(32, 16, true)), rkc_LD);
    llvm::Value *rkc_is_numerical_condition = builder.CreateICmpEQ(rkc_or, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    builder.CreateCondBr(rkc_is_numerical_condition, op_mul_2_block, error_block);

    builder.SetInsertPoint(op_mul_2_block);
    llvm::Value *rkb_is_int = builder.CreateICmpEQ(rkb_LD, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    llvm::Value *rkc_is_int = builder.CreateICmpEQ(rkc_LD, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    llvm::Value *both_int = builder.CreateAnd(rkb_is_int, rkc_is_int);
    builder.CreateCondBr(both_int, op_mul_3_block, op_mul_4_block);

    // XXX TODO discover why it seg faults when adding the third argument (the 0 on the commented line. On rkb and rkc!!!
    // op_mul_1_block true
    builder.SetInsertPoint(op_mul_3_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_value_LD = builder.CreateLoad(b_value_GEP);

    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_value_LD = builder.CreateLoad(c_value_GEP);

    //MUL
    llvm::Value *mul_b_c = builder.CreateMul(b_value_LD, c_value_LD);

    temp.clear();
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_type = builder.CreateInBoundsGEP(value_struct_type, ra, temp);
    builder.CreateStore(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), a_type);

    temp.clear();
    temp.push_back(ra_ret[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_value = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    builder.CreateStore(mul_b_c, a_value);
    builder.CreateBr(end_block);

    // op_mul_1_block false
    builder.SetInsertPoint(op_mul_4_block);
    llvm::SwitchInst *switch_type = builder.CreateSwitch(rkb_LD, error_block, 2);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_mul_5_block);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_mul_6_block);

    // op_mul_5_block (3)
    builder.SetInsertPoint(op_mul_5_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP_2 = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_bitcast = builder.CreateBitCast(b_value_GEP_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *b_load = builder.CreateLoad(llvm::Type::getDoubleTy(context), b_bitcast);
    builder.CreateBr(op_mul_7_block);

    // op_mul_6_block (19)
    builder.SetInsertPoint(op_mul_6_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP_3 = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_load_2 = builder.CreateLoad(b_value_GEP_3);
    llvm::Value *b_sitofp = builder.CreateSIToFP(b_load_2, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_mul_7_block);


    // op_mul_7_block
    builder.SetInsertPoint(op_mul_7_block);
    llvm::PHINode *phi_node = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node->addIncoming(b_load, op_mul_5_block);
    phi_node->addIncoming(b_sitofp, op_mul_6_block);
    llvm::SwitchInst *switch_type_2 = builder.CreateSwitch(rkc_LD, error_block, 2);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_mul_8_block);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_mul_9_block);

    // op_mul_8_block (3)
    builder.SetInsertPoint(op_mul_8_block);
    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP_2 = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_bitcast_2 = builder.CreateBitCast(c_value_GEP_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *c_load = builder.CreateLoad(llvm::Type::getDoubleTy(context), c_bitcast_2);
    builder.CreateBr(op_mul_10_block);

    // op_mul_9_block (19)
    builder.SetInsertPoint(op_mul_9_block);
    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP_3 = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_load_2 = builder.CreateLoad(c_value_GEP_3);
    llvm::Value *c_sitofp = builder.CreateSIToFP(c_load_2, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_mul_10_block);

    builder.SetInsertPoint(op_mul_10_block);
    llvm::PHINode *phi_node_2 = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node_2->addIncoming(c_load, op_mul_8_block);
    phi_node_2->addIncoming(c_sitofp, op_mul_9_block);
    llvm::Value *mul_b_c_2 = builder.CreateFMul(phi_node, phi_node_2);
    temp.clear();
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_type_2 = builder.CreateInBoundsGEP(value_struct_type, ra, temp);
    builder.CreateStore(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), a_type_2);
    temp.clear();
    temp.push_back(ra_ret[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_value_2 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *a_bitcast_2 = builder.CreateBitCast(a_value_2, llvm::Type::getDoublePtrTy(context));
    builder.CreateStore(mul_b_c_2, a_bitcast_2);
    builder.CreateBr(end_block);
    //


    return llvm::ConstantInt::get(context, llvm::APInt(64, 0, true));
}

llvm::Value* create_op_div_block() {
    op_div_block = llvm::BasicBlock::Create(context, "op_div", step_func);

    op_div_1_block = llvm::BasicBlock::Create(context, "op_div_1", step_func);
    op_div_4_block = llvm::BasicBlock::Create(context, "op_div_4", step_func);
    op_div_5_block = llvm::BasicBlock::Create(context, "op_div_5", step_func);
    op_div_6_block = llvm::BasicBlock::Create(context, "op_div_6", step_func);
    op_div_7_block = llvm::BasicBlock::Create(context, "op_div_7", step_func);
    op_div_8_block = llvm::BasicBlock::Create(context, "op_div_8", step_func);
    op_div_9_block = llvm::BasicBlock::Create(context, "op_div_9", step_func);
    op_div_10_block = llvm::BasicBlock::Create(context, "op_div_10", step_func);

    builder.SetInsertPoint(op_div_block);

    //
    std::vector<llvm::Value *> temp;
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true))); //registers offset
    llvm::Value *registers_GEP = builder.CreateInBoundsGEP(miniluastate_struct_type, _mls, temp);
    llvm::Value *registers_LD = builder.CreateLoad(registers_GEP);

    std::vector<llvm::Value *> ra_ret = create_ra(registers_LD);
    llvm::Value *ra = ra_ret[1]; //Value *a = R(A(inst));

    llvm::Value *b_inst = create_B();
    std::vector<llvm::Value *> rkb_return = create_rk(b_inst, registers_LD);
    llvm::Value *rkb = rkb_return[2];
    llvm::Value *rkb_LD = builder.CreateLoad(rkb);
    llvm::Value *rkb_or = builder.CreateOr(llvm::ConstantInt::get(context, llvm::APInt(32, 16, true)), rkb_LD);
    llvm::Value *rkb_is_numerical_condition = builder.CreateICmpEQ(rkb_or, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    builder.CreateCondBr(rkb_is_numerical_condition, op_div_1_block, error_block);

    //op_div_1_block
    builder.SetInsertPoint(op_div_1_block);
    llvm::Value *c_inst = create_C();
    std::vector<llvm::Value *> rkc_return = create_rk(c_inst, registers_LD);
    llvm::Value *rkc = rkc_return[2];
    llvm::Value *rkc_LD = builder.CreateLoad(rkc);
    llvm::Value *rkc_or = builder.CreateOr(llvm::ConstantInt::get(context, llvm::APInt(32, 16, true)), rkc_LD);
    llvm::Value *rkc_is_numerical_condition = builder.CreateICmpEQ(rkc_or, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    builder.CreateCondBr(rkc_is_numerical_condition, op_div_4_block, error_block);

    // op_div_4_block false
    builder.SetInsertPoint(op_div_4_block);
    llvm::SwitchInst *switch_type = builder.CreateSwitch(rkb_LD, error_block, 2);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_div_5_block);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_div_6_block);

    // op_div_5_block (3)
    builder.SetInsertPoint(op_div_5_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP_2 = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_bitcast = builder.CreateBitCast(b_value_GEP_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *b_load = builder.CreateLoad(llvm::Type::getDoubleTy(context), b_bitcast);
    builder.CreateBr(op_div_7_block);

    // op_div_6_block (19)
    builder.SetInsertPoint(op_div_6_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP_3 = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_load_2 = builder.CreateLoad(b_value_GEP_3);
    llvm::Value *b_sitofp = builder.CreateSIToFP(b_load_2, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_div_7_block);


    // op_div_7_block
    builder.SetInsertPoint(op_div_7_block);
    llvm::PHINode *phi_node = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node->addIncoming(b_load, op_div_5_block);
    phi_node->addIncoming(b_sitofp, op_div_6_block);
    llvm::SwitchInst *switch_type_2 = builder.CreateSwitch(rkc_LD, error_block, 2);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_div_8_block);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_div_9_block);

    // op_div_8_block (3)
    builder.SetInsertPoint(op_div_8_block);
    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP_2 = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_bitcast_2 = builder.CreateBitCast(c_value_GEP_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *c_load = builder.CreateLoad(llvm::Type::getDoubleTy(context), c_bitcast_2);
    builder.CreateBr(op_div_10_block);

    // op_div_9_block (19)
    builder.SetInsertPoint(op_div_9_block);
    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP_3 = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_load_2 = builder.CreateLoad(c_value_GEP_3);
    llvm::Value *c_sitofp = builder.CreateSIToFP(c_load_2, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_div_10_block);

    builder.SetInsertPoint(op_div_10_block);
    llvm::PHINode *phi_node_2 = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node_2->addIncoming(c_load, op_div_8_block);
    phi_node_2->addIncoming(c_sitofp, op_div_9_block);
    llvm::Value *div_b_c_2 = builder.CreateFDiv(phi_node, phi_node_2);
    temp.clear();
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_type_2 = builder.CreateInBoundsGEP(value_struct_type, ra, temp);
    builder.CreateStore(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), a_type_2);
    temp.clear();
    temp.push_back(ra_ret[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_value_2 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *a_bitcast_2 = builder.CreateBitCast(a_value_2, llvm::Type::getDoublePtrTy(context));
    builder.CreateStore(div_b_c_2, a_bitcast_2);
    builder.CreateBr(end_block);
    //


    return llvm::ConstantInt::get(context, llvm::APInt(64, 0, true));
}



llvm::Value* create_op_mod_block() {
    op_mod_block = llvm::BasicBlock::Create(context, "op_mod", step_func);

    op_mod_1_block = llvm::BasicBlock::Create(context, "op_mod_1", step_func);
    op_mod_2_block = llvm::BasicBlock::Create(context, "op_mod_2", step_func);
    op_mod_3_block = llvm::BasicBlock::Create(context, "op_mod_3", step_func);
    op_mod_4_block = llvm::BasicBlock::Create(context, "op_mod_4", step_func);
    op_mod_5_block = llvm::BasicBlock::Create(context, "op_mod_5", step_func);
    op_mod_6_block = llvm::BasicBlock::Create(context, "op_mod_6", step_func);
    op_mod_7_block = llvm::BasicBlock::Create(context, "op_mod_7", step_func);
    op_mod_8_block = llvm::BasicBlock::Create(context, "op_mod_8", step_func);
    op_mod_9_block = llvm::BasicBlock::Create(context, "op_mod_9", step_func);
    op_mod_10_block = llvm::BasicBlock::Create(context, "op_mod_10", step_func);

    builder.SetInsertPoint(op_mod_block);

    //
    std::vector<llvm::Value *> temp;
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true))); //registers offset
    llvm::Value *registers_GEP = builder.CreateInBoundsGEP(miniluastate_struct_type, _mls, temp);
    llvm::Value *registers_LD = builder.CreateLoad(registers_GEP);

    std::vector<llvm::Value *> ra_ret = create_ra(registers_LD);
    llvm::Value *ra = ra_ret[1]; //Value *a = R(A(inst));

    llvm::Value *b_inst = create_B();
    std::vector<llvm::Value *> rkb_return = create_rk(b_inst, registers_LD);
    llvm::Value *rkb = rkb_return[2];
    llvm::Value *rkb_LD = builder.CreateLoad(rkb);
    llvm::Value *rkb_or = builder.CreateOr(llvm::ConstantInt::get(context, llvm::APInt(32, 16, true)), rkb_LD);
    llvm::Value *rkb_is_numerical_condition = builder.CreateICmpEQ(rkb_or, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    builder.CreateCondBr(rkb_is_numerical_condition, op_mod_1_block, error_block);

    //op_mod_1_block
    builder.SetInsertPoint(op_mod_1_block);
    llvm::Value *c_inst = create_C();
    std::vector<llvm::Value *> rkc_return = create_rk(c_inst, registers_LD);
    llvm::Value *rkc = rkc_return[2];
    llvm::Value *rkc_LD = builder.CreateLoad(rkc);
    llvm::Value *rkc_or = builder.CreateOr(llvm::ConstantInt::get(context, llvm::APInt(32, 16, true)), rkc_LD);
    llvm::Value *rkc_is_numerical_condition = builder.CreateICmpEQ(rkc_or, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    builder.CreateCondBr(rkc_is_numerical_condition, op_mod_2_block, error_block);

    //op_mod_2_block
    builder.SetInsertPoint(op_mod_2_block);
    llvm::Value *rkb_is_int = builder.CreateICmpEQ(rkb_LD, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    llvm::Value *rkc_is_int = builder.CreateICmpEQ(rkc_LD, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    llvm::Value *both_int = builder.CreateAnd(rkb_is_int, rkc_is_int);
    builder.CreateCondBr(both_int, op_mod_3_block, op_mod_4_block);

    // XXX TODO discover why it seg faults when adding the third argument (the 0 on the commented line. On rkb and rkc!!!
    // op_mod_3_block true
    builder.SetInsertPoint(op_mod_3_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_value_LD = builder.CreateLoad(b_value_GEP);

    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_value_LD = builder.CreateLoad(c_value_GEP);

    //MOD
    llvm::Value *mod_b_c = builder.CreateSRem(b_value_LD, c_value_LD);

    temp.clear();
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_type = builder.CreateInBoundsGEP(value_struct_type, ra, temp);
    builder.CreateStore(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), a_type);

    temp.clear();
    temp.push_back(ra_ret[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_value = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    builder.CreateStore(mod_b_c, a_value);
    builder.CreateBr(end_block);

    // op_mod_1_block false
    builder.SetInsertPoint(op_mod_4_block);
    llvm::SwitchInst *switch_type = builder.CreateSwitch(rkb_LD, error_block, 2);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_mod_5_block);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_mod_6_block);

    // op_mod_5_block (3)
    builder.SetInsertPoint(op_mod_5_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP_2 = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_bitcast = builder.CreateBitCast(b_value_GEP_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *b_load = builder.CreateLoad(llvm::Type::getDoubleTy(context), b_bitcast);
    builder.CreateBr(op_mod_7_block);

    // op_mod_6_block (19)
    builder.SetInsertPoint(op_mod_6_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP_3 = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_load_2 = builder.CreateLoad(b_value_GEP_3);
    llvm::Value *b_sitofp = builder.CreateSIToFP(b_load_2, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_mod_7_block);


    // op_mod_7_block
    builder.SetInsertPoint(op_mod_7_block);
    llvm::PHINode *phi_node = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node->addIncoming(b_load, op_mod_5_block);
    phi_node->addIncoming(b_sitofp, op_mod_6_block);
    llvm::SwitchInst *switch_type_2 = builder.CreateSwitch(rkc_LD, error_block, 2);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_mod_8_block);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_mod_9_block);

    // op_mod_8_block (3)
    builder.SetInsertPoint(op_mod_8_block);
    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP_2 = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_bitcast_2 = builder.CreateBitCast(c_value_GEP_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *c_load = builder.CreateLoad(llvm::Type::getDoubleTy(context), c_bitcast_2);
    builder.CreateBr(op_mod_10_block);

    // op_mod_9_block (19)
    builder.SetInsertPoint(op_mod_9_block);
    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP_3 = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_load_2 = builder.CreateLoad(c_value_GEP_3);
    llvm::Value *c_sitofp = builder.CreateSIToFP(c_load_2, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_mod_10_block);

    builder.SetInsertPoint(op_mod_10_block);
    llvm::PHINode *phi_node_2 = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node_2->addIncoming(c_load, op_mod_8_block);
    phi_node_2->addIncoming(c_sitofp, op_mod_9_block);
    llvm::Value *mod_b_c_2 = builder.CreateFRem(phi_node, phi_node_2);
    temp.clear();
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_type_2 = builder.CreateInBoundsGEP(value_struct_type, ra, temp);
    builder.CreateStore(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), a_type_2);
    temp.clear();
    temp.push_back(ra_ret[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_value_2 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *a_bitcast_2 = builder.CreateBitCast(a_value_2, llvm::Type::getDoublePtrTy(context));
    builder.CreateStore(mod_b_c_2, a_bitcast_2);
    builder.CreateBr(end_block);
    //


    return llvm::ConstantInt::get(context, llvm::APInt(64, 0, true));
}

llvm::Value* create_op_idiv_block() {
    op_idiv_block = llvm::BasicBlock::Create(context, "op_idiv", step_func);

    op_idiv_1_block = llvm::BasicBlock::Create(context, "op_idiv_1", step_func);
    op_idiv_2_block = llvm::BasicBlock::Create(context, "op_idiv_2", step_func);
    op_idiv_3_block = llvm::BasicBlock::Create(context, "op_idiv_3", step_func);
    op_idiv_4_block = llvm::BasicBlock::Create(context, "op_idiv_4", step_func);
    op_idiv_5_block = llvm::BasicBlock::Create(context, "op_idiv_5", step_func);
    op_idiv_6_block = llvm::BasicBlock::Create(context, "op_idiv_6", step_func);
    op_idiv_7_block = llvm::BasicBlock::Create(context, "op_idiv_7", step_func);
    op_idiv_8_block = llvm::BasicBlock::Create(context, "op_idiv_8", step_func);
    op_idiv_9_block = llvm::BasicBlock::Create(context, "op_idiv_9", step_func);
    op_idiv_10_block = llvm::BasicBlock::Create(context, "op_idiv_10", step_func);

    builder.SetInsertPoint(op_idiv_block);

    //
    std::vector<llvm::Value *> temp;
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true))); //registers offset
    llvm::Value *registers_GEP = builder.CreateInBoundsGEP(miniluastate_struct_type, _mls, temp);
    llvm::Value *registers_LD = builder.CreateLoad(registers_GEP);

    std::vector<llvm::Value *> ra_ret = create_ra(registers_LD);
    llvm::Value *ra = ra_ret[1]; //Value *a = R(A(inst));

    llvm::Value *b_inst = create_B();
    std::vector<llvm::Value *> rkb_return = create_rk(b_inst, registers_LD);
    llvm::Value *rkb = rkb_return[2];
    llvm::Value *rkb_LD = builder.CreateLoad(rkb);
    llvm::Value *rkb_or = builder.CreateOr(llvm::ConstantInt::get(context, llvm::APInt(32, 16, true)), rkb_LD);
    llvm::Value *rkb_is_numerical_condition = builder.CreateICmpEQ(rkb_or, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    builder.CreateCondBr(rkb_is_numerical_condition, op_idiv_1_block, error_block);

    //op_idiv_1_block
    builder.SetInsertPoint(op_idiv_1_block);
    llvm::Value *c_inst = create_C();
    std::vector<llvm::Value *> rkc_return = create_rk(c_inst, registers_LD);
    llvm::Value *rkc = rkc_return[2];
    llvm::Value *rkc_LD = builder.CreateLoad(rkc);
    llvm::Value *rkc_or = builder.CreateOr(llvm::ConstantInt::get(context, llvm::APInt(32, 16, true)), rkc_LD);
    llvm::Value *rkc_is_numerical_condition = builder.CreateICmpEQ(rkc_or, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    builder.CreateCondBr(rkc_is_numerical_condition, op_idiv_2_block, error_block);

    builder.SetInsertPoint(op_idiv_2_block);
    llvm::Value *rkb_is_int = builder.CreateICmpEQ(rkb_LD, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    llvm::Value *rkc_is_int = builder.CreateICmpEQ(rkc_LD, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    llvm::Value *both_int = builder.CreateAnd(rkb_is_int, rkc_is_int);
    builder.CreateCondBr(both_int, op_idiv_3_block, op_idiv_4_block);

    // XXX TODO discover why it seg faults when adding the third argument (the 0 on the commented line. On rkb and rkc!!!
    // op_idiv_1_block true
    builder.SetInsertPoint(op_idiv_3_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_value_LD = builder.CreateLoad(b_value_GEP);

    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_value_LD = builder.CreateLoad(c_value_GEP);

    //ADD
    llvm::Value *idiv_b_c = builder.CreateSDiv(b_value_LD, c_value_LD);

    temp.clear();
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_type = builder.CreateInBoundsGEP(value_struct_type, ra, temp);
    builder.CreateStore(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), a_type);

    temp.clear();
    temp.push_back(ra_ret[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_value = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    builder.CreateStore(idiv_b_c, a_value);
    builder.CreateBr(end_block);

    // op_idiv_1_block false
    builder.SetInsertPoint(op_idiv_4_block);
    llvm::SwitchInst *switch_type = builder.CreateSwitch(rkb_LD, error_block, 2);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_idiv_5_block);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_idiv_6_block);

    // op_idiv_5_block (3)
    builder.SetInsertPoint(op_idiv_5_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP_2 = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_bitcast = builder.CreateBitCast(b_value_GEP_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *b_load = builder.CreateLoad(llvm::Type::getDoubleTy(context), b_bitcast);
    builder.CreateBr(op_idiv_7_block);

    // op_idiv_6_block (19)
    builder.SetInsertPoint(op_idiv_6_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP_3 = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_load_2 = builder.CreateLoad(b_value_GEP_3);
    llvm::Value *b_sitofp = builder.CreateSIToFP(b_load_2, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_idiv_7_block);


    // op_idiv_7_block
    builder.SetInsertPoint(op_idiv_7_block);
    llvm::PHINode *phi_node = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node->addIncoming(b_load, op_idiv_5_block);
    phi_node->addIncoming(b_sitofp, op_idiv_6_block);
    llvm::SwitchInst *switch_type_2 = builder.CreateSwitch(rkc_LD, error_block, 2);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_idiv_8_block);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_idiv_9_block);

    // op_idiv_8_block (3)
    builder.SetInsertPoint(op_idiv_8_block);
    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP_2 = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_bitcast_2 = builder.CreateBitCast(c_value_GEP_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *c_load = builder.CreateLoad(llvm::Type::getDoubleTy(context), c_bitcast_2);
    builder.CreateBr(op_idiv_10_block);

    // op_idiv_9_block (19)
    builder.SetInsertPoint(op_idiv_9_block);
    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP_3 = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_load_2 = builder.CreateLoad(c_value_GEP_3);
    llvm::Value *c_sitofp = builder.CreateSIToFP(c_load_2, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_idiv_10_block);

    builder.SetInsertPoint(op_idiv_10_block);
    llvm::PHINode *phi_node_2 = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node_2->addIncoming(c_load, op_idiv_8_block);
    phi_node_2->addIncoming(c_sitofp, op_idiv_9_block);
    llvm::Value *fdiv_b_c = builder.CreateFDiv(phi_node, phi_node_2);
    temp.clear();
    temp.push_back(fdiv_b_c);
    llvm::Value *floor_fdiv_b_c = builder.CreateCall(llvm_floor, temp);
    temp.clear();
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_type_2 = builder.CreateInBoundsGEP(value_struct_type, ra, temp);
    builder.CreateStore(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), a_type_2);
    temp.clear();
    temp.push_back(ra_ret[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_value_2 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *a_bitcast_2 = builder.CreateBitCast(a_value_2, llvm::Type::getDoublePtrTy(context));
    builder.CreateStore(floor_fdiv_b_c, a_bitcast_2);
    builder.CreateBr(end_block);
    //


    return llvm::ConstantInt::get(context, llvm::APInt(64, 0, true));
}

llvm::Value* create_op_pow_block() {
    op_pow_block = llvm::BasicBlock::Create(context, "op_pow", step_func);

    op_pow_1_block = llvm::BasicBlock::Create(context, "op_pow_1", step_func);
    op_pow_4_block = llvm::BasicBlock::Create(context, "op_pow_4", step_func);
    op_pow_5_block = llvm::BasicBlock::Create(context, "op_pow_5", step_func);
    op_pow_6_block = llvm::BasicBlock::Create(context, "op_pow_6", step_func);
    op_pow_7_block = llvm::BasicBlock::Create(context, "op_pow_7", step_func);
    op_pow_8_block = llvm::BasicBlock::Create(context, "op_pow_8", step_func);
    op_pow_9_block = llvm::BasicBlock::Create(context, "op_pow_9", step_func);
    op_pow_10_block = llvm::BasicBlock::Create(context, "op_pow_10", step_func);

    builder.SetInsertPoint(op_pow_block);

    //
    std::vector<llvm::Value *> temp;
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true))); //registers offset
    llvm::Value *registers_GEP = builder.CreateInBoundsGEP(miniluastate_struct_type, _mls, temp);
    llvm::Value *registers_LD = builder.CreateLoad(registers_GEP);

    std::vector<llvm::Value *> ra_ret = create_ra(registers_LD);
    llvm::Value *ra = ra_ret[1]; //Value *a = R(A(inst));

    llvm::Value *b_inst = create_B();
    std::vector<llvm::Value *> rkb_return = create_rk(b_inst, registers_LD);
    llvm::Value *rkb = rkb_return[2];
    llvm::Value *rkb_LD = builder.CreateLoad(rkb);
    llvm::Value *rkb_or = builder.CreateOr(llvm::ConstantInt::get(context, llvm::APInt(32, 16, true)), rkb_LD);
    llvm::Value *rkb_is_numerical_condition = builder.CreateICmpEQ(rkb_or, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    builder.CreateCondBr(rkb_is_numerical_condition, op_pow_1_block, error_block);

    //op_pow_1_block
    builder.SetInsertPoint(op_pow_1_block);
    llvm::Value *c_inst = create_C();
    std::vector<llvm::Value *> rkc_return = create_rk(c_inst, registers_LD);
    llvm::Value *rkc = rkc_return[2];
    llvm::Value *rkc_LD = builder.CreateLoad(rkc);
    llvm::Value *rkc_or = builder.CreateOr(llvm::ConstantInt::get(context, llvm::APInt(32, 16, true)), rkc_LD);
    llvm::Value *rkc_is_numerical_condition = builder.CreateICmpEQ(rkc_or, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    builder.CreateCondBr(rkc_is_numerical_condition, op_pow_4_block, error_block);

    // op_pow_4_block false
    builder.SetInsertPoint(op_pow_4_block);
    llvm::SwitchInst *switch_type = builder.CreateSwitch(rkb_LD, error_block, 2);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_pow_5_block);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_pow_6_block);

    // op_pow_5_block (3)
    builder.SetInsertPoint(op_pow_5_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP_2 = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_bitcast = builder.CreateBitCast(b_value_GEP_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *b_load = builder.CreateLoad(llvm::Type::getDoubleTy(context), b_bitcast);
    builder.CreateBr(op_pow_7_block);

    // op_pow_6_block (19)
    builder.SetInsertPoint(op_pow_6_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP_3 = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_load_2 = builder.CreateLoad(b_value_GEP_3);
    llvm::Value *b_sitofp = builder.CreateSIToFP(b_load_2, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_pow_7_block);


    // op_pow_7_block
    builder.SetInsertPoint(op_pow_7_block);
    llvm::PHINode *phi_node = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node->addIncoming(b_load, op_pow_5_block);
    phi_node->addIncoming(b_sitofp, op_pow_6_block);
    llvm::SwitchInst *switch_type_2 = builder.CreateSwitch(rkc_LD, error_block, 2);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_pow_8_block);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_pow_9_block);

    // op_pow_8_block (3)
    builder.SetInsertPoint(op_pow_8_block);
    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP_2 = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_bitcast_2 = builder.CreateBitCast(c_value_GEP_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *c_load = builder.CreateLoad(llvm::Type::getDoubleTy(context), c_bitcast_2);
    builder.CreateBr(op_pow_10_block);

    // op_pow_9_block (19)
    builder.SetInsertPoint(op_pow_9_block);
    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP_3 = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_load_2 = builder.CreateLoad(c_value_GEP_3);
    llvm::Value *c_sitofp = builder.CreateSIToFP(c_load_2, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_pow_10_block);

    builder.SetInsertPoint(op_pow_10_block);
    llvm::PHINode *phi_node_2 = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node_2->addIncoming(c_load, op_pow_8_block);
    phi_node_2->addIncoming(c_sitofp, op_pow_9_block);
    temp.clear();
    temp.push_back(phi_node);
    temp.push_back(phi_node_2);
    llvm::Value *pow_b_c = builder.CreateCall(llvm_pow, temp);
    temp.clear();
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_type_2 = builder.CreateInBoundsGEP(value_struct_type, ra, temp);
    builder.CreateStore(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), a_type_2);
    temp.clear();
    temp.push_back(ra_ret[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_value_2 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *a_bitcast_2 = builder.CreateBitCast(a_value_2, llvm::Type::getDoublePtrTy(context));
    builder.CreateStore(pow_b_c, a_bitcast_2);
    builder.CreateBr(end_block);
    //


    return llvm::ConstantInt::get(context, llvm::APInt(64, 0, true));
}

llvm::Value* create_op_unm_block() {
    op_unm_block = llvm::BasicBlock::Create(context, "op_unm", step_func);

    op_unm_1_block = llvm::BasicBlock::Create(context, "op_unm_1", step_func);
    op_unm_2_block = llvm::BasicBlock::Create(context, "op_unm_2", step_func);

    builder.SetInsertPoint(op_unm_block);

    std::vector<llvm::Value *> temp;
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true))); //registers offset
    llvm::Value *registers_GEP = builder.CreateInBoundsGEP(miniluastate_struct_type, _mls, temp);
    llvm::Value *registers_LD = builder.CreateLoad(registers_GEP);

    std::vector<llvm::Value *> ra_return = create_ra(registers_LD);
    llvm::Value * ra = ra_return[1];

    /* R(B()) */
    llvm::Value *b_inst = create_B();
    llvm::Value *b_inst_zext = builder.CreateZExt(b_inst, llvm::Type::getInt64Ty(context));
    temp.clear();
    temp.push_back(b_inst_zext);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *registers_bth = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp); /* Value *b */
    llvm::Value *b_LD = builder.CreateLoad(registers_bth);

    llvm::SwitchInst *switch_type = builder.CreateSwitch(b_LD, error_block, 2);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_unm_1_block);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_unm_2_block);

    builder.SetInsertPoint(op_unm_1_block);
    //fsub
    temp.clear();
    temp.push_back(b_inst_zext);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *registers_bth_2 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *b_bitcast = builder.CreateBitCast(registers_bth_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *b_value_LD = builder.CreateLoad(b_bitcast);
    llvm::Value *fsub = builder.CreateFSub(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), -0.0), b_value_LD);

    temp.clear();
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_type_2 = builder.CreateInBoundsGEP(value_struct_type, ra, temp);
    builder.CreateStore(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), a_type_2);
    temp.clear();
    temp.push_back(ra_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_value_2 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *a_bitcast_2 = builder.CreateBitCast(a_value_2, llvm::Type::getDoublePtrTy(context));
    builder.CreateStore(fsub, a_bitcast_2);
    builder.CreateBr(end_block);


    builder.SetInsertPoint(op_unm_2_block);
    //sub
    temp.clear();
    temp.push_back(b_inst_zext);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *registers_bth_3 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *b_value_LD_2 = builder.CreateLoad(registers_bth_3);
    llvm::Value *sub = builder.CreateSub(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0), b_value_LD_2);

    temp.clear();
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_type = builder.CreateInBoundsGEP(value_struct_type, ra, temp);
    builder.CreateStore(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), a_type);
    temp.clear();
    temp.push_back(ra_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_value = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    builder.CreateStore(sub, a_value);
    builder.CreateBr(end_block);


    return llvm::ConstantInt::get(context, llvm::APInt(64, 0, true));
}

llvm::Value* create_op_not_block() {
    op_not_block = llvm::BasicBlock::Create(context, "op_not", step_func);

    op_not_1_block = llvm::BasicBlock::Create(context, "op_not_1", step_func);
    op_not_2_block = llvm::BasicBlock::Create(context, "op_not_2", step_func);
    op_not_3_block = llvm::BasicBlock::Create(context, "op_not_3", step_func);

    builder.SetInsertPoint(op_not_block);

    std::vector<llvm::Value *> temp;
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true))); //registers offset
    llvm::Value *registers_GEP = builder.CreateInBoundsGEP(miniluastate_struct_type, _mls, temp);
    llvm::Value *registers_LD = builder.CreateLoad(registers_GEP);

    std::vector<llvm::Value *> ra_return = create_ra(registers_LD);
    llvm::Value * ra = ra_return[1];

    /* R(B()) */
    llvm::Value *b_inst = create_B();
    llvm::Value *b_inst_zext = builder.CreateZExt(b_inst, llvm::Type::getInt64Ty(context));
    temp.clear();
    temp.push_back(b_inst_zext);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *registers_bth = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp); /* Value *b */
    llvm::Value *b_LD = builder.CreateLoad(registers_bth);
    llvm::SwitchInst *switch_type = builder.CreateSwitch(b_LD, op_not_2_block, 2);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)), op_not_3_block);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)), op_not_1_block);

    builder.SetInsertPoint(op_not_1_block);
    temp.clear();
    temp.push_back(b_inst_zext);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *registers_bth_2 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp); /* Value *b */
    llvm::Value *b_bitcast = builder.CreateBitCast(registers_bth_2, llvm::Type::getInt32PtrTy(context));
    llvm::Value *b_LD_2 = builder.CreateLoad(b_bitcast);
    llvm::Value *compare = builder.CreateICmpEQ(b_LD_2, llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    builder.CreateBr(op_not_3_block);

    builder.SetInsertPoint(op_not_2_block);
    builder.CreateBr(op_not_3_block);

    builder.SetInsertPoint(op_not_3_block);
    llvm::PHINode *phi_node = builder.CreatePHI(llvm::Type::getInt1Ty(context), 2); //op
    phi_node->addIncoming(llvm::ConstantInt::getFalse(context), op_not_2_block);
    phi_node->addIncoming(compare, op_not_1_block);
    phi_node->addIncoming(llvm::ConstantInt::getTrue(context), op_not_block);
    llvm::Value *phi_zext = builder.CreateZExt(phi_node, llvm::Type::getInt32Ty(context));

    llvm::Value *a_inst_zext = builder.CreateZExt(ra_return[0], llvm::Type::getInt64Ty(context));
    temp.clear();
    temp.push_back(a_inst_zext);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *a_type = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    builder.CreateStore(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)), a_type);

    temp.clear();
    temp.push_back(a_inst_zext);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *a_value = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *a_value_bitcast = builder.CreateBitCast(a_value, llvm::Type::getInt32PtrTy(context));
    builder.CreateStore(phi_zext, a_value_bitcast);

    builder.CreateBr(end_block);

    return llvm::ConstantInt::get(context, llvm::APInt(64, 0, true));
}

llvm::Value* create_op_jmp_block() {
    op_jmp_block = llvm::BasicBlock::Create(context, "op_jmp", step_func);

    builder.SetInsertPoint(op_jmp_block);

    llvm::Value *sbx = create_sbx();
    builder.CreateBr(end_block);

    return sbx;
}

llvm::Value* create_op_eq_block() {
    op_eq_block = llvm::BasicBlock::Create(context, "op_eq", step_func);

    op_eq_1_block = llvm::BasicBlock::Create(context, "op_eq_1", step_func);
    op_eq_2_block = llvm::BasicBlock::Create(context, "op_eq_2", step_func);
    op_eq_3_block = llvm::BasicBlock::Create(context, "op_eq_3", step_func);
    op_eq_4_block = llvm::BasicBlock::Create(context, "op_eq_4", step_func);
    op_eq_5_block = llvm::BasicBlock::Create(context, "op_eq_5", step_func);
    op_eq_6_block = llvm::BasicBlock::Create(context, "op_eq_6", step_func);
    op_eq_7_block = llvm::BasicBlock::Create(context, "op_eq_7", step_func);
    op_eq_8_block = llvm::BasicBlock::Create(context, "op_eq_8", step_func);
    op_eq_9_block = llvm::BasicBlock::Create(context, "op_eq_9", step_func);
    op_eq_10_block = llvm::BasicBlock::Create(context, "op_eq_10", step_func);

    builder.SetInsertPoint(op_eq_block);
    std::vector<llvm::Value *> temp;
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true))); //registers offset
    llvm::Value *registers_GEP = builder.CreateInBoundsGEP(miniluastate_struct_type, _mls, temp);
    llvm::Value *registers_LD = builder.CreateLoad(registers_GEP);
    llvm::Value *a = create_A(); // A(inst);
    llvm::Value *b_inst = create_B();
    std::vector<llvm::Value *> rkb_return = create_rk(b_inst, registers_LD);
    llvm::Value *rkb = rkb_return[2];
    llvm::Value *rkb_LD = builder.CreateLoad(rkb);
    llvm::Value *c_inst = create_C();
    std::vector<llvm::Value *> rkc_return = create_rk(c_inst, registers_LD);
    llvm::Value *rkc = rkc_return[2];
    llvm::Value *rkc_LD = builder.CreateLoad(rkc);
    llvm::SwitchInst *switch_type = builder.CreateSwitch(rkb_LD, error_block, 4);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)), op_eq_1_block);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)), op_eq_2_block);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_eq_4_block);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_eq_7_block);

    builder.SetInsertPoint(op_eq_1_block); // TNIL
    llvm::Value *rkc_is_nil = builder.CreateICmpEQ(rkc_LD, llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    builder.CreateBr(op_eq_10_block);

    builder.SetInsertPoint(op_eq_2_block); // TBOOLEAN
    llvm::Value *rkc_is_bool = builder.CreateICmpEQ(rkc_LD, llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    builder.CreateCondBr(rkc_is_bool, op_eq_10_block, op_eq_3_block);

    builder.SetInsertPoint(op_eq_3_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *rkb_value = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *rkb_value_bitcast = builder.CreateBitCast(rkb_value, llvm::Type::getInt32PtrTy(context));
    llvm::Value *rkb_value_load = builder.CreateLoad(rkb_value_bitcast);
    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *rkc_value = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *rkc_value_bitcast = builder.CreateBitCast(rkc_value, llvm::Type::getInt32PtrTy(context));
    llvm::Value *rkc_value_load = builder.CreateLoad(rkc_value_bitcast);
    llvm::Value *rkb_rkc_compare_bool = builder.CreateICmpEQ(rkb_value_load, rkc_value_load);
    builder.CreateBr(op_eq_10_block);

    builder.SetInsertPoint(op_eq_4_block);  //TNUMFLT
    llvm::SwitchInst *switch_type_2 = builder.CreateSwitch(rkc_LD, op_eq_10_block, 2);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_eq_5_block);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_eq_6_block);

    builder.SetInsertPoint(op_eq_5_block); //rkc is float
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *rkb_value_2 = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *rkb_value_bitcast_2 = builder.CreateBitCast(rkb_value_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *rkb_value_load_2 = builder.CreateLoad(rkb_value_bitcast_2);
    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *rkc_value_2 = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *rkc_value_bitcast_2 = builder.CreateBitCast(rkc_value_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *rkc_value_load_2 = builder.CreateLoad(rkc_value_bitcast_2);
    llvm::Value *rkb_rkc_compare_fcmp = builder.CreateFCmpOEQ(rkb_value_load_2, rkc_value_load_2);
    builder.CreateBr(op_eq_10_block);

    builder.SetInsertPoint(op_eq_6_block); //rkc is int (convert to float and compare)
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *rkb_value_3 = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *rkb_value_bitcast_3 = builder.CreateBitCast(rkb_value_3, llvm::Type::getDoublePtrTy(context));
    llvm::Value *rkb_value_load_3 = builder.CreateLoad(rkb_value_bitcast_3);
    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *rkc_value_3 = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *rkc_value_load_3 = builder.CreateLoad(rkc_value_3);
    llvm::Value *rkc_value_sitofp = builder.CreateSIToFP(rkc_value_load_3, llvm::Type::getDoubleTy(context));
    llvm::Value *rkb_rkcint_compare_fcmp = builder.CreateFCmpOEQ(rkb_value_load_3, rkc_value_sitofp);
    builder.CreateBr(op_eq_10_block);


    builder.SetInsertPoint(op_eq_7_block);  //TNUMINT
    llvm::SwitchInst *switch_type_3 = builder.CreateSwitch(rkc_LD, op_eq_10_block, 2);
    switch_type_3->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_eq_8_block);
    switch_type_3->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_eq_9_block);

    builder.SetInsertPoint(op_eq_8_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *rkb_value_4 = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *rkb_value_load_4 = builder.CreateLoad(rkb_value_4);
    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *rkc_value_4 = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *rkc_value_load_4 = builder.CreateLoad(rkc_value_4);
    llvm::Value *rkb_rkc_compare_icmp = builder.CreateICmpEQ(rkb_value_load_4, rkc_value_load_4);
    builder.CreateBr(op_eq_10_block);

    builder.SetInsertPoint(op_eq_9_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *rkb_value_5 = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *rkb_value_load_5 = builder.CreateLoad(rkb_value_5);
    llvm::Value *rkb_value_sitofp = builder.CreateSIToFP(rkb_value_load_5, llvm::Type::getDoubleTy(context));
    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *rkc_value_5 = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *rkc_value_bitcast_5 = builder.CreateBitCast(rkc_value_5, llvm::Type::getDoublePtrTy(context));
    llvm::Value *rkc_value_load_5 = builder.CreateLoad(rkc_value_bitcast_5);
    llvm::Value *rkbint_rkc_compare_fcmp = builder.CreateFCmpOEQ(rkb_value_sitofp, rkc_value_load_5);
    builder.CreateBr(op_eq_10_block);

    builder.SetInsertPoint(op_eq_10_block);
    llvm::PHINode *phi_node = builder.CreatePHI(llvm::Type::getInt1Ty(context), 7);
    phi_node->addIncoming(rkc_is_nil, op_eq_1_block);
    phi_node->addIncoming(llvm::ConstantInt::get(context, llvm::APInt(1, 0, true)), op_eq_2_block);
    phi_node->addIncoming(rkb_rkc_compare_bool, op_eq_3_block);
    phi_node->addIncoming(llvm::ConstantInt::get(context, llvm::APInt(1, 0, true)), op_eq_4_block);
    phi_node->addIncoming(rkb_rkc_compare_fcmp, op_eq_5_block);
    phi_node->addIncoming(rkb_rkcint_compare_fcmp, op_eq_6_block);
    phi_node->addIncoming(llvm::ConstantInt::get(context, llvm::APInt(1, 0, true)), op_eq_7_block);
    phi_node->addIncoming(rkb_rkc_compare_icmp, op_eq_8_block);
    phi_node->addIncoming(rkbint_rkc_compare_fcmp, op_eq_9_block);
    llvm::Value *zext = builder.CreateZExt(phi_node, llvm::Type::getInt32Ty(context));
    llvm::Value *icmp_ne = builder.CreateICmpNE(zext, a);
    llvm::Value *zext_2 = builder.CreateZExt(icmp_ne, llvm::Type::getInt64Ty(context));
    builder.CreateBr(end_block);

    return zext_2;
}

llvm::Value* create_op_lt_block() {
    op_lt_block = llvm::BasicBlock::Create(context, "op_lt", step_func);

    op_lt_1_block = llvm::BasicBlock::Create(context, "op_lt_1", step_func);
    op_lt_2_block = llvm::BasicBlock::Create(context, "op_lt_2", step_func);
    op_lt_3_block = llvm::BasicBlock::Create(context, "op_lt_3", step_func);
    op_lt_4_block = llvm::BasicBlock::Create(context, "op_lt_4", step_func);
    op_lt_5_block = llvm::BasicBlock::Create(context, "op_lt_5", step_func);
    op_lt_6_block = llvm::BasicBlock::Create(context, "op_lt_6", step_func);
    op_lt_7_block = llvm::BasicBlock::Create(context, "op_lt_7", step_func);
    op_lt_8_block = llvm::BasicBlock::Create(context, "op_lt_8", step_func);
    op_lt_9_block = llvm::BasicBlock::Create(context, "op_lt_9", step_func);
    op_lt_10_block = llvm::BasicBlock::Create(context, "op_lt_10", step_func);
    op_lt_11_block = llvm::BasicBlock::Create(context, "op_lt_11", step_func);

    builder.SetInsertPoint(op_lt_block);

    //
    std::vector<llvm::Value *> temp;
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true))); //registers offset
    llvm::Value *registers_GEP = builder.CreateInBoundsGEP(miniluastate_struct_type, _mls, temp);
    llvm::Value *registers_LD = builder.CreateLoad(registers_GEP);

    llvm::Value *a = create_A(); //Value *a = A(inst);

    llvm::Value *b_inst = create_B();
    std::vector<llvm::Value *> rkb_return = create_rk(b_inst, registers_LD);
    llvm::Value *rkb = rkb_return[2];
    llvm::Value *rkb_LD = builder.CreateLoad(rkb);
    llvm::Value *rkb_or = builder.CreateOr(llvm::ConstantInt::get(context, llvm::APInt(32, 16, true)), rkb_LD);
    llvm::Value *rkb_is_numerical_condition = builder.CreateICmpEQ(rkb_or, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    builder.CreateCondBr(rkb_is_numerical_condition, op_lt_1_block, error_block);

    //op_lt_1_block
    builder.SetInsertPoint(op_lt_1_block);
    llvm::Value *c_inst = create_C();
    std::vector<llvm::Value *> rkc_return = create_rk(c_inst, registers_LD);
    llvm::Value *rkc = rkc_return[2];
    llvm::Value *rkc_LD = builder.CreateLoad(rkc);
    llvm::Value *rkc_or = builder.CreateOr(llvm::ConstantInt::get(context, llvm::APInt(32, 16, true)), rkc_LD);
    llvm::Value *rkc_is_numerical_condition = builder.CreateICmpEQ(rkc_or, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    builder.CreateCondBr(rkc_is_numerical_condition, op_lt_2_block, error_block);

    builder.SetInsertPoint(op_lt_2_block);
    llvm::Value *rkb_is_int = builder.CreateICmpEQ(rkb_LD, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    llvm::Value *rkc_is_int = builder.CreateICmpEQ(rkc_LD, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    llvm::Value *both_int = builder.CreateAnd(rkb_is_int, rkc_is_int);
    builder.CreateCondBr(both_int, op_lt_3_block, op_lt_4_block);

    // XXX TODO discover why it seg faults when adding the third argument (the 0 on the commented line. On rkb and rkc!!!
    // op_lt_1_block true
    builder.SetInsertPoint(op_lt_3_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_value_LD = builder.CreateLoad(b_value_GEP);

    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_value_LD = builder.CreateLoad(c_value_GEP);

    //SLT
    llvm::Value *slt_b_c = builder.CreateICmpSLT(b_value_LD, c_value_LD);
    builder.CreateBr(op_lt_11_block);

    // op_lt_1_block false
    builder.SetInsertPoint(op_lt_4_block);
    llvm::SwitchInst *switch_type = builder.CreateSwitch(rkb_LD, error_block, 2);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_lt_5_block);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_lt_6_block);

    // op_lt_5_block (3)
    builder.SetInsertPoint(op_lt_5_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP_2 = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_bitcast = builder.CreateBitCast(b_value_GEP_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *b_load = builder.CreateLoad(llvm::Type::getDoubleTy(context), b_bitcast);
    builder.CreateBr(op_lt_7_block);

    // op_lt_6_block (19)
    builder.SetInsertPoint(op_lt_6_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP_3 = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_load_2 = builder.CreateLoad(b_value_GEP_3);
    llvm::Value *b_sitofp = builder.CreateSIToFP(b_load_2, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_lt_7_block);


    // op_lt_7_block
    builder.SetInsertPoint(op_lt_7_block);
    llvm::PHINode *phi_node = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node->addIncoming(b_load, op_lt_5_block);
    phi_node->addIncoming(b_sitofp, op_lt_6_block);
    llvm::SwitchInst *switch_type_2 = builder.CreateSwitch(rkc_LD, error_block, 2);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_lt_8_block);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_lt_9_block);

    // op_lt_8_block (3)
    builder.SetInsertPoint(op_lt_8_block);
    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP_2 = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_bitcast_2 = builder.CreateBitCast(c_value_GEP_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *c_load = builder.CreateLoad(llvm::Type::getDoubleTy(context), c_bitcast_2);
    builder.CreateBr(op_lt_10_block);

    // op_lt_9_block (19)
    builder.SetInsertPoint(op_lt_9_block);
    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP_3 = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_load_2 = builder.CreateLoad(c_value_GEP_3);
    llvm::Value *c_sitofp = builder.CreateSIToFP(c_load_2, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_lt_10_block);

    builder.SetInsertPoint(op_lt_10_block);
    llvm::PHINode *phi_node_2 = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node_2->addIncoming(c_load, op_lt_8_block);
    phi_node_2->addIncoming(c_sitofp, op_lt_9_block);
    llvm::Value *olt_b_c_2 = builder.CreateFCmpOLT(phi_node, phi_node_2);
    builder.CreateBr(op_lt_11_block);

    builder.SetInsertPoint(op_lt_11_block);
    llvm::PHINode *phi_node_3 = builder.CreatePHI(llvm::Type::getInt1Ty(context), 2);
    phi_node_3->addIncoming(slt_b_c, op_lt_3_block);
    phi_node_3->addIncoming(olt_b_c_2, op_lt_10_block);
    llvm::Value *zext_vmLT = builder.CreateZExt(phi_node_3, llvm::Type::getInt32Ty(context));
    llvm::Value *compare_vmLT_a = builder.CreateICmpNE(zext_vmLT, a);
    llvm::Value *zext_result = builder.CreateZExt(compare_vmLT_a, llvm::Type::getInt64Ty(context));
    builder.CreateBr(end_block);

    return zext_result;
}

llvm::Value* create_op_le_block() {
    op_le_block = llvm::BasicBlock::Create(context, "op_le", step_func);

    op_le_1_block = llvm::BasicBlock::Create(context, "op_le_1", step_func);
    op_le_2_block = llvm::BasicBlock::Create(context, "op_le_2", step_func);
    op_le_3_block = llvm::BasicBlock::Create(context, "op_le_3", step_func);
    op_le_4_block = llvm::BasicBlock::Create(context, "op_le_4", step_func);
    op_le_5_block = llvm::BasicBlock::Create(context, "op_le_5", step_func);
    op_le_6_block = llvm::BasicBlock::Create(context, "op_le_6", step_func);
    op_le_7_block = llvm::BasicBlock::Create(context, "op_le_7", step_func);
    op_le_8_block = llvm::BasicBlock::Create(context, "op_le_8", step_func);
    op_le_9_block = llvm::BasicBlock::Create(context, "op_le_9", step_func);
    op_le_10_block = llvm::BasicBlock::Create(context, "op_le_10", step_func);
    op_le_11_block = llvm::BasicBlock::Create(context, "op_le_11", step_func);

    builder.SetInsertPoint(op_le_block);

    //
    std::vector<llvm::Value *> temp;
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true))); //registers offset
    llvm::Value *registers_GEP = builder.CreateInBoundsGEP(miniluastate_struct_type, _mls, temp);
    llvm::Value *registers_LD = builder.CreateLoad(registers_GEP);

    llvm::Value *a = create_A(); //Value *a = A(inst);

    llvm::Value *b_inst = create_B();
    std::vector<llvm::Value *> rkb_return = create_rk(b_inst, registers_LD);
    llvm::Value *rkb = rkb_return[2];
    llvm::Value *rkb_LD = builder.CreateLoad(rkb);
    llvm::Value *rkb_or = builder.CreateOr(llvm::ConstantInt::get(context, llvm::APInt(32, 16, true)), rkb_LD);
    llvm::Value *rkb_is_numerical_condition = builder.CreateICmpEQ(rkb_or, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    builder.CreateCondBr(rkb_is_numerical_condition, op_le_1_block, error_block);

    //op_le_1_block
    builder.SetInsertPoint(op_le_1_block);
    llvm::Value *c_inst = create_C();
    std::vector<llvm::Value *> rkc_return = create_rk(c_inst, registers_LD);
    llvm::Value *rkc = rkc_return[2];
    llvm::Value *rkc_LD = builder.CreateLoad(rkc);
    llvm::Value *rkc_or = builder.CreateOr(llvm::ConstantInt::get(context, llvm::APInt(32, 16, true)), rkc_LD);
    llvm::Value *rkc_is_numerical_condition = builder.CreateICmpEQ(rkc_or, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    builder.CreateCondBr(rkc_is_numerical_condition, op_le_2_block, error_block);

    builder.SetInsertPoint(op_le_2_block);
    llvm::Value *rkb_is_int = builder.CreateICmpEQ(rkb_LD, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    llvm::Value *rkc_is_int = builder.CreateICmpEQ(rkc_LD, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    llvm::Value *both_int = builder.CreateAnd(rkb_is_int, rkc_is_int);
    builder.CreateCondBr(both_int, op_le_3_block, op_le_4_block);

    // XXX TODO discover why it seg faults when adding the third argument (the 0 on the commented line. On rkb and rkc!!!
    // op_le_1_block true
    builder.SetInsertPoint(op_le_3_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_value_LD = builder.CreateLoad(b_value_GEP);

    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_value_LD = builder.CreateLoad(c_value_GEP);

    //SLT
    llvm::Value *sle_b_c = builder.CreateICmpSLE(b_value_LD, c_value_LD);
    builder.CreateBr(op_le_11_block);

    // op_le_1_block false
    builder.SetInsertPoint(op_le_4_block);
    llvm::SwitchInst *switch_type = builder.CreateSwitch(rkb_LD, error_block, 2);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_le_5_block);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_le_6_block);

    // op_le_5_block (3)
    builder.SetInsertPoint(op_le_5_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP_2 = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_bitcast = builder.CreateBitCast(b_value_GEP_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *b_load = builder.CreateLoad(llvm::Type::getDoubleTy(context), b_bitcast);
    builder.CreateBr(op_le_7_block);

    // op_le_6_block (19)
    builder.SetInsertPoint(op_le_6_block);
    temp.clear();
    temp.push_back(rkb_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *b_value_GEP_3 = builder.CreateInBoundsGEP(value_struct_type, rkb_return[1], temp);
    llvm::Value *b_load_2 = builder.CreateLoad(b_value_GEP_3);
    llvm::Value *b_sitofp = builder.CreateSIToFP(b_load_2, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_le_7_block);


    // op_le_7_block
    builder.SetInsertPoint(op_le_7_block);
    llvm::PHINode *phi_node = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node->addIncoming(b_load, op_le_5_block);
    phi_node->addIncoming(b_sitofp, op_le_6_block);
    llvm::SwitchInst *switch_type_2 = builder.CreateSwitch(rkc_LD, error_block, 2);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_le_8_block);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_le_9_block);

    // op_le_8_block (3)
    builder.SetInsertPoint(op_le_8_block);
    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP_2 = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_bitcast_2 = builder.CreateBitCast(c_value_GEP_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *c_load = builder.CreateLoad(llvm::Type::getDoubleTy(context), c_bitcast_2);
    builder.CreateBr(op_le_10_block);

    // op_le_9_block (19)
    builder.SetInsertPoint(op_le_9_block);
    temp.clear();
    temp.push_back(rkc_return[0]);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *c_value_GEP_3 = builder.CreateInBoundsGEP(value_struct_type, rkc_return[1], temp);
    llvm::Value *c_load_2 = builder.CreateLoad(c_value_GEP_3);
    llvm::Value *c_sitofp = builder.CreateSIToFP(c_load_2, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_le_10_block);

    builder.SetInsertPoint(op_le_10_block);
    llvm::PHINode *phi_node_2 = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node_2->addIncoming(c_load, op_le_8_block);
    phi_node_2->addIncoming(c_sitofp, op_le_9_block);
    llvm::Value *ole_b_c_2 = builder.CreateFCmpOLE(phi_node, phi_node_2);
    builder.CreateBr(op_le_11_block);

    builder.SetInsertPoint(op_le_11_block);
    llvm::PHINode *phi_node_3 = builder.CreatePHI(llvm::Type::getInt1Ty(context), 2);
    phi_node_3->addIncoming(sle_b_c, op_le_3_block);
    phi_node_3->addIncoming(ole_b_c_2, op_le_10_block);
    llvm::Value *zext_vmLE = builder.CreateZExt(phi_node_3, llvm::Type::getInt32Ty(context));
    llvm::Value *compare_vmLE_a = builder.CreateICmpNE(zext_vmLE, a);
    llvm::Value *zext_result = builder.CreateZExt(compare_vmLE_a, llvm::Type::getInt64Ty(context));
    builder.CreateBr(end_block);

    return zext_result;
}

llvm::Value* create_op_forloop_block() {
    op_forloop_block = llvm::BasicBlock::Create(context, "op_forloop", step_func);

    op_forloop_1_block = llvm::BasicBlock::Create(context, "op_forloop_1", step_func);
    op_forloop_2_block = llvm::BasicBlock::Create(context, "op_forloop_2", step_func);
    op_forloop_3_block = llvm::BasicBlock::Create(context, "op_forloop_3", step_func);
    op_forloop_4_block = llvm::BasicBlock::Create(context, "op_forloop_4", step_func);
    op_forloop_5_block = llvm::BasicBlock::Create(context, "op_forloop_5", step_func);
    op_forloop_6_block = llvm::BasicBlock::Create(context, "op_forloop_6", step_func);
    op_forloop_7_block = llvm::BasicBlock::Create(context, "op_forloop_7", step_func);
    op_forloop_8_block = llvm::BasicBlock::Create(context, "op_forloop_8", step_func);
    op_forloop_9_block = llvm::BasicBlock::Create(context, "op_forloop_9", step_func);
    op_forloop_10_block = llvm::BasicBlock::Create(context, "op_forloop_10", step_func);
    op_forloop_11_block = llvm::BasicBlock::Create(context, "op_forloop_11", step_func);
    op_forloop_12_block = llvm::BasicBlock::Create(context, "op_forloop_12", step_func);
    op_forloop_13_block = llvm::BasicBlock::Create(context, "op_forloop_13", step_func);
    op_forloop_14_block = llvm::BasicBlock::Create(context, "op_forloop_14", step_func);
    op_forloop_15_block = llvm::BasicBlock::Create(context, "op_forloop_15", step_func);
    op_forloop_16_block = llvm::BasicBlock::Create(context, "op_forloop_16", step_func);
    op_forloop_17_block = llvm::BasicBlock::Create(context, "op_forloop_17", step_func);
    op_forloop_18_block = llvm::BasicBlock::Create(context, "op_forloop_18", step_func);

    builder.SetInsertPoint(op_forloop_block);
    llvm::Value *a = create_A();
    llvm::Value *a_i64 = builder.CreateZExt(a, llvm::Type::getInt64Ty(context));
    std::vector<llvm::Value *> temp;
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true))); //registers offset
    llvm::Value *registers_GEP = builder.CreateInBoundsGEP(miniluastate_struct_type, _mls, temp);
    llvm::Value *registers_LD = builder.CreateLoad(registers_GEP);
    temp.clear();
    temp.push_back(a_i64);
    llvm::Value *r_GEP = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *limit_offset = builder.CreateAdd(a_i64, llvm::ConstantInt::get(context, llvm::APInt(64, 1, true)));
    llvm::Value *step_offset = builder.CreateAdd(a_i64, llvm::ConstantInt::get(context, llvm::APInt(64, 2, true)));

    temp.clear();
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *init_GEP = builder.CreateInBoundsGEP(value_struct_type, r_GEP, temp);
    llvm::Value *init_LD = builder.CreateLoad(init_GEP);

    //
    llvm::Value *init_or = builder.CreateOr(llvm::ConstantInt::get(context, llvm::APInt(32, 16, true)), init_LD);
    llvm::Value *init_is_numerical_condition = builder.CreateICmpEQ(init_or, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    builder.CreateCondBr(init_is_numerical_condition, op_forloop_1_block, error_block);

    //
    //op_add_1_block
    builder.SetInsertPoint(op_forloop_1_block);
    temp.clear();
    temp.push_back(step_offset);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *step_GEP = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *step_LD = builder.CreateLoad(step_GEP);
    llvm::Value *step_or = builder.CreateOr(llvm::ConstantInt::get(context, llvm::APInt(32, 16, true)), step_LD);
    llvm::Value *step_is_numerical_condition = builder.CreateICmpEQ(step_or, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    builder.CreateCondBr(step_is_numerical_condition, op_forloop_2_block, error_block);
    //
    builder.SetInsertPoint(op_forloop_2_block);
    llvm::Value *init_is_int = builder.CreateICmpEQ(init_LD, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    llvm::Value *step_is_int = builder.CreateICmpEQ(step_LD, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    llvm::Value *both_int = builder.CreateAnd(init_is_int, step_is_int);
    builder.CreateCondBr(both_int, op_forloop_3_block, op_forloop_4_block);
    //
    // // XXX TODO discover why it seg faults when adding the third argument (the 0 on the commented line. On rkb and rkc!!!
    // op_forloop_3_block true
    builder.SetInsertPoint(op_forloop_3_block);
    temp.clear();
    temp.push_back(a_i64);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *init_value_GEP = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *init_value_LD = builder.CreateLoad(init_value_GEP);
    temp.clear();
    temp.push_back(step_offset);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *step_value_GEP = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *step_value_LD = builder.CreateLoad(step_value_GEP);

    //ADD
    llvm::Value *add_init_step = builder.CreateAdd(init_value_LD, step_value_LD);

    builder.CreateStore(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), init_GEP);
    builder.CreateStore(add_init_step, init_value_GEP);
    llvm::Value *add_bitcast_double = builder.CreateBitCast(add_init_step, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_forloop_11_block);
    //
    // op_add_1_block false
    builder.SetInsertPoint(op_forloop_4_block);
    llvm::SwitchInst *switch_type = builder.CreateSwitch(init_LD, error_block, 2);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_forloop_5_block);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_forloop_6_block);
    //
    // op_add_5_block (3)
    builder.SetInsertPoint(op_forloop_5_block);
    temp.clear();
    temp.push_back(a_i64);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *init_value_GEP_2 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *init_double_bitcast = builder.CreateBitCast(init_value_GEP_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *init_double_load = builder.CreateLoad(llvm::Type::getDoubleTy(context), init_double_bitcast);
    builder.CreateBr(op_forloop_7_block);
    //
    // op_add_6_block (19)
    builder.SetInsertPoint(op_forloop_6_block);
    temp.clear();
    temp.push_back(a_i64);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *init_value_GEP_3 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *init_int_load = builder.CreateLoad(init_value_GEP_3);
    llvm::Value *init_sitofp = builder.CreateSIToFP(init_int_load, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_forloop_7_block);
    //
    // op_add_7_block
    builder.SetInsertPoint(op_forloop_7_block);
    llvm::PHINode *phi_node = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node->addIncoming(init_double_load, op_forloop_5_block);
    phi_node->addIncoming(init_sitofp, op_forloop_6_block);
    llvm::SwitchInst *switch_type_2 = builder.CreateSwitch(step_LD, error_block, 2);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_forloop_8_block);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_forloop_9_block);
    //
    // op_add_8_block (3)
    builder.SetInsertPoint(op_forloop_8_block);
    temp.clear();
    temp.push_back(step_offset);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *step_value_GEP_2 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *step_double_bitcast = builder.CreateBitCast(step_value_GEP_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *step_double_load = builder.CreateLoad(llvm::Type::getDoubleTy(context), step_double_bitcast);
    builder.CreateBr(op_forloop_10_block);
    //
    // op_add_9_block (19)
    builder.SetInsertPoint(op_forloop_9_block);
    temp.clear();
    temp.push_back(step_offset);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *step_value_GEP_3 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *step_int_load = builder.CreateLoad(step_value_GEP_3);
    llvm::Value *step_sitofp = builder.CreateSIToFP(step_int_load, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_forloop_10_block);

    //
    builder.SetInsertPoint(op_forloop_10_block);
    llvm::PHINode *phi_node_2 = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node_2->addIncoming(step_double_load, op_forloop_8_block);
    phi_node_2->addIncoming(step_sitofp, op_forloop_9_block);
    llvm::Value *fadd_init_step = builder.CreateFAdd(phi_node, phi_node_2);
    builder.CreateStore(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), init_GEP);
    temp.clear();
    temp.push_back(a_i64);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *init_value_GEP_4 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *init_value_bitcast = builder.CreateBitCast(init_value_GEP_4, llvm::Type::getDoublePtrTy(context));
    builder.CreateStore(fadd_init_step, init_value_bitcast);
    llvm::Value *add_bitcast = builder.CreateBitCast(fadd_init_step, llvm::Type::getInt64Ty(context));
    builder.CreateBr(op_forloop_11_block);
    //

    //start if (vm_LE ...)... part
    builder.SetInsertPoint(op_forloop_11_block);
    llvm::PHINode *phi_node_3 = builder.CreatePHI(llvm::Type::getInt64Ty(context), 2); //op
    phi_node_3->addIncoming(add_init_step, op_forloop_3_block);
    phi_node_3->addIncoming(add_bitcast, op_forloop_10_block);
    llvm::PHINode *phi_node_4 = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node_4->addIncoming(add_bitcast_double, op_forloop_3_block);
    phi_node_4->addIncoming(fadd_init_step, op_forloop_10_block);
    llvm::PHINode *phi_node_5 = builder.CreatePHI(llvm::Type::getInt32Ty(context), 2); //op
    phi_node_5->addIncoming(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_forloop_3_block);
    phi_node_5->addIncoming(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_forloop_10_block);
    temp.clear();
    temp.push_back(limit_offset);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *limit_GEP = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *limit_LD = builder.CreateLoad(limit_GEP);
    llvm::Value *limit_or = builder.CreateOr(llvm::ConstantInt::get(context, llvm::APInt(32, 16, true)), limit_LD);
    llvm::Value *limit_is_numerical_condition = builder.CreateICmpEQ(limit_or, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    builder.CreateCondBr(limit_is_numerical_condition, op_forloop_12_block, error_block);

    builder.SetInsertPoint(op_forloop_12_block);
    llvm::Value *init_type = builder.CreateICmpEQ(phi_node_5, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    llvm::Value *limit_type = builder.CreateICmpEQ(limit_LD, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    llvm::Value *and_types = builder.CreateAnd(init_type, limit_type);
    llvm::Value *type_selector = builder.CreateSelect(and_types,init_type ,limit_type);
    builder.CreateCondBr(type_selector, op_forloop_13_block, op_forloop_14_block);

    builder.SetInsertPoint(op_forloop_13_block);
    temp.clear();
    temp.push_back(limit_offset);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *limit_GEP_2 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *limit_LD_2 = builder.CreateLoad(limit_GEP_2);
    //SLT
    llvm::Value *sgt_init_limit = builder.CreateICmpSGT(phi_node_3, limit_LD_2);
    builder.CreateCondBr(sgt_init_limit, end_block, op_forloop_18_block);

    builder.SetInsertPoint(op_forloop_14_block);
    llvm::Value *is_float = builder.CreateICmpEQ(phi_node_5, llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)));
    llvm::Value *sitofp = builder.CreateSIToFP(phi_node_3, llvm::Type::getDoubleTy(context));
    llvm::Value *select = builder.CreateSelect(is_float, phi_node_4, sitofp);
    llvm::SwitchInst *switch_type_3 = builder.CreateSwitch(limit_LD, error_block, 2);
    switch_type_3->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_forloop_15_block);
    switch_type_3->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_forloop_16_block);

    builder.SetInsertPoint(op_forloop_15_block);
    temp.clear();
    temp.push_back(limit_offset);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *limit_GEP_3 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *limit_bitcast = builder.CreateBitCast(limit_GEP_3, llvm::Type::getDoublePtrTy(context));
    llvm::Value *limit_value = builder.CreateLoad(limit_bitcast);
    builder.CreateBr(op_forloop_17_block);

    builder.SetInsertPoint(op_forloop_16_block);
    temp.clear();
    temp.push_back(limit_offset);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *limit_GEP_4 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *limit_value_2 = builder.CreateLoad(limit_GEP_4);
    llvm::Value *sitofp_2 = builder.CreateSIToFP(limit_value_2, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_forloop_17_block);

    builder.SetInsertPoint(op_forloop_17_block);
    llvm::PHINode *phi_node_6 = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node_6->addIncoming(limit_value, op_forloop_15_block);
    phi_node_6->addIncoming(sitofp_2, op_forloop_16_block);
    llvm::Value *fcmp = builder.CreateFCmpUGT(select, phi_node_6);
    llvm::Value *select_2 = builder.CreateCondBr(fcmp, end_block, op_forloop_18_block);

    builder.SetInsertPoint(op_forloop_18_block);
    llvm::Value *sbx = create_sbx();
    llvm::Value *var_offset = builder.CreateAdd(a_i64, llvm::ConstantInt::get(context, llvm::APInt(64, 3, true)));
    temp.clear();
    temp.push_back(var_offset);
    llvm::Value *var_GEP = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *var_bitcast = builder.CreateBitCast(var_GEP, llvm::Type::getInt8PtrTy(context));
    llvm::Value *init_bitcast = builder.CreateBitCast(r_GEP, llvm::Type::getInt8PtrTy(context));
    temp.clear();
    temp.push_back(var_bitcast);
    temp.push_back(init_bitcast);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 16, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 8, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(1, 0, true)));
    builder.CreateCall(llvm_memcpy, temp);
    builder.CreateBr(end_block);

    return sbx;
}

llvm::Value* create_op_forprep_block() {

    op_forprep_block = llvm::BasicBlock::Create(context, "op_forprep", step_func);

    op_forprep_1_block = llvm::BasicBlock::Create(context, "op_forprep_1", step_func);
    op_forprep_2_block = llvm::BasicBlock::Create(context, "op_forprep_2", step_func);
    op_forprep_3_block = llvm::BasicBlock::Create(context, "op_forprep_3", step_func);
    op_forprep_4_block = llvm::BasicBlock::Create(context, "op_forprep_4", step_func);
    op_forprep_5_block = llvm::BasicBlock::Create(context, "op_forprep_5", step_func);
    op_forprep_6_block = llvm::BasicBlock::Create(context, "op_forprep_6", step_func);
    op_forprep_7_block = llvm::BasicBlock::Create(context, "op_forprep_7", step_func);
    op_forprep_8_block = llvm::BasicBlock::Create(context, "op_forprep_8", step_func);
    op_forprep_9_block = llvm::BasicBlock::Create(context, "op_forprep_9", step_func);
    op_forprep_10_block = llvm::BasicBlock::Create(context, "op_forprep_10", step_func);
    op_forprep_11_block = llvm::BasicBlock::Create(context, "op_forprep_11", step_func);

    builder.SetInsertPoint(op_forprep_block);
    llvm::Value *a = create_A();
    llvm::Value *a_i64 = builder.CreateZExt(a, llvm::Type::getInt64Ty(context));
    std::vector<llvm::Value *> temp;
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true))); //registers offset
    llvm::Value *registers_GEP = builder.CreateInBoundsGEP(miniluastate_struct_type, _mls, temp);
    llvm::Value *registers_LD = builder.CreateLoad(registers_GEP);
    temp.clear();
    temp.push_back(a_i64);
    llvm::Value *r_GEP = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *limit_offset = builder.CreateAdd(a_i64, llvm::ConstantInt::get(context, llvm::APInt(64, 1, true)));
    llvm::Value *step_offset = builder.CreateAdd(a_i64, llvm::ConstantInt::get(context, llvm::APInt(64, 2, true)));

    temp.clear();
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *init_GEP = builder.CreateInBoundsGEP(value_struct_type, r_GEP, temp);
    llvm::Value *init_LD = builder.CreateLoad(init_GEP);

    //
    llvm::Value *init_or = builder.CreateOr(llvm::ConstantInt::get(context, llvm::APInt(32, 16, true)), init_LD);
    llvm::Value *init_is_numerical_condition = builder.CreateICmpEQ(init_or, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    builder.CreateCondBr(init_is_numerical_condition, op_forprep_1_block, error_block);

    //
    //op_add_1_block
    builder.SetInsertPoint(op_forprep_1_block);
    temp.clear();
    temp.push_back(step_offset);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *step_GEP = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *step_LD = builder.CreateLoad(step_GEP);
    llvm::Value *step_or = builder.CreateOr(llvm::ConstantInt::get(context, llvm::APInt(32, 16, true)), step_LD);
    llvm::Value *step_is_numerical_condition = builder.CreateICmpEQ(step_or, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    builder.CreateCondBr(step_is_numerical_condition, op_forprep_2_block, error_block);
    //
    builder.SetInsertPoint(op_forprep_2_block);
    llvm::Value *init_is_int = builder.CreateICmpEQ(init_LD, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    llvm::Value *step_is_int = builder.CreateICmpEQ(step_LD, llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)));
    llvm::Value *both_int = builder.CreateAnd(init_is_int, step_is_int);
    builder.CreateCondBr(both_int, op_forprep_3_block, op_forprep_4_block);
    //
    // // XXX TODO discover why it seg faults when adding the third argument (the 0 on the commented line. On rkb and rkc!!!
    // op_forprep_3_block true
    builder.SetInsertPoint(op_forprep_3_block);
    temp.clear();
    temp.push_back(a_i64);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *init_value_GEP = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *init_value_LD = builder.CreateLoad(init_value_GEP);
    temp.clear();
    temp.push_back(step_offset);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    // temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)));
    llvm::Value *step_value_GEP = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *step_value_LD = builder.CreateLoad(step_value_GEP);

    //ADD
    llvm::Value *sub_init_step = builder.CreateSub(init_value_LD, step_value_LD);

    builder.CreateStore(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), init_GEP);
    builder.CreateStore(sub_init_step, init_value_GEP);
    builder.CreateBr(op_forprep_11_block);
    //
    // op_add_1_block false
    builder.SetInsertPoint(op_forprep_4_block);
    llvm::SwitchInst *switch_type = builder.CreateSwitch(init_LD, error_block, 2);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_forprep_5_block);
    switch_type->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_forprep_6_block);
    //
    // op_add_5_block (3)
    builder.SetInsertPoint(op_forprep_5_block);
    temp.clear();
    temp.push_back(a_i64);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *init_value_GEP_2 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *init_double_bitcast = builder.CreateBitCast(init_value_GEP_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *init_double_load = builder.CreateLoad(llvm::Type::getDoubleTy(context), init_double_bitcast);
    builder.CreateBr(op_forprep_7_block);
    //
    // op_add_6_block (19)
    builder.SetInsertPoint(op_forprep_6_block);
    temp.clear();
    temp.push_back(a_i64);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *init_value_GEP_3 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *init_int_load = builder.CreateLoad(init_value_GEP_3);
    llvm::Value *init_sitofp = builder.CreateSIToFP(init_int_load, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_forprep_7_block);
    //
    // op_add_7_block
    builder.SetInsertPoint(op_forprep_7_block);
    llvm::PHINode *phi_node = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node->addIncoming(init_double_load, op_forprep_5_block);
    phi_node->addIncoming(init_sitofp, op_forprep_6_block);
    llvm::SwitchInst *switch_type_2 = builder.CreateSwitch(step_LD, error_block, 2);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), op_forprep_8_block);
    switch_type_2->addCase(llvm::ConstantInt::get(context, llvm::APInt(32, 19, true)), op_forprep_9_block);
    //
    // op_add_8_block (3)
    builder.SetInsertPoint(op_forprep_8_block);
    temp.clear();
    temp.push_back(step_offset);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *step_value_GEP_2 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *step_double_bitcast = builder.CreateBitCast(step_value_GEP_2, llvm::Type::getDoublePtrTy(context));
    llvm::Value *step_double_load = builder.CreateLoad(llvm::Type::getDoubleTy(context), step_double_bitcast);
    builder.CreateBr(op_forprep_10_block);
    //
    // op_add_9_block (19)
    builder.SetInsertPoint(op_forprep_9_block);
    temp.clear();
    temp.push_back(step_offset);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *step_value_GEP_3 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *step_int_load = builder.CreateLoad(step_value_GEP_3);
    llvm::Value *step_sitofp = builder.CreateSIToFP(step_int_load, llvm::Type::getDoubleTy(context));
    builder.CreateBr(op_forprep_10_block);

    //
    builder.SetInsertPoint(op_forprep_10_block);
    llvm::PHINode *phi_node_2 = builder.CreatePHI(llvm::Type::getDoubleTy(context), 2); //op
    phi_node_2->addIncoming(step_double_load, op_forprep_8_block);
    phi_node_2->addIncoming(step_sitofp, op_forprep_9_block);
    llvm::Value *fsub_init_step = builder.CreateFSub(phi_node, phi_node_2);
    builder.CreateStore(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)), init_GEP);
    temp.clear();
    temp.push_back(a_i64);
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 1, true)));
    llvm::Value *init_value_GEP_4 = builder.CreateInBoundsGEP(value_struct_type, registers_LD, temp);
    llvm::Value *init_value_bitcast = builder.CreateBitCast(init_value_GEP_4, llvm::Type::getDoublePtrTy(context));
    builder.CreateStore(fsub_init_step, init_value_bitcast);
    builder.CreateBr(op_forprep_11_block);
    //

    //sbx
    builder.SetInsertPoint(op_forprep_11_block);
    llvm::Value *sbx = create_sbx();
    builder.CreateBr(end_block);

    return sbx;
}

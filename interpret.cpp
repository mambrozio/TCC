/*
* File: onefunction.cpp
* Author: Matheus Coelho Ambrozio
* Date: May, 2017
*
* To compile , execute on terminal :
* clang++ -o interpret interpret.cpp `llvm-config --cxxflags --ldflags --libs all --system-libs`
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

#define OP_RETURN 38

#define SIZE_A          8
#define SIZE_B          9
#define SIZE_C          9

#define SIZE_OP         6

#define POS_OP          0
#define POS_A           (POS_OP + SIZE_OP)
#define POS_C           (POS_A + SIZE_A)
#define POS_B           (POS_C + SIZE_C)



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


extern "C" size_t step(MiniLuaState, Instruction, uint32_t, Value *);


void interpret(MiniLuaState *mls) {

    //necessary LLVM initializations
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    //create the context and the main module
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> Owner(new llvm::Module("top", context));
    llvm::Module *module = Owner.get();


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
    // std::cout << "Function Step type dump:\n";
    // step_type->dump();
    // std::cout << "\n";
    // --- End of declarations


    //declare the external function
    auto step_function = llvm::Function::Create(step_type, llvm::Function::ExternalLinkage, "step", module);
//    llvm::sys::DynamicLibrary::AddSymbol("step", reinterpret_cast<void*>(&step));

    //define the interpret function
    auto functype = llvm::FunctionType::get(llvm::Type::getInt32Ty(context), p_miniluastate_struct_type, false);
    auto mainfunc = llvm::Function::Create(functype, llvm::Function::ExternalLinkage, "interpret", module);

    //get arguments
    auto argiter = mainfunc->arg_begin();
    llvm::Value *_mls = &*argiter++;
    //_mls->setName("arg");

    std::cout << "Arguments received by interpret function dump:\n";
    _mls->dump();
    //std::cout << _mls;
    std::cout << "\n";


    //create irbuilder
    llvm::IRBuilder<> builder(context);


    // --- Creating Blocks
    //create the entry block
    auto entry_block = llvm::BasicBlock::Create(context, "entry", mainfunc);

    //create the loop block
    auto loop_block = llvm::BasicBlock::Create(context, "loop", mainfunc);

    //create the end block
    auto end_block = llvm::BasicBlock::Create(context, "end", mainfunc);



    // --- Create code for the entry block
    builder.SetInsertPoint(entry_block);

    //mls (MiniLuaState)
    std::vector<llvm::Value *> temp;
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true))); //mls offset
    llvm::Value *mls_GEP = builder.CreateInBoundsGEP(miniluastate_struct_type, _mls, temp);

    //mls->proto (Proto*)
    llvm::Value *proto_LD = builder.CreateLoad(mls_GEP);


    temp.clear();
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 9, true))); //k offset
    llvm::Value *k_GEP = builder.CreateInBoundsGEP(proto_struct_type, proto_LD, temp);

    //mls->proto->k (Value*)
    llvm::Value *k_LD = builder.CreateLoad(k_GEP);


    temp.clear();
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 10, true))); //code offset
    llvm::Value *code_GEP = builder.CreateInBoundsGEP(proto_struct_type, proto_LD, temp);

    //mls->proto->code* (i32*)
    llvm::Value *code_LD = builder.CreateLoad(code_GEP);

    //mls->proto->code[i] (i32)
    llvm::Value *code_value = builder.CreateLoad(code_LD); //code[i]

    //op = mls->proto->code && 0x3F
    llvm::Value *op_value = builder.CreateAnd(code_value, llvm::APInt(32, 0x3F, true));

    //here we have all initializations needed
    //mls* (mls_GEP), inst (code_value), op (op_value), constants (k_LD)

    //check if op is OP_RETURN
    llvm::Value *is_return = builder.CreateICmpEQ(op_value, llvm::ConstantInt::get(context, llvm::APInt(32, OP_RETURN, true)));

    //create the conditional break to end_block (if true) or to loop_block (if false)
    builder.CreateCondBr(is_return, end_block, loop_block);



    // --- Create code for the loop block
    builder.SetInsertPoint(loop_block);

    //creating phi nodes
    llvm::PHINode *inst_phi_node = builder.CreatePHI(llvm::Type::getInt32Ty(context), 2); //inst = mls->proto->code[pc++]
    llvm::PHINode *op_phi_node = builder.CreatePHI(llvm::Type::getInt32Ty(context), 2); //op (inst & 0x3f)
    llvm::PHINode *pc_phi_node = builder.CreatePHI(llvm::Type::getInt32Ty(context), 2);

    //increment pc
    llvm::Value *pc = builder.CreateAdd(pc_phi_node, llvm::ConstantInt::get(context, llvm::APInt(64, 1, true)));

    //calling step function
    std::vector<llvm::Value *> step_args;
    step_args.push_back(_mls);
    step_args.push_back(op_phi_node);
    step_args.push_back(inst_phi_node);
    step_args.push_back(k_LD);
    llvm::Value *step_return = builder.CreateCall(step_function, step_args);

    //adding pc_offset to pc
    llvm::Value *new_pc = builder.CreateAdd(pc, step_return);


    //TODO check if this is really necessary. The generated IR has the same load
    // apparently yes, because as llvm ir is ssa, the assignment in the entry block is unique
    // and references the initial value. Here we have to make another assignment to get
    // the updated value.
    // Maybe the CreateLoad(mls_GEP) is the only one that is unnecessary
    llvm::Value *proto_LD_loop = builder.CreateLoad(mls_GEP);

    temp.clear();
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 10, true)));
    llvm::Value *code_GEP_loop = builder.CreateInBoundsGEP(proto_struct_type, proto_LD_loop, temp);

    llvm::Value *code_LD_loop = builder.CreateLoad(code_GEP_loop);

    //get the code[...]
    temp.clear();
    temp.push_back(new_pc);
    llvm::Value *code_GEP_offset_loop = builder.CreateInBoundsGEP(llvm::Type::getInt32Ty(context), code_LD_loop, temp);

    llvm::Value *code_LD_offset_loop = builder.CreateLoad(code_GEP_offset_loop);

    //op = mls->proto->code && 0x3F
    llvm::Value *op_value_loop = builder.CreateAnd(code_LD_offset_loop, llvm::APInt(32, 0x3F, true));

    //check if op is OP_RETURN
    llvm::Value *is_return_2 = builder.CreateICmpEQ(op_value_loop, llvm::ConstantInt::get(context, llvm::APInt(32, OP_RETURN, true)));

    //create the conditional break to end_block (if true) or to loop_block (if false)
    builder.CreateCondBr(is_return_2, end_block, loop_block);

    //updating PHI Nodes
    inst_phi_node->addIncoming(op_value_loop, loop_block);
    inst_phi_node->addIncoming(op_value, entry_block);
    op_phi_node->addIncoming(code_LD_offset_loop, loop_block);
    op_phi_node->addIncoming(code_value, entry_block);
    pc_phi_node->addIncoming(new_pc, loop_block);
    pc_phi_node->addIncoming(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)), entry_block);



    // --- Create code for the end block
    builder.SetInsertPoint(end_block);

    //creating phi node
    llvm::PHINode *out_phi_node = builder.CreatePHI(llvm::Type::getInt32Ty(context), 2); //op
    out_phi_node->addIncoming(code_value, entry_block);
    out_phi_node->addIncoming(code_LD_offset_loop, loop_block);

    //create a
    llvm::Value *shift_a = builder.CreateLShr(out_phi_node, llvm::APInt(32, POS_A, true));
    llvm::Value *a = builder.CreateAnd(shift_a, llvm::APInt(32, 0xFF, true));

    //create b
    llvm::Value *shift_b = builder.CreateLShr(out_phi_node, llvm::APInt(32, POS_B, true));
    llvm::Value *b = builder.CreateAnd(shift_b, llvm::APInt(32, 0x1FF, true));

    //cast a
    llvm::Value *a_cast = builder.CreateZExt(a, llvm::Type::getInt64Ty(context));

    temp.clear();
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 2, true)));
    llvm::Value *mls_return_begin_GEP = builder.CreateInBoundsGEP(miniluastate_struct_type, _mls, temp);

    //updating return_begin inside the MiniLuaState struct
    llvm::Value *return_begin = builder.CreateStore(a_cast, mls_return_begin_GEP);

    //creating return_end
    llvm::Value *temp_b_0 = builder.CreateAdd(b, llvm::ConstantInt::get(context, llvm::APInt(32, -1, true)));

    llvm::Value *temp_b_1 = builder.CreateAdd(temp_b_0, a);

    //cast b
    llvm::Value *b_cast = builder.CreateZExt(b, llvm::Type::getInt64Ty(context));

    temp.clear();
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 3, true)));
    llvm::Value *mls_return_end_GEP = builder.CreateInBoundsGEP(miniluastate_struct_type, _mls, temp);

    //updating return_begin inside the MiniLuaState struct
    llvm::Value *return_end = builder.CreateStore(b_cast, mls_return_end_GEP);

    builder.CreateRetVoid();



    //dump module to check ir
    std::cout << "Module:\n";
    module->dump();
    std::cout << "\n";
}


int main() {

    MiniLuaState *mls = (MiniLuaState *)calloc(1, sizeof(MiniLuaState));
    mls->return_begin = 0;
    mls->return_end = 0;    

    interpret(mls);

    return 0;
}


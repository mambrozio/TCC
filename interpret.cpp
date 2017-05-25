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


extern "C" int step(MiniLuaState, Instruction, uint32_t, Value *);


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
    llvm::FunctionType *step_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(context), args, false);
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
    llvm::Value *mlsGEP = builder.CreateInBoundsGEP(miniluastate_struct_type, _mls, temp);

    //mls->proto (Proto*)
    llvm::Value *protoLD = builder.CreateLoad(mlsGEP);


    temp.clear();
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 9, true))); //k offset
    llvm::Value *kGEP = builder.CreateInBoundsGEP(proto_struct_type, protoLD, temp);

    //mls->proto->k (Value*)
    llvm::Value *kLD = builder.CreateLoad(kGEP);


    temp.clear();
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(64, 0, true)));
    temp.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 10, true))); //code offset
    llvm::Value *codeGEP = builder.CreateInBoundsGEP(proto_struct_type, protoLD, temp);

    //mls->proto->code* (i32*)
    llvm::Value *codeLD = builder.CreateLoad(codeGEP);

    //mls->proto->code[i] (i32)
    llvm::Value *codeValue = builder.CreateLoad(codeLD); //code[0]
    
    //op = mls->proto->code && 0x3F
    llvm::Value *opValue = builder.CreateAnd(codeValue, llvm::APInt(32, 0x3F, true));
    
    //here we have all initializations needed
    //mls* (mlsGEP), inst (codeValue), op (opValue), constants (kLD)

    //check if op is OP_RETURN
    llvm::Value *isReturn = builder.CreateICmpEQ(opValue, llvm::ConstantInt::get(context, llvm::APInt(32, OP_RETURN, true)));

    //create the conditional break to end_block (if true) or to loop_block (if false)
    builder.CreateCondBr(isReturn, end_block, loop_block);



    // --- Create code for the loop block
    builder.SetInsertPoint(loop_block);
    
    llvm::PHINode *inst_phi_node = builder.CreatePHI(llvm::Type::getInt32Ty(context), 2);
    llvm::PHINode *code_phi_node = builder.CreatePHI(llvm::Type::getInt32Ty(context), 2);
    llvm::PHINode *pc_phi_node = builder.CreatePHI(llvm::Type::getInt32Ty(context), 2);
    
    




    std::vector<llvm::Value *> step_args;
    step_args.push_back(_mls);
    step_args.push_back(codeValue);
    step_args.push_back(opValue);
    step_args.push_back(kLD);

    builder.CreateCall(step_function, step_args, "pc");

    //check if op is OP_RETURN
    llvm::Value *isReturn2 = builder.CreateICmpEQ(opValue, llvm::ConstantInt::get(context, llvm::APInt(32, OP_RETURN, true)));

    //create the conditional break to end_block (if true) or to loop_block (if false)
    builder.CreateCondBr(isReturn2, end_block, loop_block);
    


    // --- Create code for the end block
    builder.SetInsertPoint(end_block);
    builder.CreateRet(llvm::ConstantInt::get(builder.getInt32Ty(), 0));
    


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


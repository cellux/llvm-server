#include <iostream>

#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

using namespace llvm;

using ByteArray = SmallVector<char, 4096>;

struct LLVMInitializer {
  LLVMInitializer() {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();
    InitializeNativeTargetDisassembler();
  }
};

class CompileContext {
  LLVMInitializer init_;
  LLVMContext ctx_;
  std::unique_ptr<ExecutionEngine> ee_;
  std::vector<std::unique_ptr<Module>> stack_;

public:
  CompileContext();

  void parse(const ByteArray &input);
  void opt();
  ByteArray dump();
  void link();
  void commit();
  ByteArray call(const std::string &funcname, int bufsize);
  void import(const std::string &path);
};

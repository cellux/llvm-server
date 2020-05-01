#include "doctest.h"

#include <iostream>

#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include "llvm/Support/DynamicLibrary.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

using namespace llvm;

struct LLVMInitializer {
  LLVMInitializer() {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();
    InitializeNativeTargetDisassembler();
  }
};

static LLVMInitializer llvm_init;

class CompileContext {
  LLVMContext ctx;
  std::unique_ptr<ExecutionEngine> ee;
  std::vector<std::unique_ptr<Module>> stack;

public:
  CompileContext() {
    auto mod = std::make_unique<Module>("empty", ctx);
    EngineBuilder builder(std::move(mod));
    std::string ErrorMsg;
    builder.setErrorStr(&ErrorMsg);
    builder.setEngineKind(EngineKind::JIT);
    builder.setVerifyModules(true);
    builder.setOptLevel(CodeGenOpt::Default);
    ee = std::unique_ptr<ExecutionEngine>(builder.create());
    if (!ee) {
      throw std::runtime_error(ErrorMsg);
    }
  }

  void parse(StringRef code) {
    MemoryBufferRef buf(code, "");
    SMDiagnostic err;
    auto mod = parseIR(buf, err, ctx);
    if (!mod) {
      std::string errmsg;
      raw_string_ostream os(errmsg);
      err.print(nullptr, os, false);
      throw std::runtime_error(os.str());
    }
    stack.push_back(std::move(mod));
  }

  void save(const std::string &path) {
    if (stack.size() < 1) {
      throw new std::runtime_error("module stack underflow");
    }
    auto &mod = stack.back();
    std::error_code ec;
    raw_fd_ostream out(path, ec);
    WriteBitcodeToFile(*mod, out);
  }

  void load(const std::string &path) {
    SMDiagnostic err;
    auto mod = parseIRFile(path, err, ctx);
    if (!mod) {
      throw std::runtime_error(err.getMessage());
    }
    stack.push_back(std::move(mod));
  }

  void link() {
    if (stack.size() < 2) {
      throw new std::runtime_error("module stack underflow");
    }
    auto src = std::move(stack.back());
    stack.pop_back();
    auto &dest = stack.back();
    auto err = Linker::linkModules(*dest, std::move(src));
    if (err) {
      throw std::runtime_error("link failure");
    }
  }

  void commit() {
    if (stack.size() < 1) {
      throw new std::runtime_error("module stack underflow");
    }
    ee->addModule(std::move(stack.back()));
    stack.pop_back();
  }

  std::unique_ptr<uint8_t[]> call(const std::string &funcname, int bufsize) {
    typedef void (*Callable)(void*);
    auto f = (Callable) ee->getFunctionAddress(funcname);
    if (!f) {
      throw std::runtime_error("unknown function");
    }
    auto response = std::make_unique<uint8_t[]>(bufsize);
    f(response.get());
    return std::move(response);
  }

  void import(const std::string &path) {
    auto err = sys::DynamicLibrary::LoadLibraryPermanently(path.c_str());
    if (err) {
      throw std::runtime_error("import error");
    }
  }
};

// tests

static const char *src_add = \
  "define i32 @add(i32, i32) {\n"
  "  %3 = alloca i32\n"
  "  %4 = alloca i32\n"
  "  store i32 %0, i32* %3\n"
  "  store i32 %1, i32* %4\n"
  "  %5 = load i32, i32* %3\n"
  "  %6 = load i32, i32* %4\n"
  "  %7 = add i32 %5, %6\n"
  "  ret i32 %7\n"
  "}\n";

static const char *src_add_user =
  "declare i32 @add(i32, i32)\n"
  "define void @add_user(i32*) {\n"
  "  %2 = call i32 @add(i32 3, i32 2)\n"
  "  store i32 %2, i32* %0\n"
  "  ret void\n"
  "}\n";

TEST_CASE("CompileContext") {
  CompileContext cc;
  cc.parse(src_add);
  cc.parse(src_add_user);
  cc.link();
  cc.commit();
  auto response = cc.call("add_user", 1);
  CHECK(response[0] == 5);
}

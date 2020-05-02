#include <iostream>
#include <fstream>

#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/AsmParser/Parser.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

#include "compiler.h"

using namespace std;
using namespace llvm;

CompileContext::CompileContext() {
  auto mod = std::make_unique<Module>("empty", ctx_);
  EngineBuilder builder(std::move(mod));
  string ErrorMsg;
  builder.setErrorStr(&ErrorMsg);
  builder.setEngineKind(EngineKind::JIT);
  builder.setVerifyModules(true);
  builder.setOptLevel(CodeGenOpt::Default);
  ee_ = std::unique_ptr<ExecutionEngine>(builder.create());
  if (!ee_) {
    throw std::runtime_error(ErrorMsg);
  }
}

void CompileContext::parse(const ByteArray &input) {
  StringRef code(input.begin(), input.size());
  MemoryBufferRef buf(code, "");
  SMDiagnostic err;
  std::unique_ptr<Module> mod = parseIR(buf, err, ctx_);
  if (!mod) {
    string errmsg;
    raw_string_ostream os(errmsg);
    err.print(nullptr, os, false);
    throw std::invalid_argument(os.str());
  }
  stack_.push_back(std::move(mod));
}

void CompileContext::opt() {
}

ByteArray CompileContext::dump() {
  if (stack_.size() < 1) {
    throw std::underflow_error("module stack underflow");
  }
  auto &mod = stack_.back();
  ByteArray response;
  raw_svector_ostream os(response);
  WriteBitcodeToFile(*mod, os);
  return std::move(response);
}

void CompileContext::link() {
  if (stack_.size() < 2) {
    throw std::underflow_error("module stack underflow");
  }
  auto src = std::move(stack_.back());
  stack_.pop_back();
  auto &dest = stack_.back();
  auto err = Linker::linkModules(*dest, std::move(src));
  if (err) {
    throw std::runtime_error("link failure");
  }
}

void CompileContext::commit() {
  if (stack_.size() < 1) {
    throw std::underflow_error("module stack underflow");
  }
  ee_->addModule(std::move(stack_.back()));
  stack_.pop_back();
}

ByteArray CompileContext::call(const string &funcname,
                               size_t bufsize) {
  typedef void (*Callable)(void*);
  auto f = (Callable) ee_->getFunctionAddress(funcname);
  if (!f) {
    throw std::invalid_argument("unknown function");
  }
  ByteArray response(bufsize);
  f(response.begin());
  return std::move(response);
}

void CompileContext::import(const string &path) {
  auto err = sys::DynamicLibrary::LoadLibraryPermanently(path.c_str());
  if (err) {
    throw std::invalid_argument("import error");
  }
}

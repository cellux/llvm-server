#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <iostream>

#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include "llvm/Support/DynamicLibrary.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/AsmParser/Parser.h>
#include "llvm/Object/ObjectFile.h"
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/Linker/Linker.h>

using namespace llvm;
using namespace llvm::orc;

static const char *add_src = \
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

static const char *mul_src = \
  "define i32 @mul(i32, i32) {\n"
  "  %3 = alloca i32\n"
  "  %4 = alloca i32\n"
  "  store i32 %0, i32* %3\n"
  "  store i32 %1, i32* %4\n"
  "  %5 = load i32, i32* %3\n"
  "  %6 = load i32, i32* %4\n"
  "  %7 = mul i32 %5, %6\n"
  "  ret i32 %7\n"
  "}\n";

static const char *get_sdl_major_version_src = \
  "%struct.SDL_version = type { i8, i8, i8 }\n"
  "declare void @SDL_GetVersion(%struct.SDL_version*)\n"
  "define i32 @get_sdl_major_version() {\n"
  "  %1 = alloca %struct.SDL_version\n"
  "  call void @SDL_GetVersion(%struct.SDL_version* %1)\n"
  "  %2 = getelementptr inbounds %struct.SDL_version, %struct.SDL_version* %1, i32 0, i32 0\n"
  "  %3 = load i8, i8* %2\n"
  "  %4 = zext i8 %3 to i32\n"
  "  ret i32 %4\n"
  "}\n";

TEST_CASE("InitializeNativeTarget*") {
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();
  InitializeNativeTargetDisassembler();
}

TEST_CASE("Module") {
  LLVMContext ctx;
  Module m("MyModule", ctx);
  CHECK(m.getModuleIdentifier() == "MyModule");
  CHECK(m.getName() == "MyModule");
  CHECK(m.getDataLayoutStr() == "");
  CHECK(m.getTargetTriple() == "");
}

TEST_CASE("parseAssembly") {
  LLVMContext ctx;
  MemoryBufferRef buf(add_src, "add");
  SMDiagnostic err;
  auto mptr = parseAssembly(buf, err, ctx);
  CHECK(mptr.get() != nullptr);
  auto f = mptr->getFunction("add");
  CHECK(f->arg_size() == 2);
}

TEST_CASE("parseAssembly with invalid source") {
  LLVMContext ctx;
  MemoryBufferRef buf("bibircsok", "_");
  SMDiagnostic err;
  auto mptr = parseAssembly(buf, err, ctx);
  CHECK(mptr.get() == nullptr);
  CHECK(err.getMessage().str() == "expected top-level entity");
}

TEST_CASE("parseIR can also parse assembly source") {
  LLVMContext ctx;
  MemoryBufferRef buf(add_src, "add");
  SMDiagnostic err;
  auto mptr = parseIR(buf, err, ctx);
  CHECK(mptr.get() != nullptr);
  auto f = mptr->getFunction("add");
  CHECK(f->arg_size() == 2);
}

TEST_CASE("compiling and calling a function") {
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();
  InitializeNativeTargetDisassembler();
  LLVMContext ctx;
  auto mod = std::make_unique<Module>("main", ctx);
  EngineBuilder builder(std::move(mod));
  std::string ErrorMsg;
  builder.setErrorStr(&ErrorMsg);
  builder.setEngineKind(EngineKind::JIT);
  builder.setVerifyModules(true);
  builder.setOptLevel(CodeGenOpt::Default);
  auto ee = builder.create();
  REQUIRE(ee);
  SUBCASE("adding and calling a function") {
    MemoryBufferRef buf(add_src, "add");
    SMDiagnostic err;
    auto mod = parseIR(buf, err, ctx);
    REQUIRE(mod.get() != nullptr);
    REQUIRE(err.getMessage() == "");
    auto addFn = mod->getFunction("add");
    REQUIRE(addFn->arg_size() == 2);
    ee->addModule(std::move(mod));
    typedef int (*AddFn)(int, int);
    auto add = (AddFn) ee->getFunctionAddress("add");
    REQUIRE(add != nullptr);
    int result = add(3, 2);
    CHECK(result == 5);
    SUBCASE("adding and calling another function") {
      MemoryBufferRef buf(mul_src, "mul");
      SMDiagnostic err;
      auto mod = parseIR(buf, err, ctx);
      ee->addModule(std::move(mod));
      typedef int (*MulFn)(int, int);
      auto mul = (MulFn) ee->getFunctionAddress("mul");
      REQUIRE(mul != nullptr);
      int result = mul(3, 2);
      CHECK(result == 6);
    }
    SUBCASE("loading a shared library and calling a function in it") {
      REQUIRE(!sys::DynamicLibrary::LoadLibraryPermanently("/usr/lib/libSDL2.so"));
      MemoryBufferRef buf(get_sdl_major_version_src, "get_sdl_major_version");
      SMDiagnostic err;
      auto mod = parseIR(buf, err, ctx);
      ee->addModule(std::move(mod));
      typedef int (*get_sdl_major_version_func)();
      auto get_sdl_major_version = (get_sdl_major_version_func) ee->getFunctionAddress("get_sdl_major_version");
      REQUIRE(get_sdl_major_version != nullptr);
      int result = get_sdl_major_version();
      CHECK(result == 2);
    }
  }
}

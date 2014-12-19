// Copyright 2014 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#include "rustllvm.h"

#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Object/Archive.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/IR/Module.h"


using namespace llvm;
using namespace llvm::sys;
using namespace llvm::object;

class RustJITMemoryManager : public SectionMemoryManager
{
    typedef SectionMemoryManager Base;

    const void *morestack;

    public:

    RustJITMemoryManager(const void *morestack_ptr)
        : morestack(morestack_ptr)
        {}

    uint64_t getSymbolAddress(const std::string &Name) override
    {
      printf("symbol: %s\n", Name.c_str());
        if (Name == "__morestack" || Name == "___morestack")
            return reinterpret_cast<uint64_t>(morestack);

        return Base::getSymbolAddress(Name);
    }
};

DEFINE_SIMPLE_CONVERSION_FUNCTIONS(RustJITMemoryManager, LLVMRustJITMemoryManagerRef)

extern "C" LLVMRustJITMemoryManagerRef LLVMRustCreateJITMemoryManager(void *morestack)
{
    return wrap(new RustJITMemoryManager(morestack));
}

extern "C" LLVMBool LLVMRustLoadDynamicLibrary(const char *path)
{
    std::string err;
    DynamicLibrary lib = DynamicLibrary::getPermanentLibrary(path, &err);

    if (!lib.isValid())
        LLVMRustSetLastError(err.c_str());

    return lib.isValid();
}

// Calls LLVMAddModule;
// exists for consistency with LLVMExecutionEngineRemoveModule
extern "C" void LLVMExecutionEngineAddModule(
    LLVMExecutionEngineRef eeref, LLVMModuleRef mref)
{
    LLVMAddModule(eeref, mref);
}

// LLVMRemoveModule exists in LLVM's C bindings,
// but it requires pointless parameters
extern "C" LLVMBool LLVMExecutionEngineRemoveModule(
    LLVMExecutionEngineRef eeref, LLVMModuleRef mref)
{
    ExecutionEngine *ee = unwrap(eeref);
    Module *m = unwrap(mref);

    return ee->removeModule(m);
}

extern "C" LLVMExecutionEngineRef LLVMBuildExecutionEngine(
    LLVMModuleRef mod, LLVMRustJITMemoryManagerRef mref)
{
    // These are necessary for code generation to work properly.
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    std::unique_ptr<Module> m(unwrap(mod));
    RustJITMemoryManager *mm = unwrap(mref);

    std::string error_str;
    TargetOptions options;

    options.JITEmitDebugInfo = true;
    options.NoFramePointerElim = true;

    ExecutionEngine *ee = EngineBuilder(std::move(m))
        .setEngineKind(EngineKind::JIT)
        .setErrorStr(&error_str)
        .setMCJITMemoryManager(mm)
        .setTargetOptions(options)
        .create();

    if (!ee)
        LLVMRustSetLastError(error_str.c_str());

    return wrap(ee);
}


extern "C" LLVMExecutionEngineRef LLVMBuildSimpleExecutionEngine(LLVMModuleRef mod)
{
  printf("LLVMBuildSimpleExecutionEngine\n");
    // These are necessary for code generation to work properly.
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    std::unique_ptr<Module> m(unwrap(mod));

    std::string error_str;
    TargetOptions options;

    options.JITEmitDebugInfo = true;
    options.NoFramePointerElim = true;

    ExecutionEngine *ee = EngineBuilder(std::move(m))
        .setEngineKind(EngineKind::JIT)
        .setErrorStr(&error_str)
        .setTargetOptions(options)
        .create();

    if (!ee)
        LLVMRustSetLastError(error_str.c_str());

    return wrap(ee);
}


extern "C" void LLVMExecutionEngineFinalizeObject(LLVMExecutionEngineRef eeref)
{
    ExecutionEngine *ee = unwrap(eeref);

    ee->finalizeObject();
}


extern "C" void *LLVMExecutionEngineGetPointerToFunction(LLVMExecutionEngineRef EE, LLVMValueRef F)
{
  unwrap(EE)->finalizeObject();

  return unwrap(EE)->getPointerToFunction(unwrap<Function>(F));
}

extern "C" void LLVMExecutionEngineRunFunction(LLVMExecutionEngineRef EE, LLVMValueRef F)
{
  unwrap(EE)->finalizeObject();
  std::vector<GenericValue> ArgVec;
  unwrap(EE)->runFunction(unwrap<Function>(F), ArgVec);
}


extern "C" LLVMValueRef LLVMGetModuleFirstFunction(LLVMModuleRef M)
{
  Module::FunctionListType &FL = unwrap(M)->getFunctionList();

  for (Module::FunctionListType::iterator it = FL.begin(); it != FL.end(); it++) {
    printf("%s\n", it->getValueName()->getKey().data());
    return wrap(it);
  }

  return 0;
}

extern "C" void LLVMExecutionEngineAddArchive(LLVMExecutionEngineRef EE, OwningBinary<Archive> *OB)
{
  unwrap(EE)->finalizeObject();
  unwrap(EE)->addArchive(std::move(*OB));
}


extern "C" LLVMModuleRef LLVMRustCreateBitcodeModule(LLVMContextRef C, char *bc, size_t len)
{
  LLVMContext *c = unwrap(C);
  std::unique_ptr<MemoryBuffer> buf = MemoryBuffer::getMemBufferCopy(StringRef(bc, len));
  ErrorOr<Module *> module = llvm::getLazyBitcodeModule(std::move(buf), *c);
  if (!module) {
    printf("Error: LLVMRustCreateBitcodeModule\n");
    return nullptr;
  }
  
  return wrap(*module);
}

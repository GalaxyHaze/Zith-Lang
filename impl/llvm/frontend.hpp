// impl/llvm/frontend.hpp — Zith IR → LLVM IR converter
#pragma once

#include "zith/vm.hpp"
#include "LLVM/LLVM.hpp"
#include <memory>

namespace zith::LLVM {

std::unique_ptr<Module> ZithToLLVM(LLVMContext& ctx, const Zith::DecodedModule* mod);

}

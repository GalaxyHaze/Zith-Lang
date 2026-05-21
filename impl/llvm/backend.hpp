// impl/llvm/backend.hpp — LLVM IR → Zith IR converter
#pragma once

#include "zith/vm.hpp"
#include "LLVM/LLVM.hpp"

namespace zith::LLVM {

Zith::DecodedModule LLVMToZith(Module* llvm_mod);

}

#pragma once

#include <llvm/CodeGen/TargetLowering.h>
#include <llvm/CodeGen/TargetSubtargetInfo.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/CodeGen/AsmPrinter.h>
#include <llvm/CodeGen/MCAsmInfo.h>
#include <llvm/CodeGen/TargetMachine.h>
#include <llvm/CodeGen/TargetOptions.h>

#include <llvm/MC/TargetRegistry.h>

#include <llvm/Target/TargetOptions.h>
#include <llvm/Target/TargetMachine.h>

namespace zith {
namespace LLVM {
namespace CodeGen {
using TargetMachine = ::llvm::TargetMachine;
using TargetSubtargetInfo = ::llvm::TargetSubtargetInfo;
}
}
}
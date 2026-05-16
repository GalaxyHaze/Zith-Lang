#pragma once

#include <llvm/Analysis/LoopAnalysis.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/Analysis/TargetTransformInfo.h>

#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar/Scalar.h>
#include <llvm/Transforms/Scalar/Scalarize.h>
#include <llvm/Transforms/Vectorize/Vectorize.h>
#include <llvm/Transforms/IPO/IPO.h>
#include <llvm/Transforms/ObjCARC/ObjCARC.h>

namespace zith {
namespace LLVM {
namespace Optimizer {
using InstCombinePass = ::llvm::InstCombinePass;
using ScalarOptsPass = ::llvm::ScalarOptsPass;
using VectorizePass = ::llvm::VectorizePass;
using IPOPrintPass = ::llvm::IPOPrintPass;
using ObjCARCOptsPass = ::llvm::ObjCARCOptsPass;
}
}
}
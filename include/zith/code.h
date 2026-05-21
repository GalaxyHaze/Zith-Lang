// include/zith/code.h — ZBC v2 opcode constants (SSA-based, LLVM-round-trippable)
#pragma once
#include "zith/vm.hpp"

// Re-export Zith opcodes for convenience
using ZithOpcode = Zith::Opcode;

static constexpr ZithOpcode OP_ADD          = Zith::OP_ADD;
static constexpr ZithOpcode OP_SUB          = Zith::OP_SUB;
static constexpr ZithOpcode OP_MUL          = Zith::OP_MUL;
static constexpr ZithOpcode OP_SDIV         = Zith::OP_SDIV;
static constexpr ZithOpcode OP_UDIV         = Zith::OP_UDIV;
static constexpr ZithOpcode OP_SREM         = Zith::OP_SREM;
static constexpr ZithOpcode OP_UREM         = Zith::OP_UREM;
static constexpr ZithOpcode OP_AND          = Zith::OP_AND;
static constexpr ZithOpcode OP_OR           = Zith::OP_OR;
static constexpr ZithOpcode OP_XOR          = Zith::OP_XOR;
static constexpr ZithOpcode OP_SHL          = Zith::OP_SHL;
static constexpr ZithOpcode OP_ASHR         = Zith::OP_ASHR;
static constexpr ZithOpcode OP_LSHR         = Zith::OP_LSHR;

static constexpr ZithOpcode OP_FADD         = Zith::OP_FADD;
static constexpr ZithOpcode OP_FSUB         = Zith::OP_FSUB;
static constexpr ZithOpcode OP_FMUL         = Zith::OP_FMUL;
static constexpr ZithOpcode OP_FDIV         = Zith::OP_FDIV;
static constexpr ZithOpcode OP_FREM         = Zith::OP_FREM;

static constexpr ZithOpcode OP_ICMP         = Zith::OP_ICMP;
static constexpr ZithOpcode OP_FCMP         = Zith::OP_FCMP;

static constexpr ZithOpcode OP_LOAD         = Zith::OP_LOAD;
static constexpr ZithOpcode OP_STORE        = Zith::OP_STORE;
static constexpr ZithOpcode OP_ALLOCA       = Zith::OP_ALLOCA;
static constexpr ZithOpcode OP_GEP          = Zith::OP_GEP;

static constexpr ZithOpcode OP_TRUNC        = Zith::OP_TRUNC;
static constexpr ZithOpcode OP_ZEXT         = Zith::OP_ZEXT;
static constexpr ZithOpcode OP_SEXT         = Zith::OP_SEXT;
static constexpr ZithOpcode OP_FPTRUNC      = Zith::OP_FPTRUNC;
static constexpr ZithOpcode OP_FPEXT        = Zith::OP_FPEXT;
static constexpr ZithOpcode OP_FPTOUI       = Zith::OP_FPTOUI;
static constexpr ZithOpcode OP_FPTOSI       = Zith::OP_FPTOSI;
static constexpr ZithOpcode OP_UITOFP       = Zith::OP_UITOFP;
static constexpr ZithOpcode OP_SITOFP       = Zith::OP_SITOFP;
static constexpr ZithOpcode OP_PTR2INT      = Zith::OP_PTR2INT;
static constexpr ZithOpcode OP_INT2PTR      = Zith::OP_INT2PTR;
static constexpr ZithOpcode OP_BITCAST      = Zith::OP_BITCAST;

static constexpr ZithOpcode OP_PHI          = Zith::OP_PHI;
static constexpr ZithOpcode OP_FREEZE       = Zith::OP_FREEZE;

static constexpr ZithOpcode OP_CALL         = Zith::OP_CALL;
static constexpr ZithOpcode OP_CALL_INDIRECT = Zith::OP_CALL_INDIRECT;
static constexpr ZithOpcode OP_INTRINSIC    = Zith::OP_INTRINSIC;

static constexpr ZithOpcode OP_ATOMICRMW    = Zith::OP_ATOMICRMW;
static constexpr ZithOpcode OP_CMPXCHG      = Zith::OP_CMPXCHG;
static constexpr ZithOpcode OP_FENCE        = Zith::OP_FENCE;

static constexpr ZithOpcode OP_EXTRACTVALUE = Zith::OP_EXTRACTVALUE;
static constexpr ZithOpcode OP_INSERTVALUE  = Zith::OP_INSERTVALUE;

static constexpr ZithOpcode OP_SELECT       = Zith::OP_SELECT;

static constexpr ZithOpcode OP_CONST_INT    = Zith::OP_CONST_INT;
static constexpr ZithOpcode OP_CONST_FLOAT  = Zith::OP_CONST_FLOAT;
static constexpr ZithOpcode OP_CONST_PTR    = Zith::OP_CONST_PTR;
static constexpr ZithOpcode OP_CONST_NULL   = Zith::OP_CONST_NULL;
static constexpr ZithOpcode OP_CONST_AGG    = Zith::OP_CONST_AGG;

static constexpr ZithOpcode OP_BR           = Zith::OP_BR;
static constexpr ZithOpcode OP_COND_BR      = Zith::OP_COND_BR;
static constexpr ZithOpcode OP_SWITCH       = Zith::OP_SWITCH;
static constexpr ZithOpcode OP_RET          = Zith::OP_RET;
static constexpr ZithOpcode OP_UNREACHABLE  = Zith::OP_UNREACHABLE;

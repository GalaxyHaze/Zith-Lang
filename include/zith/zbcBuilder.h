#pragma once
#include "zith/vm.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>

namespace zith {

using Zith::Code;
using Zith::CondCode;
using Zith::Instructions;
using Zith::Register;

class IRBuilder {
private:
    Code code;
    uint32_t nextReg = 0;

public:
    IRBuilder() = default;

    uint32_t allocReg() {
        assert(nextReg < 256 && "Register limit exceeded (max 255)");
        return nextReg++;
    }

    uint32_t currentOffset() const {
        return code.size();
    }

    // ==========================================================================
    // Core Integer ALU (0–16) — [op][dest][src1][src2] — 4 bytes
    // ==========================================================================
    void emitADD(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::ADD, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }
    void emitSUB(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::SUB, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }
    void emitMUL(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::MUL, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }
    void emitDIV_S(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::DIV_S, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }
    void emitDIV_U(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::DIV_U, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }
    void emitMOD_S(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::MOD_S, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }
    void emitMOD_U(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::MOD_U, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }
    void emitAND(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::AND, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }
    void emitOR(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::OR, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }
    void emitXOR(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::XOR, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }
    void emitSHL(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::SHL, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }
    void emitSAR(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::SAR, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }
    void emitSHR(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::SHR, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }
    void emitROL(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::ROL, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }
    void emitROR(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::ROR, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }
    void emitMULH_S(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::MULH_S, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }
    void emitMULH_U(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::MULH_U, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }

    // ==========================================================================
    // Float ALU (17–22) — [op][dest][src1][src2] — 4 bytes
    // ==========================================================================
    void emitFADD(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::FADD, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }
    void emitFSUB(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::FSUB, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }
    void emitFMUL(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::FMUL, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }
    void emitFDIV(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::FDIV, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }
    void emitFMIN(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::FMIN, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }
    void emitFMAX(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::FMAX, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }

    // ==========================================================================
    // Integer Unary (23–29) — [op][dest][src] — 3 bytes
    // ==========================================================================
    void emitNEG(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::NEG, (uint8_t)dest, (uint8_t)src});
    }
    void emitNOT(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::NOT, (uint8_t)dest, (uint8_t)src});
    }
    void emitABS(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::ABS, (uint8_t)dest, (uint8_t)src});
    }
    void emitPOPCNT(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::POPCNT, (uint8_t)dest, (uint8_t)src});
    }
    void emitCLZ(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::CLZ, (uint8_t)dest, (uint8_t)src});
    }
    void emitCTZ(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::CTZ, (uint8_t)dest, (uint8_t)src});
    }
    void emitBYTESWAP(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::BYTESWAP, (uint8_t)dest, (uint8_t)src});
    }

    // ==========================================================================
    // Float Unary (30–37) — [op][dest][src] — 3 bytes
    // ==========================================================================
    void emitFNEG(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::FNEG, (uint8_t)dest, (uint8_t)src});
    }
    void emitFABS(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::FABS, (uint8_t)dest, (uint8_t)src});
    }
    void emitFSQRT(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::FSQRT, (uint8_t)dest, (uint8_t)src});
    }
    void emitFFLOOR(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::FFLOOR, (uint8_t)dest, (uint8_t)src});
    }
    void emitFCEIL(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::FCEIL, (uint8_t)dest, (uint8_t)src});
    }
    void emitFTRUNC(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::FTRUNC, (uint8_t)dest, (uint8_t)src});
    }
    void emitFROUND(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::FROUND, (uint8_t)dest, (uint8_t)src});
    }
    void emitFCLASS(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::FCLASS, (uint8_t)dest, (uint8_t)src});
    }

    // ==========================================================================
    // Register Copy (38) — [op][dest][src] — 3 bytes
    // ==========================================================================
    void emitMOV(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::MOV, (uint8_t)dest, (uint8_t)src});
    }

    // ==========================================================================
    // Four-Operand (39–40) — [op][dest][src1][src2][src3] — 5 bytes
    // ==========================================================================
    void emitFFMA(uint32_t dest, uint32_t src1, uint32_t src2, uint32_t src3) {
        code.insert(code.end(), {Op::FFMA, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2, (uint8_t)src3});
    }
    void emitSELECT(uint32_t dest, uint32_t cond, uint32_t true_val, uint32_t false_val) {
        code.insert(code.end(), {Op::SELECT, (uint8_t)dest, (uint8_t)cond, (uint8_t)true_val, (uint8_t)false_val});
    }

    // ==========================================================================
    // Immediate Loads (41–44)
    // ==========================================================================
    void emitMOV_I8(uint32_t dest, uint8_t imm) {
        code.insert(code.end(), {Op::MOV_I8, (uint8_t)dest, imm});
    }
    void emitMOV_I32(uint32_t dest, int32_t imm) {
        code.push_back(Op::MOV_I32);
        code.push_back((uint8_t)dest);
        uint32_t u = (uint32_t)imm;
        code.push_back(u & 0xFF);
        code.push_back((u >> 8) & 0xFF);
        code.push_back((u >> 16) & 0xFF);
        code.push_back((u >> 24) & 0xFF);
    }
    void emitMOV_I64(uint32_t dest, int64_t imm) {
        code.push_back(Op::MOV_I64);
        code.push_back((uint8_t)dest);
        uint64_t u = (uint64_t)imm;
        for (int i = 0; i < 8; i++) {
            code.push_back((u >> (i * 8)) & 0xFF);
        }
    }
    void emitMOV_F64(uint32_t dest, double imm) {
        code.push_back(Op::MOV_F64);
        code.push_back((uint8_t)dest);
        uint64_t bits;
        memcpy(&bits, &imm, sizeof(double));
        for (int i = 0; i < 8; i++) {
            code.push_back((bits >> (i * 8)) & 0xFF);
        }
    }

    // ==========================================================================
    // Stack Allocate (45) — [op][dest][i32_size][i8_align] — 10 bytes
    // ==========================================================================
    void emitALLOCA(uint32_t dest, uint32_t size, uint8_t align) {
        code.push_back(Op::ALLOCA);
        code.push_back((uint8_t)dest);
        code.push_back(size & 0xFF);
        code.push_back((size >> 8) & 0xFF);
        code.push_back((size >> 16) & 0xFF);
        code.push_back((size >> 24) & 0xFF);
        code.push_back(align);
        // 3 bytes padding reserved for future
        code.push_back(0);
        code.push_back(0);
        code.push_back(0);
    }

    // ==========================================================================
    // Comparisons (46–47) — [op][dest][cond][src1][src2] — 5 bytes
    // ==========================================================================
    void emitICMP(uint32_t dest, CondCode cond, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::ICMP, (uint8_t)dest, (uint8_t)cond, (uint8_t)src1, (uint8_t)src2});
    }
    void emitFCMP(uint32_t dest, CondCode cond, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::FCMP, (uint8_t)dest, (uint8_t)cond, (uint8_t)src1, (uint8_t)src2});
    }

    // ==========================================================================
    // Shift by Immediate (48–50) — [op][dest][src][i8] — 4 bytes
    // ==========================================================================
    void emitSHL_I(uint32_t dest, uint32_t src, uint8_t shift) {
        code.insert(code.end(), {Op::SHL_I, (uint8_t)dest, (uint8_t)src, shift});
    }
    void emitSAR_I(uint32_t dest, uint32_t src, uint8_t shift) {
        code.insert(code.end(), {Op::SAR_I, (uint8_t)dest, (uint8_t)src, shift});
    }
    void emitSHR_I(uint32_t dest, uint32_t src, uint8_t shift) {
        code.insert(code.end(), {Op::SHR_I, (uint8_t)dest, (uint8_t)src, shift});
    }

    // ==========================================================================
    // GetElementPtr (51) — [op][dest][ptr][index] — 4 bytes
    // ==========================================================================
    void emitGEP(uint32_t dest, uint32_t ptr, uint32_t index) {
        code.insert(code.end(), {Op::GEP, (uint8_t)dest, (uint8_t)ptr, (uint8_t)index});
    }

    // ==========================================================================
    // Test & Freeze (52–53)
    // ==========================================================================
    void emitTEST(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.insert(code.end(), {Op::TEST, (uint8_t)dest, (uint8_t)src1, (uint8_t)src2});
    }
    void emitFREEZE(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::FREEZE, (uint8_t)dest, (uint8_t)src});
    }

    // ==========================================================================
    // Memory Loads (54–57) — [op][dest][addr] — 3 bytes
    // ==========================================================================
    void emitLOAD64(uint32_t dest, uint32_t addr) {
        code.insert(code.end(), {Op::LOAD64, (uint8_t)dest, (uint8_t)addr});
    }
    void emitLOAD32(uint32_t dest, uint32_t addr) {
        code.insert(code.end(), {Op::LOAD32, (uint8_t)dest, (uint8_t)addr});
    }
    void emitLOAD16(uint32_t dest, uint32_t addr) {
        code.insert(code.end(), {Op::LOAD16, (uint8_t)dest, (uint8_t)addr});
    }
    void emitLOAD8(uint32_t dest, uint32_t addr) {
        code.insert(code.end(), {Op::LOAD8, (uint8_t)dest, (uint8_t)addr});
    }

    // ==========================================================================
    // Memory Stores (58–61) — [op][addr][src] — 3 bytes
    // ==========================================================================
    void emitSTORE64(uint32_t addr, uint32_t src) {
        code.insert(code.end(), {Op::STORE64, (uint8_t)addr, (uint8_t)src});
    }
    void emitSTORE32(uint32_t addr, uint32_t src) {
        code.insert(code.end(), {Op::STORE32, (uint8_t)addr, (uint8_t)src});
    }
    void emitSTORE16(uint32_t addr, uint32_t src) {
        code.insert(code.end(), {Op::STORE16, (uint8_t)addr, (uint8_t)src});
    }
    void emitSTORE8(uint32_t addr, uint32_t src) {
        code.insert(code.end(), {Op::STORE8, (uint8_t)addr, (uint8_t)src});
    }

    // ==========================================================================
    // Float Memory (62–63) — [op][dest/addr][addr/src] — 3 bytes
    // ==========================================================================
    void emitLOADF64(uint32_t dest, uint32_t addr) {
        code.insert(code.end(), {Op::LOADF64, (uint8_t)dest, (uint8_t)addr});
    }
    void emitSTOREF64(uint32_t addr, uint32_t src) {
        code.insert(code.end(), {Op::STOREF64, (uint8_t)addr, (uint8_t)src});
    }

    // ==========================================================================
    // Control Flow (64–75)
    // ==========================================================================

    // Labels are represented as placeholder offsets (4 bytes, LE).
    // A linker pass resolves label IDs to actual offsets after emission.

    // Unconditional branch (5 bytes)
    void emitBR(uint32_t label_offset) {
        code.push_back(Op::BR);
        writeLabel(label_offset);
    }

    // Conditional branch (10 bytes): [BR_COND][cond][lbl_true][lbl_false]
    void emitBR_COND(uint32_t cond_reg, uint32_t label_true, uint32_t label_false) {
        code.push_back(Op::BR_COND);
        code.push_back((uint8_t)cond_reg);
        writeLabel(label_true);
        writeLabel(label_false);
    }

    // Jump table — variable length
    void emitBR_TABLE(uint32_t val_reg, uint32_t default_label, uint32_t n_cases,
                      const uint32_t* case_values, const uint32_t* case_labels) {
        code.push_back(Op::BR_TABLE);
        code.push_back((uint8_t)val_reg);
        writeLabel(default_label);
        code.push_back((uint8_t)n_cases);
        for (uint32_t i = 0; i < n_cases; i++) {
            writeI32(case_values[i]);
            writeLabel(case_labels[i]);
        }
    }

    // Return — void (1 byte) or with value (2 bytes)
    void emitRET() {
        code.push_back(Op::RET);
    }
    void emitRET_VAL(uint32_t val_reg) {
        code.push_back(Op::RET);
        code.push_back((uint8_t)val_reg);
    }

    // Indirect call — variable length
    void emitCALL(uint32_t ret_reg, uint32_t func_reg, uint32_t n_args, const uint32_t* args) {
        code.push_back(Op::CALL);
        code.push_back((uint8_t)ret_reg);
        code.push_back((uint8_t)func_reg);
        code.push_back((uint8_t)n_args);
        for (uint32_t i = 0; i < n_args; i++) {
            code.push_back((uint8_t)args[i]);
        }
    }

    // Direct call — variable length
    void emitCALL_DIRECT(uint32_t ret_reg, uint32_t func_label, uint32_t n_args, const uint32_t* args) {
        code.push_back(Op::CALL_DIRECT);
        code.push_back((uint8_t)ret_reg);
        writeLabel(func_label);
        code.push_back((uint8_t)n_args);
        for (uint32_t i = 0; i < n_args; i++) {
            code.push_back((uint8_t)args[i]);
        }
    }

    // Invoke (call with landing pad)
    void emitINVOKE(uint32_t ret_reg, uint32_t func_reg, uint32_t n_args,
                    const uint32_t* args, uint32_t normal_label, uint32_t unwind_label) {
        code.push_back(Op::INVOKE);
        code.push_back((uint8_t)ret_reg);
        code.push_back((uint8_t)func_reg);
        code.push_back((uint8_t)n_args);
        for (uint32_t i = 0; i < n_args; i++) {
            code.push_back((uint8_t)args[i]);
        }
        writeLabel(normal_label);
        writeLabel(unwind_label);
    }

    // Landing pad (exception entry)
    void emitLANDINGPAD(uint32_t dest) {
        code.insert(code.end(), {Op::LANDINGPAD, (uint8_t)dest});
    }

    // PHI node — variable length
    void emitPHI(uint32_t dest, uint32_t n_pairs,
                 const uint32_t* src_regs, const uint32_t* labels) {
        code.push_back(Op::PHI);
        code.push_back((uint8_t)dest);
        code.push_back((uint8_t)n_pairs);
        for (uint32_t i = 0; i < n_pairs; i++) {
            code.push_back((uint8_t)src_regs[i]);
            writeLabel(labels[i]);
        }
    }

    // Unreachable
    void emitUNREACHABLE() {
        code.push_back(Op::UNREACHABLE);
    }

    // External FFI call — variable length
    void emitEXTERN(uint32_t func_id, uint32_t ret_reg, uint32_t n_args, const uint32_t* args) {
        code.push_back(Op::EXTERN);
        code.push_back((uint8_t)func_id);
        code.push_back((uint8_t)ret_reg);
        code.push_back((uint8_t)n_args);
        for (uint32_t i = 0; i < n_args; i++) {
            code.push_back((uint8_t)args[i]);
        }
    }

    // Label marker (not executed; marks basic block start) — 5 bytes
    void emitLABEL(uint32_t label_id) {
        code.push_back(Op::LABEL);
        writeLabel(label_id);
    }

    // ==========================================================================
    // Type Conversions (76–79) — [op][dest][src] — 3 bytes
    // ==========================================================================
    void emitI2F(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::I2F, (uint8_t)dest, (uint8_t)src});
    }
    void emitF2I(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::F2I, (uint8_t)dest, (uint8_t)src});
    }
    void emitF2I_ROUND(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::F2I_ROUND, (uint8_t)dest, (uint8_t)src});
    }
    void emitBITCAST(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::BITCAST, (uint8_t)dest, (uint8_t)src});
    }

    // ==========================================================================
    // Extend / Truncate (80–83) — [op][dest][src] — 3 bytes
    // ==========================================================================
    void emitSEXT8(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::SEXT8, (uint8_t)dest, (uint8_t)src});
    }
    void emitSEXT16(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::SEXT16, (uint8_t)dest, (uint8_t)src});
    }
    void emitZEXT8(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::ZEXT8, (uint8_t)dest, (uint8_t)src});
    }
    void emitZEXT16(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::ZEXT16, (uint8_t)dest, (uint8_t)src});
    }

    // ==========================================================================
    // Pointer Conversions (84–85) — [op][dest][src] — 3 bytes
    // ==========================================================================
    void emitPTR2I(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::PTR2I, (uint8_t)dest, (uint8_t)src});
    }
    void emitI2PTR(uint32_t dest, uint32_t src) {
        code.insert(code.end(), {Op::I2PTR, (uint8_t)dest, (uint8_t)src});
    }

    // ==========================================================================
    // Memory Fences (86–89) — [op][ordering] — 2 bytes
    // ==========================================================================
    void emitFENCE_SEQ() {
        code.insert(code.end(), {Op::FENCE_SEQ, 0});
    }
    void emitFENCE_ACQ() {
        code.insert(code.end(), {Op::FENCE_ACQ, 0});
    }
    void emitFENCE_REL() {
        code.insert(code.end(), {Op::FENCE_REL, 0});
    }
    void emitFENCE_ACQ_REL() {
        code.insert(code.end(), {Op::FENCE_ACQ_REL, 0});
    }

    // ==========================================================================
    // Atomics (90) — [op][addr][cmp][new][ordering] — 5 bytes
    // ==========================================================================
    void emitCMPXCHG(uint32_t addr, uint32_t cmp, uint32_t new_val, uint8_t ordering) {
        code.insert(code.end(), {Op::CMPXCHG, (uint8_t)addr, (uint8_t)cmp, (uint8_t)new_val, ordering});
    }

    // ==========================================================================
    // Misc System (91–94)
    // ==========================================================================
    void emitNOP() {
        code.push_back(Op::NOP);
    }
    void emitBREAKPOINT() {
        code.push_back(Op::BREAKPOINT);
    }
    void emitYIELD() {
        code.push_back(Op::YIELD);
    }
    void emitSYSCALL(uint8_t num) {
        code.insert(code.end(), {Op::SYSCALL, num});
    }

    // ==========================================================================
    // Halt (255) — [HALT] — 1 byte
    // ==========================================================================
    void emitHALT() {
        code.push_back(Op::HALT);
    }

    // ==========================================================================
    // Debug Extensions (HALT escape: [HALT][ext_op][args...])
    // ==========================================================================
    void emitPRINT(uint32_t reg) {
        code.insert(code.end(), {Op::HALT, ExtOp::EXT_PRINT, (uint8_t)reg});
    }
    void emitINPUT(uint32_t reg) {
        code.insert(code.end(), {Op::HALT, ExtOp::EXT_INPUT, (uint8_t)reg});
    }
    void emitASSERT(uint32_t reg) {
        code.insert(code.end(), {Op::HALT, ExtOp::EXT_ASSERT, (uint8_t)reg});
    }

    // ==========================================================================
    // Finalize
    // ==========================================================================
    Code finalize() {
        return code;
    }

    const Code& getCode() const {
        return code;
    }

private:
    // Write a 32-bit label/offset in little-endian format
    void writeLabel(uint32_t label) {
        code.push_back(label & 0xFF);
        code.push_back((label >> 8) & 0xFF);
        code.push_back((label >> 16) & 0xFF);
        code.push_back((label >> 24) & 0xFF);
    }

    void writeI32(int32_t val) {
        uint32_t u = (uint32_t)val;
        code.push_back(u & 0xFF);
        code.push_back((u >> 8) & 0xFF);
        code.push_back((u >> 16) & 0xFF);
        code.push_back((u >> 24) & 0xFF);
    }
};

} // namespace zith

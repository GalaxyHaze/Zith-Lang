// Include/VM/Builder.h
#pragma once
//#include "Code.h"
#include "zith/vm.hpp"
#include <cassert>
#include <cstdint>

namespace zith {

class IRBuilder {
private:
    Code code;
    uint32_t nextReg = 0;
    
public:
    IRBuilder() = default;
    
    uint32_t allocReg() { 
        assert(nextReg < 256 && "Register limit exceeded");
        return nextReg++; 
    }
    
    // ========== SIGNED INT OPERATIONS ==========
    void emitIADD(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::iADD);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitISUB(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::iSUB);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitIMUL(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::iMUL);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitIDIV(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::iDIV);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitIMOD(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::iMOD);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitIAND(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::iAND);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitIOR(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::iOR);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitIXOR(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::iXOR);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitINOT(uint32_t dest, uint32_t src) {
        code.push_back(Op::iNOT);
        code.push_back(dest);
        code.push_back(src);
    }
    
    void emitIEQ(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::iEQ);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitINE(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::iNE);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitILT(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::iLT);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitILE(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::iLE);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitIGT(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::iGT);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitIGE(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::iGE);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitINEG(uint32_t dest, uint32_t src) {
        code.push_back(Op::iNEG);
        code.push_back(dest);
        code.push_back(src);
    }
    
    void emitISHL(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::iSHL);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitISAR(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::iSAR);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    // ========== UNSIGNED INT OPERATIONS ==========
    void emitUADD(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::uADD);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitUSUB(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::uSUB);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitUMUL(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::uMUL);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitUDIV(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::uDIV);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitUMOD(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::uMOD);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitUAND(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::uAND);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitUOR(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::uOR);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitUXOR(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::uXOR);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitUNOT(uint32_t dest, uint32_t src) {
        code.push_back(Op::uNOT);
        code.push_back(dest);
        code.push_back(src);
    }
    
    void emitUSHL(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::uSHL);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitUSHR(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::uSHR);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    // ========== FLOAT OPERATIONS ==========
    void emitFADD(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::fADD);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitFSUB(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::fSUB);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitFMUL(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::fMUL);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitFDIV(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::fDIV);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitFMOD(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::fMOD);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitFNEG(uint32_t dest, uint32_t src) {
        code.push_back(Op::fNEG);
        code.push_back(dest);
        code.push_back(src);
    }
    
    void emitFEQ(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::fEQ);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitFNE(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::fNE);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitFLT(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::fLT);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitFLE(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::fLE);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitFGT(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::fGT);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitFGE(uint32_t dest, uint32_t src1, uint32_t src2) {
        code.push_back(Op::fGE);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
    }
    
    void emitFSIN(uint32_t dest, uint32_t src) {
        code.push_back(Op::fSIN);
        code.push_back(dest);
        code.push_back(src);
    }
    
    void emitFCOS(uint32_t dest, uint32_t src) {
        code.push_back(Op::fCOS);
        code.push_back(dest);
        code.push_back(src);
    }
    
    void emitFSQRT(uint32_t dest, uint32_t src) {
        code.push_back(Op::fSQRT);
        code.push_back(dest);
        code.push_back(src);
    }
    
    void emitFABS(uint32_t dest, uint32_t src) {
        code.push_back(Op::fABS);
        code.push_back(dest);
        code.push_back(src);
    }
    
    void emitFFLOOR(uint32_t dest, uint32_t src) {
        code.push_back(Op::fFLOOR);
        code.push_back(dest);
        code.push_back(src);
    }
    
    void emitFCEIL(uint32_t dest, uint32_t src) {
        code.push_back(Op::fCEIL);
        code.push_back(dest);
        code.push_back(src);
    }
    
    void emitFFMA(uint32_t dest, uint32_t src1, uint32_t src2, uint32_t src3) {
        code.push_back(Op::fFMA);
        code.push_back(dest);
        code.push_back(src1);
        code.push_back(src2);
        code.push_back(src3);
    }
    
    // ========== DATA MOVEMENT ==========
    void emitMOV(uint32_t dest, uint32_t src) {
        code.push_back(Op::MOV);
        code.push_back(dest);
        code.push_back(src);
    }
    
    void emitMOVI(uint32_t dest, int64_t imm) {
        code.push_back(Op::MOVI);
        code.push_back(dest);
        code.push_back((uint32_t)(imm & 0xFFFFFFFFu));
        code.push_back((uint32_t)((imm >> 32) & 0xFFFFFFFFu));
    }
    
    void emitMOVF(uint32_t dest, double imm) {
        code.push_back(Op::MOVF);
        code.push_back(dest);
        uint64_t bits;
        memcpy(&bits, &imm, sizeof(double));
        code.push_back((uint32_t)(bits & 0xFFFFFFFFu));
        code.push_back((uint32_t)((bits >> 32) & 0xFFFFFFFFu));
    }
    
    void emitLD(uint32_t dest, uint32_t addr_reg) {
        code.push_back(Op::LD);
        code.push_back(dest);
        code.push_back(addr_reg);
    }
    
    void emitST(uint32_t addr_reg, uint32_t src) {
        code.push_back(Op::ST);
        code.push_back(addr_reg);
        code.push_back(src);
    }
    
    void emitLDB(uint32_t dest, uint32_t addr_reg) {
        code.push_back(Op::LDB);
        code.push_back(dest);
        code.push_back(addr_reg);
    }
    
    void emitSTB(uint32_t addr_reg, uint32_t src) {
        code.push_back(Op::STB);
        code.push_back(addr_reg);
        code.push_back(src);
    }
    
    void emitMOVZX(uint32_t dest, uint32_t src) {
        code.push_back(Op::MOVZX);
        code.push_back(dest);
        code.push_back(src);
    }
    
    void emitMOVSX(uint32_t dest, uint32_t src) {
        code.push_back(Op::MOVSX);
        code.push_back(dest);
        code.push_back(src);
    }
    
    // ========== CONTROL FLOW ==========
    uint32_t currentOffset() const {
        return code.size();
    }
    
    void emitJMP(uint32_t target) {
        code.push_back(Op::JMP);
        code.push_back(target);
    }
    
    void emitJT(uint32_t cond, uint32_t target) {
        code.push_back(Op::JT);
        code.push_back(cond);
        code.push_back(target);
    }
    
    void emitJF(uint32_t cond, uint32_t target) {
        code.push_back(Op::JF);
        code.push_back(cond);
        code.push_back(target);
    }
    
    void emitCALL(uint32_t target) {
        code.push_back(Op::CALL);
        code.push_back(target);
    }
    
    void emitRET() {
        code.push_back(Op::RET);
    }
    
    void emitEXTERN(uint32_t func_id) {
        code.push_back(Op::EXTERN);
        code.push_back(func_id);
    }
    
    // ========== TYPE CONVERSION ==========
    void emitI2F(uint32_t dest, uint32_t src) {
        code.push_back(Op::I2F);
        code.push_back(dest);
        code.push_back(src);
    }
    
    void emitF2I(uint32_t dest, uint32_t src) {
        code.push_back(Op::F2I);
        code.push_back(dest);
        code.push_back(src);
    }
    
    void emitI2U(uint32_t dest, uint32_t src) {
        code.push_back(Op::I2U);
        code.push_back(dest);
        code.push_back(src);
    }
    
    void emitU2I(uint32_t dest, uint32_t src) {
        code.push_back(Op::U2I);
        code.push_back(dest);
        code.push_back(src);
    }
    
    void emitF2ITrunc(uint32_t dest, uint32_t src) {
        code.push_back(Op::f2iTRUNC);
        code.push_back(dest);
        code.push_back(src);
    }
    
    void emitF2IRound(uint32_t dest, uint32_t src) {
        code.push_back(Op::f2iROUND);
        code.push_back(dest);
        code.push_back(src);
    }
    
    void emitSEXT8(uint32_t dest, uint32_t src) {
        code.push_back(Op::SEXT8);
        code.push_back(dest);
        code.push_back(src);
    }
    
    void emitSEXT16(uint32_t dest, uint32_t src) {
        code.push_back(Op::SEXT16);
        code.push_back(dest);
        code.push_back(src);
    }
    
    // ========== MISC ==========
    void emitNOP() {
        code.push_back(Op::NOP);
    }
    
    void emitHALT() {
        code.push_back(Op::HALT);
    }
    
    // ========== DEBUG EXTENSIONS ==========
    void emitPRINT(uint32_t reg) {
        code.push_back(Op::HALT);
        code.push_back(ExtOp::EXT_PRINT);
        code.push_back(reg);
    }
    
    void emitINPUT(uint32_t reg) {
        code.push_back(Op::HALT);
        code.push_back(ExtOp::EXT_INPUT);
        code.push_back(reg);
    }
    
    void emitASSERT(uint32_t reg) {
        code.push_back(Op::HALT);
        code.push_back(ExtOp::EXT_ASSERT);
        code.push_back(reg);
    }
    
    Code finalize() { 
        return code; 
    }
    
    const Code& getCode() const {
        return code;
    }
};

} // namespace Zith
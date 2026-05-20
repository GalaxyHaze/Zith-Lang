// Include/VM/Code.h
#pragma once
#include <cstdint>
#include <vector>

using ZithCode = std::vector<uint8_t>;

enum Op : uint8_t {
    // ========== INTEIROS ASSINADOS (0-31) ==========
    iADD = 0,    iSUB = 1,    iMUL = 2,    iDIV = 3,
    iMOD = 4,    iAND = 5,    iOR  = 6,    iXOR = 7,
    iNOT = 8,    iEQ  = 9,    iNE  = 10,   iLT  = 11,
    iLE  = 12,   iGT  = 13,   iGE  = 14,   iNEG = 15,
    
    // Bit operations
    iSHL = 16,   iSAR = 17,   iROL = 18,   iROR = 19,
    
    // Bit counting
    iCLZ = 20,   iCTZ = 21,   iPOPCNT = 22,
    
    // Special
    iABS = 23,   iIMULH = 24, iIDIVH = 25,
    
    // Reserved para futuro (26-31)
    iRES26 = 26, iRES27 = 27, iRES28 = 28,
    iRES29 = 29, iRES30 = 30, iRES31 = 31,

    // ========== INTEIROS SEM SINAL (32-63) ==========
    uADD = 32,   uSUB = 33,   uMUL = 34,   uDIV = 35,
    uMOD = 36,   uAND = 37,   uOR  = 38,   uXOR = 39,
    uNOT = 40,   uEQ  = 41,   uNE  = 42,   uLT  = 43,
    uLE  = 44,   uGT  = 45,   uGE  = 46,
    
    // Bit operations
    uSHL = 47,   uSHR = 48,   uROL = 49,   uROR = 50,
    
    // Bit test/set/clear/toggle
    uBT  = 51,   uBTS = 52,   uBTR = 53,   uBTC = 54,
    
    // Bit counting
    uCLZ = 55,   uCTZ = 56,   uPOPCNT = 57,
    
    // Special
    uUMULH = 58,
    
    // Reserved (59-63)
    uRES59 = 59, uRES60 = 60, uRES61 = 61,
    uRES62 = 62, uRES63 = 63,

    // ========== FLOATS (64-127) ==========
    // Basic arithmetic
    fADD = 64,   fSUB = 65,   fMUL = 66,   fDIV = 67,
    fMOD = 68,   fNEG = 69,
    
    // Comparisons
    fEQ  = 70,   fNE  = 71,   fLT  = 72,   fLE  = 73,
    fGT  = 74,   fGE  = 75,
    
    // Math functions
    fSIN = 76,   fCOS = 77,   fTAN = 78,   fATAN = 79,
    fATAN2 = 80, fLOG = 81,   fEXP = 82,   fLOG2 = 83,
    fEXP2 = 84,  fPOW = 85,   fSQRT = 86,  fABS = 87,
    
    // Rounding
    fFLOOR = 88, fCEIL = 89,  fROUND = 90, fTRUNC = 91,
    
    // Advanced
    fFMA = 92,   fFMS = 93,   fFNMA = 94,  fMAX = 95,
    fMIN = 96,   fRCP = 97,   fRSQRT = 98,
    
    // Reserved (99-127) - para vetorização futura
    fRES99 = 99, fRES100 = 100, fRES101 = 101, fRES102 = 102,
    // ... (muitos reservados para vector ops depois)
    fRES127 = 127,

    // ========== MOVIMENTO DE DADOS (128-159) ==========
    // Load/Store
    MOV    = 128,    // MOV dest, src (copia registro)
    LD     = 129,    // LD dest, [addr] - load 64-bit
    ST     = 130,    // ST [addr], src - store 64-bit
    LDB    = 131,    // LD dest, [addr] - load byte
    STB    = 132,    // ST [addr], src - store byte
    LDH    = 133,    // LD dest, [addr] - load halfword (16-bit)
    STH    = 134,    // ST [addr], src - store halfword
    
    // Immediate loads
    MOVI   = 135,    // MOVI dest, imm64 - load immediate int
    MOVF   = 136,    // MOVF dest, imm_double - load float immediate
    MOVC   = 137,    // MOVC dest, imm_char - load char immediate
    
    // Conversions with zero/sign extend
    MOVZX  = 138,    // Zero extend + move
    MOVSX  = 139,    // Sign extend + move
    
    // Atomic operations
    SWAP   = 140,    // SWAP r1, r2 - atomic swap registers
    XCHG   = 141,    // XCHG [addr], reg - atomic exchange with memory
    CMPXCHG = 142,   // CMPXCHG [addr], cmp_reg, new_reg - compare and swap
    XADD   = 143,    // XADD [addr], reg - atomic add
    
    // Addressing
    LEA    = 144,    // LEA dest, [base + offset] - load effective address
    
    // Reserved (145-159)
    dRES145 = 145, dRES146 = 146, dRES147 = 147,
    dRES148 = 148, dRES149 = 149, dRES150 = 150,
    dRES151 = 151, dRES152 = 152, dRES153 = 153,
    dRES154 = 154, dRES155 = 155, dRES156 = 156,
    dRES157 = 157, dRES158 = 158, dRES159 = 159,

    // ========== CONTROLE DE FLUXO (160-191) ==========
    // Unconditional
    JMP    = 160,    // JMP target
    CALL   = 161,    // CALL target
    RET    = 162,    // RET
    CALLR  = 163,    // CALLR reg (indirect call)
    JMPR   = 164,    // JMPR reg (indirect jump)
    
    // Conditional (registro + target)
    JT     = 165,    // JT cond_reg, target (jump if true/non-zero)
    JF     = 166,    // JF cond_reg, target (jump if false/zero)
    
    // Comparação com jump implícita (dest, src1, src2, target)
    JEQ    = 167,    // JEQ src1, src2, target (jump if equal)
    JNE    = 168,    // JNE src1, src2, target
    JLT    = 169,    // JLT src1, src2, target (signed less-than)
    JLE    = 170,    // JLE src1, src2, target
    JGT    = 171,    // JGT src1, src2, target
    JGE    = 172,    // JGE src1, src2, target
    
    // Unsigned comparisons with jump
    JULT   = 173,    // JULT src1, src2, target (unsigned less-than)
    JULE   = 174,    // JULE src1, src2, target
    JUGT   = 175,    // JUGT src1, src2, target
    JUGE   = 176,    // JUGE src1, src2, target
    
    // FFI / External
    EXTERN = 177,    // EXTERN func_id
    
    // Syscall / Exceptions
    SYSCALL = 178,   // SYSCALL syscall_num
    
    // Reserved (179-191)
    cfRES179 = 179, cfRES180 = 180, cfRES181 = 181,
    cfRES182 = 182, cfRES183 = 183, cfRES184 = 184,
    cfRES185 = 185, cfRES186 = 186, cfRES187 = 187,
    cfRES188 = 188, cfRES189 = 189, cfRES190 = 190,
    cfRES191 = 191,

    // ========== COMPARAÇÃO (192-210) ==========
    // Simple comparisons (result in dest)
    EQ     = 192,    // EQ dest, src1, src2 (dest = src1 == src2)
    NE     = 193,    // NE dest, src1, src2
    LT     = 194,    // LT dest, src1, src2 (signed)
    LE     = 195,
    GT     = 196,
    GE     = 197,
    
    // Unsigned
    ULT    = 198,    // ULT dest, src1, src2
    ULE    = 199,
    UGT    = 200,
    UGE    = 201,
    
    // Bitwise test
    TEST   = 202,    // TEST dest, src1, src2 (dest = src1 & src2)
    CMP    = 203,    // CMP dest, src1, src2 (dest = cmp result: -1, 0, 1)
    
    // Reserved (204-210)
    cRES204 = 204, cRES205 = 205, cRES206 = 206,
    cRES207 = 207, cRES208 = 208, cRES209 = 209,
    cRES210 = 210,

    // ========== CONVERSÃO DE TIPOS (211-230) ==========
    I2F    = 211,    // int to float
    F2I    = 212,    // float to int (truncate)
    I2U    = 213,    // int to unsigned (bit reinterpret)
    U2I    = 214,    // unsigned to int
    
    F2ITRUNC = 215,  // float to int truncate
    F2IROUND = 216,  // float to int round
    
    // 32/64-bit conversions
    I32TO64 = 217,   // Sign extend 32 to 64
    U32TO64 = 218,   // Zero extend 32 to 64
    I64TO32 = 219,   // Truncate 64 to 32
    
    // Float precision
    F32TO64 = 220,   // float (32-bit) to double (64-bit)
    F64TO32 = 221,   // double to float
    
    // Byte/halfword operations
    SEXT8  = 222,    // Sign extend byte to 64-bit
    SEXT16 = 223,    // Sign extend 16-bit to 64-bit
    ZEXT8  = 224,    // Zero extend byte
    ZEXT16 = 225,    // Zero extend 16-bit
    
    // Reserved (226-230)
    cvRES226 = 226, cvRES227 = 227, cvRES228 = 228,
    cvRES229 = 229, cvRES230 = 230,

    // ========== MISC / SYSTEM (231-254) ==========
    NOP    = 231,    // NOP - no operation
    
    // Memory ordering (C++11 / LLVM model)
    FENCE_SEQ   = 232,  // Sequentially Consistent
    FENCE_ACQ   = 233,  // Acquire
    FENCE_REL   = 234,  // Release
    FENCE_ACQ_REL = 235, // Acquire/Release
    
    // Stack operations (para quando houver stack virtual)
    PUSH   = 236,    // PUSH src
    POP    = 237,    // POP dest
    
    // Allocation
    ALLOCA = 238,    // ALLOCA dest, size - stack allocate
    YIELD  = 239,    // YIELD - hint to scheduler
    BREAKPOINT = 240,
    // Reserved (241-254)
    mRES241 = 241,mRES242 = 242, mRES243 = 243,
    mRES244 = 244,mRES245 = 245, mRES246 = 246,
    mRES247 = 247,mRES248 = 248, mRES249 = 249, 
    mRES250 = 250,mRES251 = 251, mRES252 = 252, 
    mRES253 = 253,mRES254 = 254,

    // ========== EXTENSION SET (255) ==========
    // 0xFF é HALT normal, mas pode ter subcommands em debug mode
    HALT   = 255,
};

// Extension set para debug (usados quando opcode=0xFF em debug mode)
enum ExtOp : uint32_t {
    EXT_PRINT  = 0,     // Imprimir registrador
    EXT_INPUT  = 1,     // Ler entrada
    EXT_ASSERT = 2,     // Assert (falha se != 0)
    EXT_TRACE  = 3,     // Trace execution
    EXT_BREAKPOINT = 4, // Breakpoint
};
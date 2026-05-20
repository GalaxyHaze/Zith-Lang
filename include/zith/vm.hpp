#pragma once
#include "zith/types.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdint.h>
#include <stddef.h>
#include <string_view>
#include <type_traits>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

    // --- Core Types ---
    typedef uint8_t code_t;
    typedef uint8_t reg_t;

    // Runtime Value (matches register size)
    typedef union {
        int64_t     as_i64; // Serve para: bool, char, int8, int16, int32, int64
        uint64_t    as_u64; // Serve para: uint8, uint16, uint32, uint64, size_t
        double      as_f64; // Serve para: double (e float se convertido)
        void*       as_ptr; // Ponteiros e Handles
        
        // Apenas se precisares de passar structs de 16 bytes POR VALOR
        struct { uintptr_t low; uintptr_t high; } as_small;
    } ZithFFiValue;


    // --- FFI Definitions ---
    
    // Type IDs for the FFI system to know how to marshal data
    typedef enum {
        ZITH_FFI_VOID,
        ZITH_FFI_I64,  // Covers int8, int16, int32, int64
        ZITH_FFI_U64,
        ZITH_FFI_F64,  // Covers float, double,
        ZITH_FFI_SMALL,
        ZITH_FFI_PTR   // Opaque pointer
    } ZithFFIType;

    // Describes the C function signature
    typedef struct {
        ZithFFIType ret_type;
        ZithFFIType* arg_types; // Pointer to array of types
        uint8_t      arg_count;
    } ZithFFISignature;

    // The actual entry passed to the VM
    typedef struct {
        void*             func_ptr; // The C function address
        ZithFFISignature  sig;      // How to call it
    } ZithFFIFunction;

    // --- Module & Function ---

    typedef struct {
        code_t*     code;
        size_t      code_size;
        char*       name;
    } ZithVmFunction;

    typedef struct {
        ZithVmFunction* fns;
        size_t          fn_count;
    } ZithVmModule;

    // --- Execution ---

    // Updated signature: 
    // 'externs' is the array of C functions you prepared.
    // 'extern_count' is the length of that array.
    int ZithVmExecute(
        ZithVmModule* module, 
        ZithFFIFunction* externs, 
        size_t extern_len,
        code_t* body
    );

#ifdef __cplusplus
}
#endif

#include <unordered_map>
namespace Zith {

    // Include/VM/Code.h
#pragma once
#include <cstdint>
#include <vector>

using Code = std::vector<uint32_t>;

// Organização dos 256 opcodes (.zbc Intermediate Representation):
// 0-31:   Signed Integer Arithmetic & Logic
// 32-63:  Unsigned Integer Arithmetic & Logic
// 64-127: Floating Point & Vector Operations
// 128-159: Data Movement (Load/Store/Atomic)
// 160-191: Control Flow
// 192-210: Comparison & Flags
// 211-230: Type Conversions
// 231-254: System, Memory, & Misc
// 255:    HALT / Extension Set

enum Op : uint32_t {
    // ========== INTEIROS ASSINADOS (0-31) ==========
    iADD = 0,    iSUB = 1,    iMUL = 2,    iDIV = 3,
    iMOD = 4,    iAND = 5,    iOR  = 6,    iXOR = 7,
    iNOT = 8,    iEQ  = 9,    iNE  = 10,   iLT  = 11,
    iLE  = 12,   iGT  = 13,   iGE  = 14,   iNEG = 15,
    
    // Extended signed ops
    iADC  = 16,  // Add with Carry
    iSBB  = 17,  // Subtract with Borrow
    iINC  = 18,  // Increment
    iDEC  = 19,  // Decrement
    iSHL  = 20,  // Shift Left Logical
    iSAR  = 21,  // Shift Right Arithmetic
    iROL  = 22,  // Rotate Left
    iROR  = 23,  // Rotate Right
    iCLZ  = 24,  // Count Leading Zeros (signed context)
    iCTZ  = 25,  // Count Trailing Zeros
    iSEXT = 26,  // Sign Extend
    iPOPCNT = 27, // Population Count
    iBYTESWAP = 28, // Byte Swap
    iABS  = 29,  // Absolute Value
    iIMULH = 30, // Signed Multiply High (result >> 32)
    iIDIVH = 31, // Signed Divide High (Remainder logic)

    // ========== INTEIROS SEM SINAL (32-63) ==========
    uADD = 32,   uSUB = 33,   uMUL = 34,   uDIV = 35,
    uMOD = 36,   uAND = 37,   uOR  = 38,   uXOR = 39,
    uNOT = 40,   uEQ  = 41,   uNE  = 42,   uLT  = 43,
    uLE  = 44,   uGT  = 45,   uGE  = 46,   uSHL = 47,
    uSHR = 48,   // Shift Right Logical
    
    // Extended unsigned ops
    uADC  = 49,  uSBB  = 50,  uINC  = 51,  uDEC  = 52,
    uROL  = 53,  uROR  = 54,  uBT   = 55,  // Bit Test
    uBTS  = 56,  // Bit Test and Set
    uBTR  = 57,  // Bit Test and Reset
    uBTC  = 58,  // Bit Test and Complement
    uCLZ  = 59,  // Count Leading Zeros (unsigned context)
    uBSF  = 60,  // Bit Scan Forward
    uPOPCNT = 61, // Population Count
    uBYTESWAP = 62, // Byte Swap
    uUMULH = 63, // Unsigned Multiply High

    // ========== FLOATS (64-127) ==========
    fADD = 64,   fSUB = 65,   fMUL = 66,   fDIV = 67,
    fMOD = 68,   fNEG = 69,   fEQ  = 70,   fNE  = 71,
    fLT  = 72,   fLE  = 73,   fGT  = 74,   fGE  = 75,
    fSIN = 76,   fCOS = 77,   fSQRT = 78,  fABS = 79,
    fFLOOR = 80, fCEIL = 81,
    
    fSUBR = 82,  fDIVR = 83,
    fMAX  = 84,  fMIN  = 85,
    fFMA  = 86,  // Fused Multiply-Add
    fFMS  = 87,  // Fused Multiply-Sub
    fFNMA = 88,  // Fused Negate Multiply-Add
    fRCP  = 89,  fRSQRT = 90,
    fTAN  = 91,  fATAN = 92,  fATAN2 = 93,
    fLOG  = 94,  fEXP  = 95,
    fLOG2 = 96,  fEXP2 = 97,
    fPOW  = 98,
    fROUND = 99, fTRUNC = 100,
    fGETEXP = 101, fGETMAN = 102, fSCALE = 103,
    fCLASS = 104,
    fUCOMI = 105, fCOMI = 106,
    fMOV  = 107,  fMOVZ = 108,
    fMOVA = 109,  fMOVU = 110,
    fSWAP = 111,
    fUNPCKL = 112, fUNPCKH = 113, fSHUF = 114, fBLEND = 115,
    // Reserved space for future vector ops (116-127)
    fRES116 = 116, fRES117 = 117, fRES118 = 118, fRES119 = 119,
    fRES120 = 120, fRES121 = 121, fRES122 = 122, fRES123 = 123,
    fRES124 = 124, fRES125 = 125, fRES126 = 126, fRES127 = 127,

    // ========== MOVIMENTO DE DADOS (128-159) ==========
    MOV    = 128,    MOVI   = 129,    MOVF   = 130,
    LD     = 131,    ST     = 132,
    LDB    = 133,    LDH    = 134,    // Load Byte/Halfword
    STB    = 135,    STH    = 136,    // Store Byte/Halfword
    // Slots 137-140 FREED (LDBZ, LDBS, LDHZ, LDHS removed)
    RES137 = 137,  RES138 = 138,  RES139 = 139,  RES140 = 140,
    
    LEA    = 141,    // Load Effective Address
    SWAP   = 142,    XCHG   = 143,    CMPXCHG = 144,
    XADD   = 145,    MOVZX  = 146,    MOVSX   = 147,
    // Reserved space for future addressing modes (148-159)
    RES148 = 148,  RES149 = 149,  RES150 = 150,  RES151 = 151,
    RES152 = 152,  RES153 = 153,  RES154 = 154,  RES155 = 155,
    RES156 = 156,  RES157 = 157,  RES158 = 158,  RES159 = 159,

    // ========== CONTROLE DE FLUXO (160-191) ==========
    JMP    = 160,    JT     = 161,    JF     = 162,
    CALL   = 163,    RET    = 164,    EXTERN = 165,
    
    JO     = 166,    JNO    = 167,    JB     = 168,
    JAE    = 169,    JZ     = 170,    JNZ    = 171,
    JBE    = 172,    JA     = 173,    JS     = 174,
    JNS    = 175,    JP     = 176,    JNP    = 177,
    JL     = 178,    JGE    = 179,    JLE    = 180,
    JG     = 181,
    
    CALLR  = 182,    JMPR   = 183,
    // Slots 184-187 FREED (LOOP, LOOPE, LOOPNE, JCXZ removed)
    RES184 = 184,  RES185 = 185,  RES186 = 186,  RES187 = 187,
    
    SYSCALL    = 188,    IRET   = 189,
    BND_CHK = 191,  // Bounds Check (formerly BOUND)

    // ========== COMPARAÇÃO GENÉRICA (192-210) ==========
    EQ     = 192,    NE     = 193,    LT     = 194,
    LE     = 195,    GT     = 196,    GE     = 197,
    TEST   = 198,    CMP    = 199,
    SETO   = 200,    SETNO  = 201,    SETB   = 202,
    SETAE  = 203,    SETZ   = 204,    SETNZ  = 205,
    SETS   = 206,    SETNS  = 207,    SETL   = 208,
    SETG   = 209,    SETP   = 210,

    // ========== CONVERSÃO DE TIPOS (211-230) ==========
    I2F    = 211,    F2I    = 212,    I2U    = 213,
    U2I    = 214,    
    f2iTRUNC = 215,  // Truncate Float to Int
    f2iROUND = 216,  // Round Float to Int
    F2D    = 217,    D2F    = 218,    I2D    = 219,
    D2I    = 220,    SEXT8  = 221,    SEXT16 = 222,
    ZEXT8  = 223,    ZEXT16 = 224,    CVTTSD2SI = 225,
    CVTSD2SI = 226,  CVTSI2SD = 227,  CVTSS2SD = 228,
    CVTSD2SS = 229,  PTR2I  = 230,

    // ========== MISC (231-254) ==========
    NOP    = 231,    PUSH   = 232,    POP    = 233,
    ENTER  = 234,    LEAVE  = 235,
    //BND_CHK = 236,  // Bounds Check (Moved here from 191 in cleanup? No, mapped to 191 above. Let's place gCLZ here)
    
    // gCLZ takes 237. Renamed from CLZ for generic usage
    gCLZ   = 237, 
    yield  = 238, 
    
    // Memory Ordering (C++11 / LLVM Model)
    FENCE_SEQ   = 239,  // Sequentially Consistent (mfence)
    FENCE_ACQ   = 240,  // Acquire (lfence)
    FENCE_REL   = 241,  // Release (sfence)
    FENCE_ACQ_REL = 242, // Acquire/Release

    // New Essential Primitives
    TPB   = 243,  // Get Thread Pointer Base
    ALLOCA = 244, // Stack Allocate (Alloca)
    ASM = 245,  // Inline Assembly Container

    // Slots 246-247 FREED (INVD, WBINVD removed)
    RES246 = 246, RES247 = 247,

    INT    = 248,    INT3   = 249,    INTO   = 250,
    // Slot 251 FREED (Redundant IRET removed)
    RES251 = 251,
    /*SYSCALL = 252,*/   SYSRET = 253,   UNREACHABLE    = 254,

    // ========== EXTENSION SET (255) ==========
    HALT   = 255,
    };

    // Extension set para debug (usados quando opcode=0xFF)
    enum ExtOp : uint32_t {
    EXT_PRINT  = 0,
    EXT_INPUT  = 1,
    EXT_OPEN   = 2,
    EXT_READ   = 3,
    EXT_WRITE  = 4,
    EXT_CLOSE  = 5,
    EXT_ASSERT = 6,
    };


    // 1. High-level IR Value (Used during construction/AST)
    // Note: We don't use this during execution, only during setup.

class IRValue {

    union {
        int64_t     i64;
        uint64_t    u64;
        double      f64;
        const char* str;
        std::nullptr_t none;
    } value;
    enum class type { I64, U64, F64, STR, NONE } tag;

    public:
    
    template<class T>
    IRValue(T val) { this->set(val); }

    bool isEmpty() const { return tag == type::NONE; }

    // Versão 1: Passando a TAG via template
    template<type Tag>
    auto is() const {
        // Precisamos de if constexpr para que o compilador ignore os tipos que não batem com a Tag
        if constexpr (Tag == type::I64) return (tag == Tag) ? std::optional<int64_t>(value.i64) : std::nullopt;
        else if constexpr (Tag == type::U64) return (tag == Tag) ? std::optional<uint64_t>(value.u64) : std::nullopt;
        else if constexpr (Tag == type::F64) return (tag == Tag) ? std::optional<double>(value.f64) : std::nullopt;
        else if constexpr (Tag == type::STR) return (tag == Tag) ? std::optional<const char*>(value.str) : std::nullopt;
        else return std::nullopt;
    }

    // Versão 2: Passando o TIPO via template (mais comum)
    template<class T>
    std::optional<T> is() const {
        using D = std::decay_t<T>;
        // Normaliza o tipo (ex: int vira int64_t)
        using U = std::conditional_t<std::is_integral_v<D> && !std::is_same_v<D, bool>,
                    std::conditional_t<std::is_signed_v<D>, int64_t, uint64_t>,
                    std::conditional_t<std::is_convertible_v<D, const char*>, const char*, D>
                  >;

        if constexpr (std::is_same_v<U, int64_t>) {
            if (tag == type::I64) return static_cast<T>(value.i64);
        } 
        else if constexpr (std::is_same_v<U, uint64_t>) {
            if (tag == type::U64) return static_cast<T>(value.u64);
        } 
        else if constexpr (std::is_same_v<U, double>) {
            if (tag == type::F64) return static_cast<T>(value.f64);
        } 
        else if constexpr (std::is_same_v<U, const char*>) {
            if (tag == type::STR) return value.str;
        }

        return std::nullopt; 
    }

    template<class T>
    void set(T&& val) {
        using D = std::decay_t<T>;
        using U = std::conditional_t<std::is_integral_v<D> && !std::is_same_v<D, bool>,
                    std::conditional_t<std::is_signed_v<D>, int64_t, uint64_t>,
                    std::conditional_t<std::is_convertible_v<D, const char*>, const char*, D>
                  >;

        // Atribuição segura via casting para a union
        if constexpr (std::is_same_v<U, int64_t>) { value.i64 = (int64_t)val; tag = type::I64; }
        else if constexpr (std::is_same_v<U, uint64_t>) { value.u64 = (uint64_t)val; tag = type::U64; }
        else if constexpr (std::is_same_v<U, double>) { value.f64 = (double)val; tag = type::F64; }
        else if constexpr (std::is_same_v<U, const char*>) { value.str = (const char*)val; tag = type::STR; }
        else { value.none = nullptr; tag = type::NONE; }
    }
    };

    struct Instruction {
        uint8_t opcode;
        // Flexible arguments for the builder, but we will constrain these to max 3 registers later
        std::vector<IRValue> args; 
    };

    struct Function {
        std::string name;
        std::vector<Instruction> body;
        std::vector<std::string> param_names;
    };

    inline std::unordered_map<std::string_view, Function> functions = {};

    // 2. Instruction Definition
    

    // 3. Function Definition
    

    // 4. The Module
    struct Module {
        std::vector<Function> functions;
        
        // "Flatten" this Module into the C ABI format
        // This function allocates memory that must be freed manually later
        std::unique_ptr<ZithVmModule, void(*)(ZithVmModule*)> compile();
    };

    // 5. The Wrapper Context
    class Context {
    public:
        Context();
        ~Context();

        int run(const Module& module, const std::string& entryFunctionName);
    };
}
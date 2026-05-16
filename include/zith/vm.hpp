#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdint.h>
#include <stddef.h>
#include <type_traits>

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

namespace Zith {

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

public:
    enum class type { I64, U64, F64, STR, NONE } tag;

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

    // 2. Instruction Definition
    struct Instruction {
        uint8_t opcode;
        // Flexible arguments for the builder, but we will constrain these to max 3 registers later
        std::vector<IRValue> args; 
    };

    // 3. Function Definition
    struct Function {
        std::string name;
        std::vector<Instruction> body;
        std::vector<std::string> param_names;
    };

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
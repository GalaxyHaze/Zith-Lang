#pragma once

#include "hir/hir-module.hpp"
#include "memory/string-interner.hpp"
#include "types/type-intern.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace zith::interp {

enum class HirInterpStatus : uint8_t {
    Ok,
    MissingMain,
    UnsupportedExpr,
    MissingExtern,
    InternalError,
};

struct HirInterpResult {
    HirInterpStatus status = HirInterpStatus::Ok;
    int64_t exitCode       = 0;
    std::string message;
    std::string output;
};

class HirInterpreter {
public:
    HirInterpreter(const hir::HirModule &module, const memory::StringInterner &interner,
                   const types::TypeIntern &types);

    HirInterpResult runMain();

private:
    struct Value {
        enum class Kind : uint8_t { Int, Ptr, String, Invalid };

        Kind kind = Kind::Invalid;
        int64_t i = 0;
        std::string_view text;
    };

    struct Frame {
        const hir::HirFunction *fn = nullptr;
        size_t block               = 0;
        memory::DynArray<Value> regs;
        memory::DynArray<int64_t> slots;

        explicit Frame(const hir::HirFunction *function, memory::Arena &arena)
            : regs(arena), slots(arena) {
            fn = function;
        }
    };

    const hir::HirModule &module_;
    const memory::StringInterner &interner_;
    const types::TypeIntern &types_;
    memory::Arena frameArena_;
    std::string output_;

    bool findFunction(std::string_view name, const hir::HirFunction *&fn) const;
    const hir::HirFunction *findMain() const;
    Value eval(hir::HirExprId id, Frame &frame, HirInterpResult &result);
    bool runFunction(const hir::HirFunction &fn, memory::DynArray<Value> &args,
                     int64_t &returnValue, HirInterpResult &result);
    bool runExtern(const hir::HirFunction &fn, memory::DynArray<Value> &args, int64_t &returnValue);
};

} // namespace zith::interp

#pragma once

#include "common/test-output-buffer.hpp"
#include "hir/hir-module.hpp"
#include "memory/string-interner.hpp"
#include "types/type-intern.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

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
    static constexpr uint32_t kInvalidObject = ~uint32_t{0};

    struct Value {
        enum class Kind : uint8_t { Int, Float, Ptr, String, Aggregate, Slice, Dyn, Invalid };

        Kind kind = Kind::Invalid;
        int64_t i = 0;
        double f = 0.0;
        std::string_view text;
        Value *cell = nullptr;
        uint32_t object = kInvalidObject;
        size_t offset = 0;
        size_t length = 0;
        memory::InternedId vtable = memory::kInvalidInternedId;
    };

    struct RuntimeObject {
        types::TypeId type = types::kInvalidType;
        memory::DynArray<Value> fields;
        std::vector<char> bytes;
        bool alive = true;

        RuntimeObject(memory::Arena &arena, types::TypeId type_) : type(type_), fields(arena) {}
    };

    struct Frame {
        const hir::HirFunction *fn = nullptr;
        size_t block               = 0;
        memory::DynArray<Value> regs;
        memory::DynArray<Value> slots;

        explicit Frame(const hir::HirFunction *function, memory::Arena &arena, size_t slot_count)
            : regs(arena), slots(arena) {
            fn = function;
            slots.resize(slot_count);
        }
    };

    const hir::HirModule &module_;
    const memory::StringInterner &interner_;
    const types::TypeIntern &types_;
    memory::Arena frameArena_;
    common::TestOutputBuffer output_;
    std::vector<RuntimeObject> objects_;
    size_t slotCount_ = 0;

    bool findFunction(std::string_view name, const hir::HirFunction *&fn) const;
    const hir::HirFunction *findMain() const;
    uint32_t addObject(types::TypeId type);
    Value pointerToCell(Value &cell) const;
    Value pointerToObject(uint32_t object) const;
    Value addressOf(hir::HirExprId id, Frame &frame, HirInterpResult &result);
    Value readPointer(const Value &pointer, HirInterpResult &result);
    bool writePointer(const Value &pointer, Value value, HirInterpResult &result);
    bool readByte(const Value &pointer, size_t index, char &value) const;
    bool writeByte(const Value &pointer, size_t index, char value);
    Value cloneAggregate(const Value &value);
    bool isAggregateType(types::TypeId type) const;
    Value eval(hir::HirExprId id, Frame &frame, HirInterpResult &result);
    bool runFunction(const hir::HirFunction &fn, memory::DynArray<Value> &args,
                     Value &returnValue, HirInterpResult &result);
    bool runExtern(const hir::HirFunction &fn, memory::DynArray<Value> &args, Value &returnValue);
};

} // namespace zith::interp

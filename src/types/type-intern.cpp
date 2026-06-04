#include "type-intern.hpp"

namespace zith::types {

    TypeIntern::TypeIntern(memory::Arena &arena) : types_(arena) {}

    TypeId TypeIntern::intern(TypeData data) {
        (void)data;
        return kErrorType;
    }

    TypeId TypeIntern::internInt(IntWidth w) {
        (void)w;
        return kIntType;
    }
    TypeId TypeIntern::internFloat(FloatWidth w) {
        (void)w;
        return kFloatType;
    }
    TypeId TypeIntern::internPtr(TypeId pointee) {
        (void)pointee;
        return kErrorType;
    }
    TypeId TypeIntern::internArray(TypeId elem, uint32_t count) {
        (void)elem;
        (void)count;
        return kErrorType;
    }
    TypeId TypeIntern::internFn(std::vector<TypeId> params, TypeId ret) {
        (void)params;
        (void)ret;
        return kErrorType;
    }
    const TypeData &TypeIntern::lookup(TypeId id) const {
        (void)id;
        static TypeData d;
        return d;
    }
    TypeKind TypeIntern::kindOf(TypeId id) const {
        (void)id;
        return TypeKind::Error;
    }

} // namespace zith::types

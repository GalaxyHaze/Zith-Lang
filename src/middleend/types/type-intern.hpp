#pragma once

#include "infra/alloc/arena.hpp"
#include "infra/collections/dyn-array.hpp"
#include "middleend/types/type-kind.hpp"

namespace zith::middleend::types {

    class TypeIntern {
        infra::collections::DynArray<TypeData> types_;

    public:
        explicit TypeIntern(infra::alloc::Arena& arena);

        TypeId intern(TypeData data);
        TypeId internInt(IntWidth w);
        TypeId internFloat(FloatWidth w);
        TypeId internPtr(TypeId pointee);
        TypeId internArray(TypeId elem, uint32_t count);
        TypeId internFn(std::vector<TypeId> params, TypeId ret);

        const TypeData& lookup(TypeId id) const;
        TypeKind kindOf(TypeId id) const;
    };

}

#pragma once

#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"
#include "types/type-kind.hpp"

namespace zith::types {

    class TypeIntern {
        memory::DynArray<TypeData> types_;

    public:
        explicit TypeIntern(memory::Arena &arena);

        TypeId intern(TypeData data);
        TypeId internInt(IntWidth w);
        TypeId internFloat(FloatWidth w);
        TypeId internPtr(TypeId pointee);
        TypeId internArray(TypeId elem, uint32_t count);
        TypeId internFn(memory::DynArray<TypeId> params, TypeId ret);

        const TypeData &lookup(TypeId id) const;
        TypeKind kindOf(TypeId id) const;
    };

} // namespace zith::types

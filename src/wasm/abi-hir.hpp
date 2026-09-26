#pragma once

#include "hir/hir-module.hpp"
#include "memory/arena.hpp"
#include "memory/span.hpp"
#include "memory/string-interner.hpp"
#include "session/compilation-session.hpp"
#include "types/type-intern.hpp"
#include "types/type-kind.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <span>
#include <string_view>
#include <vector>

namespace zith::wasm {

/// Versioned flat HIR blob produced by the browser ABI.
///
/// The blob is deliberately self-contained. It stores the lowered function
/// bodies, string pool, and type rows that VM v2 lowering needs, without
/// pointing into the compilation session's arena.
struct FlatHirBlob {
    std::vector<uint8_t> bytes;
};

struct EncodeHirResult {
    bool ok = false;
    std::string message;
    FlatHirBlob blob;
};

/// Serialize the current lowered HIR module into a flat, stateless blob.
auto encodeHir(const session::CompilationSession &session) -> EncodeHirResult;

/// Rebuilt compiler state for a decoded flat HIR blob.
struct DecodedHir {
    bool ok = false;
    std::string message;
    memory::Arena arena;
    memory::StringInterner interner;
    types::TypeIntern types;
    hir::HirModule module;

    DecodedHir()
        : interner(arena), types(arena, interner), module(arena) {}
};

/// Decode flat HIR and rebuild all compiler state needed by VM v2 lowering.
/// The output object must outlive the decoded arena state.
auto decodeHir(std::span<const uint8_t> blob, DecodedHir &out) -> bool;

} // namespace zith::wasm

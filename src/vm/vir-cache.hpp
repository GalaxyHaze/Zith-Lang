#pragma once

#include "hir/hir-module.hpp"
#include "memory/arena.hpp"
#include "memory/string-interner.hpp"
#include "types/type-intern.hpp"
#include "vm/typed-ir.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace zith::cache {
struct Artifact;
}

namespace zith::vm {

struct CachedVirResult {
    bool ok = false;
    bool reused = false;
    std::string message;
};

[[nodiscard]] bool encodeCachedModule(const Module &module, std::vector<uint8_t> &payload);

[[nodiscard]] bool decodeCachedModule(std::span<const uint8_t> payload,
                                      uint16_t section_schema_version,
                                      memory::StringInterner &interner, memory::Arena &arena,
                                      Module &out);

/// Reuse a compatible optional VIR section, or lower the provided HIR when
/// the section is absent, malformed, or version-incompatible.
[[nodiscard]] CachedVirResult loadOrLowerCachedModule(
    const cache::Artifact *artifact, const hir::HirModule &hir,
    memory::StringInterner &interner, const types::TypeIntern &types,
    memory::Arena &arena, Module &out);

} // namespace zith::vm

#pragma once

#include "cache/cache-types.hpp"
#include "hir/hir-module.hpp"
#include "memory/flat-map.hpp"
#include "memory/string-interner.hpp"
#include "types/type-intern.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace zith::cache {

// Builds a cache::Artifact from compiler session state.  Extracts the public
// ABI surface (exported/module-visible declarations), generic blueprints, and
// concrete HIR bodies into the self-contained compact representation used by
// the zirl format.
class ArtifactBuilder {
public:
    ArtifactBuilder(const types::TypeIntern &types, const hir::HirModule &hir,
                    const memory::StringInterner &interner, ContentFingerprint source_fingerprint,
                    FrontendIdentity frontend_identity,
                    SemanticCompatibilityProfile semantic_profile);

    [[nodiscard]] Artifact build(std::string_view canonical_path, std::string_view module_name,
                                 const std::vector<DependencyRecord> &deps);

private:
    const types::TypeIntern &types_;
    const hir::HirModule &hir_;
    const memory::StringInterner &interner_;
    ContentFingerprint source_fingerprint_;
    FrontendIdentity frontend_identity_;
    SemanticCompatibilityProfile semantic_profile_;

    // String interning into the artifact's own string table.
    std::vector<std::string> strings_;
    memory::FlatMap<std::string_view, uint32_t> string_index_;
    [[nodiscard]] uint32_t internString(std::string_view s);

    // Type interning into the artifact's compact type table.
    std::vector<CompactType> compact_types_;
    memory::FlatMap<types::TypeId, uint32_t> type_index_;
    std::vector<uint32_t> function_signature_type_ids_;
    [[nodiscard]] uint32_t internType(types::TypeId id);
    [[nodiscard]] uint32_t internFunctionSignature(const hir::HirFunction &function);
    [[nodiscard]] CompactType convertType(types::TypeId id);
    [[nodiscard]] CompactExpr convertExpr(hir::HirExprId id);

    // Compute the public ABI hash from exported declarations.
    [[nodiscard]] uint64_t computePublicAbiHash() const;
};

} // namespace zith::cache

#pragma once

#include <cstdint>
#include <string_view>

namespace zith::zirl {

// Zith Intermediate Representation on disk (zirl).
//
// One artifact per canonical module path.  Layout:
//
//   [fixed header]
//   [identity strings]
//   [dependency records]
//   [section table]
//   section payloads
//
// Container and HIR versions evolve independently. Section IDs and flags make
// optional metadata skippable without weakening required HIR validation.

inline constexpr uint32_t kMagic = 0x5A49524Cu; // "ZIRL"
// Version 20 replaces the positional section table and legacy frontend cache
// key with explicit section IDs and a canonical HIR compatibility header.
inline constexpr uint32_t kContainerVersion = 20;
inline constexpr uint8_t kEndianLittle   = 1;
inline constexpr uint16_t kSectionSchemaVersion = 1;
inline constexpr uint16_t kMetadataSectionSchemaVersion = 2;
inline constexpr uint16_t kSectionFlagRequired = 1;
inline constexpr uint16_t kKnownSectionFlags = kSectionFlagRequired;
inline constexpr uint16_t kFixedHeaderSize = 96;
inline constexpr uint16_t kSectionEntrySize = 24;

enum class SectionId : uint16_t {
    Header    = 0,
    Metadata  = 1, // sec2: strings, paths, identifiers, constants
    Decls     = 2, // sec3: exported/module-visible declarations
    Templates = 3, // sec4: generic blueprints
    Code      = 4, // sec5: concrete lowered HIR bodies
    Attrs     = 5, // sec6: HIR residual ownership/call/fn attributes
    Instantiations = 6, // optional summary; concrete bodies are in Code
    FrontendProvenance = 7, // optional producer identity, not an HIR compatibility key
    Debug     = 8, // optional, reserved
    Vir       = 9, // optional derived VM IR; never required to hydrate HIR
};

struct SectionEntry {
    uint16_t id = 0;
    uint16_t flags = 0;
    uint16_t schema_version = kSectionSchemaVersion;
    uint16_t reserved = 0;
    uint64_t offset = 0;
    uint64_t size   = 0;
};

struct FileHeader {
    uint32_t magic              = kMagic;
    uint32_t container_version  = kContainerVersion;
    uint32_t hir_schema_version = 0;
    uint8_t endianness          = kEndianLittle;
    uint8_t reserved             = 0;
    uint16_t section_count      = 0;
    uint32_t header_size        = 0; // bytes from file start to end of section table
    uint32_t checksum           = 0; // FNV-1a fold over identity, dependencies, and sections
    uint64_t required_features   = 0;
    uint64_t optional_features   = 0;
    uint64_t source_fp_primary   = 0;
    uint64_t source_fp_secondary = 0;
    uint32_t semantic_profile_version = 0;
    uint32_t public_abi_hi      = 0;
    uint32_t public_abi_lo      = 0;
    uint32_t dep_count          = 0;
    uint32_t decl_count         = 0;
    uint32_t template_count     = 0;
    uint32_t fn_count           = 0;
    uint32_t canonical_path_len = 0; // followed by canonical_path bytes
    uint32_t module_name_len    = 0;
    uint32_t semantic_profile_name_len = 0;
};

[[nodiscard]] uint32_t fnv1a32(std::string_view data) noexcept;
[[nodiscard]] uint32_t fnv1a32(const uint8_t *data, size_t len) noexcept;

} // namespace zith::zirl

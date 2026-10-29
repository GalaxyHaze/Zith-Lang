#pragma once

#include "cache/cache-types.hpp"
#include "zirl/zirl-buffer.hpp"

namespace zith::zirl {

// Debug section (SectionId::Debug). Carries assert/check locations for the
// native test runner. Layout: a u32 count followed by records of
// `{test_name, failure_id, path, line, column}`.
inline constexpr uint16_t kDebugSectionSchemaVersion = 1;

[[nodiscard]] bool encodeDebug(const cache::Artifact &artifact, ByteWriter &w);
[[nodiscard]] bool decodeDebug(ByteReader &r, cache::Artifact &out);

} // namespace zith::zirl

#pragma once

#include "cache/cache-types.hpp"
#include "zirl/zirl-buffer.hpp"

namespace zith::zirl {

// Debug section (SectionId::Debug). Carries the assert-site location table the
// native test runner uses to resolve a failing `#[test]` slot back to a source
// position. Layout: a u32 count followed by `count` records of
// `{test_name, assert_id, path, line, column}`.
inline constexpr uint16_t kDebugSectionSchemaVersion = 1;

[[nodiscard]] bool encodeDebug(const cache::Artifact &artifact, ByteWriter &w);
[[nodiscard]] bool decodeDebug(ByteReader &r, cache::Artifact &out);

} // namespace zith::zirl

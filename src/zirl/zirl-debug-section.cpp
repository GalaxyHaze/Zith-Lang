#include "zirl-debug-section.hpp"

namespace zith::zirl {

bool encodeDebug(const cache::Artifact &artifact, ByteWriter &w) {
    w.writeU32(static_cast<uint32_t>(artifact.test_assert_locations.size()));
    for (const auto &location : artifact.test_assert_locations) {
        w.writeBlob(location.test_name);
        w.writeU32(location.assert_id);
        w.writeBlob(location.path);
        w.writeU32(location.line);
        w.writeU32(location.column);
    }
    return true;
}

bool decodeDebug(ByteReader &r, cache::Artifact &out) {
    uint32_t count = 0;
    if (!r.readU32(count))
        return false;
    if (!r.canReadU32Count(count))
        return false;
    out.test_assert_locations.resize(count);
    for (auto &location : out.test_assert_locations) {
        if (!r.readBlob(location.test_name) || !r.readU32(location.assert_id) ||
            !r.readBlob(location.path) || !r.readU32(location.line) ||
            !r.readU32(location.column))
            return false;
    }
    return true;
}

} // namespace zith::zirl

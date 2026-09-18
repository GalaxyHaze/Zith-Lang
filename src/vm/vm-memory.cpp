#include "vm/vm-memory.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace zith::vm {

namespace {

auto alignUp(std::size_t value, std::size_t alignment) -> std::size_t {
    if (alignment == 0)
        return value;
    // Only power-of-two alignments are valid. A non-power-of-two mask would
    // overflow, so keep the contract safe by refusing alignment.
    if ((alignment & (alignment - 1)) != 0)
        return std::numeric_limits<std::size_t>::max();
    const std::size_t mask = alignment - 1;
    if (value > std::numeric_limits<std::size_t>::max() - mask)
        return std::numeric_limits<std::size_t>::max();
    return (value + mask) & ~mask;
}

} // namespace

LinearMemory::LinearMemory(std::size_t initialCapacity) {
    const std::size_t reserveSize = std::max<std::size_t>(initialCapacity, 1);
    bytes_.reserve(reserveSize);
}

auto LinearMemory::read(std::size_t offset, std::size_t count) const -> std::vector<uint8_t> {
    std::vector<uint8_t> result;
    if (offset > bytes_.size() || count > bytes_.size() - offset)
        return result;
    result.resize(count);
    std::memcpy(result.data(), bytes_.data() + offset, count);
    return result;
}

auto LinearMemory::cstring(std::size_t offset) const -> std::string_view {
    if (offset >= bytes_.size())
        return {};
    const auto begin = bytes_.begin() + static_cast<std::ptrdiff_t>(offset);
    const auto end   = std::find(begin, bytes_.end(), static_cast<std::uint8_t>(0));
    return std::string_view(reinterpret_cast<const char *>(&*begin),
                            static_cast<std::size_t>(end - begin));
}

auto LinearMemory::write(std::size_t offset, std::span<const std::uint8_t> data) -> bool {
    if (offset > bytes_.size() || data.size() > bytes_.size() - offset)
        return false;
    if (!data.empty())
        std::memcpy(bytes_.data() + offset, data.data(), data.size());
    return true;
}

auto LinearMemory::copy(std::size_t dstOffset, std::size_t srcOffset, std::size_t count) -> bool {
    if (dstOffset > bytes_.size() || srcOffset > bytes_.size() ||
        count > bytes_.size() - dstOffset || count > bytes_.size() - srcOffset)
        return false;
    if (count > 0)
        std::memmove(bytes_.data() + dstOffset, bytes_.data() + srcOffset, count);
    return true;
}

auto LinearMemory::allocBytes(std::size_t count, std::size_t alignment) -> std::size_t {
    if (count == 0)
        count = 1;
    if (alignment == 0)
        alignment = 1;

    const std::size_t aligned = alignUp(bump_, alignment);
    if (count > std::numeric_limits<std::size_t>::max() - aligned)
        return std::numeric_limits<std::size_t>::max();
    const std::size_t end = aligned + count;
    if (end > bytes_.size())
        bytes_.resize(end);
    if (end > bytes_.size())
        return std::numeric_limits<std::size_t>::max();

    bump_ = end;
    return aligned;
}

auto LinearMemory::mallocBytes(std::size_t count, std::size_t alignment) -> std::size_t {
    if (count == 0)
        count = 1;
    if (alignment == 0)
        alignment = 1;

    const std::size_t aligned = alignUp(heap_, alignment);
    if (count > std::numeric_limits<std::size_t>::max() - aligned)
        return std::numeric_limits<std::size_t>::max();
    const std::size_t end = aligned + count;
    if (end > bytes_.size())
        bytes_.resize(end);
    if (end > bytes_.size())
        return std::numeric_limits<std::size_t>::max();

    heap_ = end;
    return aligned;
}

} // namespace zith::vm

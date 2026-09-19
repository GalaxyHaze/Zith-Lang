#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace zith::vm {

/// Shared linear address space used by the v2 VM. All IR pointers are guest
/// offsets into this buffer; the host never exposes native addresses to IR.
class LinearMemory {
public:
    explicit LinearMemory(std::size_t initialCapacity = 65536);

    [[nodiscard]] auto size() const noexcept -> std::size_t {
        return bytes_.size();
    }

    [[nodiscard]] auto capacity() const noexcept -> std::size_t {
        return bytes_.capacity();
    }

    [[nodiscard]] auto read(std::size_t offset, std::size_t count) const -> std::vector<uint8_t>;
    [[nodiscard]] auto cstring(std::size_t offset) const -> std::string_view;

    auto write(std::size_t offset, std::span<const std::uint8_t> data) -> bool;
    auto copy(std::size_t dstOffset, std::size_t srcOffset, std::size_t count) -> bool;

    [[nodiscard]] auto allocBytes(std::size_t count, std::size_t alignment) -> std::size_t;
    [[nodiscard]] auto mallocBytes(std::size_t count, std::size_t alignment) -> std::size_t;
    auto freeBytes(std::size_t offset) -> bool;
    auto writeString(std::size_t offset, std::string_view text) -> bool;

private:
    std::vector<uint8_t> bytes_;
    std::size_t bump_ = 0;
    std::size_t heap_ = 0;
    std::vector<std::pair<std::size_t, std::size_t>> freeBlocks_;
};

} // namespace zith::vm

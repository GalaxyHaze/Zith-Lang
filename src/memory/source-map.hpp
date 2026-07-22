#pragma once
#include "memory/arena.hpp"
#include "common/flat-map.hpp"
#include "common/optional.hpp"
#include "common/result.hpp"
#include "memory/source-file.hpp"
#include "common/span.hpp"

#include <functional>
#include <string>

namespace zith::memory {

struct SourceLoc;

class SourceMap {
    memory::Arena file_arena;
    memory::DynArray<SourceLoc> files;
    FlatMap<std::string, FileId> cache;

public:
    SourceMap();

    Result<FileId> addFile(std::string_view path, std::string_view content);

    bool isValid(FileId id) const noexcept;

    auto snippet(const Span &a) noexcept -> Result<std::string_view>;

    auto loadFile(std::string_view path, bool write = false) -> Result<FileId>;

    auto get(FileId id) noexcept -> Optional<std::reference_wrapper<SourceLoc>>;

    auto loc(const Span &a) const noexcept -> Loc;
};

} // namespace zith::memory

#pragma once
#include "memory/source-file.hpp"
#include "memory/arena.hpp"
#include "memory/optional.hpp"
#include "memory/result.hpp"
#include "span.hpp"

#include <functional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace zith::memory {

    struct SourceLoc;

    class SourceMap {
        memory::Arena file_arena;
        memory::DynArray<SourceLoc> files;
        std::unordered_map<std::string, FileId> cache;
        std::shared_mutex rw_mutex;

    public:
        SourceMap();

        Result<FileId> addFile(std::string_view path, std::string_view content);

        bool isValid(FileId id) noexcept;

        auto snippet(const Span &a) noexcept -> Result<std::string_view>;

        auto loadFile(std::string_view path, bool write = false) -> Result<FileId>;

        auto get(FileId id) noexcept -> Optional<std::reference_wrapper<SourceLoc>>;

        auto loc(const Span &a) noexcept -> Loc;
    };

} // namespace zith::memory

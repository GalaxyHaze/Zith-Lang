#include "source-map.hpp"

#include "memory/result.hpp"
#include "memory/source-file.hpp"
#include "memory/span.hpp"

#ifndef ZITH_IS_WASM
#include <system_error>
#endif

namespace zith::memory {

SourceMap::SourceMap() : files(file_arena) {}

static bool is_valid_utf8(std::string_view s) noexcept {
    for (size_t i = 0; i < s.size(); i++) {
        auto c = static_cast<unsigned char>(s[i]);
        if (c <= 0x7F)
            continue;
        size_t follow;
        if ((c & 0xE0) == 0xC0)
            follow = 1;
        else if ((c & 0xF0) == 0xE0)
            follow = 2;
        else if ((c & 0xF8) == 0xF0)
            follow = 3;
        else
            return false;
        if (i + follow >= s.size())
            return false;
        for (size_t j = 1; j <= follow; j++)
            if ((static_cast<unsigned char>(s[i + j]) & 0xC0) != 0x80)
                return false;
        i += follow;
    }
    return true;
}

Result<FileId> SourceMap::addFile(const std::string_view path, const std::string_view content) {
    if (!is_valid_utf8(content))
        return Error{"File is not valid UTF-8"};

    auto *existing = cache.get(std::string(path));
    if (existing) {
        FileId id = *existing;
        files[id] = SourceLoc{std::string(content), std::string(path), file_arena};
        files[id].buildLines();
        return id;
    }

    FileId id = static_cast<FileId>(files.size());
    cache.insert(std::string(path), id);
    auto &loc = files.emplace(std::string(content), std::string(path), file_arena);
    loc.buildLines();
    return id;
}

bool SourceMap::isValid(FileId id) const noexcept {
    return id < files.size();
}

auto SourceMap::loadFile(const std::string_view path, const bool write) -> Result<FileId> {
    (void)path;
    (void)write;
#ifdef ZITH_IS_WASM
    return Error{"loadFile not available in WASM; use addFile instead"};
#else

    auto *existing = cache.get(std::string(path));
    if (existing)
        return *existing;

    std::error_code error;

    if (write) {
        auto file = mio::make_mmap_sink(std::string(path), error);
        if (error)
            return Error{error.message()};

        auto view = std::string_view{file.data(), file.size()};
        if (!is_valid_utf8(view))
            return Error{"File is not valid UTF-8"};

        SourceLoc loc{std::move(file), std::string(path), file_arena};
        loc.buildLines();

        FileId id = static_cast<FileId>(files.size());
        cache.insert(std::string(path), id);
        files.emplace(std::move(loc));
        return id;
    } else {
        auto file = mio::make_mmap_source(std::string(path), error);
        if (error)
            return Error{error.message()};

        auto view = std::string_view{file.data(), file.size()};
        if (!is_valid_utf8(view))
            return Error{"File is not valid UTF-8"};

        SourceLoc loc{std::move(file), std::string(path), file_arena};
        loc.buildLines();

        FileId id = static_cast<FileId>(files.size());
        cache.insert(std::string(path), id);
        files.emplace(std::move(loc));
        return id;
    }
#endif
}

auto SourceMap::get(FileId id) noexcept -> Optional<std::reference_wrapper<SourceLoc>> {
    if (id < files.size()) {
        return std::ref(files[id]);
    }
    return nullptr;
}

auto SourceMap::snippet(const Span &a) noexcept -> Result<std::string_view> {
    if (a.file >= files.size()) {
        return Error{"Invalid File ID in Span"};
    }
    return files[a.file].snippet(a);
}

auto SourceMap::loc(const Span &a) const noexcept -> Loc {
    if (a.file >= files.size()) {
        return Loc{0, 0};
    }
    return files[a.file].loc(a.start);
}

} // namespace zith::memory

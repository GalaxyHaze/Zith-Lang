#include "source-map.hpp"

#include "memory/source-file.hpp"
#include "memory/span.hpp"
#include "memory/result.hpp"

#include <mutex>
#include <shared_mutex>

namespace zith::memory {

    static bool is_valid_utf8(std::string_view s) noexcept {
        for (size_t i = 0; i < s.size(); i++) {
            auto c = static_cast<unsigned char>(s[i]);
            if (c <= 0x7F)
                continue;
            size_t follow;
            if ((c & 0xE0) == 0xC0)
                follow = 1; // 2-byte
            else if ((c & 0xF0) == 0xE0)
                follow = 2; // 3-byte
            else if ((c & 0xF8) == 0xF0)
                follow = 3; // 4-byte
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

    Result<FileId> SourceMap::add_file(const std::string_view path,
                                                          const std::string_view content) {
        if (!is_valid_utf8(content))
            return Error{"File is not valid UTF-8"};

        {
            std::shared_lock lock(rw_mutex);
            auto it = cache.find(std::string(path));
            if (it != cache.end()) {
                FileId id = it->second;
                lock.unlock();
                std::unique_lock ulock(rw_mutex);
                files[id] = SourceLoc(std::string(content), std::string(path));
                files[id].buildLines();
                return id;
            }
        }

        std::unique_lock lock(rw_mutex);

        auto it = cache.find(std::string(path));
        if (it != cache.end()) {
            FileId id = it->second;
            files[id] = SourceLoc(std::string(content), std::string(path));
            files[id].buildLines();
            return id;
        }

        FileId id = static_cast<FileId>(files.size());
        cache.emplace(path, id);
        auto &loc = files.emplace(std::string(content), std::string(path));
        loc.buildLines();
        return id;
    }

    bool SourceMap::isValid(FileId id) noexcept {
        std::shared_lock lock(rw_mutex);
        return id < files.size();
    }

    [[nodiscard]] auto SourceMap::load_file(const std::string_view path, const bool write)
            -> Result<FileId> {
        {
            std::shared_lock lock(rw_mutex);
            auto it = cache.find(std::string(path));
            if (it != cache.end())
                return it->second;
        }

        std::unique_lock lock(rw_mutex);

        auto it = cache.find(std::string(path));
        if (it != cache.end())
            return it->second;

        std::error_code error;

        if (write) {
            auto file = mio::make_mmap_sink(std::string(path), error);
            if (error)
                return Error{error.message()};

            auto view = std::string_view{file.data(), file.size()};
            if (!is_valid_utf8(view))
                return Error{"File is not valid UTF-8"};

            SourceLoc loc{std::move(file), std::string(path), {}};
            loc.buildLines();

            FileId id = static_cast<FileId>(files.size());
            cache.emplace(path, id);
            files.emplace(std::move(loc));
            return id;
        } else {
            auto file = mio::make_mmap_source(std::string(path), error);
            if (error)
                return Error{error.message()};

            auto view = std::string_view{file.data(), file.size()};
            if (!is_valid_utf8(view))
                return Error{"File is not valid UTF-8"};

            SourceLoc loc{std::move(file), std::string(path), {}};
            loc.buildLines();

            FileId id = static_cast<FileId>(files.size());
            cache.emplace(path, id);
            files.emplace(std::move(loc));
            return id;
        }
    }

    auto SourceMap::get(FileId id) noexcept
            -> Optional<std::reference_wrapper<SourceLoc>> {
        std::shared_lock lock(rw_mutex);
        if (id < files.size()) {
            return std::ref(files[id]);
        }
        return nullptr;
    }

    auto SourceMap::snippet(const Span &a) noexcept -> Result<std::string_view> {
        std::shared_lock lock(rw_mutex);
        if (a.file >= files.size()) {
            return Error{"Invalid File ID in Span"};
        }
        return files[a.file].snippet(a);
    }

    auto SourceMap::loc(const Span &a) noexcept -> Loc {
        std::shared_lock lock(rw_mutex);
        if (a.file >= files.size()) {
            return Loc{0, 0};
        }
        return files[a.file].loc(a.start);
    }

} // namespace zith::memory

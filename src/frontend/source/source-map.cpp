#include "source-map.hpp"

#include "frontend/source/source-file.hpp"
#include "frontend/source/span.hpp"
#include "infra/util/result.hpp"

#include <mutex>
#include <shared_mutex>

namespace zith::frontend {

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

    zith::infra::util::Result<FileId> SourceMap::add_file(const std::string_view path,
                                                          const std::string_view content) {
        if (!is_valid_utf8(content))
            return zith::infra::util::Error{"File is not valid UTF-8"};

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

        FileId id = static_cast<FileId>(files.size());
        cache.emplace(path, id);
        files.emplace_back(std::string(content), std::string(path));
        files.back().buildLines();
        return id;
    }

    bool SourceMap::isValid(FileId id) noexcept {
        std::shared_lock lock(rw_mutex);
        return id < files.size();
    }

    auto SourceMap::load_file(const std::string_view path, const bool write)
            -> zith::infra::util::Result<FileId> {
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
                return zith::infra::util::Error{error.message()};

            auto view = std::string_view{file.data(), file.size()};
            if (!is_valid_utf8(view))
                return zith::infra::util::Error{"File is not valid UTF-8"};

            SourceLoc loc{std::move(file), std::string(path), {}};
            loc.buildLines();

            FileId id = static_cast<FileId>(files.size());
            cache.emplace(path, id);
            files.emplace_back(std::move(loc));
            return id;
        } else {
            auto file = mio::make_mmap_source(std::string(path), error);
            if (error)
                return zith::infra::util::Error{error.message()};

            auto view = std::string_view{file.data(), file.size()};
            if (!is_valid_utf8(view))
                return zith::infra::util::Error{"File is not valid UTF-8"};

            SourceLoc loc{std::move(file), std::string(path), {}};
            loc.buildLines();

            FileId id = static_cast<FileId>(files.size());
            cache.emplace(path, id);
            files.emplace_back(std::move(loc));
            return id;
        }
    }

    auto SourceMap::get(FileId id) noexcept
            -> zith::infra::util::Optional<std::reference_wrapper<SourceLoc>> {
        std::shared_lock lock(rw_mutex);
        if (id < files.size()) {
            return std::ref(files[id]);
        }
        return nullptr;
    }

    auto SourceMap::snippet(const Span &a) noexcept -> zith::infra::util::Result<std::string_view> {
        std::shared_lock lock(rw_mutex);
        if (a.file >= files.size()) {
            return zith::infra::util::Error{"Invalid File ID in Span"};
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

} // namespace zith::frontend

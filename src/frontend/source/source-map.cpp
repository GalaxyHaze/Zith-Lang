#include "source-map.hpp"
#include "frontend/source/source-file.hpp"
#include <mutex>
#include <shared_mutex>

namespace zith::frontend {

        auto SourceMap::add_file(const std::string_view path, const std::string_view content) -> FileId {
            {
                std::shared_lock lock(rw_mutex);
                auto it = cache.find(std::string(path));
                if (it != cache.end()) return it->second;
            }

            std::unique_lock lock(rw_mutex);

            auto it = cache.find(std::string(path));
            if (it != cache.end()) return it->second;

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

        auto SourceMap::load_file(const std::string_view path, const bool write) -> zith::infra::util::Result<FileId> {
            {
                std::shared_lock lock(rw_mutex);
                auto it = cache.find(std::string(path));
                if (it != cache.end()) return it->second;
            }

            std::unique_lock lock(rw_mutex);

            auto it = cache.find(std::string(path));
            if (it != cache.end()) return it->second;

            std::error_code error;

            if (write) {
                auto file = mio::make_mmap_sink(path, error);
                if (error) return zith::infra::util::Error{error.message()};
                SourceLoc loc{std::move(file), std::string(path), {}};
                loc.buildLines();
                FileId id = static_cast<FileId>(files.size());
                cache.emplace(path, id);
                files.emplace_back(std::move(loc));
                return id;
            } else {
                auto file = mio::make_mmap_source(path, error);
                if (error) return zith::infra::util::Error{error.message()};
                SourceLoc loc{std::move(file), std::string(path), {}};
                loc.buildLines();
                FileId id = static_cast<FileId>(files.size());
                cache.emplace(path, id);
                files.emplace_back(std::move(loc));
                return id;
            }
        }

        auto SourceMap::get(FileId id) noexcept -> zith::infra::util::Optional<std::reference_wrapper<SourceLoc>> {
            std::shared_lock lock(rw_mutex);
            if (id < files.size()) {
                return std::ref(files[id]);
            }
            return nullptr;
        }

        auto SourceMap::snippet(const Span& a) noexcept -> zith::infra::util::Result<std::string_view> {
            std::shared_lock lock(rw_mutex);
            if (a.file >= files.size()) {
                return zith::infra::util::Error{"Invalid File ID in Span"};
            }
            return files[a.file].snippet(a);
        }

        auto SourceMap::loc(const Span& a) noexcept -> Loc {
            std::shared_lock lock(rw_mutex);
            if (a.file >= files.size()) {
                return Loc{0, 0};
            }
            return files[a.file].loc(a.start);
        }

} // namespace zith::frontend

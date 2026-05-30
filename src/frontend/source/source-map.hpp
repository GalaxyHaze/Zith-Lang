#pragma once
// #include "source-file.hpp"
#include "frontend/source/source-file.hpp"
#include "infra/util/optional.hpp"
#include "infra/util/result.hpp"
#include "span.hpp"

#include <functional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace zith::frontend {

    struct SourceLoc;

    class SourceMap {
        // inline static Arena* pimp = nullptr;
        inline static std::vector<SourceLoc> files                  = {};
        inline static std::unordered_map<std::string, FileId> cache = {};

        // Mutex estático para proteger os dados estáticos acima
        inline static std::shared_mutex rw_mutex = {};

    public:
        SourceMap()                  = delete;
        SourceMap(SourceMap &&)      = delete;
        SourceMap(const SourceMap &) = delete;

        static zith::infra::util::Result<FileId> add_file(std::string_view path,
                                                          std::string_view content);

        // Não precisa de const noexcept em funções estáticas
        static bool isValid(FileId id) noexcept;

        static auto snippet(const Span &a) noexcept -> zith::infra::util::Result<std::string_view>;

        static auto load_file(std::string_view path, bool write = false)
                -> zith::infra::util::Result<FileId>;

        static auto get(FileId id) noexcept
                -> zith::infra::util::Optional<std::reference_wrapper<SourceLoc>>;

        static auto loc(const Span &a) noexcept -> Loc;
    };

} // namespace zith::frontend

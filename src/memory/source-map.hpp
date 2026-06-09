#pragma once
// #include "source-file.hpp"
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
        // inline static Arena* pimp = nullptr;
        inline static memory::Arena file_arena;
        inline static memory::DynArray<SourceLoc> files{file_arena};
        inline static std::unordered_map<std::string, FileId> cache = {};

        // Mutex estático para proteger os dados estáticos acima
        inline static std::shared_mutex rw_mutex = {};

    public:
        SourceMap()                  = delete;
        SourceMap(SourceMap &&)      = delete;
        SourceMap(const SourceMap &) = delete;

        static Result<FileId> add_file(std::string_view path,
                                                          std::string_view content);

        // Não precisa de const noexcept em funções estáticas
        static bool isValid(FileId id) noexcept;

        static auto snippet(const Span &a) noexcept -> Result<std::string_view>;

        static auto load_file(std::string_view path, bool write = false)
                -> Result<FileId>;

        static auto get(FileId id) noexcept
                -> Optional<std::reference_wrapper<SourceLoc>>;

        static auto loc(const Span &a) noexcept -> Loc;
    };

} // namespace zith::memory

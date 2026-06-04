#pragma once

#ifndef __cplusplus
#error "This header requires C++"
#endif

#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include "zith/ast.h"
#include "zith/diagnostics.h"
#include "zith/import.h"
#include "zith/memory.h"
#include "zith/parser.h"
#include "zith/token.h"

namespace zith {
    class Arena {
        struct Deleter {
            void operator()(ZithArena *a) const { zith_arena_destroy(a); }
        };

        std::unique_ptr<ZithArena, Deleter> handle_;

    public:
        explicit Arena(size_t initial = 65536)
            : handle_(zith_arena_create(initial)) { if (!handle_) throw std::bad_alloc(); }

        [[nodiscard]] void *alloc(size_t size) const { return zith_arena_alloc(handle_.get(), size); }
        char *strdup(const char *s) const { return zith_arena_strdup(handle_.get(), s); }

        [[nodiscard]] char *strdup(std::string_view sv) const {
            char *p = static_cast<char *>(alloc(sv.size() + 1));
            if (p) {
                memcpy(p, sv.data(), sv.size());
                p[sv.size()] = '\0';
            }
            return p;
        }

        [[nodiscard]] ZithArena *get() const { return handle_.get(); }
    };

    inline ZithTokenStream tokenize(const Arena &arena, std::string_view source) {
        return zith_tokenize(arena.get(), source.data(), source.size());
    }

    inline std::pair<char *, size_t> load_file(const Arena &arena, const char *path) {
        size_t size = 0;
        char *data = zith_load_file_to_arena(arena.get(), path, &size);
        if (!data) throw std::runtime_error(std::string("Failed to load file: ") + std::string(path));
        return {data, size};
    }

    namespace debug {
        inline const char *token_type_name(ZithTokenType t) { return zith_token_type_name(t); }
        void print_tokens(ZithTokenStream tokens);
        void print_ast(const ZithNode *node, int indent = 0);
    }
}

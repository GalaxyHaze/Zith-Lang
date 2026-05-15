#include "pipeline.hpp"

#include <iostream>

namespace zith::cli::pipeline {

    void print_error(const std::string &msg) {
        std::cerr << "[error] " << msg << "\n";
    }
    void print_info(const std::string &msg) {
        std::cout << "[*] " << msg << "\n";

    }
    void print_success(const std::string &msg, const std::string &success) {
        std::cout << "[ok] " << msg << ": " << success << "\n";
    }
    void print_not_implemented(const std::string &msg) {
        std::cerr << "\n[!] Command '" << msg << "' is not implemented yet.\n"
                  << "    This feature will be available in a future version of Zith.\n"
                  << "    Track progress at: https://github.com/GalaxyHaze/Zith\n\n";
    }

    ZithArena *tokenize_file(const std::string &src_path, ZithTokenStream &out_stream,
                             const char **out_source, size_t *out_source_len, bool verbose) {
        ZithArena *arena = zith_arena_create(64 * 1024);
        if (!arena) {
            print_error("Failed to create memory arena");
            return nullptr;
        }

        size_t file_size   = 0;
        const char *source = zith_load_file_to_arena(arena, src_path.c_str(), &file_size);
        if (!source) {
            print_error("Failed to load file: " + src_path);
            zith_arena_destroy(arena);
            return nullptr;
        }

        out_stream = zith_tokenize(arena, source, file_size);
        if (!out_stream.data) {
            zith_arena_destroy(arena);
            return nullptr;
        }

        if (verbose)
            print_info("Tokenized " + std::to_string(out_stream.len) + " tokens from " + src_path);

        if (out_source)
            *out_source = source;
        if (out_source_len)
            *out_source_len = file_size;
        return arena;
    }

} // namespace zith::cli::pipeline

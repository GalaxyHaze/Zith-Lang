#pragma once

#include <string>
#include <zith/zith.hpp>

namespace zith::cli::pipeline {

    void print_error(const std::string &msg);
    void print_info(const std::string &msg);
    void print_success(const std::string &msg, const std::string& success);
    void print_not_implemented(const std::string &msg);

    ZithArena *tokenize_file(const std::string &src_path,
                             ZithTokenStream &out_stream,
                             const char **out_source,
                             size_t *out_source_len,
                             bool verbose);

} // namespace zith::cli::pipeline

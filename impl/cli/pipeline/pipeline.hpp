#pragma once

#include <string>
#include <zith/zith.hpp>

namespace zith::cli::pipeline {

ZithArena *tokenize_file(const std::string &src_path,
                         ZithTokenStream &out_stream,
                         const char **out_source,
                         size_t *out_source_len,
                         bool verbose);

} // namespace zith::cli::pipeline

#include "../pipeline/pipeline.hpp"
#include "commands.hpp"

namespace zith::cli::commands {

int cmd_test(const std::string & /*input_file*/, bool /*verbose*/) {
    zith::cli::pipeline::print_not_implemented("test");
    return 1;
}

int cmd_fmt(const std::string & /*input_file*/, bool /*check_only*/, bool /*verbose*/) {
    zith::cli::pipeline::print_not_implemented("fmt");
    return 1;
}

int cmd_docs(const std::string & /*input_file*/, const std::string & /*output_dir*/,
             bool /*verbose*/) {
    zith::cli::pipeline::print_not_implemented("docs");
    return 1;
}

} // namespace zith::cli::commands

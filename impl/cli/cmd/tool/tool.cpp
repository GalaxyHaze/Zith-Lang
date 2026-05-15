#include <cli/pipeline/pipeline.hpp>
#include <cli/cmd/commands.hpp>

namespace zith::cli::commands {

int cmd_test(const std::string & /*input_file*/, bool /*verbose*/) {
    pipeline::print_not_implemented("test");
    return 1;
}

int cmd_fmt(const std::string & /*input_file*/, bool /*check_only*/, bool /*verbose*/) {
    pipeline::print_not_implemented("fmt");
    return 1;
}

int cmd_docs(const std::string & /*input_file*/, const std::string & /*output_dir*/,
             bool /*verbose*/) {
    pipeline::print_not_implemented("docs");
    return 1;
}

} // namespace zith::cli::commands

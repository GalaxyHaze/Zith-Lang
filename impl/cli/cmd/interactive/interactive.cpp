#include <cli/pipeline/pipeline.hpp>
#include <cli/cmd/commands.hpp>

namespace zith::cli::commands {

int cmd_repl(bool /*verbose*/) {
    pipeline::print_not_implemented("repl");
    return 1;
}

} // namespace zith::cli::commands

#include "../../pipeline/pipeline.hpp"
#include "../commands.hpp"

namespace zith::cli::commands {

int cmd_repl(bool /*verbose*/) {
    zith::cli::pipeline::print_not_implemented("repl");
    return 1;
}

} // namespace zith::cli::commands

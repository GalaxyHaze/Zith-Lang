#pragma once

#include <string>
#include <vector>

namespace zith::cli::commands {

// build/ — compilation pipeline
int cmd_check(const std::string &input_file, const std::string &mode_str, bool verbose,
              const std::vector<std::string> &include_dirs);
int cmd_compile(const std::string &input_file, const std::string &output_file,
                const std::string &mode_str, bool interpreted, bool verbose,
                const std::vector<std::string> &include_dirs);
int cmd_build(const std::string &input_file, const std::string &output_file,
              const std::string &mode_str, bool verbose,
              const std::vector<std::string> &include_dirs);

// run/ — execution
int cmd_execute(const std::string &target, bool interpreted, bool verbose,
                const std::vector<std::string> &include_dirs);
int cmd_run(const std::string &input_file, const std::string &output_file,
            const std::string &mode_str, bool interpreted, bool verbose,
            const std::vector<std::string> &include_dirs);

// project/ — project management
int cmd_new(const std::string &project_name, bool verbose);
int cmd_clean(bool verbose);

// tool/ — development tools
int cmd_test(const std::string &input_file, bool verbose);
int cmd_fmt(const std::string &input_file, bool check_only, bool verbose);
int cmd_docs(const std::string &input_file, const std::string &output_dir, bool verbose);

// interactive/ — REPL
int cmd_repl(bool verbose);

// info/ — information
int cmd_version();
int cmd_help();

} // namespace zith::cli::commands

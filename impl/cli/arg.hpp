// impl/cli/arg.hpp — Minimal command-line argument parser
// No external dependencies, C++17, ~100 lines
#pragma once

#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace zith::arg {

struct Option {
    std::string name;
    std::string short_name;
    std::string description;
    std::string *value = nullptr;
    bool *flag = nullptr;
    bool required = false;
    bool repeatable = false;
    std::vector<std::string> *list = nullptr;
};

struct Subcommand {
    std::string name;
    std::string description;
    std::vector<Option> options;
    std::vector<std::string> positional;
    std::function<int()> handler;
};

class Parser {
    std::string prog_desc_;
    std::vector<Option> global_opts_;
    std::vector<Subcommand> subs_;
    std::string help_text_;
    bool parsed_ = false;
    int argc_;
    const char *const *argv_;

public:
    Parser(std::string desc, int argc, const char *const *argv)
        : prog_desc_(std::move(desc)), argc_(argc), argv_(argv) {}

    void add_option(std::string name, std::string short_name, std::string desc,
                    std::string *value, bool required = false) {
        global_opts_.push_back({std::move(name), std::move(short_name),
                                std::move(desc), value, nullptr, required});
    }

    void add_flag(std::string name, std::string short_name, std::string desc,
                  bool *flag) {
        global_opts_.push_back({std::move(name), std::move(short_name),
                                std::move(desc), nullptr, flag});
    }

    void add_repeatable(std::string name, std::string short_name, std::string desc,
                        std::vector<std::string> *list) {
        global_opts_.push_back({std::move(name), std::move(short_name),
                                std::move(desc), nullptr, nullptr, false, true, list});
    }

    void add_subcommand(std::string name, std::string desc,
                        std::function<int()> handler) {
        subs_.push_back({std::move(name), std::move(desc), {}, {}, std::move(handler)});
    }

    bool parse() {
        parsed_ = true;
        bool help_requested = false;
        for (int i = 1; i < argc_; ++i) {
            std::string_view arg(argv_[i]);

            if (arg == "--help" || arg == "-h") {
                help_requested = true;
                continue;
            }

            if (arg.substr(0, 2) == "--" || arg.substr(0, 1) == "-") {
                bool found = false;
                std::string key(arg);
                std::string_view val;

                auto eq_pos = arg.find('=');
                if (eq_pos != std::string_view::npos) {
                    key = std::string(arg.substr(0, eq_pos));
                    val = arg.substr(eq_pos + 1);
                }

                for (auto &opt : global_opts_) {
                    if (key == opt.name || key == opt.short_name) {
                        found = true;
                        if (opt.flag) {
                            *opt.flag = true;
                        } else if (opt.list) {
                            if (!val.empty()) {
                                opt.list->push_back(std::string(val));
                            } else if (i + 1 < argc_) {
                                opt.list->push_back(argv_[++i]);
                            }
                        } else if (opt.value) {
                            if (!val.empty()) {
                                *opt.value = std::string(val);
                            } else if (i + 1 < argc_) {
                                *opt.value = argv_[++i];
                            } else {
                                return false;
                            }
                        }
                        break;
                    }
                }
                if (!found) return false;
            } else if (!subs_.empty()) {
                for (auto &sub : subs_) {
                    if (sub.name == arg) {
                        return true;
                    }
                }
                return false;
            }
        }

        if (help_requested) {
            build_help();
            return false;
        }
        return true;
    }

    bool is_subcommand(const std::string &name) const {
        for (int i = 1; i < argc_; ++i) {
            if (argv_[i] == name) return true;
        }
        return false;
    }

    const std::string &help() {
        if (help_text_.empty()) build_help();
        return help_text_;
    }

private:
    void build_help() {
        help_text_ = prog_desc_ + "\n\nUsage:\n  zith [options] [subcommand]\n\nOptions:\n";
        for (auto &opt : global_opts_) {
            help_text_ += "  " + opt.short_name + "," + opt.name;
            if (opt.value) help_text_ += " <val>";
            help_text_ += "  " + opt.description + "\n";
        }
        if (!subs_.empty()) {
            help_text_ += "\nSubcommands:\n";
            for (auto &sub : subs_) {
                help_text_ += "  " + sub.name + "  " + sub.description + "\n";
            }
        }
    }
};

} // namespace zith::arg

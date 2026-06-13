#pragma once
#include "cli/options.hpp"
#include <cstdlib>
#include <filesystem>
#include <string>
#include <toml++/toml.hpp>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace zith::cli {

namespace {

std::string getExecutableDir() {
#ifdef _WIN32
    char buf[4096];
    DWORD len = GetModuleFileNameA(NULL, buf, sizeof(buf));
    if (len == 0 || len == sizeof(buf))
        return {};
    return std::filesystem::path(buf).parent_path().string();
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0)
        return {};
    return std::filesystem::path(buf).parent_path().string();
#else
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len == -1)
        return {};
    buf[len] = '\0';
    return std::filesystem::path(buf).parent_path().string();
#endif
}

bool find_flags_file(std::filesystem::path &out) {
    std::string exe_dir = getExecutableDir();
    if (exe_dir.empty())
        return false;

    // Development: <exe_dir>/../ZithFlags.toml  (exe in build/, toml at root)
    std::filesystem::path candidate =
        std::filesystem::path(exe_dir).parent_path() / "ZithFlags.toml";
    if (std::filesystem::exists(candidate)) {
        out = candidate;
        return true;
    }

    // Installed alongside exe: <exe_dir>/ZithFlags.toml
    candidate = std::filesystem::path(exe_dir) / "ZithFlags.toml";
    if (std::filesystem::exists(candidate)) {
        out = candidate;
        return true;
    }

    // Installed in standard location: <exe_dir>/../share/zith/ZithFlags.toml
    candidate = std::filesystem::path(exe_dir).parent_path() / "share" / "zith" / "ZithFlags.toml";
    if (std::filesystem::exists(candidate)) {
        out = candidate;
        return true;
    }

    // TODO: add ZITH_PATH / ZITH_HOME env var support
    // TODO: add compile-time fallback path via -DZITH_FLAGS_DIR

    return false;
}

} // namespace

inline Options loadZithFlags() {
    std::filesystem::path flags_path;
    if (!find_flags_file(flags_path))
        return {};

    auto process = [](const toml::table &tbl) -> Options {
        Options opts;

        if (auto v = tbl["mode"].value<std::string>())
            opts.mode = *v;
        if (auto v = tbl["opt_level"].value<int64_t>()) {
            if (*v >= 0 && *v <= 3)
                opts.opt_level = static_cast<int>(*v);
        }
        if (auto v = tbl["debug_level"].value<int64_t>()) {
            if (*v >= 0 && *v <= 3)
                opts.debug_level = static_cast<int>(*v);
        }
        if (auto v = tbl["verbose"].value<bool>())
            opts.verbose = *v;
        if (auto v = tbl["strict"].value<bool>())
            opts.strict = *v;
        if (auto v = tbl["strip_debug"].value<bool>())
            opts.strip_debug = *v;
        if (auto v = tbl["lto"].value<bool>())
            opts.lto = *v;
        if (auto v = tbl["color"].value<std::string>())
            opts.color = *v;

        if (auto arr = tbl["include_dirs"].as_array()) {
            for (auto &elem : *arr) {
                if (auto s = elem.value<std::string>())
                    opts.include_dirs.push_back(*s);
            }
        }

        if (auto v = tbl["emit_target"].value<std::string>())
            opts.emit_target = *v;

        return opts;
    };

#if TOML_EXCEPTIONS
    try {
        return process(toml::parse_file(flags_path.string()));
    } catch (const toml::parse_error &) {
        return {};
    }
#else
    auto result = toml::parse_file(flags_path.string());
    if (!result)
        return {};
    return process(result);
#endif
}

} // namespace zith::cli

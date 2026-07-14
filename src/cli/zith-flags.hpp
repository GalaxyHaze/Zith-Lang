#pragma once

#ifndef ZITH_IS_WASM
#include <filesystem>
#endif

#ifdef _WIN32
#include <Windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif !defined(ZITH_IS_WASM)
#include <unistd.h>
#endif

namespace zith {

#ifndef ZITH_IS_WASM
namespace {

std::string getExecutableDir() {
#ifdef _WIN32
    char buf[4096];
    DWORD len = GetModuleFileNameA(nullptr, buf, sizeof(buf));
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

    return false;
}

} // namespace
#endif

} // namespace zith

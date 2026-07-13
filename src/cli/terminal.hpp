#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace zith {

struct Options;

namespace term {

namespace ansi {
inline constexpr std::string_view reset = "\033[0m";
inline constexpr std::string_view bold  = "\033[1m";

inline constexpr std::string_view red     = "\033[31m";
inline constexpr std::string_view green   = "\033[32m";
inline constexpr std::string_view yellow  = "\033[33m";
inline constexpr std::string_view blue    = "\033[34m";
inline constexpr std::string_view magenta = "\033[35m";
inline constexpr std::string_view cyan    = "\033[36m";

inline constexpr std::string_view bright_red    = "\033[91m";
inline constexpr std::string_view bright_green  = "\033[92m";
inline constexpr std::string_view bright_yellow = "\033[93m";
inline constexpr std::string_view bright_cyan   = "\033[96m";
} // namespace ansi

struct Term {
    bool cerrOn = false;
    bool coutOn = false;
};

Term init(const Options &opts);
Term init();                        // auto-detect (for commands without Options)
bool useColor(const Options &opts); // quick check: should stderr use color?

inline const char *err(const Term &t, const char *code) {
    return t.cerrOn ? code : "";
}
inline const char *err_rst(const Term &t) {
    return t.cerrOn ? "\033[0m" : "";
}

inline const char *out(const Term &t, const char *code) {
    return t.coutOn ? code : "";
}
inline const char *out_rst(const Term &t) {
    return t.coutOn ? "\033[0m" : "";
}

inline void enableVirtual() {
#ifdef _WIN32
    HANDLE handles[] = {GetStdHandle(STD_OUTPUT_HANDLE), GetStdHandle(STD_ERROR_HANDLE)};
    for (auto h : handles) {
        if (h == INVALID_HANDLE_VALUE || h == NULL)
            continue;
        DWORD mode = 0;
        if (GetConsoleMode(h, &mode)) {
            mode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
            SetConsoleMode(h, mode);
        }
    }
#endif
}

struct UsagePrinter {
    FILE *out    = nullptr;
    bool colorOn = false;

    UsagePrinter(FILE *out_, bool colorOn_) : out(out_), colorOn(colorOn_) {}

    UsagePrinter() = default;

    void print(const char *ansi, const char *fmt, ...) const {
        if (colorOn)
            fputs(ansi, out);
        va_list args;
        va_start(args, fmt);
        vfprintf(out, fmt, args);
        va_end(args);
        if (colorOn)
            fputs("\033[0m", out);
    }

    void bold(const char *text) const {
        print("\033[1m", "%s", text);
    }
    void green(const char *text) const {
        print("\033[32m", "%s", text);
    }
    void yellow(const char *text) const {
        print("\033[33m", "%s", text);
    }
    void cyan(const char *text) const {
        print("\033[36m", "%s", text);
    }
    void red(const char *text) const {
        print("\033[31m", "%s", text);
    }

    void section(const char *title) const {
        bold(title);
        fputc('\n', out);
    }

    void flag(const char *flags, const char *desc) const {
        fputs("    ", out);
        print("\033[36m", "%-46s", flags);
        fprintf(out, " %s\n", desc);
    }
};

[[noreturn]] inline void panic(const char *file, int line, const char *fn, const char *msg) {
    std::fprintf(stderr, "\npanic: %s\n  at %s:%d (%s)\n", msg, file, line, fn);
    std::abort();
}

} // namespace term
} // namespace zith

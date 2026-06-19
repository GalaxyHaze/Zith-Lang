#pragma once

#include <cstring>
#include <string>

#ifdef _WIN32
#include <cstdlib>
#include <windows.h>
#endif

namespace zith::support {

#ifdef _WIN32

inline char *mkdtemp(char *tmpl) {
    char *X = std::strstr(tmpl, "XXXXXX");
    if (!X)
        return nullptr;

    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";

    for (int attempt = 0; attempt < 100; ++attempt) {
        for (int i = 0; i < 6; ++i)
            X[i] = chars[rand() % (sizeof(chars) - 1)];
        X[6] = '\0';

        if (CreateDirectoryA(tmpl, NULL))
            return tmpl;

        if (GetLastError() != ERROR_ALREADY_EXISTS)
            return nullptr;
    }
    return nullptr;
}

#else

inline char *mkdtemp(char *tmpl) {
    return ::mkdtemp(tmpl);
}

#endif

inline void enableVirtualTerminal() {
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

} // namespace zith::support

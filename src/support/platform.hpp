#pragma once

#include <cstring>

#ifdef _WIN32
#include <cstdlib>
#include <Windows.h>
#else
#include <stdlib.h>
#include <unistd.h>
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

        if (CreateDirectoryA(tmpl, nullptr))
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

} // namespace zith::support

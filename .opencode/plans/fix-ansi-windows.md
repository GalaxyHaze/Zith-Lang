# Fix: Corrupted ANSI Escape Codes on Windows

## Problem
The `TerminalEmitter` in `impl/diagnostics/emitter.hpp` uses raw ANSI escape codes (`\033[...m`) for colored diagnostic output. On Windows, these codes appear as garbled characters like `←[1m←[31m` because Virtual Terminal Processing is not enabled by default.

## Solution
Enable Virtual Terminal Processing at program startup on Windows by calling `SetConsoleMode` with `ENABLE_VIRTUAL_TERMINAL_PROCESSING` for both stdout and stderr handles.

## Changes

### File: `impl/cli/CLI.hpp`

**Add Windows header and initialization function** (after line 5, before other includes):
```cpp
#ifdef _WIN32
#include <windows.h>
#endif
```

**Add helper function** (after the `using` declarations, before `zith_run`):
```cpp
#ifdef _WIN32
static void enable_virtual_terminal_processing() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hErr, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hErr, dwMode);
        }
    }
}
#endif
```

**Call initialization at start of `zith_run`** (first line of function body):
```cpp
extern "C" inline int zith_run(const int argc, const char *const argv[]) {
#ifdef _WIN32
    enable_virtual_terminal_processing();
#endif
    CLI::App app{"Zith - A low-level general-purpose language"};
    // ... rest of function
```

## Verification
After applying:
1. Rebuild: `cmake --build scripts/cmake-build-debug`
2. Run: `scripts\cmake-build-debug\bin\zith.exe check sample.zith`
3. Error messages should display with proper colors instead of garbled `←[...m` sequences

// impl/diagnostics/diagnostics.hpp — C++ DiagManager wrapper
#pragma once

#include <zith/diagnostics.h>
#ifdef __cplusplus
struct ZithArena;

class DiagManager {
public:
    DiagManager() : diags_{nullptr, 0, 0}, had_error_(false) {}

    void error(ZithSourceLoc loc, const char *msg);
    void warning(ZithSourceLoc loc, const char *msg);
    void note(ZithSourceLoc loc, const char *msg);
    static void info(const char *msg);

    void print_all(const char *source, size_t source_len,
                   const char *filename = "<input>") const;
    void print_summary(const char *filename = "<input>") const;

    [[nodiscard]] bool had_error() const { return had_error_; }
    [[nodiscard]] const ZithDiagList &list() const { return diags_; }

    void set_arena(ZithArena *arena) { arena_ = arena; }

private:
    ZithDiagList diags_;
    ZithArena *arena_ = nullptr;
    bool had_error_;

    void emit(ZithSourceLoc loc, ZithDiagSeverity severity, const char *msg);
};

#define DIAG_ERROR(dm, loc, msg)   (dm).error(loc, msg)
#define DIAG_WARN(dm, loc, msg)    (dm).warning(loc, msg)
#define DIAG_NOTE(dm, loc, msg)    (dm).note(loc, msg)
#define DIAG_INFO(dm, msg)         (dm).info(msg)

#endif

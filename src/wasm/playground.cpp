#include "capi/zithc-capi.h"
#include <cstring>
#include <string>
#include <vector>

extern "C" {
// Import from JS: zith.host_write
__attribute__((import_module("zith"), import_name("host_write"))) void
host_write(int stream, const char *ptr, int len);
}

extern "C" __attribute__((export_name("zith_alloc"))) void *zith_alloc(int size) {
    return new char[size];
}

extern "C" __attribute__((export_name("zith_free"))) void zith_free(void *ptr, int size) {
    delete[] static_cast<char *>(ptr);
}

static std::string last_error;

extern "C" __attribute__((export_name("zith_last_error_ptr"))) const char *zith_last_error_ptr() {
    return last_error.data();
}

extern "C" __attribute__((export_name("zith_last_error_len"))) int zith_last_error_len() {
    return last_error.size();
}

extern "C" __attribute__((export_name("zith_compile_source"))) int
zith_compile_source(const char *ptr, int len, int mode, int opt_level, int emit_mask) {
    last_error.clear();

    // Validate parameters
    if (mode < 0 || mode > 5) {
        last_error = "invalid mode (must be 0-5)";
        host_write(2, last_error.data(), last_error.size());
        return 1;
    }
    if (opt_level < 0 || opt_level > 3) {
        last_error = "invalid optimization level (must be 0-3)";
        host_write(2, last_error.data(), last_error.size());
        return 1;
    }

    auto *session = zithc_session_create_from_buffer("playground.zith", ptr, len);
    if (!session) {
        last_error = "failed to create session";
        return 1;
    }

    zithc_session_set_mode(session, static_cast<uint8_t>(mode));
    zithc_session_set_opt_level(session, static_cast<uint8_t>(opt_level));
    zithc_session_set_emit_flags(session, emit_mask & 1, emit_mask & 2, emit_mask & 4,
                                 emit_mask & 8);

    // Stop at HIR lowered since we do not have an LLVM codegen backend under WASM.
    bool ok = zithc_run_to(session, ZITHC_STAGE_HIR_LOWERED);

    // Flush any output buffered in the session (e.g. AST/HIR printouts, compile messages)
    const char *buffered_out = zithc_session_flush_output(session);
    if (buffered_out && buffered_out[0] != '\0') {
        host_write(1, buffered_out, std::strlen(buffered_out));
    }

    // Accumulate diagnostic reports
    size_t count = zithc_diag_count(session);
    for (size_t i = 0; i < count; ++i) {
        zithc_diagnostic diag = zithc_diag_get(session, i);
        std::string msg;
        if (diag.severity == ZITHC_SEVERITY_ERROR)
            msg += "error";
        else if (diag.severity == ZITHC_SEVERITY_WARNING)
            msg += "warning";
        else if (diag.severity == ZITHC_SEVERITY_NOTE)
            msg += "note";
        msg += ": ";
        msg += diag.message;
        msg += "\n";
        host_write(2, msg.data(), msg.size());
        last_error += msg;
    }

    zithc_session_destroy(session);
    return ok ? 0 : 1;
}

extern "C" __attribute__((export_name("zith_run_source"))) int zith_run_source(const char *ptr,
                                                                               int len) {
    last_error.clear();
    auto *session = zithc_session_create_from_buffer("playground.zith", ptr, len);
    if (!session) {
        last_error = "failed to create session";
        return 1;
    }

    // Since we do not have an interpreter, we just run the compiler pipeline to type check and
    // lower to HIR.
    bool ok = zithc_run_to(session, ZITHC_STAGE_HIR_LOWERED);

    // Flush buffered compiler output (if any)
    const char *buffered_out = zithc_session_flush_output(session);
    if (buffered_out && buffered_out[0] != '\0') {
        host_write(1, buffered_out, std::strlen(buffered_out));
    }

    // Provide diagnostic output to stderr (stream 2) and last_error
    size_t count = zithc_diag_count(session);
    for (size_t i = 0; i < count; ++i) {
        zithc_diagnostic diag = zithc_diag_get(session, i);
        std::string msg;
        if (diag.severity == ZITHC_SEVERITY_ERROR)
            msg += "error";
        else if (diag.severity == ZITHC_SEVERITY_WARNING)
            msg += "warning";
        else if (diag.severity == ZITHC_SEVERITY_NOTE)
            msg += "note";
        msg += ": ";
        msg += diag.message;
        msg += "\n";
        host_write(2, msg.data(), msg.size());
        last_error += msg;
    }

    zithc_session_destroy(session);
    return ok ? 0 : 1;
}

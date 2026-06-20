#include "zithc-capi.h"
#include "cli/compilation-session.hpp"
#include "cli/options.hpp"
#include "diagnostics/diagnostic.hpp"
#include "diagnostics/diagnostic-engine.hpp"

#include <cstring>
#include <new>
#include <stdexcept>
#include <string>

struct zithc_session {
    zith::cli::Options opts;
    zith::cli::CompilationSession session;
    std::string last_error;

    zithc_session(const char *file_path)
        : opts(), session(opts, file_path) {
        session.setBuffered(true);
    }
    zithc_session(const char *uri, const char *content, size_t length)
        : opts(), session(opts, uri) {
        session.setBuffered(true);
        session.setContent(std::string(content, length));
    }
};

static_assert(static_cast<int>(zith::diagnostics::Severity::Note) == ZITHC_SEVERITY_NOTE,
              "severity enum mismatch");
static_assert(static_cast<int>(zith::cli::Stage::Source) == ZITHC_STAGE_SOURCE,
              "stage enum mismatch");

extern "C" {

zithc_session *zithc_session_create(const char *file_path) {
    if (!file_path || file_path[0] == '\0')
        return nullptr;
    try {
        auto *s = new zithc_session(file_path);
        return s;
    } catch (const std::exception &) {
        return nullptr;
    }
}

zithc_session *zithc_session_create_from_buffer(const char *uri, const char *content,
                                                 size_t length) {
    if (!uri || uri[0] == '\0' || !content)
        return nullptr;
    try {
        auto *s = new zithc_session(uri, content, length);
        return s;
    } catch (const std::exception &) {
        return nullptr;
    }
}

void zithc_session_destroy(zithc_session *session) {
    delete session;
}

bool zithc_run(zithc_session *session) {
    if (!session) return false;
    try {
        return session->session.run();
    } catch (const std::exception &e) {
        session->last_error = e.what();
        return false;
    }
}

bool zithc_run_to(zithc_session *session, int stage) {
    if (!session) return false;
    if (stage < ZITHC_STAGE_SOURCE || stage > ZITHC_STAGE_ZIR_INTERPRETED)
        return false;
    try {
        auto s = static_cast<zith::cli::Stage>(stage);
        return session->session.runTo(s);
    } catch (const std::exception &e) {
        session->last_error = e.what();
        return false;
    }
}

size_t zithc_diag_count(zithc_session *session) {
    if (!session) return 0;
    return session->session.diags().all().size();
}

zithc_diagnostic zithc_diag_get(zithc_session *session, size_t index) {
    zithc_diagnostic result = {ZITHC_SEVERITY_ERROR, 0, nullptr, {0, 0}};
    if (!session) return result;
    auto all = session->session.diags().all();
    if (index >= all.size()) return result;
    auto &d = all[index];
    result.severity = static_cast<zithc_severity>(static_cast<int>(d.severity));
    result.code     = d.code;
    result.message  = d.message.c_str();
    result.span     = {d.primary.start, d.primary.end};
    return result;
}

bool zithc_has_errors(zithc_session *session) {
    return session && session->session.hasErrors();
}

void zithc_emit_diagnostics(zithc_session *session) {
    if (session) session->session.emitDiagnostics();
}

zithc_position zithc_offset_to_position(zithc_session *session, uint32_t offset) {
    zithc_position result = {0, 0};
    if (!session) return result;
    try {
        auto &s = session->session;
        zith::memory::Span span{s.fileId(), offset, offset + 1};
        auto loc = s.sourceMap().loc(span);
        result.line = loc.line > 0 ? loc.line - 1 : 0;
        result.col  = loc.col  > 0 ? loc.col  - 1 : 0;
    } catch (...) {}
    return result;
}

const char *zithc_last_error(zithc_session *session) {
    if (!session) return "null session";
    return session->last_error.c_str();
}

} // extern "C"

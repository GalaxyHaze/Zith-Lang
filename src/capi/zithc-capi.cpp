#include "zithc-capi.h"
#include "cli/compilation-session.hpp"
#include "cli/options.hpp"
#include "diagnostics/diagnostic.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "diagnostics/error-codes.hpp"
#include "sema/heuristic-engine.hpp"

#include <cstring>
#include <new>

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
    auto *s = new (std::nothrow) zithc_session(file_path);
    return s;
}

zithc_session *zithc_session_create_from_buffer(const char *uri, const char *content,
                                                 size_t length) {
    if (!uri || uri[0] == '\0' || !content)
        return nullptr;
    auto *s = new (std::nothrow) zithc_session(uri, content, length);
    return s;
}

void zithc_session_destroy(zithc_session *session) {
    delete session;
}

void zithc_session_add_include_dir(zithc_session *session, const char *dir) {
    if (session && dir)
        session->opts.include_dirs.emplace_back(dir);
}

bool zithc_run(zithc_session *session) {
    if (!session) return false;
    return session->session.run();
}

bool zithc_run_to(zithc_session *session, int stage) {
    if (!session) return false;
    if (stage < ZITHC_STAGE_SOURCE || stage > ZITHC_STAGE_ZIR_INTERPRETED)
        return false;
    auto s = static_cast<zith::cli::Stage>(stage);
    return session->session.runTo(s);
}

size_t zithc_diag_count(zithc_session *session) {
    if (!session) return 0;
    return session->session.diags().all().size();
}

zithc_diagnostic zithc_diag_get(zithc_session *session, size_t index) {
    zithc_diagnostic result = {ZITHC_SEVERITY_ERROR, 0, nullptr, {0, 0}};
    if (!session) return result;
    auto &all = session->session.diags().diagnostics();
    if (index >= all.size()) return result;
    auto &d = all[index];
    // Auto-fill suggestions for UndefinedIdent diagnostics
    if (d.code == zith::diagnostics::err::UndefinedIdent && d.suggestions.empty()) {
        zith::sema::HeuristicEngine heuristic;
        heuristic.generate(d, const_cast<zith::import::SymbolTable &>(
                                session->session.symbolTable()),
                           d.suggestions);
    }
    result.severity = static_cast<zithc_severity>(static_cast<int>(d.severity));
    result.code     = d.code;
    result.message  = d.message.c_str();
    result.span     = {d.primary.start, d.primary.end};
    return result;
}
size_t zithc_diag_suggestion_count(zithc_session *session, size_t diag_index) {
    if (!session) return 0;
    auto &all = session->session.diags().diagnostics();
    if (diag_index >= all.size()) return 0;
    return all[diag_index].suggestions.size();
}

const char *zithc_diag_suggestion_get(zithc_session *session, size_t diag_index,
                                      size_t sug_index) {
    if (!session) return nullptr;
    auto &all = session->session.diags().diagnostics();
    if (diag_index >= all.size()) return nullptr;
    auto &sug = all[diag_index].suggestions;
    if (sug_index >= sug.size()) return nullptr;
    return sug[sug_index].c_str();
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
    auto &s = session->session;
    zith::memory::Span span{s.fileId(), offset, offset + 1};
    auto loc = s.sourceMap().loc(span);
    result.line = loc.line > 0 ? loc.line - 1 : 0;
    result.col  = loc.col  > 0 ? loc.col  - 1 : 0;
    return result;
}

const char *zithc_last_error(zithc_session *session) {
    if (!session) return "null session";
    return session->last_error.c_str();
}

} // extern "C"

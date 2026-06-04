// impl/diagnostics/diagnostics.cpp — Diagnostic system implementation
//
// Centralizes all diagnostic emission and printing logic.
// Implements the v1 (legacy) C API and the v2 (new) C++ API bridge.
#include "diagnostics/diagnostics.hpp"

#include "zith/memory.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// ============================================================================
// Internal Helpers (v1 legacy)
// ============================================================================

static bool find_source_line(const char *source, size_t source_len, size_t line_num,
                              const char **out_start, size_t *out_len) {
    const char *p   = source;
    const char *end = source + source_len;
    size_t cur      = 1;

    while (p < end && cur < line_num) {
        if (*p++ == '\n')
            cur++;
    }
    if (cur != line_num)
        return false;

    const char *line_start = p;
    while (p < end && *p != '\n')
        p++;

    *out_start = line_start;
    *out_len   = static_cast<size_t>(p - line_start);
    return true;
}

static const char *severity_label(ZithDiagSeverity s) {
    switch (s) {
    case ZITH_DIAG_ERROR:
        return "error";
    case ZITH_DIAG_WARNING:
        return "warning";
    case ZITH_DIAG_NOTE:
        return "note";
    default:
        return "info";
    }
}

// ============================================================================
// v1 C API — zith_diag_print_all (legacy, unchanged)
// ============================================================================

void zith_diag_print_all(const ZithDiagList *diags, const char *source, size_t source_len,
                          const char *filename) {
    if (!diags || diags->count == 0)
        return;

    for (size_t i = 0; i < diags->count; ++i) {
        const ZithDiagnostic *d = &diags->items[i];

        fprintf(stderr, "%s:%zu:%zu: %s: %s\n", filename ? filename : "<input>", d->loc.line,
                d->loc.index, severity_label(d->severity), d->message);

        if (source && source_len > 0) {
            const char *line_ptr = nullptr;
            size_t line_len      = 0;

            if (find_source_line(source, source_len, d->loc.line, &line_ptr, &line_len)) {
                fprintf(stderr, "  %.*s\n", static_cast<int>(line_len), line_ptr);

                fprintf(stderr, "  ");
                size_t col = d->loc.index;
                for (size_t c = 0; c < col && c < line_len; ++c) {
                    fputc(line_ptr[c] == '\t' ? '\t' : ' ', stderr);
                }
                fprintf(stderr, "^\n");
            }
        }
    }

    size_t errors = 0, warnings = 0;
    for (size_t i = 0; i < diags->count; ++i) {
        if (diags->items[i].severity == ZITH_DIAG_ERROR)
            errors++;
        else if (diags->items[i].severity == ZITH_DIAG_WARNING)
            warnings++;
    }

    if (errors > 0 || warnings > 0) {
        fprintf(stderr, "\n%s: ", filename ? filename : "<input>");
        if (errors)
            fprintf(stderr, "%zu error(s)", errors);
        if (errors && warnings)
            fprintf(stderr, ", ");
        if (warnings)
            fprintf(stderr, "%zu warning(s)", warnings);
        fprintf(stderr, "\n\n");
    }
}

// ============================================================================
// v2 C API — ZithDiagBag bridge
// ============================================================================

ZithDiagBag *zith_diag_bag_create(void) {
    auto *bag = new ZithDiagBag();
    bag->emitter.set_source_map(&bag->source_map);
    return bag;
}

void zith_diag_bag_destroy(ZithDiagBag *bag) {
    delete bag;
}

void zith_diag_bag_emit(ZithDiagBag *bag,
                         const char *message,
                         ZithSourceSpan span,
                         int severity,
                         uint32_t code) {
    using namespace zith::diag;

    DiagLevel level;
    switch (severity) {
    case 0: level = DiagLevel::Fatal; break;
    case 1: level = DiagLevel::Bug; break;
    case 2: level = DiagLevel::Error; break;
    case 3: level = DiagLevel::Warning; break;
    default: level = DiagLevel::Error; break;
    }

    DiagnosticBuilder(level, static_cast<DiagCode>(code))
        .with_raw_message(message)
        .with_span(SourceSpan{span.start, span.end, span.file_id})
        .emit(bag->bag);
}

void zith_diag_bag_finalize(ZithDiagBag *bag) {
    bag->bag.finalize();
}

void zith_diag_bag_print(const ZithDiagBag *bag) {
    const_cast<ZithDiagBag*>(bag)->emitter.emit(bag->bag, stderr);
}

int zith_diag_bag_had_errors(const ZithDiagBag *bag) {
    return bag->bag.had_errors() ? 1 : 0;
}

size_t zith_diag_bag_error_count(const ZithDiagBag *bag) {
    return bag->bag.error_count();
}

size_t zith_diag_bag_warning_count(const ZithDiagBag *bag) {
    return bag->bag.warning_count();
}

char *zith_diag_bag_get_json(const ZithDiagBag *bag) {
    if (!bag) return nullptr;

    zith::diag::JsonEmitter emitter(&bag->source_map);
    std::string json = emitter.emit_to_string(bag->bag);
    if (json.empty() || json == "[]\n") return nullptr;

    char *result = static_cast<char *>(malloc(json.size() + 1));
    if (result) {
        memcpy(result, json.c_str(), json.size() + 1);
    }
    return result;
}

// ============================================================================
// C++ DiagManager Implementation
// ============================================================================

DiagManager::DiagManager() {
    emitter_.set_source_map(&source_map_);
}

void DiagManager::emit(const ZithSourceLoc loc, const ZithDiagSeverity severity, const char *msg) {
    using namespace zith::diag;

    DiagLevel level;
    switch (severity) {
    case ZITH_DIAG_ERROR:   level = DiagLevel::Error; break;
    case ZITH_DIAG_WARNING: level = DiagLevel::Warning; break;
    case ZITH_DIAG_NOTE:    level = DiagLevel::Note; break;
    default:                level = DiagLevel::Help; break;
    }

    build(level, DiagCode::UnexpectedToken)
        .with_raw_message(msg ? std::string(msg) : std::string())
        .with_span(SourceSpan::from_loc(loc, source_map_.add_or_get_file("<input>", "")))
        .emit(bag_);

    legacy_cached_ = false;
}

void DiagManager::error(const ZithSourceLoc loc, const char *msg) {
    emit(loc, ZITH_DIAG_ERROR, msg);
}

void DiagManager::warning(const ZithSourceLoc loc, const char *msg) {
    emit(loc, ZITH_DIAG_WARNING, msg);
}

void DiagManager::note(const ZithSourceLoc loc, const char *msg) {
    emit(loc, ZITH_DIAG_NOTE, msg);
}

void DiagManager::info(const char *msg) {
    printf("[*] %s\n", msg);
}

ZithDiagList DiagManager::build_legacy_list() const {
    legacy_storage_.clear();
    legacy_list_.items = nullptr;
    legacy_list_.count = 0;
    legacy_list_.capacity = 0;

    for (const auto& diag : bag_.diagnostics()) {
        ZithDiagnostic zd;
        zd.message = diag.message.get().c_str();
        zd.loc = diag.primary_span.start;
        switch (diag.level) {
        case zith::diag::DiagLevel::Error:
        case zith::diag::DiagLevel::Fatal:
        case zith::diag::DiagLevel::Bug:
            zd.severity = ZITH_DIAG_ERROR; break;
        case zith::diag::DiagLevel::Warning:
            zd.severity = ZITH_DIAG_WARNING; break;
        default:
            zd.severity = ZITH_DIAG_NOTE; break;
        }
        legacy_storage_.push_back(zd);
    }

    legacy_list_.items = legacy_storage_.data();
    legacy_list_.count = legacy_storage_.size();
    legacy_list_.capacity = legacy_storage_.size();
    legacy_cached_ = true;
    return legacy_list_;
}

ZithDiagList DiagManager::list() const {
    return build_legacy_list();
}

void DiagManager::print_all(const char *source, const size_t source_len, const char *filename) const {
    if (source && filename) {
        auto* mutable_this = const_cast<DiagManager*>(this);
        mutable_this->source_map_.add_or_get_file(
            filename ? filename : "<input>",
            source ? std::string_view(source, source_len) : std::string_view());

        const_cast<zith::diag::DiagnosticBag&>(bag_).finalize();
        emitter_.emit(bag_, stderr);
    } else {
        auto list = build_legacy_list();
        zith_diag_print_all(&list, source, source_len, filename);
    }
}

void DiagManager::print_summary(const char *filename) const {
    auto list = build_legacy_list();

    size_t errors = 0, warnings = 0;
    for (size_t i = 0; i < list.count; ++i) {
        if (list.items[i].severity == ZITH_DIAG_ERROR)
            errors++;
        else if (list.items[i].severity == ZITH_DIAG_WARNING)
            warnings++;
    }

    if (errors > 0 || warnings > 0) {
        fprintf(stderr, "\n%s: ", filename ? filename : "<input>");
        if (errors)
            fprintf(stderr, "%zu error(s)", errors);
        if (errors && warnings)
            fprintf(stderr, ", ");
        if (warnings)
            fprintf(stderr, "%zu warning(s)", warnings);
        fprintf(stderr, "\n\n");
    }
}

// ============================================================================
// Debug output helpers (unchanged)
// ============================================================================

#ifndef ZITH_NO_DEBUG

#include <cstdarg>

void debug_print(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

void debug_println(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

void debug_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

#endif // ZITH_NO_DEBUG

// ============================================================================
// I/O error reporting (unchanged)
// ============================================================================

void zith_io_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[zith] ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

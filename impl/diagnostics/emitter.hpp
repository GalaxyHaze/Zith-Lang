#pragma once

#include "diagnostics/diagnostic_bag.hpp"
#include "diagnostics/span.hpp"

#include <cstdio>
#include <string>
#include <string_view>

namespace zith::diag {

enum class EmitFormat {
    Terminal,
    Json,
};

class TerminalEmitter {
public:
    explicit TerminalEmitter(const SourceMap* source_map = nullptr)
        : source_map_(source_map) {}

    void emit(const DiagnosticBag& bag, FILE* out = stderr) const;
    void emit_single(const Diagnostic& diag, FILE* out = stderr) const;
    void emit_summary(const DiagnosticBag& bag, FILE* out = stderr) const;

    void set_source_map(const SourceMap* sm) { source_map_ = sm; }

private:
    const SourceMap* source_map_;

    void render_header(const Diagnostic& diag, FILE* out) const;
    void render_code_frame(const Diagnostic& diag, FILE* out) const;
    void render_line(FILE* out, const SourceFile* file, size_t line_1based, size_t highlight_col, size_t highlight_len) const;
    void render_children(const Diagnostic& diag, FILE* out) const;
    void render_suggestions(const Diagnostic& diag, FILE* out) const;

    static void set_color(FILE* out, const char* ansi);
    static void reset_color(FILE* out);

    static constexpr const char* RED    = "\033[31m";
    static constexpr const char* GREEN  = "\033[32m";
    static constexpr const char* YELLOW = "\033[33m";
    static constexpr const char* CYAN   = "\033[36m";
    static constexpr const char* BOLD   = "\033[1m";
    static constexpr const char* RESET  = "\033[0m";
    static constexpr const char* GRAY   = "\033[90m";
};

class JsonEmitter {
public:
    explicit JsonEmitter(const SourceMap* source_map = nullptr)
        : source_map_(source_map) {}

    void emit(const DiagnosticBag& bag, FILE* out = stdout) const;
    void emit_single(const Diagnostic& diag, FILE* out = stdout) const;

    void set_source_map(const SourceMap* sm) { source_map_ = sm; }

private:
    const SourceMap* source_map_;

    void render_diagnostic_json(const Diagnostic& diag, FILE* out, bool trailing_comma) const;
    static void write_escaped_json_string(FILE* out, std::string_view s);
};

} // namespace zith::diag

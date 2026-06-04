#include "diagnostics/emitter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zith::diag {

// ============================================================================
// TerminalEmitter
// ============================================================================

void TerminalEmitter::emit(const DiagnosticBag &bag, FILE *out) const {
    if (bag.total_count() == 0)
        return;

    if (!bag.is_finalized()) {
        const_cast<DiagnosticBag &>(bag).finalize();
    }

    for (const auto *diag : bag.sorted_view()) {
        emit_single(*diag, out);
    }

    emit_summary(bag, out);
}

void TerminalEmitter::emit_single(const Diagnostic &diag, FILE *out) const {
    render_header(diag, out);
    if (diag.has_primary_span) {
        render_code_frame(diag, out);
    }
    render_suggestions(diag, out);
    render_children(diag, out);
}

void TerminalEmitter::emit_summary(const DiagnosticBag &bag, FILE *out) {
    const size_t errors   = bag.error_count();
    const size_t warnings = bag.warning_count();

    if (errors > 0 || warnings > 0) {
        set_color(out, BOLD);
        if (errors > 0) {
            set_color(out, RED);
            fprintf(out, "error: %zu ", errors);
            fprintf(out, errors == 1 ? "error" : "errors");
            reset_color(out);
        }
        if (errors > 0 && warnings > 0) {
            fprintf(out, ", ");
        }
        if (warnings > 0) {
            set_color(out, YELLOW);
            fprintf(out, "%zu ", warnings);
            fprintf(out, warnings == 1 ? "warning" : "warnings");
            reset_color(out);
        }
        fprintf(out, "\n\n");
    }
}

void TerminalEmitter::render_header(const Diagnostic &diag, FILE *out) {
    set_color(out, BOLD);
    switch (diag.level) {
    case DiagLevel::Fatal:
    case DiagLevel::Bug:
    case DiagLevel::Error:
        set_color(out, RED);
        fprintf(out, "error");
        break;
    case DiagLevel::Warning:
        set_color(out, YELLOW);
        fprintf(out, "warning");
        break;
    case DiagLevel::Note:
        set_color(out, CYAN);
        fprintf(out, "note");
        break;
    case DiagLevel::Help:
    case DiagLevel::Hint:
        set_color(out, GREEN);
        fprintf(out, "%s", diag.level == DiagLevel::Help ? "help" : "hint");
        break;
    }
    reset_color(out);

    fprintf(out, "[%s]", diag_code_string(diag.code));

    set_color(out, BOLD);
    fprintf(out, ": %s", diag.message.get().c_str());
    reset_color(out);
    fprintf(out, "\n");
}

void TerminalEmitter::render_code_frame(const Diagnostic &diag, FILE *out) const {
    if (!source_map_)
        return;

    const SourceFile *file = source_map_->lookup(diag.primary_span.file_id);
    if (!file)
        return;

    // ── file:line:col header ──
    set_color(out, GRAY);
    fprintf(out, "  %s%s:%zu:%zu%s\n", BOLD, file->filename.c_str(), diag.primary_span.start.line,
            diag.primary_span.start.index, RESET);

    // Determine the line range to show (context ±2 lines)
    size_t start_line     = diag.primary_span.start.line;
    size_t context_before = (start_line > 2) ? start_line - 2 : 1;
    size_t context_after  = std::min(start_line + 2, file->line_count());

    // Render context lines
    for (size_t ln = context_before; ln <= context_after; ++ln) {
        render_line(out, file, ln, (ln == start_line) ? diag.primary_span.start.index : SIZE_MAX,
                    (ln == start_line) ? 1 : 0);
    }

    // Render secondary spans
    for (const auto &ls : diag.secondary_spans) {
        if (ls.span.start.line == diag.primary_span.start.line)
            continue;
        render_line(out, file, ls.span.start.line, ls.span.start.index, 1);
        // Label on next line
        if (ls.label.has_value()) {
            set_color(out, CYAN);
            fprintf(out, "     %s\n", ls.label.get().c_str());
            reset_color(out);
        }
    }
}

void TerminalEmitter::render_line(FILE *out, const SourceFile *file, size_t line_1based,
                                  const size_t highlight_col, size_t highlight_len) {
    const std::string_view text = file->line_text(line_1based);
    if (text.empty())
        return;

    // Line number
    set_color(out, GRAY);
    fprintf(out, " %4zu ", line_1based);
    reset_color(out);

    // Pipe separator
    set_color(out, GRAY);
    fprintf(out, "| ");
    reset_color(out);

    // Line content
    fprintf(out, "%.*s\n", static_cast<int>(text.size()), text.data());

    // Underline (caret)
    if (highlight_col != SIZE_MAX && highlight_col <= text.size()) {
        set_color(out, GRAY);
        fprintf(out, "      | ");
        reset_color(out);
        for (size_t c = 0; c < highlight_col && c < text.size(); ++c) {
            fputc(text[c] == '\t' ? '\t' : ' ', out);
        }
        set_color(out, RED);
        set_color(out, BOLD);
        fprintf(out, "^");
        for (size_t c = 1; c < highlight_len && (highlight_col + c) < text.size(); ++c) {
            fprintf(out, "~");
        }
        reset_color(out);
        fprintf(out, "\n");
    }
}

void TerminalEmitter::render_children(const Diagnostic &diag, FILE *out) {
    for (const auto &child_ptr : diag.children) {
        if (!child_ptr)
            continue;
        const auto &child = *child_ptr;
        std::string msg   = child.message.get();
        if (msg.empty())
            continue;

        switch (child.level) {
        case DiagLevel::Note:
            set_color(out, CYAN);
            fprintf(out, "  = note: ");
            break;
        case DiagLevel::Help:
            set_color(out, GREEN);
            fprintf(out, "  = help: ");
            break;
        case DiagLevel::Hint:
            set_color(out, GREEN);
            fprintf(out, "  = hint: ");
            break;
        default:
            fprintf(out, "  = ");
            break;
        }
        reset_color(out);
        fprintf(out, "%s\n", msg.c_str());
    }
}

void TerminalEmitter::render_suggestions(const Diagnostic &diag, FILE *out) const {
    for (const auto &sug : diag.suggestions) {
        if (!sug.label.empty()) {
            set_color(out, GREEN);
            fprintf(out, "  = suggestion: %s\n", sug.label.c_str());
            reset_color(out);
        }

        if (!sug.replacement.empty() && source_map_) {
            if (const SourceFile *file = source_map_->lookup(sug.span.file_id)) {
                std::string_view orig = file->line_text(sug.span.start.line);
                set_color(out, RED);
                fprintf(out, "    - %.*s\n", static_cast<int>(orig.size()), orig.data());
                reset_color(out);
                set_color(out, GREEN);
                fprintf(out, "    + %s\n", sug.replacement.c_str());
                reset_color(out);
            }
        }
    }
}

void TerminalEmitter::set_color(FILE *out, const char *ansi) {
    fprintf(out, "%s", ansi);
}

void TerminalEmitter::reset_color(FILE *out) {
    fprintf(out, "%s", RESET);
}

// ============================================================================
// JsonEmitter
// ============================================================================

void JsonEmitter::write_escaped_json_string(FILE* out, std::string_view s) {
    fputc('"', out);
    for (char c : s) {
        switch (c) {
        case '"':  fprintf(out, "\\\""); break;
        case '\\': fprintf(out, "\\\\"); break;
        case '\n': fprintf(out, "\\n"); break;
        case '\r': fprintf(out, "\\r"); break;
        case '\t': fprintf(out, "\\t"); break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                fprintf(out, "\\u%04x", static_cast<unsigned char>(c));
            } else {
                fputc(c, out);
            }
            break;
        }
    }
    fputc('"', out);
}

static std::string escape_json_string(std::string_view s) {
    std::string result;
    result.reserve(s.size() + 8);
    result.push_back('"');
    for (char c : s) {
        switch (c) {
        case '"':  result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                result += buf;
            } else {
                result.push_back(c);
            }
            break;
        }
    }
    result.push_back('"');
    return result;
}

void JsonEmitter::emit(const DiagnosticBag& bag, FILE* out) const {
    if (bag.total_count() == 0) {
        fprintf(out, "[]\n");
        return;
    }

    fprintf(out, "[\n");
    size_t count = bag.sorted_view().size();
    for (size_t i = 0; i < count; ++i) {
        render_diagnostic_json(*bag.sorted_view()[i], out, (i + 1 < count));
    }
    fprintf(out, "]\n");
}

void JsonEmitter::emit_single(const Diagnostic& diag, FILE* out) const {
    render_diagnostic_json(diag, out, false);
    fprintf(out, "\n");
}

void JsonEmitter::render_diagnostic_json(const Diagnostic& diag, FILE* out, bool trailing_comma) const {
    fprintf(out, "  {\n");

    fprintf(out, "    \"severity\": ");
    write_escaped_json_string(out, diag_level_label(diag.level));
    fprintf(out, ",\n");

    fprintf(out, "    \"code\": ");
    write_escaped_json_string(out, diag_code_string(diag.code));
    fprintf(out, ",\n");

    fprintf(out, "    \"message\": ");
    write_escaped_json_string(out, diag.message.get());
    fprintf(out, ",\n");

    if (diag.has_primary_span && source_map_) {
        const SourceFile* file = source_map_->lookup(diag.primary_span.file_id);
        if (file) {
            fprintf(out, "    \"file\": ");
            write_escaped_json_string(out, file->filename);
            fprintf(out, ",\n");
        }
        fprintf(out, "    \"line\": %zu,\n", diag.primary_span.start.line);
        fprintf(out, "    \"column\": %zu,\n", diag.primary_span.start.index);

        // End span
        if (diag.primary_span.end.line != diag.primary_span.start.line ||
            diag.primary_span.end.index != diag.primary_span.start.index) {
            fprintf(out, "    \"end_line\": %zu,\n", diag.primary_span.end.line);
            fprintf(out, "    \"end_column\": %zu,\n", diag.primary_span.end.index);
        }
    }

    // Suggestions
    if (!diag.suggestions.empty()) {
        fprintf(out, "    \"suggestions\": [\n");
        for (size_t j = 0; j < diag.suggestions.size(); ++j) {
            const auto& sug = diag.suggestions[j];
            fprintf(out, "      {\n");
            fprintf(out, "        \"label\": ");
            write_escaped_json_string(out, sug.label);
            fprintf(out, ",\n");
            fprintf(out, "        \"replacement\": ");
            write_escaped_json_string(out, sug.replacement);
            fprintf(out, ",\n");
            fprintf(out, "        \"span\": {\n");
            fprintf(out, "          \"start\": {\"line\": %zu, \"column\": %zu},\n",
                    sug.span.start.line, sug.span.start.index);
            fprintf(out, "          \"end\": {\"line\": %zu, \"column\": %zu}\n",
                    sug.span.end.line, sug.span.end.index);
            fprintf(out, "        },\n");
            fprintf(out, "        \"machine_applicable\": %s\n",
                    sug.is_machine_applicable ? "true" : "false");
            fprintf(out, "      }%s\n", (j + 1 < diag.suggestions.size()) ? "," : "");
        }
        fprintf(out, "    ],\n");
    }

    // Children (notes, helps, hints)
    if (!diag.children.empty()) {
        fprintf(out, "    \"children\": [\n");
        for (size_t j = 0; j < diag.children.size(); ++j) {
            if (!diag.children[j]) continue;
            const auto& child = *diag.children[j];
            std::string msg = child.message.get();
            fprintf(out, "      {\n");
            fprintf(out, "        \"level\": ");
            write_escaped_json_string(out, diag_level_label(child.level));
            fprintf(out, ",\n");
            fprintf(out, "        \"message\": ");
            write_escaped_json_string(out, msg);
            fprintf(out, "\n");
            fprintf(out, "      }%s\n", (j + 1 < diag.children.size()) ? "," : "");
        }
        fprintf(out, "    ]\n");
    }

    // Remove trailing comma if this is the last field
    // (simplified: just close the object)
    fprintf(out, "  }%s\n", trailing_comma ? "," : "");
}

} // namespace zith::diag

std::string zith::diag::JsonEmitter::emit_to_string(const DiagnosticBag& bag) const {
    if (bag.total_count() == 0) {
        return "[]\n";
    }

    if (!bag.is_finalized()) {
        const_cast<DiagnosticBag&>(bag).finalize();
    }

    std::string result = "[\n";
    size_t count = bag.sorted_view().size();
    for (size_t i = 0; i < count; ++i) {
        const auto* diag = bag.sorted_view()[i];
        bool trailing = (i + 1 < count);

        result += "  {\n";
        result += "    \"severity\": " + escape_json_string(diag_level_label(diag->level)) + ",\n";
        result += "    \"code\": " + escape_json_string(diag_code_string(diag->code)) + ",\n";
        result += "    \"message\": " + escape_json_string(diag->message.get()) + ",\n";

        if (diag->has_primary_span && source_map_) {
            const SourceFile* file = source_map_->lookup(diag->primary_span.file_id);
            if (file) {
                result += "    \"file\": " + escape_json_string(file->filename) + ",\n";
            }
            result += "    \"line\": " + std::to_string(diag->primary_span.start.line) + ",\n";
            result += "    \"column\": " + std::to_string(diag->primary_span.start.index) + ",\n";

            if (diag->primary_span.end.line != diag->primary_span.start.line ||
                diag->primary_span.end.index != diag->primary_span.start.index) {
                result += "    \"end_line\": " + std::to_string(diag->primary_span.end.line) + ",\n";
                result += "    \"end_column\": " + std::to_string(diag->primary_span.end.index) + ",\n";
            }
        }

        // Suggestions
        if (!diag->suggestions.empty()) {
            result += "    \"suggestions\": [\n";
            for (size_t j = 0; j < diag->suggestions.size(); ++j) {
                const auto& sug = diag->suggestions[j];
                result += "      {\n";
                result += "        \"label\": " + escape_json_string(sug.label) + ",\n";
                result += "        \"replacement\": " + escape_json_string(sug.replacement) + ",\n";
                result += "        \"span\": {\n";
                result += R"(          "start": {"line": )" + std::to_string(sug.span.start.line) +
                          ", \"column\": " + std::to_string(sug.span.start.index) + "},\n";
                result += R"(          "end": {"line": )" + std::to_string(sug.span.end.line) +
                          ", \"column\": " + std::to_string(sug.span.end.index) + "}\n";
                result += "        },\n";
                result += "        \"machine_applicable\": " +
                          std::string(sug.is_machine_applicable ? "true" : "false") + "\n";
                result += "      }";
                if (j + 1 < diag->suggestions.size()) result += ",";
                result += "\n";
            }
            result += "    ],\n";
        }

        // Children (notes, helps, hints)
        if (!diag->children.empty()) {
            result += "    \"children\": [\n";
            bool first_child = true;
            for (const auto& child_ptr : diag->children) {
                if (!child_ptr) continue;
                const auto& child = *child_ptr;
                std::string msg = child.message.get();
                if (!first_child) result += ",\n";
                first_child = false;
                result += "      {\n";
                result += "        \"level\": " + escape_json_string(diag_level_label(child.level)) + ",\n";
                result += "        \"message\": " + escape_json_string(msg) + "\n";
                result += "      }";
            }
            result += "\n    ]\n";
        }

        result += "  }";
        if (trailing) result += ",";
        result += "\n";
    }
    result += "]\n";
    return result;
}

#include "cli/terminal.hpp"
#include "diagnostic-engine.hpp"
#include "diagnostics/diagnostic.hpp"
#include "diagnostics/error-codes.hpp"
#include "memory/source-file.hpp"
#include "memory/source-map.hpp"

#include <cstdio>
#include <string>

using namespace zith::term;

namespace zith::diagnostics {
namespace {

struct LineInfo {
    size_t line_num;
    size_t line_start;
    size_t line_end;
    std::string_view text;
};

LineInfo findLine(std::string_view source, memory::ByteOffset offset) {
    size_t start = offset;
    while (start > 0 && source[start - 1] != '\n')
        start--;

    size_t end = offset;
    while (end < source.size() && source[end] != '\n')
        end++;

    size_t line_num = 1;
    for (size_t i = 0; i < start; i++) {
        if (source[i] == '\n')
            line_num++;
    }

    return {line_num, start, end, source.substr(start, end - start)};
}

void printSeverityLabel(Severity sev, bool color, const ColorTheme &theme) {
    if (color) {
        switch (sev) {
        case Severity::Error:
            std::fputs(theme.severity_error.data(), stderr);
            break;
        case Severity::Warning:
            std::fputs(theme.severity_warning.data(), stderr);
            break;
        case Severity::Bug:
            std::fputs(theme.severity_bug.data(), stderr);
            break;
        case Severity::Note:
            std::fputs(theme.severity_note.data(), stderr);
            break;
        }
    }
    switch (sev) {
    case Severity::Error:
        std::fputs("error", stderr);
        break;
    case Severity::Warning:
        std::fputs("warning", stderr);
        break;
    case Severity::Bug:
        std::fputs("bug", stderr);
        break;
    case Severity::Note:
        std::fputs("note", stderr);
        break;
    }
    if (color)
        std::fputs(ansi::reset.data(), stderr);
}

} // anonymous namespace

void DiagnosticEngine::emitLabelLine(const char *label, std::string_view msg) const {
    if (use_color_)
        std::fputs(theme_.note_prefix.data(), stderr);
    std::fprintf(stderr, "   %s %.*s\n", label, static_cast<int>(msg.size()), msg.data());
    if (use_color_)
        std::fputs(ansi::reset.data(), stderr);
}

void DiagnosticEngine::emitOne(const Diagnostic &d, std::string_view source,
                               const char *location_line, bool has_secondary_labels) const {
    auto info = lookupError(d.code);

    printSeverityLabel(d.severity, use_color_, theme_);
    if (info) {
        std::fprintf(stderr, "[%c%04u]: %s\n", info->prefix, info->code, d.message.c_str());
    } else {
        std::fprintf(stderr, "[%04u]: %s\n", d.code, d.message.c_str());
    }

    if (location_line && location_line[0]) {
        if (use_color_)
            std::fputs(theme_.location.data(), stderr);
        std::fputs(location_line, stderr);
        if (use_color_)
            std::fputs(ansi::reset.data(), stderr);
        std::fputc('\n', stderr);

        if (!source.empty() && d.primary.start < source.size() && d.primary.end <= source.size()) {
            auto line = findLine(source, d.primary.start);

            std::fprintf(stderr, "   |\n");
            if (use_color_)
                std::fputs(theme_.line_no.data(), stderr);
            std::fprintf(stderr, " %zu | ", line.line_num);
            if (use_color_)
                std::fputs(ansi::reset.data(), stderr);
            std::fprintf(stderr, "%.*s\n", static_cast<int>(line.text.size()), line.text.data());

            size_t col = d.primary.start >= line.line_start ? d.primary.start - line.line_start : 0;
            size_t end_col = d.primary.end >= line.line_start ? d.primary.end - line.line_start : 0;
            if (end_col > line.text.size())
                end_col = line.text.size();
            if (col > line.text.size())
                col = line.text.size();

            std::fputs("   | ", stderr);
            if (use_color_)
                std::fputs(theme_.underline.data(), stderr);
            for (size_t i = 0; i < col; i++)
                std::fputc(' ', stderr);
            for (size_t i = col; i < col + 1 && i < line.text.size(); i++)
                std::fputc('^', stderr);
            for (size_t i = col + 1; i < end_col; i++)
                std::fputc('~', stderr);

            for (auto &lbl : d.labels) {
                if (lbl.span.start == d.primary.start && lbl.span.end == d.primary.end) {
                    std::fprintf(stderr, " %s", lbl.message.c_str());
                    break;
                }
            }
            if (use_color_)
                std::fputs(ansi::reset.data(), stderr);
            std::fputc('\n', stderr);

            if (has_secondary_labels) {
                for (auto &lbl : d.labels) {
                    if (lbl.span.start == d.primary.start && lbl.span.end == d.primary.end)
                        continue;
                    auto lbl_loc = source_map_ ? source_map_->loc(lbl.span) : memory::Loc{};
                    std::fprintf(stderr, "     = note: %s (at %u:%u)\n", lbl.message.c_str(),
                                 lbl_loc.line, lbl_loc.col);
                }
            }
        }
    }

    if (info && !info->why.empty())
        emitLabelLine("= why:", info->why);
    if (info && !info->note.empty())
        emitLabelLine("= note:", info->note);
    for (auto &s : d.suggestions)
        emitLabelLine("= help:", s);

    std::fputc('\n', stderr);
}

void DiagnosticEngine::emit() const {
    if (suppress_emit_)
        return;
    for (auto &d : this->all()) {
        if (source_map_) {
            auto maybe_src = source_map_->get(d.primary.file);
            if (maybe_src.isValid()) {
                auto &src = maybe_src.value().get();
                auto loc  = source_map_->loc(d.primary);
                char buf[256];
                std::snprintf(buf, sizeof(buf), "  --> %s:%u:%u", src.path.c_str(), loc.line,
                              loc.col);
                emitOne(d, src.getSlice(), buf, true);
                continue;
            }
        }
        emitOne(d, {}, "", false);
    }
}

void DiagnosticEngine::emitTo(std::string_view source_text) const {
    if (source_map_) {
        emit();
        return;
    }
    for (auto &d : this->all())
        emitOne(d, source_text, "  --> <input>", false);
}

} // namespace zith::diagnostics

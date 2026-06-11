#include "diagnostic-engine.hpp"
#include "diagnostics/diagnostic.hpp"
#include "diagnostics/error-codes.hpp"
#include "memory/source-map.hpp"
#include "memory/source-file.hpp"

#include <cstdio>
#include <string>

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
            case Severity::Error:   std::fputs("error", stderr);   break;
            case Severity::Warning: std::fputs("warning", stderr); break;
            case Severity::Bug:     std::fputs("bug", stderr);     break;
            case Severity::Note:    std::fputs("note", stderr);    break;
        }
        if (color)
            std::fputs(ansi::reset.data(), stderr);
    }

} // anonymous namespace

    void DiagnosticEngine::emit() const {
        for (auto &d : this->all()) {
            auto info = lookupError(d.code);

            printSeverityLabel(d.severity, use_color_, theme_);
            if (info) {
                std::fprintf(stderr, "[%c%04u]: %s\n", info->prefix, info->code, d.message.c_str());
            } else {
                std::fprintf(stderr, "[%04u]: %s\n", d.code, d.message.c_str());
            }

            // Location via SourceMap (file:line:col)
            auto loc = source_map_ ? source_map_->loc(d.primary) : memory::Loc{};
            auto maybe_src = source_map_ ? source_map_->get(d.primary.file) : memory::Optional<std::reference_wrapper<memory::SourceLoc>>(nullptr);
            if (maybe_src.isValid()) {
                auto &src = maybe_src.value().get();
                if (use_color_)
                    std::fputs(theme_.location.data(), stderr);
                std::fprintf(stderr, "  --> %s:%u:%u",
                             std::string(src.path).c_str(), loc.line, loc.col);
                if (use_color_)
                    std::fputs(ansi::reset.data(), stderr);
                std::fputc('\n', stderr);
            }

            // Suggestions
            if (!d.suggestions.empty()) {
                for (auto &s : d.suggestions) {
                    if (use_color_)
                        std::fputs(theme_.help_prefix.data(), stderr);
                    std::fprintf(stderr, "   = help: %s\n", s.c_str());
                    if (use_color_)
                        std::fputs(ansi::reset.data(), stderr);
                }
            }

            // Tip from registry
            if (info && !info->tip.empty()) {
                if (use_color_)
                    std::fputs(theme_.tip_prefix.data(), stderr);
                std::fprintf(stderr, "   = tip: %s\n", std::string(info->tip).c_str());
                if (use_color_)
                    std::fputs(ansi::reset.data(), stderr);
            }

            std::fputc('\n', stderr);
        }
    }

    void DiagnosticEngine::emitTo(std::string_view source_text) const {
        for (auto &d : this->all()) {
            auto info = lookupError(d.code);

            // Header
            printSeverityLabel(d.severity, use_color_, theme_);
            if (info) {
                std::fprintf(stderr, "[%c%04u]: %s\n", info->prefix, info->code, d.message.c_str());
            } else {
                std::fprintf(stderr, "[%04u]: %s\n", d.code, d.message.c_str());
            }

            // Location: file:line:col
            std::string_view file_path = "<unknown>";
            std::string_view file_source = source_text;

            auto maybe_src = source_map_ ? source_map_->get(d.primary.file) : memory::Optional<std::reference_wrapper<memory::SourceLoc>>(nullptr);
            if (maybe_src.isValid()) {
                auto &src = maybe_src.value().get();
                file_path = src.path;
                file_source = src.getSlice();
            }

            auto loc = source_map_ ? source_map_->loc(d.primary) : memory::Loc{};
            if (use_color_)
                std::fputs(theme_.location.data(), stderr);
            std::fprintf(stderr, "  --> %s:%u:%u",
                         std::string(file_path).c_str(), loc.line, loc.col);
            if (use_color_)
                std::fputs(ansi::reset.data(), stderr);
            std::fputc('\n', stderr);

            // Source line with underline
            if (d.primary.start < file_source.size()) {
                auto line = findLine(file_source, d.primary.start);

                std::fprintf(stderr, "   |\n");

                // Line number + content
                if (use_color_)
                    std::fputs(theme_.line_no.data(), stderr);
                std::fprintf(stderr, " %zu | ", line.line_num);
                if (use_color_)
                    std::fputs(ansi::reset.data(), stderr);
                std::fprintf(stderr, "%.*s\n",
                             static_cast<int>(line.text.size()),
                             line.text.data());

                // Underline
                size_t col = d.primary.start - line.line_start;
                size_t end_col = d.primary.end - line.line_start;
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

                // Label message on the right if there's a primary label
                bool has_primary_label = false;
                for (auto &lbl : d.labels) {
                    if (lbl.span.start == d.primary.start && lbl.span.end == d.primary.end) {
                        std::fprintf(stderr, " %s", lbl.message.c_str());
                        has_primary_label = true;
                        break;
                    }
                }
                if (use_color_)
                    std::fputs(ansi::reset.data(), stderr);
                std::fputc('\n', stderr);

                // Secondary labels
                for (auto &lbl : d.labels) {
                    if (lbl.span.start == d.primary.start && lbl.span.end == d.primary.end)
                        continue;
                    auto lbl_loc = source_map_ ? source_map_->loc(lbl.span) : memory::Loc{};
                    std::fprintf(stderr, "     = note: %s (at %u:%u)\n",
                                 lbl.message.c_str(), lbl_loc.line, lbl_loc.col);
                }
            }

            // Suggestions
            if (!d.suggestions.empty()) {
                for (auto &s : d.suggestions) {
                    if (use_color_)
                        std::fputs(theme_.help_prefix.data(), stderr);
                    std::fprintf(stderr, "   = help: %s\n", s.c_str());
                    if (use_color_)
                        std::fputs(ansi::reset.data(), stderr);
                }
            }

            // Tip from registry
            if (info && !info->tip.empty()) {
                if (use_color_)
                    std::fputs(theme_.tip_prefix.data(), stderr);
                std::fprintf(stderr, "   = tip: %s\n", std::string(info->tip).c_str());
                if (use_color_)
                    std::fputs(ansi::reset.data(), stderr);
            }

            std::fputc('\n', stderr);
        }
    }

} // namespace zith::diagnostics

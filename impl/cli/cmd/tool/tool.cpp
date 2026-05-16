#include <cli/pipeline/pipeline.hpp>
#include <cli/cmd/commands.hpp>
#include <zith/ast.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace zith::cli::commands {

int cmd_test(const std::string & /*input_file*/, bool /*verbose*/) {
    pipeline::print_not_implemented("test");
    return 1;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool is_ctrl_keyword(const ZithTokenType t) {
    return t == ZITH_TOKEN_IF || t == ZITH_TOKEN_FOR || t == ZITH_TOKEN_ELSE;
}

// Normalise spacing inside a line: remove extraneous spaces around
// parentheses, brackets, semicolons, and before opening braces.
static std::string normalise_line(std::string s) {
    // 1) collapse runs of space to a single space
    {
        std::string r;
        r.reserve(s.size());
        bool prev_space = false;
        for (char c : s) {
            if (c == ' ' || c == '\t') {
                if (!prev_space) r += ' ';
                prev_space = true;
            } else {
                r += c;
                prev_space = false;
            }
        }
        s = std::move(r);
    }

    // 2) remove spaces before closing brackets / semicolons / commas
    {
        std::string r;
        r.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == ' ' && i + 1 < s.size()) {
                const char n = s[i + 1];
                if (n == ')' || n == ']' || n == ';' || n == ',' || n == ':')
                    continue; // skip space
            }
            r += s[i];
        }
        s = std::move(r);
    }

    // 3) remove spaces after opening brackets, and before them too
    {
        std::string r;
        r.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == ' ' && i > 0) {
                const char p = s[i - 1];
                if (p == '(' || p == '[')
                    continue; // skip space after opening bracket
            }
            if (s[i] == ' ' && i + 1 < s.size()) {
                const char n = s[i + 1];
                if (n == '(' || n == '[')
                    continue; // skip space before opening bracket
            }
            r += s[i];
        }
        s = std::move(r);
    }

    // 4) space before '{' is already a single space from step 1 — keep it

    // 5) ensure space after ','
    {
        std::string r;
        r.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            r += s[i];
            if (s[i] == ',' && i + 1 < s.size() && s[i + 1] != ' ')
                r += ' ';
        }
        s = std::move(r);
    }

    // 6) ensure space after ':' when followed by non-space, non-':'
    {
        std::string r;
        r.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            r += s[i];
            if (s[i] == ':' && i + 1 < s.size() && s[i + 1] != ' ' && s[i + 1] != ':')
                r += ' ';
        }
        s = std::move(r);
    }

    return s;
}

// ---------------------------------------------------------------------------
// cmd_fmt
// ---------------------------------------------------------------------------

int cmd_fmt(const std::string &input_file, bool check_only, bool verbose) {
    if (verbose)
        pipeline::print_info("Formatting '" + input_file + "'...");

    ZithTokenStream stream{};
    const char *source = nullptr;
    size_t src_size = 0;
    ZithArena *arena =
        pipeline::tokenize_file(input_file, stream, &source, &src_size, verbose);
    if (!arena)
        return 1;

    ZithNode *ast = zith_parse_with_source(arena, source, src_size, input_file.c_str(),
                                           stream, nullptr, 0);
    if (!ast) {
        pipeline::print_error("Parse failed -- cannot format invalid code");
        zith_arena_destroy(arena);
        return 1;
    }

    std::string src(source, src_size);

    // -- Count lines -------------------------------------------------------
    size_t line_count = 1;
    for (char c : src)
        if (c == '\n') ++line_count;

    // -- Per-line info from token stream -----------------------------------
    struct LineInfo {
        int brace_delta = 0;
        bool has_lbrace  = false;
        bool has_ctrl_kw = false; // if/for/else without { on same line
    };
    std::vector<LineInfo> info(line_count + 2);

    // First pass: brace balance
    for (size_t i = 0; i < stream.len; ++i) {
        uint64_t ln = stream.data[i].loc.line;
        if (ln > line_count) continue;
        if (stream.data[i].type == ZITH_TOKEN_LBRACE) {
            ++info[ln].brace_delta;
            info[ln].has_lbrace = true;
        } else if (stream.data[i].type == ZITH_TOKEN_RBRACE) {
            --info[ln].brace_delta;
        }
    }

    // Second pass: unbraced control-flow keywords
    for (size_t i = 0; i < stream.len; ++i) {
        uint64_t ln = stream.data[i].loc.line;
        if (ln > line_count) continue;
        if (!is_ctrl_keyword(stream.data[i].type))
            continue;
        // Look for '{' anywhere on the same line
        bool has_brace = false;
        for (size_t j = 0; j < stream.len && stream.data[j].loc.line == ln; ++j) {
            if (stream.data[j].type == ZITH_TOKEN_LBRACE) {
                has_brace = true;
                break;
            }
        }
        if (!has_brace)
            info[ln].has_ctrl_kw = true;
    }

    // -- Compute brace-based indent levels --------------------------------
    std::vector<int> indent_at(line_count + 2, 0);
    int indent = 0;
    for (size_t ln = 1; ln <= line_count; ++ln) {
        indent_at[ln] = indent;
        indent += info[ln].brace_delta;
        if (indent < 0) indent = 0;
    }

    // -- Reconstruct with normalised content ------------------------------
    std::string out;
    out.reserve(src.size() + 64);
    std::istringstream lines(src);
    std::string line;
    size_t ln = 1;
    int extra = 0; // extra indent for next non-empty line (unbraced ctrl flow)

    while (std::getline(lines, line)) {
        size_t first = line.find_first_not_of(" \t\r");

        // Empty lines: just emit newline, do NOT consume 'extra'
        if (first == std::string::npos) {
            out += '\n';
            ++ln;
            continue;
        }

        std::string content = normalise_line(line.substr(first));

        // Apply extra indent from an unbraced control-flow keyword on the
        // previous line, then consume it.
        int line_indent = indent_at[ln] + extra;
        const int consumed_extra = extra;
        extra = 0;

        // Lines starting with '}' are at indent-1
        if (line[first] == '}')
            line_indent = std::max(0, line_indent - 1);

        // If this line is an unbraced if/for/else whose body starts on the
        // next line, accumulate extra indent for the body.
        if (info[ln].has_ctrl_kw) {
            const bool body_on_same =
                !content.empty() &&
                (content.back() == ';' || content.back() == '{' || content.back() == '}' || content[0] == '{');
            if (!body_on_same)
                extra = consumed_extra + 1;
        }

        for (int i = 0; i < line_indent; ++i)
            out += "    ";
        out += content;
        out += '\n';

        ++ln;
    }

    if (!out.empty() && out.back() != '\n')
        out += '\n';

    // -- Check-only or write-back -----------------------------------------
    if (check_only) {
        bool pass = (src == out);
        if (!pass && !src.empty() && src.back() != '\n')
            pass = (src + '\n' == out);
        if (pass) {
            pipeline::print_success("Format check passed", input_file);
            zith_arena_destroy(arena);
            return 0;
        }
        pipeline::print_error("File " + input_file + " is not formatted correctly");
        zith_arena_destroy(arena);
        return 1;
    }

    std::ofstream ofs(input_file);
    if (!ofs) {
        pipeline::print_error("Failed to write: " + input_file);
        zith_arena_destroy(arena);
        return 1;
    }
    ofs << out;
    pipeline::print_success("Formatted", input_file);
    zith_arena_destroy(arena);
    return 0;
}

int cmd_docs(const std::string & /*input_file*/, const std::string & /*output_dir*/,
             bool /*verbose*/) {
    pipeline::print_not_implemented("docs");
    return 1;
}

} // namespace zith::cli::commands

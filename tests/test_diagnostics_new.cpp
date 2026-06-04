#include "diagnostics/diagnostics.hpp"
#include "diagnostics/format.hpp"
#include "diagnostics/builder.hpp"
#include "diagnostics/diagnostic-bag.hpp"
#include "diagnostics/emitter.hpp"
#include "diagnostics/heuristic-engine.hpp"
#include "diagnostics/span.hpp"

#include "test.h"
#include <cstdio>
#include <cstring>
#include <string>

using namespace zith::diag;

static SourceMap make_test_map() {
    SourceMap sm;
    sm.add_file("test.zith",
        "fn add(a: i32, b: i32) -> i32 {\n"
        "    return a + b;\n"
        "}\n"
        "\n"
        "fn main() {\n"
        "    let x = 42;\n"
        "    x = 99;\n"
        "    let y: String = 42;\n"
        "    println(z);\n"
        "}\n");
    return sm;
}

TEST_CASE("SourceMap builds line index correctly", "[diag][core]") {
    SourceMap sm;
    FileId fid = sm.add_file("test.zith",
        "line1\n"
        "line2\n"
        "line3");

    const SourceFile* file = sm.lookup(fid);
    REQUIRE(file != nullptr);
    REQUIRE(file->line_count() == 3);
    REQUIRE(file->line_text(1) == "line1");
    REQUIRE(file->line_text(2) == "line2");
    REQUIRE(file->line_text(3) == "line3");
}

TEST_CASE("SourceSpan merge and containment", "[diag][core]") {
    SourceMap sm;
    FileId fid = sm.add_file("test.zith", "abcdefghijklmnop");

    SourceSpan inner = {{2, 1}, {4, 1}, fid};
    SourceSpan outer = {{0, 1}, {10, 1}, fid};

    REQUIRE(outer.contains(inner));
    REQUIRE_FALSE(inner.contains(outer));
}

TEST_CASE("DiagCode string conversion", "[diag][core]") {
    REQUIRE(std::string(diag_code_string(DiagCode::MissingSemicolon)) == "E0001");
    REQUIRE(std::string(diag_code_string(DiagCode::TypeMismatch)) == "E0201");
    REQUIRE(std::string(diag_code_string(DiagCode::ImmutableAssignment)) == "E0301");
    REQUIRE(std::string(diag_code_string(DiagCode::UnusedVariable)) == "W0001");
    REQUIRE(std::string(diag_code_string(DiagCode::InternalCompilerError)) == "I0001");
}

TEST_CASE("DiagLevel ordering", "[diag][core]") {
    REQUIRE(Diagnostic{}.priority_order() == 2);
    Diagnostic fatal;   fatal.level = DiagLevel::Fatal;
    Diagnostic warning; warning.level = DiagLevel::Warning;
    Diagnostic note;    note.level = DiagLevel::Note;
    Diagnostic help;    help.level = DiagLevel::Help;

    REQUIRE(fatal.priority_order() < warning.priority_order());
    REQUIRE(warning.priority_order() < note.priority_order());
    REQUIRE(note.priority_order() < help.priority_order());
}

TEST_CASE("DiagnosticBuilder produces a complete Diagnostic", "[diag][builder]") {
    DiagnosticBag bag;
    SourceMap sm = make_test_map();
    FileId fid = 0;

    SourceSpan span = {{5, 5}, {5, 6}, fid};

    DiagnosticBuilder(DiagLevel::Error, DiagCode::ImmutableAssignment, span)
        .with_message("cannot assign to immutable binding `x`")
        .with_secondary_span({{4, 8}, {4, 9}, fid}, "`x` declared immutable here")
        .with_suggestion({{4, 0}, {4, 3}, fid}, "var ", "use `var` instead of `let`", true)
        .with_note("bindings with `let` are immutable by default")
        .with_help("use `var x = ...` to declare a mutable binding")
        .emit(bag);

    REQUIRE(bag.error_count() == 1);
    REQUIRE(bag.total_count() == 1);

    const auto& diags = bag.diagnostics();
    const auto& d = diags[0];

    REQUIRE(d.level == DiagLevel::Error);
    REQUIRE(d.code == DiagCode::ImmutableAssignment);
    REQUIRE(d.message.get() == "cannot assign to immutable binding `x`");
    REQUIRE(d.has_primary_span);
    REQUIRE(d.primary_span.start.line == 5);
    REQUIRE(d.primary_span.start.index == 5);
    REQUIRE(d.secondary_spans.size() == 1);
    REQUIRE(d.suggestions.size() == 1);
    REQUIRE(d.suggestions[0].is_machine_applicable == true);
    REQUIRE(d.suggestions[0].replacement == "var ");
    REQUIRE(d.children.size() == 2);
}

TEST_CASE("DiagnosticBuilder with positional arguments", "[diag][builder]") {
    DiagnosticBag bag;

    DiagnosticBuilder(DiagLevel::Error, DiagCode::UndefinedIdentifier)
        .with_message("undefined identifier `{}`", "foo")
        .with_help("did you mean `{}`?", "foobar")
        .emit(bag);

    const auto& diags = bag.diagnostics();
    REQUIRE(diags.size() == 1);
    REQUIRE(diags[0].message.get() == "undefined identifier `foo`");

    const auto& children = diags[0].children;
    REQUIRE(children.size() == 1);
    REQUIRE(children[0]->message.get() == "did you mean `foobar`?");
}

TEST_CASE("Cascading errors are suppressed", "[diag][suppression]") {
    DiagnosticBag bag;
    SourceMap sm = make_test_map();
    FileId fid = 0;

    SourceSpan def_span = {{0, 0}, {0, 20}, fid};
    DiagnosticBuilder(DiagLevel::Error, DiagCode::UndefinedType, def_span)
        .with_message("undefined type `Foo`")
        .emit(bag);

    SourceSpan use_span = {{0, 14}, {0, 17}, fid};
    DiagnosticBuilder(DiagLevel::Error, DiagCode::TypeMismatch, use_span)
        .with_message("cannot use `Foo` as a parameter type")
        .with_cause(DiagCode::UndefinedType)
        .emit(bag);

    bag.finalize();

    const auto& diags = bag.diagnostics();
    REQUIRE(diags.size() == 1);
    REQUIRE(diags[0].code == DiagCode::UndefinedType);

    bool found_cascade = false;
    for (const auto& child : diags[0].children) {
        if (child->message.get().find("cannot use") != std::string::npos) {
            found_cascade = true;
            break;
        }
    }
    REQUIRE(found_cascade);
}

TEST_CASE("Duplicate errors are aggregated", "[diag][aggregation]") {
    DiagnosticBag bag;
    SourceMap sm = make_test_map();
    FileId fid = 0;

    for (int i = 0; i < 5; ++i) {
        DiagnosticBuilder(DiagLevel::Error, DiagCode::UndefinedIdentifier)
            .with_message("undefined identifier `z`")
            .with_span({{static_cast<size_t>(7 + i), 5}, {static_cast<size_t>(8 + i), 5}, fid})
            .emit(bag);
    }

    bag.finalize();

    const auto& diags = bag.diagnostics();
    REQUIRE(diags.size() == 1);
    REQUIRE(diags[0].code == DiagCode::UndefinedIdentifier);

    bool found_repeat_note = false;
    for (const auto& child : diags[0].children) {
        if (child->message.get().find("repeats") != std::string::npos) {
            found_repeat_note = true;
            break;
        }
    }
    REQUIRE(found_repeat_note);
}

TEST_CASE("Diagnostics are sorted by severity then position", "[diag][sorting]") {
    DiagnosticBag bag;
    SourceMap sm = make_test_map();
    FileId fid = 0;

    DiagnosticBuilder(DiagLevel::Warning, DiagCode::UnusedVariable)
        .with_message("unused variable `y`")
        .with_span({{2, 8}, {2, 9}, fid})
        .emit(bag);

    DiagnosticBuilder(DiagLevel::Error, DiagCode::ImmutableAssignment)
        .with_message("cannot assign to immutable binding")
        .with_span({{5, 4}, {5, 5}, fid})
        .emit(bag);

    DiagnosticBuilder(DiagLevel::Warning, DiagCode::ShadowedName)
        .with_message("shadowed name `x`")
        .with_span({{1, 8}, {1, 9}, fid})
        .emit(bag);

    bag.finalize();

    const auto& sorted = bag.sorted_view();
    REQUIRE(sorted.size() == 3);

    REQUIRE(sorted[0]->level == DiagLevel::Error);
    REQUIRE(sorted[0]->code == DiagCode::ImmutableAssignment);

    REQUIRE(sorted[1]->level == DiagLevel::Warning);
    REQUIRE(sorted[2]->level == DiagLevel::Warning);
}

TEST_CASE("TerminalEmitter produces expected output format", "[diag][emitter]") {
    DiagnosticBag bag;
    SourceMap sm = make_test_map();
    FileId fid = 0;

    // ZithSourceLoc is {index, line}. This span targets line 5, index 4.
    SourceSpan span = {{4, 5}, {5, 5}, fid};

    DiagnosticBuilder(DiagLevel::Error, DiagCode::ImmutableAssignment, span)
        .with_message("cannot assign to immutable binding `x`")
        .with_secondary_span({{8, 4}, {9, 4}, fid}, "declared immutable here")
        .with_suggestion({{0, 4}, {3, 4}, fid}, "var x = 42", "use `var` instead of `let`", true)
        .with_note("bindings with `let` are immutable by default")
        .with_help("use `var x = ...` to declare a mutable binding")
        .emit(bag);

    bag.finalize();

    TerminalEmitter emitter(&sm);

    FILE* f = tmpfile();
    REQUIRE(f != nullptr);

    emitter.emit(bag, f);
    rewind(f);

    char buf[4096];
    size_t nread = fread(buf, 1, sizeof(buf) - 1, f);
    buf[nread] = '\0';
    fclose(f);

    std::string_view output(buf, nread);

    REQUIRE(output.find("error") != std::string_view::npos);
    REQUIRE(output.find("[E0301]") != std::string_view::npos);
    REQUIRE(output.find("cannot assign to immutable binding") != std::string_view::npos);
    REQUIRE(output.find("test.zith") != std::string_view::npos);
    REQUIRE(output.find("5:4") != std::string_view::npos);
    REQUIRE(output.find("= note:") != std::string_view::npos);
    REQUIRE(output.find("= help:") != std::string_view::npos);
    REQUIRE(output.find("= suggestion:") != std::string_view::npos);
}

TEST_CASE("JsonEmitter produces valid JSON", "[diag][emitter]") {
    DiagnosticBag bag;
    SourceMap sm = make_test_map();
    FileId fid = 0;

    DiagnosticBuilder(DiagLevel::Error, DiagCode::TypeMismatch)
        .with_message("expected `i32`, found `String`")
        .with_span({{6, 14}, {6, 16}, fid})
        .with_note("cannot convert `String` to `i32`")
        .with_suggestion({{6, 0}, {6, 10}, fid}, "let y: String = \"42\"", "use a string literal", false)
        .emit(bag);

    bag.finalize();

    JsonEmitter emitter(&sm);
    FILE* f = tmpfile();
    REQUIRE(f != nullptr);

    emitter.emit(bag, f);
    rewind(f);

    char buf[4096];
    size_t nread = fread(buf, 1, sizeof(buf) - 1, f);
    buf[nread] = '\0';
    fclose(f);

    std::string_view output(buf, nread);

    REQUIRE(output.find("\"severity\"") != std::string_view::npos);
    REQUIRE(output.find("\"code\"") != std::string_view::npos);
    REQUIRE(output.find("\"E0201\"") != std::string_view::npos);
    REQUIRE(output.find("\"message\"") != std::string_view::npos);
    REQUIRE(output.find("\"file\"") != std::string_view::npos);
    REQUIRE(output.find("\"line\"") != std::string_view::npos);
    REQUIRE(output.find("\"column\"") != std::string_view::npos);
    REQUIRE(output.find("\"suggestions\"") != std::string_view::npos);
    REQUIRE(output.find("\"children\"") != std::string_view::npos);
    REQUIRE(output[0] == '[');
}

TEST_CASE("HeuristicEngine suggests close matches for undefined identifiers", "[diag][heuristic]") {
    HeuristicEngine engine;
    engine.set_known_names({"foo", "bar", "baz", "foobar", "hello"});

    Diagnostic diag;
    diag.level = DiagLevel::Error;
    diag.code = DiagCode::UndefinedIdentifier;
    diag.message = LazyMessage("undefined identifier `foob`");

    auto suggestions = engine.generate(diag);
    REQUIRE_FALSE(suggestions.empty());
    REQUIRE(suggestions[0].label.find("foo") != std::string::npos);
}

TEST_CASE("HeuristicEngine gives immutable assignment hints", "[diag][heuristic]") {
    HeuristicEngine engine;

    Diagnostic diag;
    diag.level = DiagLevel::Error;
    diag.code = DiagCode::ImmutableAssignment;
    diag.message = LazyMessage("cannot assign to immutable binding `x`");

    auto suggestions = engine.generate(diag);
    REQUIRE(suggestions.size() >= 1);
    REQUIRE(suggestions[0].label.find("var") != std::string::npos);
}

TEST_CASE("HeuristicEngine suggests semicolon fix", "[diag][heuristic]") {
    HeuristicEngine engine;

    Diagnostic diag;
    diag.level = DiagLevel::Error;
    diag.code = DiagCode::MissingSemicolon;
    diag.message = LazyMessage("expected `;`");

    auto suggestions = engine.generate(diag);
    REQUIRE(suggestions.size() >= 1);
    REQUIRE(suggestions[0].is_machine_applicable == true);
    REQUIRE(suggestions[0].label.find(";") != std::string::npos);
}

TEST_CASE("Full pipeline: build -> suppress -> sort -> emit", "[diag][e2e]") {
    DiagnosticBag bag;
    SourceMap sm = make_test_map();
    FileId fid = 0;

    DiagnosticBuilder(DiagLevel::Error, DiagCode::TypeMismatch)
        .with_message("expected `i32`, found `String`")
        .with_span({{6, 14}, {6, 16}, fid})
        .emit(bag);

    DiagnosticBuilder(DiagLevel::Error, DiagCode::UndefinedType)
        .with_message("undefined type `Foo`")
        .with_span({{0, 5}, {0, 20}, fid})
        .emit(bag);

    DiagnosticBuilder(DiagLevel::Error, DiagCode::TypeMismatch)
        .with_message("cannot use `Foo` as parameter type")
        .with_span({{1, 14}, {1, 17}, fid})
        .with_cause(DiagCode::UndefinedType)
        .emit(bag);

    DiagnosticBuilder(DiagLevel::Warning, DiagCode::UnusedVariable)
        .with_message("unused variable `y`")
        .with_span({{7, 8}, {7, 9}, fid})
        .emit(bag);

    DiagnosticBuilder(DiagLevel::Error, DiagCode::ImmutableAssignment)
        .with_message("cannot assign to immutable binding `x`")
        .with_span({{5, 4}, {5, 5}, fid})
        .with_note("`x` was declared with `let`")
        .with_help("use `var x = 42` instead")
        .emit(bag);

    bag.finalize();

    REQUIRE(bag.error_count() == 3);

    const auto& sorted = bag.sorted_view();

    REQUIRE(sorted[0]->level == DiagLevel::Error);
    REQUIRE(sorted[1]->level == DiagLevel::Error);
    REQUIRE(sorted[2]->level == DiagLevel::Error);
    REQUIRE(sorted[3]->level == DiagLevel::Warning);

    const auto& undef_type = *sorted[1];
    REQUIRE(undef_type.code == DiagCode::UndefinedType);

    bool cascade_demoted = false;
    for (const auto& child : undef_type.children) {
        if (child->code == DiagCode::TypeMismatch &&
            child->message.get().find("cannot use") != std::string::npos) {
            cascade_demoted = true;
            break;
        }
    }
    REQUIRE(cascade_demoted);

    TerminalEmitter emitter(&sm);
    FILE* f = tmpfile();
    REQUIRE(f != nullptr);
    emitter.emit(bag, f);
    rewind(f);

    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);

    std::string_view out(buf, n);

    REQUIRE(out.find("[E0201]") != std::string_view::npos);
    REQUIRE(out.find("[E0204]") != std::string_view::npos);
    REQUIRE(out.find("[E0301]") != std::string_view::npos);
    REQUIRE(out.find("[W0001]") != std::string_view::npos);
}

TEST_CASE("LazyMessage evaluates only once", "[diag][lazy]") {
    int eval_count = 0;

    LazyMessage msg([&]() -> std::string {
        ++eval_count;
        return "expensive computation";
    });

    REQUIRE(eval_count == 0);
    REQUIRE(msg.get() == "expensive computation");
    REQUIRE(eval_count == 1);
    REQUIRE(msg.get() == "expensive computation");
    REQUIRE(eval_count == 1);
}

TEST_CASE("DiagnosticBag caps at MAX_DIAGNOSTICS", "[diag][cap]") {
    DiagnosticBag bag;
    SourceMap sm = make_test_map();
    FileId fid = 0;

    for (size_t i = 0; i < MAX_DIAGNOSTICS + 10; ++i) {
        DiagnosticBuilder(DiagLevel::Error, DiagCode::UnexpectedToken)
            .with_message("unexpected token")
            .with_span({{0, 0}, {0, 1}, fid})
            .emit(bag);
    }

    bag.finalize();
    REQUIRE(bag.total_count() <= MAX_DIAGNOSTICS);
    REQUIRE(bag.error_count() <= MAX_DIAGNOSTICS);
}

TEST_CASE("C ABI bridge works correctly", "[diag][cabi]") {
    ZithDiagBag* bag = zith_diag_bag_create();
    REQUIRE(bag != nullptr);

    ZithSourceSpan span = {};
    span.start.line = 1;
    span.start.index = 5;
    span.end.line = 1;
    span.end.index = 8;

    zith_diag_bag_emit(bag, "test error message", span, 2, 0x0001);
    zith_diag_bag_finalize(bag);

    REQUIRE(zith_diag_bag_had_errors(bag));
    REQUIRE(zith_diag_bag_error_count(bag) == 1);

    zith_diag_bag_destroy(bag);
}

TEST_MAIN()

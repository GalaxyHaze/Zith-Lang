#include "diagnostics/diagnostics.hpp"
#include "diagnostics/format.hpp"
#include "diagnostics/builder.hpp"
#include "diagnostics/diagnostic_bag.hpp"
#include "diagnostics/emitter.hpp"
#include "diagnostics/heuristic_engine.hpp"
#include "diagnostics/span.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

using namespace zith::diag;

// ============================================================================
// Helpers
// ============================================================================

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

// ============================================================================
// 1. Core Data Structures
// ============================================================================

TEST_CASE("SourceMap builds line index correctly", "[diag][core]") {
    SourceMap sm;
    FileId fid = sm.add_file("test.zith",
        "line1\n"
        "line2\n"
        "line3\n");

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
    REQUIRE(Diagnostic{}.priority_order() == 2); // Error default
    Diagnostic fatal;   fatal.level = DiagLevel::Fatal;
    Diagnostic warning; warning.level = DiagLevel::Warning;
    Diagnostic note;    note.level = DiagLevel::Note;
    Diagnostic help;    help.level = DiagLevel::Help;

    REQUIRE(fatal.priority_order() < warning.priority_order());
    REQUIRE(warning.priority_order() < note.priority_order());
    REQUIRE(note.priority_order() < help.priority_order());
}

// ============================================================================
// 2. DiagnosticBuilder (Fluent API)
// ============================================================================

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

// ============================================================================
// 3. Cascading Suppression
// ============================================================================

TEST_CASE("Cascading errors are suppressed", "[diag][suppression]") {
    DiagnosticBag bag;
    SourceMap sm = make_test_map();
    FileId fid = 0;

    // Primary error: undefined type `Foo`
    SourceSpan def_span = {{0, 3}, {0, 6}, fid};
    DiagnosticBuilder(DiagLevel::Error, DiagCode::UndefinedType, def_span)
        .with_message("undefined type `Foo`")
        .emit(bag);

    // Cascading error: using Foo in a function signature
    SourceSpan use_span = {{1, 14}, {1, 17}, fid};
    DiagnosticBuilder(DiagLevel::Error, DiagCode::TypeMismatch, use_span)
        .with_message("cannot use `Foo` as a parameter type")
        .with_cause(DiagCode::UndefinedType)
        .emit(bag);

    bag.finalize();

    // The cascading error should be demoted to a note under the primary error
    const auto& diags = bag.diagnostics();
    REQUIRE(diags.size() == 1); // Only the primary error remains
    REQUIRE(diags[0].code == DiagCode::UndefinedType);

    // The cascading error should now be a child note
    bool found_cascade = false;
    for (const auto& child : diags[0].children) {
        if (child->message.get().find("cannot use") != std::string::npos) {
            found_cascade = true;
            break;
        }
    }
    REQUIRE(found_cascade);
}

// ============================================================================
// 4. Aggregate Duplicates
// ============================================================================

TEST_CASE("Duplicate errors are aggregated", "[diag][aggregation]") {
    DiagnosticBag bag;
    SourceMap sm = make_test_map();
    FileId fid = 0;

    // Emit 5 identical errors
    for (int i = 0; i < 5; ++i) {
        DiagnosticBuilder(DiagLevel::Error, DiagCode::UndefinedIdentifier)
            .with_message("undefined identifier `z`")
            .with_span({{static_cast<size_t>(7 + i), 5}, {static_cast<size_t>(8 + i), 5}, fid})
            .emit(bag);
    }

    bag.finalize();

    // Should be aggregated to 1 error + note about repetitions
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

// ============================================================================
// 5. Sorting
// ============================================================================

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

    // Errors should come first, sorted by position
    REQUIRE(sorted[0]->level == DiagLevel::Error);
    REQUIRE(sorted[0]->code == DiagCode::ImmutableAssignment);

    // Then warnings, sorted by position
    REQUIRE(sorted[1]->level == DiagLevel::Warning);
    REQUIRE(sorted[2]->level == DiagLevel::Warning);
}

// ============================================================================
// 6. TerminalEmitter Output
// ============================================================================

TEST_CASE("TerminalEmitter produces expected output format", "[diag][emitter]") {
    DiagnosticBag bag;
    SourceMap sm = make_test_map();
    FileId fid = 0;

    SourceSpan span = {{5, 4}, {5, 5}, fid};

    DiagnosticBuilder(DiagLevel::Error, DiagCode::ImmutableAssignment, span)
        .with_message("cannot assign to immutable binding `x`")
        .with_secondary_span({{4, 8}, {4, 9}, fid}, "declared immutable here")
        .with_suggestion({{4, 0}, {4, 3}, fid}, "var x = 42", "use `var` instead of `let`", true)
        .with_note("bindings with `let` are immutable by default")
        .with_help("use `var x = ...` to declare a mutable binding")
        .emit(bag);

    bag.finalize();

    // Capture emitter output
    TerminalEmitter emitter(&sm);

    // Use a temporary file for capture
    FILE* f = tmpfile();
    REQUIRE(f != nullptr);

    emitter.emit(bag, f);
    rewind(f);

    char buf[4096];
    size_t nread = fread(buf, 1, sizeof(buf) - 1, f);
    buf[nread] = '\0';
    fclose(f);

    std::string_view output(buf, nread);

    // Check key format elements
    REQUIRE(output.find("error") != std::string_view::npos);
    REQUIRE(output.find("[E0301]") != std::string_view::npos);
    REQUIRE(output.find("cannot assign to immutable binding") != std::string_view::npos);
    REQUIRE(output.find("test.zith") != std::string_view::npos);
    REQUIRE(output.find("5:4") != std::string_view::npos);
    REQUIRE(output.find("= note:") != std::string_view::npos);
    REQUIRE(output.find("= help:") != std::string_view::npos);
    REQUIRE(output.find("= suggestion:") != std::string_view::npos);
}

// ============================================================================
// 7. JsonEmitter Output
// ============================================================================

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

    // Check JSON structure
    REQUIRE(output.find("\"severity\"") != std::string_view::npos);
    REQUIRE(output.find("\"code\"") != std::string_view::npos);
    REQUIRE(output.find("\"E0201\"") != std::string_view::npos);
    REQUIRE(output.find("\"message\"") != std::string_view::npos);
    REQUIRE(output.find("\"file\"") != std::string_view::npos);
    REQUIRE(output.find("\"line\"") != std::string_view::npos);
    REQUIRE(output.find("\"column\"") != std::string_view::npos);
    REQUIRE(output.find("\"suggestions\"") != std::string_view::npos);
    REQUIRE(output.find("\"children\"") != std::string_view::npos);
    REQUIRE(output[0] == '['); // JSON array
}

// ============================================================================
// 8. HeuristicEngine Suggestions
// ============================================================================

TEST_CASE("HeuristicEngine suggests close matches for undefined identifiers", "[diag][heuristic]") {
    HeuristicEngine engine;
    engine.set_known_names({"foo", "bar", "baz", "foobar", "hello"});

    Diagnostic diag;
    diag.level = DiagLevel::Error;
    diag.code = DiagCode::UndefinedIdentifier;
    diag.message = LazyMessage("undefined identifier `foob`");

    auto suggestions = engine.generate(diag);
    REQUIRE_FALSE(suggestions.empty());
    REQUIRE(suggestions[0].label.find("foobar") != std::string::npos);
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

// ============================================================================
// 9. End-to-End: Full Diagnostic Pipeline
// ============================================================================

TEST_CASE("Full pipeline: build -> suppress -> sort -> emit", "[diag][e2e]") {
    DiagnosticBag bag;
    SourceMap sm = make_test_map();
    FileId fid = 0;

    // Simulate a compile session with mixed errors
    // 1. A real type error (primary)
    DiagnosticBuilder(DiagLevel::Error, DiagCode::TypeMismatch)
        .with_message("expected `i32`, found `String`")
        .with_span({{6, 14}, {6, 16}, fid})
        .emit(bag);

    // 2. A cascade: use of undefined type Foo (caused by earlier type error)
    DiagnosticBuilder(DiagLevel::Error, DiagCode::UndefinedType)
        .with_message("undefined type `Foo`")
        .with_span({{1, 8}, {1, 11}, fid})
        .emit(bag);

    // 3. Another cascade
    DiagnosticBuilder(DiagLevel::Error, DiagCode::TypeMismatch)
        .with_message("cannot use `Foo` as parameter type")
        .with_span({{1, 14}, {1, 17}, fid})
        .with_cause(DiagCode::UndefinedType)
        .emit(bag);

    // 4. A real warning
    DiagnosticBuilder(DiagLevel::Warning, DiagCode::UnusedVariable)
        .with_message("unused variable `y`")
        .with_span({{7, 8}, {7, 9}, fid})
        .emit(bag);

    // 5. A real error (immutable assignment)
    DiagnosticBuilder(DiagLevel::Error, DiagCode::ImmutableAssignment)
        .with_message("cannot assign to immutable binding `x`")
        .with_span({{5, 4}, {5, 5}, fid})
        .with_note("`x` was declared with `let`")
        .with_help("use `var x = 42` instead")
        .emit(bag);

    bag.finalize();

    // Verify suppression worked
    REQUIRE(bag.error_count() == 3); // TypeMismatch, UndefinedType, ImmutableAssignment

    const auto& sorted = bag.sorted_view();

    // Errors come first
    REQUIRE(sorted[0]->level == DiagLevel::Error);
    REQUIRE(sorted[1]->level == DiagLevel::Error);
    REQUIRE(sorted[2]->level == DiagLevel::Error);

    // Warning last
    REQUIRE(sorted[3]->level == DiagLevel::Warning);

    // The cascading error (cannot use Foo) should have been demoted
    // to a child of the UndefinedType error
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

    // Output should also work cleanly
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
    REQUIRE(out.find("error[E0201]") != std::string_view::npos);
    REQUIRE(out.find("error[E0102]") != std::string_view::npos);
    REQUIRE(out.find("error[E0301]") != std::string_view::npos);
    REQUIRE(out.find("warning[W0001]") != std::string_view::npos);
}

// ============================================================================
// 10. LazyMessage Evaluation
// ============================================================================

TEST_CASE("LazyMessage evaluates only once", "[diag][lazy]") {
    int eval_count = 0;

    LazyMessage msg([&]() -> std::string {
        ++eval_count;
        return "expensive computation";
    });

    REQUIRE(eval_count == 0); // Not yet evaluated
    REQUIRE(msg.get() == "expensive computation");
    REQUIRE(eval_count == 1);
    REQUIRE(msg.get() == "expensive computation");
    REQUIRE(eval_count == 1); // Cached
}

// ============================================================================
// 11. Cap Limit
// ============================================================================

TEST_CASE("DiagnosticBag caps at MAX_DIAGNOSTICS", "[diag][cap]") {
    DiagnosticBag bag;
    SourceMap sm = make_test_map();
    FileId fid = 0;

    // Emit more than MAX_DIAGNOSTICS
    for (size_t i = 0; i < MAX_DIAGNOSTICS + 10; ++i) {
        DiagnosticBuilder(DiagLevel::Error, DiagCode::UnexpectedToken)
            .with_message("unexpected token")
            .with_span({{0, 0}, {0, 1}, fid})
            .emit(bag);
    }

    // Should cap at MAX_DIAGNOSTICS
    bag.finalize();
    REQUIRE(bag.total_count() <= MAX_DIAGNOSTICS);
    REQUIRE(bag.error_count() <= MAX_DIAGNOSTICS);
}

// ============================================================================
// 12. C ABI Bridge
// ============================================================================

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

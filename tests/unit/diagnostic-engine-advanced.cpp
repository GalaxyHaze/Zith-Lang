#include "../test-common.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "diagnostics/error-codes.hpp"
#include "memory/arena.hpp"
#include "memory/span.hpp"

#include <cstdio>
#include <string>

using namespace zith::diagnostics;
using zith::memory::Arena;
using zith::memory::FileId;
using zith::memory::Span;

static Span makeSpan(uint32_t start = 0, uint32_t end = 0, FileId file = FileId{0}) {
    return Span{file, start, end};
}

static void test_report_and_all() {
    Arena arena;
    DiagnosticEngine de(arena);

    CHECK(de.all().empty(), "empty engine has no diagnostics");
    CHECK(!de.hasErrors(), "empty engine has no errors");

    de.report(Severity::Error, err::UnknownToken, "bad token", makeSpan(0, 5));
    CHECK_EQ(de.all().size(), size_t(1), "one diagnostic after report");
    CHECK(de.hasErrors(), "error reports cause hasErrors");
}

static void test_report_warning() {
    Arena arena;
    DiagnosticEngine de(arena);

    de.report(Severity::Warning, err::UnusedDecl, "unused x", makeSpan());
    CHECK_EQ(de.all().size(), size_t(1), "warning stored");
    CHECK(!de.hasErrors(), "warnings are not errors");
}

static void test_report_bug() {
    Arena arena;
    DiagnosticEngine de(arena);

    de.report(Severity::Bug, err::InvalidIR, "internal error", makeSpan());
    CHECK(de.hasErrors(), "Bug severity is an error");
}

static void test_report_note() {
    Arena arena;
    DiagnosticEngine de(arena);

    de.report(Severity::Note, err::UnknownToken, "hint", makeSpan());
    CHECK(!de.hasErrors(), "Note is not an error");
}

static void test_color_toggle() {
    Arena arena;
    DiagnosticEngine de(arena);

    CHECK(!de.useColor(), "color defaults to off");
    de.setColor(true);
    CHECK(de.useColor(), "color enabled");
    de.setColor(false);
    CHECK(!de.useColor(), "color disabled");
}

static void test_clear() {
    Arena arena;
    DiagnosticEngine de(arena);

    de.report(Severity::Error, err::UnknownToken, "e1", makeSpan());
    de.report(Severity::Warning, err::UnusedDecl, "w1", makeSpan());
    CHECK_EQ(de.all().size(), size_t(2), "two diagnostics before clear");

    de.clear();
    CHECK(de.all().empty(), "empty after clear");
    CHECK(!de.hasErrors(), "no errors after clear");
}

static void test_clear_empty() {
    Arena arena;
    DiagnosticEngine de(arena);

    de.clear();
    CHECK(de.all().empty(), "clearing empty engine is safe");
}

static void test_set_source_map() {
    Arena arena;
    DiagnosticEngine de(arena);

    CHECK(de.sourceMap() == nullptr, "sourceMap defaults to null");
    // We can't easily create a real SourceMap here, so test the setter/getter
    de.setSourceMap(nullptr);
    CHECK(de.sourceMap() == nullptr, "null sourceMap stays null");
}

static void test_suppress_emit() {
    Arena arena;
    DiagnosticEngine de(arena);

    de.setSuppressEmit(true);
    de.report(Severity::Error, err::UnknownToken, "e1", makeSpan());
    // emit() should not crash when suppress is on
    de.emit();
    CHECK(true, "emit with suppress on does not crash");
}

static void test_emit_to() {
    Arena arena;
    DiagnosticEngine de(arena);

    de.report(Severity::Error, err::UnknownToken, "bad token", makeSpan(0, 5));
    de.report(Severity::Warning, err::UnusedDecl, "unused", makeSpan(10, 16));
    // emitTo with empty source text should not crash
    de.emitTo("");
    CHECK(true, "emitTo with empty source does not crash");
}

static void test_emit_to_with_source() {
    Arena arena;
    DiagnosticEngine de(arena);

    de.report(Severity::Error, err::UnknownToken, "bad ^", makeSpan(0, 1));
    de.emitTo("hello world");
    CHECK(true, "emitTo with real source does not crash");
}

static void test_report_diagnostic_struct() {
    Arena arena;
    DiagnosticEngine de(arena);

    Diagnostic d{Severity::Error, err::UnknownToken, "test msg", makeSpan(1, 3), {}, {}};
    de.report(std::move(d));

    CHECK_EQ(de.all().size(), size_t(1), "report from Diagnostic struct");
    auto &reported = de.all()[0];
    CHECK_EQ(reported.message, "test msg", "message preserved");
}

int main() {
    std::printf("diagnostic-engine-advanced tests\n");
    std::printf("================================\n\n");

    test_report_and_all();
    test_report_warning();
    test_report_bug();
    test_report_note();
    test_color_toggle();
    test_clear();
    test_clear_empty();
    test_set_source_map();
    test_suppress_emit();
    test_emit_to();
    test_emit_to_with_source();
    test_report_diagnostic_struct();

    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}

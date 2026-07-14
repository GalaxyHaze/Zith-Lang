#include "cli/options.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "diagnostics/error-codes.hpp"
#include "memory/arena.hpp"
#include "session/compilation-session.hpp"
#include "session/pipeline-plan.hpp"
#include "test-common.hpp"

#include <cstdio>
#include <string_view>

using namespace zith;

struct SemaTest {
    memory::Arena arena;
    Options opts;

    SemaTest() : opts(arena) {}

    struct Result {
        bool ok;
        size_t errorCount;
        bool hasErrorCode(diagnostics::ErrCode code) const {
            for (const auto &d : diags.all()) {
                if (d.severity == diagnostics::Severity::Error && d.code == code) {
                    return true;
                }
            }
            return false;
        }
        const diagnostics::DiagnosticEngine &diags;
    };

    Result run(std::string_view input, session::Stage target = session::Stage::TypeChecked) {
        session::CompilationSession session(opts, "test.zith");
        session.setBuffered(true);
        session.setContent(std::string(input));
        bool ok     = session.runTo(target);
        size_t errs = 0;
        for (const auto &d : session.diags().all()) {
            if (d.severity == diagnostics::Severity::Error) {
                errs++;
                std::printf("    [Diag] Code: %u, Message: %s\n", d.code, d.message.c_str());
            }
        }
        return {ok && errs == 0, errs, session.diags()};
    }
};

// ── Tests ────────────────────────────────────────────────────

static void test_basic_unification() {
    SemaTest t;
    auto r = t.run("fn main(): i32 {\n"
                   "    var x: i32 = 42;\n"
                   "    return x;\n"
                   "}\n");
    CHECK(r.ok, "Basic variable unification with return");
}

static void test_type_mismatch() {
    SemaTest t;
    auto r = t.run("fn main(): i32 {\n"
                   "    var x: i32 = true;\n"
                   "    return x;\n"
                   "}\n");
    CHECK(!r.ok, "Type mismatch on initialization fails");
    CHECK(r.hasErrorCode(diagnostics::err::TypeMismatch), "Reports TypeMismatch (3001)");
}

static void test_return_type_mismatch() {
    SemaTest t;
    auto r = t.run("fn main(): bool {\n"
                   "    return 42;\n"
                   "}\n");
    CHECK(!r.ok, "Return type mismatch fails");
    CHECK(r.hasErrorCode(diagnostics::err::TypeMismatch), "Reports TypeMismatch (3001) for return");
}

static void test_control_flow_ok() {
    SemaTest t;
    auto r = t.run("fn main(): bool {\n"
                   "    var x: i32 = 5;\n"
                   "    if (x < 10) {\n"
                   "        true\n"
                   "    } else {\n"
                   "        false\n"
                   "    }\n"
                   "}\n");
    CHECK(r.ok, "Control flow with compatible branches compiles");
}

static void test_undefined_identifier() {
    SemaTest t;
    auto r = t.run("fn main() {\n"
                   "    var x: i32 = y;\n"
                   "}\n");
    CHECK(!r.ok, "Reference to undefined identifier fails");
    CHECK(r.hasErrorCode(diagnostics::err::UndefinedIdent), "Reports UndefinedIdent (2001)");
}

static void test_wrong_arity() {
    SemaTest t;
    auto r = t.run("fn add(a: i32, b: i32): i32 {\n"
                   "    return a + b;\n"
                   "}\n"
                   "fn main() {\n"
                   "    var x: i32 = add(1);\n"
                   "}\n");
    CHECK(!r.ok, "Function call with incorrect arity fails");
    CHECK(r.hasErrorCode(diagnostics::err::NoMatchingFn), "Reports NoMatchingFn (2007)");
}

static void test_marker_jump_ok() {
    SemaTest t;
    auto r = t.run("fn main() {\n"
                   "    var x: i32 = 0;\n"
                   "    marker my_loop {\n"
                   "        x = x + 1;\n"
                   "        if (x < 10) {\n"
                   "            jump my_loop;\n"
                   "        }\n"
                   "    }\n"
                   "}\n",
                   session::Stage::HirLowered);
    CHECK(r.ok, "Valid marker and jump setup lowers successfully");
}

static void test_marker_jump_undefined() {
    SemaTest t;
    auto r = t.run("fn main() {\n"
                   "    jump nonexistent_label;\n"
                   "}\n");
    CHECK(!r.ok, "Jump to undefined marker fails");
    CHECK(r.hasErrorCode(diagnostics::err::UndefinedIdent), "Reports UndefinedIdent (2001)");
}

static void test_extern_fn_call_ok() {
    SemaTest t;
    auto r = t.run("extern fn putchar(c: i32): i32\n"
                   "fn main() {\n"
                   "    putchar(65);\n"
                   "}\n");
    CHECK(r.ok, "Calling extern function with correct argument type works");
}

static void test_extern_fn_call_bad_arg() {
    SemaTest t;
    auto r = t.run("extern fn putchar(c: i32): i32\n"
                   "fn main() {\n"
                   "    putchar(true);\n"
                   "}\n");
    CHECK(!r.ok, "Calling extern function with incorrect argument type fails");
    CHECK(r.hasErrorCode(diagnostics::err::NoMatchingFn), "Reports NoMatchingFn (2007)");
}

static void test_type_alias_unification() {
    SemaTest t;
    auto r = t.run("alias MyInt = i32;\n"
                   "fn main(): MyInt {\n"
                   "    var x: MyInt = 100;\n"
                   "    var y: i32 = x;\n"
                   "    return y;\n"
                   "}\n");
    CHECK(r.ok, "Type alias unifies successfully with underlying primitive type");
}

static void test_type_alias_invalid_assignment() {
    SemaTest t;
    auto r = t.run("alias MyInt = i32;\n"
                   "fn main() {\n"
                   "    var x: MyInt = true;\n"
                   "}\n");
    CHECK(!r.ok, "Invalid assignment to type aliased variable fails");
    CHECK(r.hasErrorCode(diagnostics::err::TypeMismatch), "Reports TypeMismatch (3001)");
}

static void test_binary_op_type_error() {
    SemaTest t;
    auto r = t.run("fn main() {\n"
                   "    var x: i32 = 1 + true;\n"
                   "}\n");
    CHECK(!r.ok, "Addition between incompatible types fails");
    CHECK(r.hasErrorCode(diagnostics::err::TypeMismatch), "Reports TypeMismatch (3001)");
}

static void test_while_loop_ok() {
    SemaTest t;
    auto r = t.run("fn main() {\n"
                   "    var x: i32 = 0;\n"
                   "    while (x < 5) {\n"
                   "        x = x + 1;\n"
                   "    }\n"
                   "}\n");
    CHECK(r.ok, "While loop with boolean condition compiles");
}

static void test_unary_op_ok() {
    SemaTest t;
    auto r = t.run("fn main(): bool {\n"
                   "    var x: bool = true;\n"
                   "    return !x;\n"
                   "}\n");
    CHECK(r.ok, "Unary negation compiles");
}

static void test_sema() {
    test_basic_unification();
    test_type_mismatch();
    test_return_type_mismatch();
    test_control_flow_ok();
    test_undefined_identifier();
    test_wrong_arity();
    test_marker_jump_ok();
    test_marker_jump_undefined();
    test_extern_fn_call_ok();
    test_extern_fn_call_bad_arg();
    test_type_alias_unification();
    test_type_alias_invalid_assignment();
    test_binary_op_type_error();
    test_while_loop_ok();
    test_unary_op_ok();
}

TEST_MAIN(sema)

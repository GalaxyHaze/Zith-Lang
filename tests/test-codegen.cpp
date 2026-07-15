#include "cli/options.hpp"
#include "memory/arena.hpp"
#include "session/compilation-session.hpp"
#include "test-common.hpp"

#include <cstdio>
#include <string>
#include <string_view>

using namespace zith;

struct CodegenTest {
    memory::Arena arena;
    Options opts;

    CodegenTest() : opts(arena) {}

    struct Result {
        bool ok;
        size_t errorCount;
        int exitCode;
        std::string output;
    };

    Result run(std::string_view file_name, std::string_view input) {
        std::string path = std::string("/tmp/") + std::string(file_name);
        session::CompilationSession session(opts, path);
        session.setBuffered(true);
        session.setAlwaysEmitObject(true);
        session.setContent(std::string(input));

        bool ok     = session.run();
        size_t errs = 0;
        for (const auto &d : session.diags().all()) {
            if (d.severity == diagnostics::Severity::Error) {
                errs++;
                std::printf("    [Diag] Code: %u, Message: %s\n", d.code, d.message.c_str());
            }
        }

        if (ok && errs == 0)
            ok = session.linkAndExec();

        return {ok && errs == 0, errs, session.childExitCode(), session.flushOutput()};
    }
};

static void test_return_literal() {
    CodegenTest t;
    auto r = t.run("codegen-return-literal.zith",
                   "fn main(): i32 {\n"
                   "    return 7;\n"
                   "}\n");
    CHECK(r.ok, "Literal return reaches executable");
    CHECK_EQ(r.exitCode, 7, "Literal return value is preserved");
}

static void test_ref_deref_local() {
    CodegenTest t;
    auto r = t.run("codegen-ref-deref-local.zith",
                   "fn main(): i32 {\n"
                   "    var x: i32 = 41;\n"
                   "    var p: *i32 = &x;\n"
                   "    return *p;\n"
                   "}\n");
    CHECK(r.ok, "Taking a reference to a local and dereferencing compiles");
    CHECK_EQ(r.exitCode, 41, "Dereferencing a local pointer returns the pointed value");
}

static void test_ref_deref_param() {
    CodegenTest t;
    auto r = t.run("codegen-ref-deref-param.zith",
                   "fn reflect(x: i32): i32 {\n"
                   "    var p: *i32 = &x;\n"
                   "    return *p;\n"
                   "}\n"
                   "fn main(): i32 {\n"
                   "    return reflect(23);\n"
                   "}\n");
    CHECK(r.ok, "Taking a reference to a parameter compiles");
    CHECK_EQ(r.exitCode, 23, "Parameter references round-trip through memory");
}

static void test_pointer_parameter_call() {
    CodegenTest t;
    auto r = t.run("codegen-pointer-param-call.zith",
                   "fn load(p: *i32): i32 {\n"
                   "    return *p;\n"
                   "}\n"
                   "fn main(): i32 {\n"
                   "    var x: i32 = 19;\n"
                   "    return load(&x);\n"
                   "}\n");
    CHECK(r.ok, "Passing a reference as a pointer argument compiles");
    CHECK_EQ(r.exitCode, 19, "Pointer arguments dereference correctly in callees");
}

static void test_double_pointer_roundtrip() {
    CodegenTest t;
    auto r = t.run("codegen-double-pointer-roundtrip.zith",
                   "fn main(): i32 {\n"
                   "    var x: i32 = 9;\n"
                   "    var p: *i32 = &x;\n"
                   "    var pp: **i32 = &p;\n"
                   "    return **pp;\n"
                   "}\n");
    CHECK(r.ok, "Double indirection compiles");
    CHECK_EQ(r.exitCode, 9, "Double dereference returns the original value");
}

static void test_deref_ref_expression_chain() {
    CodegenTest t;
    auto r = t.run("codegen-deref-ref-expression-chain.zith",
                   "fn main(): i32 {\n"
                   "    var x: i32 = 12;\n"
                   "    return *&x;\n"
                   "}\n");
    CHECK(r.ok, "Immediate deref-ref chain compiles");
    CHECK_EQ(r.exitCode, 12, "Immediate deref-ref chain preserves the value");
}

static void test_unsigned_comparison() {
    CodegenTest t;
    auto r = t.run("codegen-unsigned-cmp.zith",
                   "fn main(): i32 {\n"
                   "    var x: u32 = 4294967295;\n"
                   "    var y: u32 = 1;\n"
                   "    if (x > y) { return 1; }\n"
                   "    return 0;\n"
                   "}\n");
    CHECK(r.ok, "Unsigned comparison compiles and runs");
    CHECK_EQ(r.exitCode, 1, "4294967295u32 > 1u32 (unsigned comparison must use UGT not SGT)");
}

static void test_forward_reference() {
    CodegenTest t;
    auto r = t.run("codegen-forward-ref.zith",
                   "fn main(): i32 {\n"
                   "    return helper();\n"
                   "}\n"
                   "fn helper(): i32 {\n"
                   "    return 42;\n"
                   "}\n");
    CHECK(r.ok, "Forward reference compiles and runs");
    CHECK_EQ(r.exitCode, 42, "Function defined after main is resolved");
}

static void test_codegen() {
    test_return_literal();
    test_ref_deref_local();
    test_ref_deref_param();
    test_pointer_parameter_call();
    test_double_pointer_roundtrip();
    test_deref_ref_expression_chain();
    test_unsigned_comparison();
    test_forward_reference();
}

TEST_MAIN(codegen)

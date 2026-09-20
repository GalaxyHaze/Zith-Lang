#include "cli/options.hpp"
#include "diagnostics/error-codes.hpp"
#include "session/compilation-session.hpp"
#include "session/frontend-context.hpp"
#include "test-common.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace zith;

namespace {

struct SessionRunner {
    memory::Arena arena;
    Options opts;
    std::filesystem::path root;

    SessionRunner()
        : opts(arena), root(std::filesystem::temp_directory_path() / "zith-stdlib-io-tests") {
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
        opts.targetStage = session::Stage::HirLowered;
#ifdef ZITH_STDLIB_DIR
        opts.includeDirs.push(ZITH_STDLIB_DIR);
#endif
    }

    ~SessionRunner() {
        std::filesystem::remove_all(root);
    }

    struct Result {
        bool ok = false;
        struct Diag {
            diagnostics::ErrCode code{};
            std::string message;
        };
        std::vector<Diag> diags;

        [[nodiscard]] bool hasErrorCode(diagnostics::ErrCode code) const {
            for (const auto &diag : diags) {
                if (diag.code == code)
                    return true;
            }
            return false;
        }

        [[nodiscard]] bool hasMessage(std::string_view needle) const {
            for (const auto &diag : diags) {
                if (diag.message.find(needle) != std::string::npos)
                    return true;
            }
            return false;
        }
    };

    Result run(std::string_view input, session::Stage target = session::Stage::HirLowered) {
        const auto path = root / "main.zith";
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << input;
        file.close();

        session::FrontendConfig config;
        config.workspaceRoot      = root.string();
        config.maxFrontendWorkers = 1;
        config.compilerVersion    = "test";
        for (const auto &dir : opts.includeDirs)
            config.includeRoots.push_back(dir);
        auto context = std::make_shared<session::FrontendContext>(config);
        session::CompilationSession session(opts, path.string(), std::move(context));
        session.setBuffered(true);

        const bool ok = session.runTo(target);
        Result result;
        result.ok = ok && session.diags().errorCount() == 0;
        for (const auto &diag : session.diags().all()) {
            result.diags.push_back({static_cast<diagnostics::ErrCode>(diag.code), diag.message});
            if (diag.severity == diagnostics::Severity::Error) {
                std::printf("    [StdlibIoDiag] Code: %u, Message: %s\n", diag.code,
                            diag.message.c_str());
            }
        }
        return result;
    }
};

void test_dyn_textsink_simple_receiver() {
    SessionRunner t;
    auto r = t.run("trait TextSink {\n"
                   "    fn capacity(self): u64;\n"
                   "    fn text(self): []char;\n"
                   "}\n"
                   "struct ByteBuffer {\n"
                   "    data: []char,\n"
                   "}\n"
                   "implement ByteBuffer as TextSink {\n"
                   "    fn capacity(self): u64 { return 0; }\n"
                   "    fn text(self): []char { return self.data; }\n"
                   "}\n"
                   "fn write(d: dyn TextSink) {\n"
                   "    let c = d.capacity();\n"
                   "}\n"
                   "fn main(): i32 {\n"
                   "    var b = ByteBuffer { data: \"\" };\n"
                   "    write(b);\n"
                   "    return 0;\n"
                   "}\n");
    CHECK(r.ok, "dyn TextSink with simple trait receivers compiles");
}

void test_qualified_trait_receiver_is_currently_blocked() {
    SessionRunner t;
    auto r = t.run("trait TextSink {\n"
                   "    fn capacity(self: view Self): u64;\n"
                   "}\n"
                   "struct ByteBuffer {\n"
                   "    data: []char,\n"
                   "}\n"
                   "implement ByteBuffer as TextSink {\n"
                   "    fn capacity(self: view Self): u64 { return 0; }\n"
                   "}\n"
                   "fn main(): i32 { return 0; }\n");
    CHECK(!r.ok, "qualified trait receiver remains a recorded compiler blocker");
    CHECK(r.hasErrorCode(diagnostics::err::TraitMethodSignatureMismatch),
          "qualified receiver fails with E2022");
}

void test_variadic_dyn_formatable_uses_bridge() {
    SessionRunner t;
    auto r = t.run("pub trait Formatable {\n"
                   "    fn format(self): []char;\n"
                   "}\n"
                   "struct S {}\n"
                   "implement S as Formatable {\n"
                   "    fn format(self): []char { return \"x\"; }\n"
                   "}\n"
                   "fn get(v: dyn Formatable): []char { return v.format(); }\n"
                   "fn writeValues(msg: []char, values: [...]dyn Formatable) {\n"
                   "    var i: u64 = 0;\n"
                   "    let n = @lengthOf(values);\n"
                   "    for (i < n) {\n"
                   "        let s = get(raw values[i]);\n"
                   "        i = i + 1;\n"
                   "    }\n"
                   "}\n"
                   "fn main(): i32 {\n"
                   "    writeValues(\"hello\", S{});\n"
                   "    return 0;\n"
                   "}\n");
    CHECK(r.ok, "variadic dyn Formatable call through a bridge compiles");
}

void test_generic_union_result_helper_blocked() {
    SessionRunner t;
    auto r = t.run("union Result<T, E> {\n"
                   "    T,\n"
                   "    E,\n"
                   "}\n"
                   "fn Ok<T, E>(value: T): Result<T, E> {\n"
                   "    var r: Result<T, E> = Result<T, E>{ value };\n"
                   "    return r;\n"
                   "}\n"
                   "fn main(): i32 {\n"
                   "    let a: Result<u32, u32> = Ok<u32, u32>(7);\n"
                   "    return 0;\n"
                   "}\n");
    CHECK(!r.ok, "generic Ok helper is currently blocked");
    CHECK(r.hasErrorCode(diagnostics::err::TypeMismatch) ||
              r.hasErrorCode(diagnostics::err::GenericCannotInfer),
          "blocked generic helper reports type/inference error");
}

void test_generic_union_direct_construction_blocked() {
    SessionRunner t;
    auto r = t.run("union Result<T, E> {\n"
                   "    T,\n"
                   "    E,\n"
                   "}\n"
                   "fn main(): i32 {\n"
                   "    var r: Result<u32, u32> = Result<u32, u32>{ 7 };\n"
                   "    let n: u32 = raw r as u32;\n"
                   "    return n as i32 - 7;\n"
                   "}\n");
    CHECK(r.ok, "direct generic union construction plus raw cast works");
}

} // namespace

int main() {
    std::printf("stdlib io tests\n");
    std::printf("===============\n\n");
    g_test_passed = 0;
    g_test_failed = 0;
    test_dyn_textsink_simple_receiver();
    test_qualified_trait_receiver_is_currently_blocked();
    test_variadic_dyn_formatable_uses_bridge();
    test_generic_union_result_helper_blocked();
    test_generic_union_direct_construction_blocked();
    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}

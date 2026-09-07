// Copyright (c) 2024-2026, Zith contributors
// SPDX-License-Identifier: Apache-2.0

#include "cinterop/c-header.hpp"

#include "test-common.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace zith::cinterop;

namespace {

// Ensure the temp directory exists and return its path.
std::string tempDir() {
    static const std::string dir = [] {
        const std::string path = "/tmp/zith-cinterop-tests";
        std::filesystem::create_directories(path);
        return path;
    }();
    return dir;
}

// Write content to a .h file under the temp dir and return its path.
std::string writeTempHeader(const std::string &filename, const std::string &content) {
    const std::string path = tempDir() + "/" + filename;
    std::ofstream out(path, std::ios::trunc);
    out << content;
    out.close();
    return path;
}

// ===== Block 1 — Tests without libclang (always runnable) =====

void test_without_libclang() {
    std::printf("=== Without libclang ===\n");

    // 1. available() matches ZITH_ENABLE_C_INTEROP
#ifdef ZITH_ENABLE_C_INTEROP
    CHECK(available(), "available() returns true when ZITH_ENABLE_C_INTEROP is defined");
#else
    CHECK(!available(), "available() returns false when ZITH_ENABLE_C_INTEROP is not defined");
#endif

    // 2. parseHeader on nonexistent file returns fallback diagnostic
    const auto artifact = parseHeader("nonexistent.h", {});
    CHECK(artifact != nullptr, "parseHeader returns non-null artifact");
    CHECK(artifact->headerPath == "nonexistent.h", "artifact preserves headerPath");
    CHECK(artifact->functions.empty(), "no functions when libclang unavailable");
    CHECK(!artifact->diagnostics.empty(), "diagnostics present when libclang unavailable");
    CHECK(artifact->diagnostics.front().message.find("libclang") != std::string::npos ||
              artifact->diagnostics.front().message.find("extern fn") != std::string::npos,
          "fallback diagnostic mentions libclang or extern fn");

    // 3. ParseOptions defaults are reasonable
    ParseOptions opts;
    CHECK(opts.targetTriple.empty(), "default targetTriple is empty");
    CHECK(opts.sysroot.empty(), "default sysroot is empty");
    CHECK(opts.includeDirs.empty(), "default includeDirs is empty");
    CHECK(opts.defines.empty(), "default defines is empty");
}

void test_system_include_dirs() {
    std::printf("=== systemIncludeDirs ===\n");

    const auto dirs = systemIncludeDirs("", "");
    CHECK(!dirs.empty(), "systemIncludeDirs returns at least one directory");
    bool all_exist = true;
    for (const auto &dir : dirs) {
        std::error_code error;
        if (!std::filesystem::is_directory(dir, error))
            all_exist = false;
    }
    CHECK(all_exist, "every reported system include directory exists");

    const auto again = systemIncludeDirs("", "");
    CHECK(again == dirs, "repeated calls return the memoised result");

#ifdef ZITH_ENABLE_C_INTEROP
    bool has_stdint = false;
    for (const auto &dir : dirs) {
        std::error_code error;
        if (std::filesystem::exists(std::filesystem::path(dir) / "stdint.h", error))
            has_stdint = true;
    }
    CHECK(has_stdint, "a discovered directory contains stdint.h");
#endif
}

#ifdef ZITH_ENABLE_C_INTEROP

// ===== Block 2 — Tests WITH libclang =====

// Helper: write a header and parse it, returning the artifact.
std::shared_ptr<const CHeaderArtifact> parseHeaderFromContent(const std::string &filename,
                                                              const std::string &content,
                                                              const ParseOptions &opts = {}) {
    return parseHeader(writeTempHeader(filename, content), opts);
}

// Helper: find a function by name.
const Function *findFunction(const CHeaderArtifact &artifact, const std::string &name) {
    for (const auto &f : artifact.functions) {
        if (f.name == name)
            return &f;
    }
    return nullptr;
}

const Constant *findConstant(const CHeaderArtifact &artifact, const std::string &name) {
    for (const auto &c : artifact.constants)
        if (c.name == name)
            return &c;
    return nullptr;
}

void test_scalar_types() {
    std::printf("=== Scalar types ===\n");

    // 4. void foo(void)
    {
        const auto art = parseHeaderFromContent("void_foo.h", "void foo(void);\n");
        CHECK(!art->diagnostics.empty() || art->functions.size() == 1,
              "void foo(void): 1 function");
        if (const auto *f = findFunction(*art, "foo")) {
            CHECK_EQ(static_cast<int>(f->result.kind), static_cast<int>(TypeKind::Void),
                     "void foo(void): result is Void");
            CHECK(f->parameters.empty(), "void foo(void): no parameters");
        }
    }

    // 5. int bar(float x, double y)
    {
        const auto art = parseHeaderFromContent("int_bar.h", "int bar(float x, double y);\n");
        CHECK(!art->diagnostics.empty() || art->functions.size() == 1,
              "int bar(float, double): 1 function");
        if (const auto *f = findFunction(*art, "bar")) {
            CHECK_EQ(static_cast<int>(f->result.kind), static_cast<int>(TypeKind::Integer),
                     "int bar: result is Integer");
            CHECK_EQ(f->result.bits, 32, "int bar: result is 32 bits");
            CHECK(f->result.isSigned, "int bar: result is signed");
            CHECK_EQ(f->parameters.size(), 2u, "int bar: 2 parameters");
            if (f->parameters.size() == 2) {
                CHECK_EQ(static_cast<int>(f->parameters[0].kind), static_cast<int>(TypeKind::Float),
                         "int bar: first param is Float (float)");
                CHECK_EQ(f->parameters[0].bits, 32, "int bar: float is 32 bits");
                CHECK_EQ(static_cast<int>(f->parameters[1].kind), static_cast<int>(TypeKind::Float),
                         "int bar: second param is Float (double)");
                CHECK_EQ(f->parameters[1].bits, 64, "int bar: double is 64 bits");
            }
        }
    }
}

void test_pointer_and_const() {
    std::printf("=== Pointers and const ===\n");

    // 6. void baz(int *p, const char *s)
    {
        const auto art =
            parseHeaderFromContent("ptr_const.h", "void baz(int *p, const char *s);\n");
        CHECK(!art->diagnostics.empty() || art->functions.size() == 1, "baz: 1 function");
        if (const auto *f = findFunction(*art, "baz")) {
            CHECK_EQ(f->parameters.size(), 2u, "baz: 2 parameters");
            if (f->parameters.size() == 2) {
                CHECK_EQ(static_cast<int>(f->parameters[0].kind),
                         static_cast<int>(TypeKind::Pointer),
                         "baz: first param is Pointer (int *)");
                CHECK(f->parameters[0].pointee != nullptr, "baz: int* has pointee");
                if (f->parameters[0].pointee) {
                    CHECK(!f->parameters[0].pointee->isConst, "baz: int* pointee is not const");
                }
                CHECK_EQ(static_cast<int>(f->parameters[1].kind),
                         static_cast<int>(TypeKind::Pointer),
                         "baz: second param is Pointer (const char *)");
                CHECK(f->parameters[1].pointee != nullptr, "baz: const char* has pointee");
                if (f->parameters[1].pointee) {
                    CHECK(f->parameters[1].pointee->isConst, "baz: const char* pointee is const");
                }
            }
        }
    }
}

void test_decayed_and_special_parameter_types() {
    std::printf("=== Decayed and special parameter types ===\n");

    // 6b. Array parameters import as the pointer types carried by the function type.
    {
        const auto art = parseHeaderFromContent("decay_arrays.h", "void f(char buf[20]);\n"
                                                                  "void g(int m[3][4]);\n");
        CHECK(art->diagnostics.empty(), "decayed array parameters import without diagnostics");
        CHECK_EQ(art->functions.size(), 2u, "both array-parameter functions are imported");
        if (const auto *f = findFunction(*art, "f")) {
            CHECK_EQ(f->parameters.size(), 1u, "f keeps its single parameter");
            if (!f->parameters.empty()) {
                CHECK_EQ(static_cast<int>(f->parameters[0].kind),
                         static_cast<int>(TypeKind::Pointer), "char buf[20] decays to Pointer");
            }
        }
        if (const auto *g = findFunction(*art, "g")) {
            CHECK_EQ(g->parameters.size(), 1u, "g keeps its single parameter");
            if (!g->parameters.empty()) {
                CHECK_EQ(static_cast<int>(g->parameters[0].kind),
                         static_cast<int>(TypeKind::Pointer), "int m[3][4] decays to Pointer");
            }
        }
    }

    // 6c. va_list parameters import once libclang carries the decayed type.
    {
        const auto art = parseHeaderFromContent("va_list_param.h",
                                                "#include <stdarg.h>\n"
                                                "int vprintf(const char *fmt, va_list ap);\n");
        CHECK(art->diagnostics.empty(), "va_list parameter imports without diagnostics");
        if (const auto *f = findFunction(*art, "vprintf")) {
            CHECK_EQ(f->parameters.size(), 2u, "vprintf keeps both parameters");
        }
    }

    // 6d. Function-pointer parameters import as opaque pointers.
    {
        const auto art = parseHeaderFromContent(
            "callback_param.h", "#include <stddef.h>\n"
                                "void qsort(void *base, size_t nmemb, size_t size,\n"
                                "           int (*compar)(const void *, const void *));\n");
        CHECK(art->diagnostics.empty(), "callback parameter imports without diagnostics");
        if (const auto *f = findFunction(*art, "qsort")) {
            CHECK_EQ(f->parameters.size(), 4u, "qsort keeps all four parameters");
            if (f->parameters.size() == 4U) {
                CHECK_EQ(static_cast<int>(f->parameters[3].kind),
                         static_cast<int>(TypeKind::Pointer),
                         "the comparator parameter becomes an opaque Pointer");
            }
        }
    }
}

void test_skips_and_dedupe() {
    std::printf("=== Skips and dedupe ===\n");

    // 6e. One unsupported decl is skipped silently; siblings still import.
    {
        const auto art = parseHeaderFromContent("mixed_support.h", "int take(_Atomic(int) box);\n"
                                                                   "int keep(int x);\n");
        CHECK(art->diagnostics.empty(), "unsupported decl does not fail the import");
        CHECK_EQ(art->functions.size(), 1u, "only the supported declaration is imported");
        CHECK(findFunction(*art, "keep") != nullptr, "the supported sibling is present");
        CHECK(findFunction(*art, "take") == nullptr, "the unsupported decl is absent");
        bool found_skip = false;
        for (const auto &skipped : art->skippedFunctions)
            found_skip = found_skip || skipped.find("take") != std::string::npos;
        CHECK(found_skip, "skippedFunctions records take and its reason");
    }

    // 6f. Same-name glibc-style duplicates collapse to one binding.
    {
        const auto art = parseHeaderFromContent("duplicate_names.h",
                                                "int dup_fn(int) __asm__(\"first_linkage\");\n"
                                                "int dup_fn(int) __asm__(\"second_linkage\");\n");
        bool counted   = false;
        for (const auto &f : art->functions) {
            if (f.name == "dup_fn")
                counted = counted || f.linkageName == "first_linkage";
        }
        CHECK_EQ(art->functions.size(), 1u, "duplicate C names collapse to one function");
        CHECK(counted, "the first declaration's linkage name wins");
    }
}

void test_struct_and_enum() {
    std::printf("=== Structs and enums ===\n");

    // 7. struct Point
    {
        const auto art =
            parseHeaderFromContent("struct_point.h", "struct Point { int x, y; };\n"
                                                     "struct Point get_origin(void);\n");
        CHECK(!art->diagnostics.empty() || art->functions.size() == 1, "struct Point: 1 function");
        if (const auto *f = findFunction(*art, "get_origin")) {
            CHECK_EQ(static_cast<int>(f->result.kind), static_cast<int>(TypeKind::Record),
                     "get_origin: result is Record");
            CHECK(f->result.name.find("Point") != std::string::npos,
                  "get_origin: result name contains Point");
        }
    }

    // 7a. Simple records passed/returned by value are validated for both ABI targets.
    for (const auto *triple : {"x86_64-unknown-linux-gnu", "aarch64-unknown-linux-gnu"}) {
        ParseOptions opts;
        opts.targetTriple = triple;
        const auto art    = parseHeaderFromContent("validated_records.h",
                                                   "struct Point { int x, y; };\n"
                                                      "int classify(struct Point p);\n"
                                                      "struct Point get_point(void);\n"
                                                      "struct Inner { int v; };\n"
                                                      "struct Outer { struct Inner inner; int extra; };\n"
                                                      "int classify_outer(struct Outer o);\n"
                                                      "struct Outer get_outer(void);\n",
                                                   opts);
        const std::string simple_no_diag =
            std::string("simple record parse has no diagnostics: ") + triple;
        CHECK_EQ(art->diagnostics.size(), 0u, simple_no_diag.c_str());
        if (const auto *f = findFunction(*art, "classify")) {
            const std::string classify_param =
                std::string("classify keeps its by-value parameter: ") + triple;
            CHECK_EQ(f->parameters.size(), 1u, classify_param.c_str());
            if (!f->parameters.empty()) {
                const auto &point = f->parameters[0];
                const std::string point_verified =
                    std::string("classify's Point layout is verified: ") + triple;
                const std::string point_size  = std::string("Point is 8 bytes: ") + triple;
                const std::string point_align = std::string("Point aligns to 4: ") + triple;
                const std::string point_fields =
                    std::string("Point has two validated fields: ") + triple;
                CHECK(point.hasVerifiedLayout, point_verified.c_str());
                CHECK_EQ(point.sizeBytes, 8u, point_size.c_str());
                CHECK_EQ(point.alignBytes, 4u, point_align.c_str());
                CHECK_EQ(point.recordFields.size(), 2u, point_fields.c_str());
            }
        } else {
            const std::string classify_imports = std::string("classify imports on ") + triple;
            CHECK(false, classify_imports.c_str());
        }
        if (const auto *f = findFunction(*art, "get_point")) {
            const std::string result_verified =
                std::string("get_point's Point result layout is verified: ") + triple;
            const std::string result_fields =
                std::string("returned Point has two validated fields: ") + triple;
            CHECK(f->result.hasVerifiedLayout, result_verified.c_str());
            CHECK_EQ(f->result.recordFields.size(), 2u, result_fields.c_str());
        } else {
            const std::string get_point_imports = std::string("get_point imports on ") + triple;
            CHECK(false, get_point_imports.c_str());
        }
        if (const auto *f = findFunction(*art, "classify_outer")) {
            const std::string outer_verified =
                std::string("nested Outer layout is verified: ") + triple;
            CHECK(f->parameters.size() == 1u && f->parameters[0].hasVerifiedLayout,
                  outer_verified.c_str());
        } else {
            const std::string outer_imports = std::string("classify_outer imports on ") + triple;
            CHECK(false, outer_imports.c_str());
        }
    }

    // 7b. Unsupported record layouts produce explicit skips instead of imports.
    for (const auto *triple : {"x86_64-unknown-linux-gnu", "aarch64-unknown-linux-gnu"}) {
        ParseOptions opts;
        opts.targetTriple = triple;
        const auto art =
            parseHeaderFromContent("unsupported_records.h",
                                   "struct Bits { unsigned int a : 4; unsigned int b : 4; };\n"
                                   "int with_bits(struct Bits b);\n"
                                   "struct Packed { int x; } __attribute__((packed));\n"
                                   "int with_packed(struct Packed p);\n"
                                   "struct Anonymous { struct { int x; }; };\n"
                                   "int with_anonymous(struct Anonymous a);\n"
                                   "struct Flexible { int count; int data[]; };\n"
                                   "int with_flexible(struct Flexible f);\n"
                                   "struct Wide { long double value; };\n"
                                   "int with_wide(struct Wide w);\n"
                                   "struct Int128 { __int128 value; };\n"
                                   "int with_int128(struct Int128 i);\n",
                                   opts);
        const std::string bits_skip = std::string("bitfield record function is skipped: ") + triple;
        const std::string packed_skip = std::string("packed record function is skipped: ") + triple;
        const std::string anonymous_skip =
            std::string("anonymous record function is skipped: ") + triple;
        const std::string flexible_skip =
            std::string("flexible array record function is skipped: ") + triple;
        const std::string wide_skip =
            std::string("long double record function is skipped: ") + triple;
        const std::string int128_skip =
            std::string("__int128 record function is skipped: ") + triple;
        CHECK(findFunction(*art, "with_bits") == nullptr, bits_skip.c_str());
        CHECK(findFunction(*art, "with_packed") == nullptr, packed_skip.c_str());
        CHECK(findFunction(*art, "with_anonymous") == nullptr, anonymous_skip.c_str());
        CHECK(findFunction(*art, "with_flexible") == nullptr, flexible_skip.c_str());
        CHECK(findFunction(*art, "with_wide") == nullptr, wide_skip.c_str());
        CHECK(findFunction(*art, "with_int128") == nullptr, int128_skip.c_str());

        int explicit_skips = 0;
        for (const auto &skipped : art->skippedFunctions) {
            if (skipped.find("with_bits") != std::string::npos ||
                skipped.find("with_packed") != std::string::npos ||
                skipped.find("with_anonymous") != std::string::npos ||
                skipped.find("with_flexible") != std::string::npos ||
                skipped.find("with_wide") != std::string::npos ||
                skipped.find("with_int128") != std::string::npos)
                explicit_skips++;
        }
        const std::string skips_all = std::string("every unsupported record has a skip: ") + triple;
        CHECK_EQ(explicit_skips, 6, skips_all.c_str());
    }

    // 8. enum Color
    {
        const auto art = parseHeaderFromContent("enum_color.h", "enum Color { R, G, B };\n"
                                                                "enum Color next(enum Color c);\n");
        CHECK(!art->diagnostics.empty() || art->functions.size() == 1, "enum Color: 1 function");
        if (const auto *f = findFunction(*art, "next")) {
            CHECK_EQ(static_cast<int>(f->result.kind), static_cast<int>(TypeKind::Enum),
                     "next: result is Enum");
            CHECK(f->result.name.find("Color") != std::string::npos,
                  "next: result name contains Color");
        }
    }
}

void test_filtering() {
    std::printf("=== Filtering ===\n");

    // 9. Variadic function is included and flagged for codegen
    {
        const auto art =
            parseHeaderFromContent("variadic.h", "int printf(const char *fmt, ...);\n");
        if (const auto *f = findFunction(*art, "printf")) {
            CHECK(f->isVariadic, "clang reports printf as variadic");
            CHECK_EQ(f->parameters.size(), 1u, "printf keeps its fixed parameter");
        } else {
            CHECK(false, "variadic printf is included");
        }
    }

    // 10. static function is excluded
    {
        const auto art =
            parseHeaderFromContent("static_fn.h", "static int helper(void) { return 1; }\n"
                                                  "int caller(void);\n");
        CHECK(findFunction(*art, "helper") == nullptr, "static helper is excluded");
        CHECK(findFunction(*art, "caller") != nullptr, "non-static caller is included");
    }

    // 11. inline function is excluded
    {
        const auto art =
            parseHeaderFromContent("inline_fn.h", "inline int fast(void) { return 0; }\n"
                                                  "int slow(void);\n");
        CHECK(findFunction(*art, "fast") == nullptr, "inline fast is excluded");
        CHECK(findFunction(*art, "slow") != nullptr, "non-inline slow is included");
    }
}

void test_dependencies() {
    std::printf("=== Dependencies ===\n");

    // 13. #include <stdint.h> produces a resolved dependency
    {
        const auto art   = parseHeaderFromContent("has_stdint.h", "#include <stdint.h>\n"
                                                                    "uint32_t id(void);\n");
        bool foundStdint = false;
        for (const auto &dep : art->dependencies) {
            if (dep.find("stdint.h") != std::string::npos) {
                foundStdint = true;
                break;
            }
        }
        CHECK(foundStdint, "#include <stdint.h> appears in dependencies");
    }

    // 14. Dependencies are deduplicated and sorted
    {
        // Write a helper header and a main header that includes it plus stddef.h
        writeTempHeader("helper_dep.h", "int helper_dep_fn(void);\n");
        const auto art = parseHeaderFromContent("multi_include.h",
                                                "#include <stddef.h>\n"
                                                "#include \"helper_dep.h\"\n"
                                                "int main_fn(void);\n",
                                                ParseOptions{});
        // Check no duplicates
        for (size_t i = 1; i < art->dependencies.size(); ++i) {
            CHECK(art->dependencies[i - 1] != art->dependencies[i],
                  "dependencies have no adjacent duplicates");
            CHECK(art->dependencies[i - 1] <= art->dependencies[i], "dependencies are sorted");
        }
    }
}

void test_parse_errors() {
    std::printf("=== Parse errors ===\n");

    // 15. #error produces a diagnostic
    {
        const auto art = parseHeaderFromContent("error_header.h", "#error \"fail deliberately\"\n");
        bool foundError = false;
        for (const auto &d : art->diagnostics) {
            if (d.message.find("fail deliberately") != std::string::npos) {
                foundError = true;
                break;
            }
        }
        CHECK(foundError, "#error produces diagnostic with the message");
    }
}

void test_parse_options() {
    std::printf("=== ParseOptions ===\n");

    // 16. Invalid sysroot produces a diagnostic
    {
        ParseOptions opts;
        opts.sysroot      = "/nonexistent/sysroot";
        opts.targetTriple = "x86_64-unknown-linux-gnu";
        const auto art    = parseHeader(writeTempHeader("sysroot_test.h", "int fn(void);\n"), opts);
        // With a fake sysroot, we might get parse errors; verify artifact is non-null
        CHECK(art != nullptr, "invalid sysroot still returns artifact");
        // The diagnostics should include something about inability to find headers
        // or the parse simply fails. Either way, artifact is returned.
    }

    // 17. includeDirs allows resolving an extra header
    {
        // Create a directory for extra headers
        const std::string extraDir = tempDir() + "/extra_includes";
        std::filesystem::create_directories(extraDir);
        {
            std::ofstream out(extraDir + "/extra.h");
            out << "int extra_fn(void);\n";
            out.close();
        }

        ParseOptions opts;
        opts.includeDirs.push_back(extraDir);
        const auto art = parseHeaderFromContent("uses_extra.h",
                                                "#include \"extra.h\"\n"
                                                "int main_fn(void);\n",
                                                opts);
        CHECK(findFunction(*art, "main_fn") != nullptr, "main_fn also found alongside extra_fn");
        // extra_fn lives in a transitively included header and is not
        // returned in `functions` (only declarations from the main file
        // are visited), but the include dir should still resolve.
        bool foundExtraDep = false;
        for (const auto &dep : art->dependencies) {
            if (dep.find("extra.h") != std::string::npos)
                foundExtraDep = true;
        }
        CHECK(foundExtraDep, "extra.h appears in dependencies via custom includeDir");
    }

    // 18. defines allows conditional compilation
    {
        ParseOptions opts;
        opts.defines.push_back("ANSWER=42");
        const auto art = parseHeaderFromContent("define_test.h",
                                                "#if ANSWER != 42\n"
                                                "#error \"wrong answer\"\n"
                                                "#endif\n"
                                                "int the_fn(void);\n",
                                                opts);
        // If the define works, there's no #error and we see the function
        CHECK(findFunction(*art, "the_fn") != nullptr,
              "function compiled correctly when ANSWER=42 defined");
    }

    // 18b. defines missing causes #error
    {
        const auto art = parseHeaderFromContent("define_fail.h", "#if ANSWER != 42\n"
                                                                 "#error \"wrong answer\"\n"
                                                                 "#endif\n"
                                                                 "int the_fn(void);\n");
        // Without the define, clang will produce a diagnostic for #error or
        // for ANSWER being undefined; either way, the_fn likely won't appear.
        bool hasError = !art->diagnostics.empty();
        CHECK(hasError || findFunction(*art, "the_fn") == nullptr,
              "without ANSWER=42, parse either fails or function is absent");
    }
}

void test_object_like_macro_constants() {
    std::printf("=== Object-like macro constants ===\n");

    {
        const auto art =
            parseHeaderFromContent("scalar_constants.h", "#define ANSWER 42\n"
                                                         "#define RATIO 1.5f\n"
                                                         "#define YES true\n"
                                                         "#define LETTER 'A'\n"
                                                         "#define BIG 12345678901234567890u64\n"
                                                         "#define NEG -7i64\n"
                                                         "#define NAME(x) x\n");
        CHECK(art->diagnostics.empty(), "scalar macro constants do not fail the import");
        if (const auto *c = findConstant(*art, "ANSWER")) {
            CHECK_EQ(static_cast<int>(c->kind), static_cast<int>(ConstantKind::Integer),
                     "ANSWER is Integer");
            CHECK_EQ(c->bits, 32, "ANSWER is i32");
            CHECK(c->isSigned, "ANSWER is signed");
            CHECK_EQ(c->integerValue, 42, "ANSWER has value 42");
        } else {
            CHECK(false, "ANSWER imported as a constant");
        }
        if (const auto *c = findConstant(*art, "RATIO")) {
            CHECK_EQ(static_cast<int>(c->kind), static_cast<int>(ConstantKind::Float),
                     "RATIO is Float");
            CHECK_EQ(c->bits, 32, "RATIO is f32");
            CHECK_EQ(c->floatValue, 1.5, "RATIO has value 1.5");
        } else {
            CHECK(false, "RATIO imported as a constant");
        }
        if (const auto *c = findConstant(*art, "YES")) {
            CHECK_EQ(static_cast<int>(c->kind), static_cast<int>(ConstantKind::Bool),
                     "YES is Bool");
            CHECK(c->boolValue, "YES is true");
        } else {
            CHECK(false, "YES imported as a constant");
        }
        if (const auto *c = findConstant(*art, "LETTER")) {
            CHECK_EQ(static_cast<int>(c->kind), static_cast<int>(ConstantKind::Char),
                     "LETTER is Char");
            CHECK_EQ(static_cast<char>(c->charValue), 'A', "LETTER is 'A'");
        } else {
            CHECK(false, "LETTER imported as a constant");
        }
        if (const auto *c = findConstant(*art, "BIG")) {
            CHECK_EQ(static_cast<int>(c->kind), static_cast<int>(ConstantKind::Integer),
                     "BIG is Integer");
            CHECK_EQ(c->bits, 64, "BIG is u64");
            CHECK(!c->isSigned, "BIG is unsigned");
            CHECK_EQ(static_cast<unsigned long long>(c->integerValue), 12345678901234567890ULL,
                     "BIG preserves u64 magnitude");
        } else {
            std::printf("skips: ");
            for (const auto &skipped : art->skippedFunctions)
                std::printf("%s | ", skipped.c_str());
            std::printf("\n");
            CHECK(false, "BIG imported as a constant");
        }
        if (const auto *c = findConstant(*art, "NEG")) {
            CHECK_EQ(static_cast<int>(c->kind), static_cast<int>(ConstantKind::Integer),
                     "NEG is Integer");
            CHECK_EQ(c->bits, 64, "NEG is i64");
            CHECK(c->isSigned, "NEG is signed");
            CHECK_EQ(c->integerValue, -7, "NEG is -7");
        } else {
            CHECK(false, "NEG imported as a constant");
        }
        CHECK(findConstant(*art, "NAME") == nullptr, "function-like macro is not a constant");
        bool found_skip = false;
        for (const auto &skipped : art->skippedFunctions)
            found_skip = found_skip || skipped.find("NAME") != std::string::npos;
        CHECK(found_skip, "function-like macro appears in skips");
    }

    {
        const auto art = parseHeaderFromContent("unsupported_constants.h",
                                                "#define TOKEN 1 + 2\n"
                                                "#define MSG \"hello\"\n"
                                                "#define TOOBIG 9999999999999999999999999\n");
        CHECK(art->diagnostics.empty(), "unsupported constants do not fail the import");
        CHECK(findConstant(*art, "TOKEN") == nullptr, "TOKEN is not a scalar constant");
        CHECK(findConstant(*art, "MSG") == nullptr, "MSG is not a constant");
        CHECK(findConstant(*art, "TOOBIG") == nullptr, "out-of-range integer is skipped");
        bool found_token  = false;
        bool found_msg    = false;
        bool found_toobig = false;
        for (const auto &skipped : art->skippedFunctions) {
            found_token  = found_token || skipped.find("TOKEN") != std::string::npos;
            found_msg    = found_msg || skipped.find("MSG") != std::string::npos;
            found_toobig = found_toobig || skipped.find("TOOBIG") != std::string::npos;
        }
        CHECK(found_token, "TOKEN appears in skips");
        CHECK(found_msg, "MSG appears in skips");
        CHECK(found_toobig, "TOOBIG appears in skips");
    }

    {
        // Compiler predefines are not part of the main-file macro visit; the
        // artifact must not expose them even when a name collides with a test.
        const auto art = parseHeaderFromContent("predefined.h", "#define __LINE__ 123\n");
        CHECK(findConstant(*art, "__LINE__") == nullptr,
              "builtin/predefined macros are not imported");
    }
}

#endif // ZITH_ENABLE_C_INTEROP

} // namespace

void test_cinterop() {
    test_without_libclang();
    test_system_include_dirs();
#ifdef ZITH_ENABLE_C_INTEROP
    test_scalar_types();
    test_pointer_and_const();
    test_decayed_and_special_parameter_types();
    test_skips_and_dedupe();
    test_struct_and_enum();
    test_filtering();
    test_dependencies();
    test_parse_errors();
    test_parse_options();
    test_object_like_macro_constants();
#endif
}
TEST_MAIN(cinterop)

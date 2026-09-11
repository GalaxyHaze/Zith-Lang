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
        : opts(arena), root(std::filesystem::temp_directory_path() / "zith-generic-hashmap-tests") {
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
        opts.targetStage = session::Stage::TypeChecked;
#ifdef ZITH_STDLIB_DIR
        // Mirror `zithc --include stdlib` so tests can import std/alloc and friends.
        opts.includeDirs.push(ZITH_STDLIB_DIR);
#endif
    }

    ~SessionRunner() {
        std::filesystem::remove_all(root);
    }

    bool run(std::string_view input) {
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
        const bool ok = session.runTo(session::Stage::TypeChecked);
        for (const auto &d : session.diags().all()) {
            if (d.severity == diagnostics::Severity::Error) {
                std::printf("    [Diag] Code: %u, Message: %s\n", static_cast<unsigned>(d.code),
                            d.message.c_str());
            }
        }
        return ok && session.diags().errorCount() == 0;
    }
};

void test_nested_generic_calls_propagate_bounds() {
    SessionRunner t;
    const bool ok = t.run("trait Hashable {\n"
                          "    fn hash(self): u64;\n"
                          "}\n"
                          "implement i32 as Hashable {\n"
                          "    fn hash(self): u64 { return self as u64; }\n"
                          "}\n"
                          "struct Entry<T> { key: T }\n"
                          "fn contains<K: Hashable>(self: view Entry<K>): u64 {\n"
                          "    return self->key.hash();\n"
                          "}\n"
                          "fn put<K: Hashable>(self: lend Entry<K>) {\n"
                          "    let x = contains(view self);\n"
                          "    self->key = 0 as K;\n"
                          "}\n"
                          "fn main(): i32 { return 0; }\n");
    CHECK(ok, "nested generic call propagates caller bounds and args");
}

void test_reified_struct_reuses_args_identity() {
    SessionRunner t;
    const bool ok = t.run("struct Pair<A, B> { left: A, right: B }\n"
                          "fn get<A, B>(p: Pair<A, B>): A { return p.left; }\n"
                          "fn apply<A, B>(p: Pair<A, B>) {\n"
                          "    let x = get(p);\n"
                          "}\n"
                          "fn main(): i32 { return 0; }\n");
    CHECK(ok, "generic inference matches reified struct type args by index");
}

void test_hashmap_stdlib_checks() {
    SessionRunner t;
    const std::string input =
        "import \"stdlib.h\"\n"
        "pub trait Hashable { fn hash(self): u64; }\n"
        "implement i32 as Hashable { fn hash(self): u64 { return self as u64; } }\n"
        "pub struct Entry<K, V> { key: K, value: V, occupied: bool = false, next: u64 = 0 }\n"
        "pub struct HashMap<K, V> {\n"
        "    count: u64 = 0,\n"
        "    capacity: u64 = 0,\n"
        "    table: ?*Entry<K, V> = null,\n"
        "    head: u64 = 0,\n"
        "}\n"
        "pub fn reserve<K, V>(var self: lend HashMap<K, V>, capacity: u64): bool {\n"
        "    let mem: ?raw opaque = calloc(capacity, @sizeOf(Entry<K, V>));\n"
        "    if (mem is null) { return false; }\n"
        "    let table: ?*Entry<K, V> = mem as ?*Entry<K, V>;\n"
        "    if (table is null) { free(mem); return false; }\n"
        "    free(self->table);\n"
        "    self->table = table;\n"
        "    self->capacity = capacity;\n"
        "    self->count = 0u64;\n"
        "    self->head = 0u64;\n"
        "    return true;\n"
        "}\n"
        "pub fn init<K, V>(var self: lend HashMap<K, V>): bool {\n"
        "    return reserve<K, V>(lend self, 64u64);\n"
        "}\n"
        "pub fn contains<K: Hashable, V>(self: view HashMap<K, V>, key: K): bool {\n"
        "    if (self->table is null or self->count == 0u64) { return false; }\n"
        "    var entries: []Entry<K, V> = raw self->table[0u64..self->capacity];\n"
        "    var index = self->head;\n"
        "    for (index != 0u64) {\n"
        "        let current = raw entries[index - 1u64];\n"
        "        if (current.occupied and current.key.hash() == key.hash() and current.key == key) "
        "{\n"
        "            return true;\n"
        "        }\n"
        "        index = current.next;\n"
        "    }\n"
        "    return false;\n"
        "}\n"
        "pub fn put<K: Hashable, V>(var self: lend HashMap<K, V>, key: K, value: V): bool {\n"
        "    if (self->table is null) {\n"
        "        if not(init<K, V>(lend self)) { return false; }\n"
        "    }\n"
        "    if (not contains<K, V>(view self, key)) {\n"
        "        if (self->count >= self->capacity) { return false; }\n"
        "        var entries: []Entry<K, V> = raw self->table[0u64..self->capacity];\n"
        "        let index = self->count + 1u64;\n"
        "        entries[index - 1u64] = Entry<K, V>{ key, value, true, self->head };\n"
        "        self->head = index;\n"
        "        self->count = self->count + 1u64;\n"
        "    }\n"
        "    return true;\n"
        "}\n"
        "fn main(): i32 { return 0; }\n";
    CHECK(t.run(input), "hash-map shaped stdlib surface type-checks");
}

void test_inplace_trait_passes_check() {
    SessionRunner t;
    const bool ok = t.run("from std/new\n"
                          "from std/alloc\n"
                          "\n"
                          "struct Box { value: i64 }\n"
                          "\n"
                          "implement Box as InPlace {\n"
                          "    fn inplace(var self, allocator: dyn Allocator, args: opaque): "
                          "bool {\n"
                          "        let data = allocate(allocator, @sizeOf(i64), 1u64);\n"
                          "        return not (data is null);\n"
                          "    }\n"
                          "    fn clean(var self, allocator: dyn Allocator) {}\n"
                          "}\n"
                          "\n"
                          "fn main(): i32 {\n"
                          "    var box = Box { value: 7 };\n"
                          "    let heap = HeapAllocator {};\n"
                          "    if not box.InPlace.inplace(heap, 0 as opaque) { return 1; }\n"
                          "    box.InPlace.clean(heap);\n"
                          "    return 0;\n"
                          "}\n");
    CHECK(ok, "stdlib InPlace imports, Box conforms, and the trait calls type-check");
}

void test_generic_hashmap() {
    test_nested_generic_calls_propagate_bounds();
    test_reified_struct_reuses_args_identity();
    test_hashmap_stdlib_checks();
    test_inplace_trait_passes_check();
}

} // namespace

TEST_MAIN(generic_hashmap)

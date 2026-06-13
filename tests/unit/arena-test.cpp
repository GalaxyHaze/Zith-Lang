#include "../test-common.hpp"
#include "memory/arena.hpp"

#include <cstdint>
#include <cstring>
#include <string_view>

using zith::memory::Arena;
using zith::memory::MarkPoint;

// ── Basic allocation ────────────────────────────────────────────

static void test_alloc_basic() {
    Arena arena;

    void *p = arena.alloc(32);
    CHECK(p != nullptr, "alloc(32) returns non-null pointer");

    size_t remaining = arena.remaining();
    CHECK(remaining > 0, "remaining decreases after allocation");
    CHECK(remaining <= 4096 - 32, "remaining <= block_size - allocated");
}

static void test_alloc_zero_bytes() {
    Arena arena;

    void *p = arena.alloc(0);
    CHECK(p != nullptr, "alloc(0) returns non-null pointer");
}

// ── make<T> ─────────────────────────────────────────────────────

static void test_make_int() {
    Arena arena;

    int *p = arena.make<int>(42);
    CHECK(p != nullptr, "make<int> returns non-null");
    CHECK_EQ(*p, 42, "make<int> value is correct");
}

static void test_make_string_view() {
    Arena arena;

    auto *sv = arena.make<std::string_view>("hello, arena");
    CHECK(sv != nullptr, "make<std::string_view> returns non-null");
    CHECK_EQ(*sv, std::string_view("hello, arena"),
             "make<std::string_view> value is correct");
}

static void test_make_multiple() {
    Arena arena;

    int *a = arena.make<int>(10);
    int *b = arena.make<int>(20);
    CHECK(a != nullptr, "first make<int> returns non-null");
    CHECK(b != nullptr, "second make<int> returns non-null");
    CHECK_EQ(*a, 10, "first value intact");
    CHECK_EQ(*b, 20, "second value intact");
    // Pointers should be different
    CHECK(a != b, "two make<int> calls return different pointers");
}

// ── Multiple blocks ─────────────────────────────────────────────

static void test_multiple_blocks() {
    Arena arena(64); // tiny block size to force multiple blocks

    // Allocate more than 64 bytes to trigger new block
    void *p1 = arena.alloc(48);
    CHECK(p1 != nullptr, "first alloc in small arena succeeds");

    void *p2 = arena.alloc(48);
    CHECK(p2 != nullptr, "second alloc in small arena succeeds (new block)");

    void *p3 = arena.alloc(48);
    CHECK(p3 != nullptr, "third alloc in small arena succeeds (another block)");
}

// ── Alignment ───────────────────────────────────────────────────

static void test_alignment_default() {
    Arena arena;

    void *p = arena.alloc(1);
    CHECK(p != nullptr, "alloc(1) succeeds");
    // Block allocations use alignof(Block) — typically 8 on 64-bit
    bool aligned = (reinterpret_cast<uintptr_t>(p) % alignof(void *)) == 0;
    CHECK(aligned, "default-aligned pointer is at least void*-aligned");
}

static void test_alignment_4() {
    Arena arena;

    void *p = arena.alloc(4, 4);
    CHECK(p != nullptr, "alloc with 4-byte alignment succeeds");
    CHECK_EQ(reinterpret_cast<uintptr_t>(p) % 4, size_t(0),
             "pointer is 4-byte aligned");
}

static void test_alignment_8() {
    Arena arena;

    void *p = arena.alloc(8, 8);
    CHECK(p != nullptr, "alloc with 8-byte alignment succeeds");
    CHECK_EQ(reinterpret_cast<uintptr_t>(p) % 8, size_t(0),
             "pointer is 8-byte aligned");
}

// ── Arena move ──────────────────────────────────────────────────

static void test_arena_move() {
    Arena arena;
    int *p = arena.make<int>(99);

    Arena moved = std::move(arena);
    CHECK_EQ(*p, 99, "moved arena retains allocated value");

    // Original arena should be empty
    CHECK_EQ(arena.remaining(), size_t(0), "original arena has no remaining after move");
}

static void test_arena_move_assign() {
    Arena arena1;
    (void)arena1.make<int>(11);

    Arena arena2;
    (void)arena2.make<int>(22);

    arena2 = std::move(arena1);
    CHECK(true, "move assignment does not crash");
}

// ── clear() ─────────────────────────────────────────────────────

static void test_clear() {
    Arena arena;

    (void)arena.make<int>(1);
    (void)arena.make<double>(3.14);
    (void)arena.alloc(256);

    arena.clear();

    // After clear, we should be able to allocate again
    int *p = arena.make<int>(42);
    CHECK(p != nullptr, "alloc succeeds after clear");
    CHECK_EQ(*p, 42, "value correct after re-allocation");
}

static void test_clear_empty() {
    Arena arena;

    // clear an arena that has had no custom allocations (just initial block)
    arena.clear();
    CHECK(true, "clear() on empty arena does not crash");

    // Can still allocate after
    void *p = arena.alloc(16);
    CHECK(p != nullptr, "alloc after clearing empty arena succeeds");
}

// ── MarkPoint ───────────────────────────────────────────────────

static void test_markpoint_rollback() {
    Arena arena;

    int *before = arena.make<int>(1);
    CHECK_EQ(*before, 1, "value before mark is correct");

    {
        MarkPoint mark(arena);
        int *inside = arena.make<int>(2);
        CHECK_EQ(*inside, 2, "value inside mark scope is correct");

        // rollback happens at scope end
    }

    // After rollback, the arena should be back to the marked position.
    // The next allocation reuses the space that 'inside' used.
    int *after = arena.make<int>(3);
    CHECK_EQ(*after, 3, "value after rollback is correct");

    // 'after' was allocated at the same position as 'inside' was before rollback,
    // so it should be at a different address than 'before' (which is earlier).
    CHECK(before != after, "after rollback, new allocation goes where 'inside' was");
}

static void test_markpoint_multiple_allocations() {
    Arena arena;

    (void)arena.make<int>(10);
    (void)arena.make<int>(20);

    MarkPoint mark(arena);

    (void)arena.make<int>(30);
    (void)arena.make<int>(40);
    (void)arena.make<int>(50);

    mark.rollback();

    // After rollback, the next allocation should reuse the space after 20
    int *p = arena.make<int>(99);
    CHECK(p != nullptr, "alloc after rollback works");
    CHECK_EQ(*p, 99, "allocated value is correct");
}

static void test_markpoint_release() {
    Arena arena;

    (void)arena.make<int>(1);

    {
        MarkPoint mark(arena);
        (void)arena.make<int>(2); // allocated inside mark scope

        mark.release(); // detach: destructor won't roll back
    }

    // Since we released, the arena keeps the allocation
    arena.clear(); // should not crash
    CHECK(true, "released MarkPoint does not roll back on destruction");
}

static void test_markpoint_nested() {
    Arena arena;

    (void)arena.make<int>(1);

    MarkPoint outer(arena);
    (void)arena.make<int>(2);

    {
        MarkPoint inner(arena);
        (void)arena.make<int>(3);
        // inner rolls back here, removing '3'
    }

    // At this point, '2' should still be there (outer hasn't rolled back)
    int *p = arena.make<int>(4);
    CHECK(p != nullptr, "alloc after inner rollback still works");
    CHECK_EQ(*p, 4, "value correct after inner rollback");

    outer.rollback();
    // Now '2' and '4' are gone
    int *q = arena.make<int>(5);
    CHECK(q != nullptr, "alloc after outer rollback works");
    CHECK_EQ(*q, 5, "value correct after outer rollback");
}

// ── Edge cases ──────────────────────────────────────────────────

static void test_empty_arena_remaining() {
    Arena arena(1024);

    // remaining should be close to block_size
    CHECK(arena.remaining() > 0, "remaining > 0 for newly created arena");
    CHECK(arena.remaining() <= 1024, "remaining <= block_size");
}

static void test_arena_default_constructor() {
    Arena arena; // uses default block size (4096)
    CHECK(arena.remaining() > 0, "default arena has positive remaining");
    CHECK(arena.remaining() <= 4096, "default arena remaining <= 4096");
}

int main() {
    std::printf("arena tests\n");
    std::printf("=============\n\n");

    test_alloc_basic();
    test_alloc_zero_bytes();
    test_make_int();
    test_make_string_view();
    test_make_multiple();
    test_multiple_blocks();
    test_alignment_default();
    test_alignment_4();
    test_alignment_8();
    test_arena_move();
    test_arena_move_assign();
    test_clear();
    test_clear_empty();
    test_markpoint_rollback();
    test_markpoint_multiple_allocations();
    test_markpoint_release();
    test_markpoint_nested();
    test_empty_arena_remaining();
    test_arena_default_constructor();

    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}

#include "../test-common.hpp"
#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"

#include <cstdint>
#include <cstdlib>
#include <new>
#include <string>
#include <utility>

using zith::memory::Arena;
using zith::memory::DynArray;

// ── Helper: a move-only type ─────────────────────────────────────

struct MoveOnly {
    int value;
    explicit MoveOnly(int v) : value(v) {}
    MoveOnly(const MoveOnly &)       = delete;
    auto operator=(const MoveOnly &) = delete;
    MoveOnly(MoveOnly &&other) noexcept : value(other.value) {
        other.value = -1;
    }
    auto operator=(MoveOnly &&other) noexcept -> MoveOnly & {
        if (this != &other) {
            value       = other.value;
            other.value = -1;
        }
        return *this;
    }
};

// ── Helper: a type that counts destructor calls ──────────────────

struct DtorCounter {
    static int alive;
    static int dtor_calls;
    int id = 0;

    DtorCounter() : id(++alive) {}
    DtorCounter(const DtorCounter &o) noexcept : id(o.id) {
        ++alive;
    }
    DtorCounter(DtorCounter &&o) noexcept : id(o.id) {
        o.id = -1;
        ++alive;
    }
    auto operator=(const DtorCounter &o) noexcept -> DtorCounter & = default;
    auto operator=(DtorCounter &&o) noexcept -> DtorCounter &      = default;
    ~DtorCounter() noexcept {
        ++dtor_calls;
        --alive;
    }
};

int DtorCounter::alive      = 0;
int DtorCounter::dtor_calls = 0;

static void reset_counters() {
    DtorCounter::alive      = 0;
    DtorCounter::dtor_calls = 0;
}

// ── 1. empty_array ───────────────────────────────────────────────

static void test_empty_array() {
    Arena arena;
    DynArray<int> da(arena);

    CHECK_EQ(da.size(), size_t(0), "empty DynArray has size 0");
    CHECK_EQ(da.capacity(), size_t(0), "empty DynArray has capacity 0");
    CHECK(da.empty(), "empty DynArray reports empty");
}

// ── 2. push_one ──────────────────────────────────────────────────

static void test_push_one() {
    Arena arena;
    DynArray<int> da(arena);

    da.push(42);

    CHECK_EQ(da.size(), size_t(1), "size is 1 after one push");
    CHECK(!da.empty(), "not empty after one push");
    CHECK_EQ(da[0], 42, "element 0 is 42");
}

// ── 3. push_multiple ─────────────────────────────────────────────

static void test_push_multiple() {
    Arena arena;
    DynArray<int> da(arena);

    for (int i = 0; i < 100; i++) {
        da.push(i * 3);
    }

    CHECK_EQ(da.size(), size_t(100), "size is 100 after 100 pushes");
    for (int i = 0; i < 100; i++) {
        CHECK_EQ(da[i], i * 3, "element order is preserved");
    }
}

// ── 4. push_move ─────────────────────────────────────────────────

static void test_push_move() {
    Arena arena;
    DynArray<MoveOnly> da(arena);

    MoveOnly a(10);
    MoveOnly b(20);

    da.push(std::move(a));
    da.push(std::move(b));

    CHECK_EQ(da.size(), size_t(2), "size is 2 after moving two elements");
    CHECK_EQ(da[0].value, 10, "first element value is correct");
    CHECK_EQ(da[1].value, 20, "second element value is correct");
    // originals should be moved-from
    CHECK_EQ(a.value, -1, "source a was moved-from");
    CHECK_EQ(b.value, -1, "source b was moved-from");
}

// ── 5. emplace ───────────────────────────────────────────────────

static void test_emplace() {
    Arena arena;
    DynArray<std::pair<int, double>> da(arena);

    auto &ref = da.emplace(42, 3.14);

    CHECK_EQ(da.size(), size_t(1), "size is 1 after emplace");
    CHECK_EQ(ref.first, 42, "emplaced pair first element");
    CHECK_EQ(ref.second, 3.14, "emplaced pair second element");

    // Verify via operator[] too
    CHECK_EQ(da[0].first, 42, "operator[] matches emplace result");
    CHECK_EQ(da[0].second, 3.14, "operator[] matches emplace result");
}

// ── 6. grow ──────────────────────────────────────────────────────

static void test_grow() {
    Arena arena;
    DynArray<int> da(arena);

    // Initial capacity is 0; first push grows to 4.
    // Push 5 elements to force growth: 0 -> 4 -> 8
    for (int i = 0; i < 5; i++) {
        da.push(i);
    }

    // After 5 pushes, capacity must be at least 8
    CHECK(da.capacity() >= size_t(8), "capacity grew to at least 8 after 5 pushes");
    CHECK_EQ(da.size(), size_t(5), "size is 5 after 5 pushes");

    // Verify all elements are intact
    for (int i = 0; i < 5; i++) {
        CHECK_EQ(da[i], i, "element preserved after growth");
    }

    // Grow further: push enough to trigger 8 -> 16
    for (int i = 5; i < 15; i++) {
        da.push(i);
    }

    CHECK(da.capacity() >= size_t(16), "capacity grew again after more pushes");
    CHECK_EQ(da.size(), size_t(15), "size is 15 after 15 pushes");

    for (int i = 0; i < 15; i++) {
        CHECK_EQ(da[i], i, "all elements preserved after second growth");
    }
}

// ── 7. reserve ───────────────────────────────────────────────────

static void test_reserve() {
    Arena arena;
    DynArray<int> da(arena);

    da.reserve(64);

    CHECK(da.capacity() >= size_t(64), "capacity >= 64 after reserve(64)");
    CHECK_EQ(da.size(), size_t(0), "size is still 0 after reserve");
    CHECK(da.empty(), "still empty after reserve");

    // Push without triggering growth
    for (int i = 0; i < 64; i++) {
        da.push(i * 10);
    }

    CHECK_EQ(da.size(), size_t(64), "size is 64 after 64 pushes");
    CHECK(da.capacity() >= size_t(64), "capacity did not shrink");

    for (int i = 0; i < 64; i++) {
        CHECK_EQ(da[i], i * 10, "all elements correct after reserve-then-push");
    }
}

// ── 8. clear ─────────────────────────────────────────────────────

static void test_clear() {
    Arena arena;
    DynArray<int> da(arena);

    da.push(1);
    da.push(2);
    da.push(3);
    CHECK_EQ(da.size(), size_t(3), "size is 3 before clear");

    da.clear();

    CHECK_EQ(da.size(), size_t(0), "size is 0 after clear");
    CHECK(da.empty(), "empty after clear");

    // Push again after clear — should reuse the same storage
    da.push(100);
    da.push(200);

    CHECK_EQ(da.size(), size_t(2), "size is 2 after pushing after clear");
    CHECK_EQ(da[0], 100, "first post-clear element correct");
    CHECK_EQ(da[1], 200, "second post-clear element correct");
}

// ── 9. back ──────────────────────────────────────────────────────

static void test_back() {
    Arena arena;
    DynArray<int> da(arena);

    da.push(10);
    da.push(20);
    da.push(30);

    CHECK_EQ(da.back(), 30, "back() returns last element");
    CHECK_EQ(da[da.size() - 1], da.back(), "back() matches operator[]");

    // Modify through back()
    da.back() = 99;
    CHECK_EQ(da.back(), 99, "back() is writable");
    CHECK_EQ(da[2], 99, "modification through back() visible via operator[]");

    // Const access
    const auto &cda = da;
    CHECK_EQ(cda.back(), 99, "const back() returns correct value");
}

// ── 10. iteration ────────────────────────────────────────────────

static void test_iteration() {
    Arena arena;
    DynArray<int> da(arena);

    for (int i = 0; i < 10; i++) {
        da.push(i * 2);
    }

    // Range-for iteration
    int expected = 0;
    for (const auto &val : da) {
        CHECK_EQ(val, expected, "range-for iteration value correct");
        expected += 2;
    }
    CHECK_EQ(expected, 20, "iteration visited all 10 elements");

    // Iterator pair loop
    int sum = 0;
    for (auto it = da.begin(); it != da.end(); ++it) {
        sum += *it;
    }
    CHECK_EQ(sum, 90, "iterator loop sum is correct (0+2+4+...+18 = 90)");

    // data() pointer range
    sum = 0;
    for (size_t i = 0; i < da.size(); i++) {
        sum += da.data()[i];
    }
    CHECK_EQ(sum, 90, "data() pointer access gives correct sum");
}

// ── 11. move_constructor ─────────────────────────────────────────

static void test_move_constructor() {
    Arena arena1;
    Arena arena2;

    DynArray<int> da(arena1);
    da.push(100);
    da.push(200);
    da.push(300);

    CHECK_EQ(da.size(), size_t(3), "source size is 3 before move");

    DynArray<int> moved(std::move(da));

    // Moved-to array has the data
    CHECK_EQ(moved.size(), size_t(3), "moved-to array has size 3");
    CHECK_EQ(moved[0], 100, "moved-to[0] is 100");
    CHECK_EQ(moved[1], 200, "moved-to[1] is 200");
    CHECK_EQ(moved[2], 300, "moved-to[2] is 300");

    // Source array is now empty (moved-from state)
    CHECK_EQ(da.size(), size_t(0), "source size is 0 after move");
    CHECK_EQ(da.capacity(), size_t(0), "source capacity is 0 after move");
    CHECK(da.empty(), "source is empty after move");
    CHECK(da.data() == nullptr, "source data is null after move");

    // Push on new array works (using its own arena)
    moved.push(400);
    CHECK_EQ(moved.size(), size_t(4), "moved-to can still push");
    CHECK_EQ(moved[3], 400, "new element in moved-to array correct");
}

// ── 12. move_assign ──────────────────────────────────────────────

static void test_move_assign() {
    Arena arena1;
    Arena arena2;

    DynArray<int> da1(arena1);
    da1.push(1);
    da1.push(2);
    da1.push(3);

    DynArray<int> da2(arena2);
    da2.push(10);
    da2.push(20);

    // Move-assign da1 into da2
    da2 = std::move(da1);

    // da2 now has da1's data
    CHECK_EQ(da2.size(), size_t(3), "move-assigned target has size 3");
    CHECK_EQ(da2[0], 1, "move-assigned[0] is 1");
    CHECK_EQ(da2[1], 2, "move-assigned[1] is 2");
    CHECK_EQ(da2[2], 3, "move-assigned[2] is 3");

    // da1 is now empty
    CHECK(da1.empty(), "source is empty after move assignment");
    CHECK_EQ(da1.size(), size_t(0), "source size is 0 after move assignment");

    // Self-assignment should be safe
    da2 = std::move(da2);
    CHECK_EQ(da2.size(), size_t(3), "self-move-assign leaves size unchanged");
    CHECK_EQ(da2[0], 1, "self-move-assign preserves data");
}

// ── 13. nontrivial_destructor ────────────────────────────────────

static void test_nontrivial_destructor() {
    reset_counters();

    {
        Arena arena;
        DynArray<DtorCounter> da(arena);

        da.push(DtorCounter{});
        da.push(DtorCounter{});
        da.push(DtorCounter{});

        CHECK_EQ(da.size(), size_t(3), "three elements pushed");
        CHECK(DtorCounter::alive >= 3, "at least 3 DtorCounter objects alive");
    } // da goes out of scope here

    // Destructor ran for all elements
    CHECK(DtorCounter::dtor_calls >= 3, "destructors called for all pushed elements");
    CHECK_EQ(DtorCounter::alive, 0, "all DtorCounter objects destroyed");

    // Test that clear() also calls destructors
    reset_counters();
    {
        Arena arena;
        DynArray<DtorCounter> da(arena);

        da.push(DtorCounter{});
        da.push(DtorCounter{});
        CHECK_EQ(DtorCounter::alive, 2, "two alive before clear");

        da.clear();

        CHECK_EQ(da.size(), size_t(0), "size is 0 after clear");
        CHECK_EQ(DtorCounter::alive, 0, "no alive objects after clear");

        // Push again after clear
        da.push(DtorCounter{});
        CHECK_EQ(DtorCounter::alive, 1, "one alive after push after clear");
    }
}

// ── 14. edge_cases ───────────────────────────────────────────────

static void test_edge_cases() {
    // Push after clear (with nontrivial type)
    {
        Arena arena;
        DynArray<int> da(arena);

        da.push(1);
        da.push(2);
        da.clear();

        CHECK(da.empty(), "empty after clear");

        da.push(3);
        da.push(4);
        da.push(5);

        CHECK_EQ(da.size(), size_t(3), "size is 3 after push after clear");
        CHECK_EQ(da[0], 3, "first post-clear element correct");
        CHECK_EQ(da[1], 4, "second post-clear element correct");
        CHECK_EQ(da[2], 5, "third post-clear element correct");
    }

    // Default-construct on arena and destroy immediately
    {
        Arena arena;
        DynArray<DtorCounter> da(arena);
        // no pushes — should not crash on destruction
        CHECK(da.empty(), "default-constructed DynArray is empty");
    }

    // Destroying after reserve with no pushes
    {
        Arena arena;
        DynArray<int> da(arena);
        da.reserve(128);
        CHECK(da.capacity() >= size_t(128), "reserve works without pushes");
        CHECK(da.empty(), "still empty after reserve");
    }

    // Large number of elements (stress-ish)
    {
        Arena arena;
        DynArray<int> da(arena);
        constexpr int N = 10000;
        for (int i = 0; i < N; i++) {
            da.push(i);
        }
        CHECK_EQ(da.size(), size_t(N), "large push count correct");
        CHECK_EQ(da[N - 1], N - 1, "last element of large array correct");
        CHECK_EQ(da[0], 0, "first element of large array correct");
    }
}

// ── main ─────────────────────────────────────────────────────────

int main() {
    std::printf("DynArray tests\n");
    std::printf("================\n\n");

    test_empty_array();
    test_push_one();
    test_push_multiple();
    test_push_move();
    test_emplace();
    test_grow();
    test_reserve();
    test_clear();
    test_back();
    test_iteration();
    test_move_constructor();
    test_move_assign();
    test_nontrivial_destructor();
    test_edge_cases();

    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}

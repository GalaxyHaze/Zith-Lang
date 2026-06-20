#include "../test-common.hpp"
#include "memory/flat_map.hpp"

#include <cstdint>
#include <cstdio>
#include <numeric>
#include <string>
#include <string_view>
#include <utility>

using zith::memory::FlatMap;

// ── 1. empty_map ─────────────────────────────────────────────────

static void test_empty_map() {
    FlatMap<int, int> map;

    CHECK_EQ(map.size(), size_t(0), "default-constructed map has size 0");
    CHECK(map.empty(), "default-constructed map is empty");
}

// ── 2. insert_and_get ────────────────────────────────────────────

static void test_insert_and_get() {
    FlatMap<int, int> map;

    map.insert(42, 100);

    int *v = map.get(42);
    CHECK(v != nullptr, "get(42) returns non-null after insert");
    CHECK_EQ(*v, 100, "retrieved value is 100");

    // Non-existing key
    CHECK(map.get(99) == nullptr, "get(99) returns nullptr for missing key");
}

// ── 3. insert_overwrite ──────────────────────────────────────────

static void test_insert_overwrite() {
    FlatMap<int, int> map;

    map.insert(1, 10);
    CHECK_EQ(*map.get(1), 10, "initial value is 10");

    // Overwrite
    map.insert(1, 20);
    CHECK_EQ(*map.get(1), 20, "value overwritten to 20");
    CHECK_EQ(map.size(), size_t(1), "size stays 1 after overwrite");

    // Overwrite via returned reference
    int &ref = map.insert(1, 30);
    CHECK_EQ(ref, 30, "returned reference is 30");
    CHECK_EQ(*map.get(1), 30, "value is 30 after second overwrite");
}

// ── 4. contains ──────────────────────────────────────────────────

static void test_contains() {
    FlatMap<int, int> map;

    CHECK(!map.contains(1), "empty map does not contain 1");

    map.insert(1, 10);
    map.insert(2, 20);
    map.insert(3, 30);

    CHECK(map.contains(1), "map contains key 1 after insert");
    CHECK(map.contains(2), "map contains key 2 after insert");
    CHECK(map.contains(3), "map contains key 3 after insert");
    CHECK(!map.contains(4), "map does not contain key 4 (never inserted)");
    CHECK(!map.contains(0), "map does not contain key 0 (never inserted)");
}

// ── 5. operator_bracket ──────────────────────────────────────────

static void test_operator_bracket() {
    FlatMap<int, int> map;

    // operator[] on missing key inserts default
    int &v = map[42];
    CHECK_EQ(v, 0, "default-constructed value via operator[] is 0");
    CHECK_EQ(map.size(), size_t(1), "size is 1 after operator[] on missing key");
    CHECK(map.contains(42), "contains key 42 after operator[]");

    // operator[] on existing key returns existing value
    v = 99;
    CHECK_EQ(map[42], 99, "operator[] returns 99 after assignment");
    CHECK_EQ(map.size(), size_t(1), "size unchanged after assigning via operator[]");

    // Multiple operator[] insertions
    map[1] = 10;
    map[2] = 20;
    map[3] = 30;
    CHECK_EQ(map.size(), size_t(4), "size is 4 after bracket-inserting 3 more keys");
    CHECK_EQ(map[1], 10, "map[1] is 10");
    CHECK_EQ(map[2], 20, "map[2] is 20");
    CHECK_EQ(map[3], 30, "map[3] is 30");
}

// ── 6. erase ─────────────────────────────────────────────────────

static void test_erase() {
    FlatMap<int, int> map;

    map.insert(1, 10);
    map.insert(2, 20);
    map.insert(3, 30);
    CHECK_EQ(map.size(), size_t(3), "size is 3 before erase");

    map.erase(2);
    CHECK_EQ(map.size(), size_t(2), "size is 2 after erasing key 2");
    CHECK(map.contains(1), "key 1 still present");
    CHECK(!map.contains(2), "key 2 erased");
    CHECK(map.contains(3), "key 3 still present");
    CHECK(map.get(2) == nullptr, "get(2) returns nullptr after erase");

    // Erase remaining keys one by one
    map.erase(1);
    CHECK_EQ(map.size(), size_t(1), "size is 1 after erasing key 1");
    CHECK(!map.contains(1), "key 1 erased");

    map.erase(3);
    CHECK_EQ(map.size(), size_t(0), "size is 0 after erasing all keys");
    CHECK(map.empty(), "map empty after erasing all keys");
}

// ── 7. erase_nonexistent ─────────────────────────────────────────

static void test_erase_nonexistent() {
    FlatMap<int, int> map;

    map.insert(1, 10);
    map.insert(2, 20);

    // Erasing a key that doesn't exist is a no-op
    map.erase(99);
    CHECK_EQ(map.size(), size_t(2), "size unchanged after erasing nonexistent key");
    CHECK(map.contains(1), "key 1 still present");
    CHECK(map.contains(2), "key 2 still present");

    // Erasing from empty map is also a no-op (no crash)
    FlatMap<int, int> empty;
    empty.erase(42);
    CHECK(empty.empty(), "erasing from empty map is a no-op");
}

// ── 8. clear ─────────────────────────────────────────────────────

static void test_clear() {
    FlatMap<int, int> map;

    map.insert(1, 10);
    map.insert(2, 20);
    map.insert(3, 30);
    CHECK_EQ(map.size(), size_t(3), "size is 3 before clear");

    map.clear();

    CHECK_EQ(map.size(), size_t(0), "size is 0 after clear");
    CHECK(map.empty(), "map is empty after clear");
    CHECK(!map.contains(1), "key 1 gone after clear");
    CHECK(!map.contains(2), "key 2 gone after clear");
    CHECK(!map.contains(3), "key 3 gone after clear");
    CHECK(map.get(1) == nullptr, "get(1) returns nullptr after clear");

    // Can insert again after clear — slots are reused
    map.insert(4, 40);
    CHECK_EQ(map.size(), size_t(1), "size is 1 after insert after clear");
    CHECK_EQ(*map.get(4), 40, "new key-value inserted after clear");
}

// ── 9. many_elements ─────────────────────────────────────────────

static void test_many_elements() {
    FlatMap<int, int> map;
    constexpr int N = 1000;

    for (int i = 0; i < N; i++) {
        map.insert(i, i * 2);
    }

    CHECK_EQ(map.size(), size_t(N), "size is 1000 after 1000 inserts");

    // Verify all retrievable
    for (int i = 0; i < N; i++) {
        int *v = map.get(i);
        CHECK(v != nullptr, "get returns non-null for inserted key");
        if (v) {
            CHECK_EQ(*v, i * 2, "value matches i*2");
        }
    }

    // Verify non-existing keys still don't exist
    CHECK(!map.contains(-1), "does not contain -1");
    CHECK(!map.contains(N + 1), "does not contain N+1");
}

// ── 10. string_keys ──────────────────────────────────────────────

static void test_string_keys() {
    FlatMap<std::string, int> map;

    // Insert with std::string
    map.insert(std::string("hello"), 1);
    map.insert(std::string("world"), 2);
    map.insert(std::string("foo"), 3);

    CHECK_EQ(map.size(), size_t(3), "size is 3 after string inserts");
    CHECK_EQ(*map.get(std::string("hello")), 1, "get with std::string works");

    // Transparent lookup: get with string_view (const char* is ambiguous for std::string keys)
    int *v = map.get(std::string_view("hello"));
    CHECK(v != nullptr, "transparent get with string_view finds key");
    CHECK_EQ(*v, 1, "transparent get returns correct value");

    v = map.get(std::string_view("world"));
    CHECK(v != nullptr, "transparent get for 'world' works");
    CHECK_EQ(*v, 2, "transparent get returns 2");

    // contains with transparent lookup
    CHECK(map.contains(std::string_view("foo")), "contains with string_view finds 'foo'");
    CHECK(!map.contains(std::string_view("nonexistent")), "contains with string_view returns false for missing");

    // operator[] with string_view
    CHECK_EQ(map[std::string_view("hello")], 1, "operator[] with string_view on existing key");
    map[std::string_view("new_key")] = 42;
    CHECK_EQ(map.size(), size_t(4), "operator[] with string_view inserts new key");
    CHECK_EQ(*map.get(std::string_view("new_key")), 42, "inserted via operator[] with string_view");
}

// ── 11. iteration ────────────────────────────────────────────────

static void test_iteration() {
    FlatMap<int, int> map;

    map.insert(1, 10);
    map.insert(2, 20);
    map.insert(3, 30);
    map.insert(4, 40);
    map.insert(5, 50);

    // Sum values via range-for
    int sum_values = 0;
    int count = 0;
    for (const auto &[k, v] : map) {
        sum_values += v;
        count++;
    }
    CHECK_EQ(count, 5, "iteration visited 5 elements");
    CHECK_EQ(sum_values, 150, "sum of values is 10+20+30+40+50 = 150");

    // Iterator pair loop
    int sum_keys = 0;
    for (auto it = map.begin(); it != map.end(); ++it) {
        auto [k, v] = *it;
        sum_keys += k;
    }
    CHECK_EQ(sum_keys, 15, "sum of keys is 1+2+3+4+5 = 15");

    // Check that iteration visits each key exactly once
    bool seen[6] = {false};
    for (const auto &[k, v] : map) {
        CHECK(k >= 1 && k <= 5, "key in expected range");
        CHECK(!seen[k], "each key visited at most once");
        seen[k] = true;
    }
    for (int i = 1; i <= 5; i++) {
        CHECK(seen[i], "key %d was visited by iteration");
    }

    // Empty map iteration (only != operator available on iterator)
    FlatMap<int, int> empty;
    CHECK(!(empty.begin() != empty.end()), "begin == end for empty map");
    int empty_count = 0;
    for (const auto &[k, v] : empty) {
        (void)k;
        (void)v;
        empty_count++;
    }
    CHECK_EQ(empty_count, 0, "iteration over empty map yields 0 elements");
}

// ── 12. move_constructor ─────────────────────────────────────────

static void test_move_constructor() {
    FlatMap<int, int> map;
    map.insert(1, 10);
    map.insert(2, 20);
    map.insert(3, 30);
    CHECK_EQ(map.size(), size_t(3), "source size is 3 before move");

    FlatMap<int, int> moved(std::move(map));

    // Moved-to map has the data
    CHECK_EQ(moved.size(), size_t(3), "moved-to map has size 3");
    CHECK_EQ(*moved.get(1), 10, "moved-to map has key 1 with value 10");
    CHECK_EQ(*moved.get(2), 20, "moved-to map has key 2 with value 20");
    CHECK_EQ(*moved.get(3), 30, "moved-to map has key 3 with value 30");

    // Source is empty after move
    CHECK_EQ(map.size(), size_t(0), "source size is 0 after move");
    CHECK(map.empty(), "source is empty after move");
    CHECK(!map.contains(1), "source does not contain key 1 after move");
    CHECK(map.get(1) == nullptr, "source get returns nullptr after move");

    // Moved-to map can still be modified
    moved.insert(4, 40);
    CHECK_EQ(moved.size(), size_t(4), "moved-to map grows after move");
    CHECK_EQ(*moved.get(4), 40, "new key in moved-to map is correct");

    // Source can be reused (assign new values)
    map.insert(5, 50);
    CHECK_EQ(map.size(), size_t(1), "reused source map works after move");
    CHECK_EQ(*map.get(5), 50, "reused source map key-value correct");
}

// ── 13. move_assign ──────────────────────────────────────────────

static void test_move_assign() {
    FlatMap<int, int> map1;
    map1.insert(1, 10);
    map1.insert(2, 20);
    map1.insert(3, 30);

    FlatMap<int, int> map2;
    map2.insert(10, 100);
    map2.insert(20, 200);

    // Move-assign
    map2 = std::move(map1);

    // map2 now has map1's data
    CHECK_EQ(map2.size(), size_t(3), "move-assigned target has size 3");
    CHECK_EQ(*map2.get(1), 10, "move-assigned target has key 1 value 10");
    CHECK_EQ(*map2.get(2), 20, "move-assigned target has key 2 value 20");
    CHECK_EQ(*map2.get(3), 30, "move-assigned target has key 3 value 30");
    CHECK(!map2.contains(10), "old map2 data gone after move-assign");
    CHECK(!map2.contains(20), "old map2 data gone after move-assign");

    // map1 is empty after move
    CHECK(map1.empty(), "source is empty after move-assign");
    CHECK_EQ(map1.size(), size_t(0), "source size is 0 after move-assign");

    // Self-assignment safety
    map2 = std::move(map2);
    CHECK_EQ(map2.size(), size_t(3), "self-move-assign leaves size unchanged");
    CHECK_EQ(*map2.get(1), 10, "self-move-assign preserves data");
}

// ── 14. reserve ──────────────────────────────────────────────────

static void test_reserve() {
    FlatMap<int, int> map;

    // Pre-reserve capacity
    map.reserve(64);

    // Should be able to insert 45 elements (64 * 0.7 = 44.8) without rehash
    for (int i = 0; i < 44; i++) {
        map.insert(i, i * 10);
    }

    CHECK_EQ(map.size(), size_t(44), "44 elements inserted after reserve");
    for (int i = 0; i < 44; i++) {
        CHECK_EQ(*map.get(i), i * 10, "all reserved elements correct");
    }

    // One more insert should trigger rehash (load factor > 0.7)
    map.insert(44, 440);
    CHECK_EQ(map.size(), size_t(45), "45 elements after crossing load factor");
    CHECK_EQ(*map.get(44), 440, "new element correct");

    // All previous elements still intact after rehash
    for (int i = 0; i < 44; i++) {
        CHECK_EQ(*map.get(i), i * 10, "all elements preserved after rehash");
    }
}

// ── 15. string_view_key (additional: transparent lookup with string_view keys) ──

static void test_string_view_key() {
    FlatMap<std::string_view, int> map;

    // Insert with string_view
    map.insert(std::string_view("alpha"), 1);
    map.insert(std::string_view("beta"), 2);

    CHECK_EQ(map.size(), size_t(2), "size is 2 with string_view keys");

    // Look up by string_view (transparent)
    int *v = map.get(std::string_view("alpha"));
    CHECK(v != nullptr, "transparent get with string_view on string_view map");
    CHECK_EQ(*v, 1, "value correct for transparent lookup");

    // Look up by std::string (transparent)
    std::string key = "beta";
    v = map.get(key);
    CHECK(v != nullptr, "transparent get with std::string on string_view map");
    CHECK_EQ(*v, 2, "value correct for std::string lookup");

    // contains with transparent
    CHECK(map.contains(std::string_view("alpha")), "contains with string_view on string_view map");
    CHECK(!map.contains(std::string_view("gamma")), "contains returns false for missing key");
}

// ── 16. erase_and_reinsert (slots with FM_ERASED are reused) ─────

static void test_erase_and_reinsert() {
    FlatMap<int, int> map;

    map.insert(1, 10);
    map.insert(2, 20);
    map.insert(3, 30);

    map.erase(2);
    CHECK(!map.contains(2), "key 2 erased");

    // Re-insert a key that previously existed — should reuse the erased slot
    map.insert(2, 200);
    CHECK(map.contains(2), "key 2 re-inserted");
    CHECK_EQ(*map.get(2), 200, "re-inserted key 2 has new value 200");
    CHECK_EQ(map.size(), size_t(3), "size is 3 after erase and re-insert");

    // Re-insert a completely new key after an erase
    map.insert(4, 40);
    CHECK_EQ(map.size(), size_t(4), "size is 4 after adding new key post-erase");
    CHECK_EQ(*map.get(4), 40, "new key 4 value correct");
}

// ── 17. copy_constructor (the API supports copy) ─────────────────

static void test_copy_constructor() {
    FlatMap<int, int> map;
    map.insert(1, 10);
    map.insert(2, 20);

    FlatMap<int, int> copy(map);

    CHECK_EQ(copy.size(), size_t(2), "copied map has same size");
    CHECK_EQ(*copy.get(1), 10, "copied map has key 1");
    CHECK_EQ(*copy.get(2), 20, "copied map has key 2");

    // Original unchanged
    CHECK_EQ(map.size(), size_t(2), "original unchanged after copy");
    CHECK_EQ(*map.get(1), 10, "original key 1 unchanged");

    // Modifying copy does not affect original
    copy.insert(3, 30);
    CHECK_EQ(copy.size(), size_t(3), "copy can be modified independently");
    CHECK_EQ(map.size(), size_t(2), "original size unchanged after modifying copy");
    CHECK(!map.contains(3), "original does not have key 3");
}

// ── 18. copy_assign ──────────────────────────────────────────────

static void test_copy_assign() {
    FlatMap<int, int> map1;
    map1.insert(1, 10);
    map1.insert(2, 20);

    FlatMap<int, int> map2;
    map2.insert(10, 100);

    map2 = map1;

    CHECK_EQ(map2.size(), size_t(2), "copy-assigned map has source size");
    CHECK_EQ(*map2.get(1), 10, "copy-assigned map has key 1");
    CHECK_EQ(*map2.get(2), 20, "copy-assigned map has key 2");
    CHECK(!map2.contains(10), "old data gone after copy-assign");

    // Self-assignment safety
    map2 = map2;
    CHECK_EQ(map2.size(), size_t(2), "self-copy-assign leaves size unchanged");
    CHECK_EQ(*map2.get(1), 10, "self-copy-assign preserves data");
}

// ── 19. large_string_keys ────────────────────────────────────────

static void test_large_string_keys() {
    FlatMap<std::string, int> map;

    // Insert many string keys to exercise rehashing with strings
    for (int i = 0; i < 500; i++) {
        std::string key = "key_" + std::to_string(i);
        map.insert(std::move(key), i);
    }

    CHECK_EQ(map.size(), size_t(500), "500 string keys inserted");

    for (int i = 0; i < 500; i++) {
        std::string key = "key_" + std::to_string(i);
        CHECK(map.contains(key), (std::string("contains string key ") + std::to_string(i)).c_str());
        CHECK_EQ(*map.get(key), i, (std::string("value correct for string key ") + std::to_string(i)).c_str());
    }

    // Transparent lookup for all (via string_view to avoid ambiguous hash)
    for (int i = 0; i < 500; i++) {
        std::string key = "key_" + std::to_string(i);
        CHECK_EQ(*map.get(std::string_view(key)), i, "transparent lookup works for all string keys");
    }
}

// ── main ─────────────────────────────────────────────────────────

int main() {
    std::printf("FlatMap tests\n");
    std::printf("==============\n\n");

    test_empty_map();
    test_insert_and_get();
    test_insert_overwrite();
    test_contains();
    test_operator_bracket();
    test_erase();
    test_erase_nonexistent();
    test_clear();
    test_many_elements();
    test_string_keys();
    test_iteration();
    test_move_constructor();
    test_move_assign();
    test_reserve();
    test_string_view_key();
    test_erase_and_reinsert();
    test_copy_constructor();
    test_copy_assign();
    test_large_string_keys();

    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}

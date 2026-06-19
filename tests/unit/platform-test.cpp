#include "../test-common.hpp"
#include "support/platform.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static struct Cleanup {
    std::vector<std::string> dirs;
    ~Cleanup() {
        for (auto &d : dirs) {
            std::error_code ec;
            fs::remove_all(d, ec);
        }
    }
} cleanup;

static std::string make_tmp_dir() {
    auto base        = fs::temp_directory_path();
    std::string tmpl = (base / "zith_platform_test_XXXXXX").string();
    char *d          = zith::support::mkdtemp(tmpl.data());
    if (d) {
        cleanup.dirs.push_back(d);
        return d;
    }
    return {};
}

static void test_mkdtemp_basic() {
    auto dir = make_tmp_dir();
    CHECK(!dir.empty(), "mkdtemp returns non-empty path");
    if (dir.empty())
        return;

    CHECK(fs::is_directory(dir), "temporary directory exists on disk");
    CHECK(dir.find("XXXXXX") == std::string::npos, "XXXXXX was replaced with random characters");
}

static void test_mkdtemp_invalid() {
    char tmpl[] = "/tmp/no_template_suffix";
    char *d     = zith::support::mkdtemp(tmpl);
    CHECK(d == nullptr, "mkdtemp returns nullptr when template lacks XXXXXX");
}

static void test_mkdtemp_empty() {
    char tmpl[] = "";
    char *d     = zith::support::mkdtemp(tmpl);
    CHECK(d == nullptr, "mkdtemp returns nullptr for empty template");
}

static void test_enable_virtual_terminal() {
    zith::support::enableVirtualTerminal();
    CHECK(true, "enableVirtualTerminal() completed without crash");
}

int main() {
    std::printf("platform tests\n");
    std::printf("================\n\n");

    test_mkdtemp_basic();
    test_mkdtemp_invalid();
    test_mkdtemp_empty();
    test_enable_virtual_terminal();

    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}

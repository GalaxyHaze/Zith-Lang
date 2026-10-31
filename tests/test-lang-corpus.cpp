// Runs the Zith-- language corpus under `tests/lang/` through the real
// `zithc test` command and asserts every discovered `#[test]` passes.
//
// The corpus is staged into a scratch directory in the build tree first because
// `zithc` writes its `cache/` and `target/` artifacts next to the source files;
// running in place would leave build output inside the checked-in tree.
#include "test-common.hpp"

#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace {

namespace fs = std::filesystem;

std::string readFile(const fs::path &path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

// Extract the value preceding `label` in the `N passed; M failed; T total`
// summary line. Returns -1 when the label is absent or not a plain integer.
int summaryValue(std::string_view text, std::string_view label) {
    const auto labelPos = text.rfind(label);
    if (labelPos == std::string_view::npos)
        return -1;
    const auto space = text.rfind(' ', labelPos);
    if (space == std::string_view::npos)
        return -1;
    const auto start = text.rfind(' ', space > 0 ? space - 1 : 0);
    const std::string_view digits =
        text.substr(start == std::string_view::npos ? 0 : start + 1, space - (start + 1));
    int value            = -1;
    const auto *begin    = digits.data();
    const auto *end      = digits.data() + digits.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc{} || ptr != end)
        return -1;
    return value;
}

void test_lang_corpus() {
#ifndef ZITHC_BINARY
    CHECK(true, "zithc language corpus is not built without the CLI target");
#else
    const fs::path root = fs::path(ZITH_LANG_CORPUS_WORKDIR);
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    fs::copy(fs::path(ZITH_LANG_CORPUS_DIR), root / "lang", fs::copy_options::recursive, ec);
    CHECK(!ec, "the language corpus is staged into a scratch directory");

    const fs::path stdoutPath = root / "lang.stdout";
    const fs::path stderrPath = root / "lang.stderr";
    const std::string command = std::string("\"") + ZITHC_BINARY + "\" --include \"" +
                                ZITH_STDLIB_DIR + "\" test \"" + (root / "lang").string() +
                                "\" > \"" + stdoutPath.string() + "\" 2> \"" + stderrPath.string() +
                                "\"";
    const int status          = std::system(command.c_str());
#ifndef _WIN32
    const int exitCode = (status != -1 && WIFEXITED(status)) ? WEXITSTATUS(status) : -1;
#else
    const int exitCode = status;
#endif
    const std::string out = readFile(stdoutPath);
    const std::string err = readFile(stderrPath);

    CHECK_EQ(exitCode, 0, "the language corpus passes through `zithc test`");
    if (exitCode != 0) {
        std::cerr << "    stdout:\n" << out << "\n    stderr:\n" << err << "\n";
    }
    CHECK(out.find("0 failed") != std::string::npos, "the corpus reports zero failures");

    const int passed = summaryValue(out, "passed");
    const int failed = summaryValue(out, "failed");
    const int total  = summaryValue(out, "total");
    CHECK(passed > 0, "the corpus reports at least one passing test");
    CHECK_EQ(failed, 0, "the corpus summary counts no failed tests");
    CHECK(passed == total, "every discovered corpus test passed");
    CHECK(out.find("arithmetic::addition_and_subtraction") != std::string::npos,
          "the corpus reports a known test name");
#endif
}

} // namespace

TEST_MAIN(lang_corpus)

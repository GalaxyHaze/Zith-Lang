#include "common/test-log-protocol.hpp"
#include "common/test-output-buffer.hpp"
#include "test-common.hpp"

#include <algorithm>
#include <string>

namespace {

void appendLog(zith::common::TestOutputBuffer &buffer, const std::string &testName,
               const std::string &text) {
    buffer.append(zith::common::kTestLogBeginPrefix);
    buffer.append(testName);
    buffer.append("\n");
    buffer.append(text);
    buffer.append(zith::common::kTestLogEndMarker);
}

void test_test_output_buffer() {
    zith::common::TestOutputBuffer buffer;
    buffer.append("unframed sentinel\n");
    buffer.append(zith::common::kTestLogContextPrefix);
    buffer.append("suite::first\n");

    appendLog(buffer, "suite::first", std::string(2048U, 'x') + "\n");
    appendLog(buffer, "suite::first", std::string(2048U, 'y') + "\n");

    buffer.append(zith::common::kTestLogContextPrefix);
    buffer.append("suite::second\n");
    appendLog(buffer, "suite::second", std::string(zith::common::kTestLogCapacity, 'z'));

    const auto output = buffer.take();
    CHECK(output.find("unframed sentinel\n") != std::string::npos, "unframed output is preserved");
    CHECK_EQ(static_cast<std::size_t>(std::count(output.begin(), output.end(), 'x')), 2048U,
             "the first log call is retained");
    CHECK_EQ(static_cast<std::size_t>(std::count(output.begin(), output.end(), 'y')), 2047U,
             "multiple log calls share the per-test capacity");
    CHECK_EQ(static_cast<std::size_t>(std::count(output.begin(), output.end(), 'z')),
             zith::common::kTestLogCapacity, "a new test receives the full log capacity");
}

} // namespace

TEST_MAIN(test_output_buffer)

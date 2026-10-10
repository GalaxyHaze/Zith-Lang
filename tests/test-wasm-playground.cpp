#include "test-common.hpp"

#include <cstddef>
#include <string>
#include <string_view>

extern "C" int zith_run_source(const char *ptr, int len);
extern "C" const char *zith_last_diagnostics_json_ptr();
extern "C" int zith_last_diagnostics_json_len();

namespace {

std::string host_output;
std::string host_errors;

auto diagnosticsJson() -> std::string {
    const char *ptr = zith_last_diagnostics_json_ptr();
    const int len   = zith_last_diagnostics_json_len();
    if (ptr == nullptr || len < 0)
        return {};
    return std::string(ptr, static_cast<std::size_t>(len));
}

auto runSource(std::string_view source) -> int {
    return zith_run_source(source.data(), static_cast<int>(source.size()));
}

} // namespace

extern "C" void host_write(int stream, const char *ptr, int len) {
    if (ptr == nullptr || len <= 0)
        return;
    auto &output = stream == 1 ? host_output : host_errors;
    output.append(ptr, static_cast<std::size_t>(len));
}

void test_wasm_playground_structured_diagnostics() {
    const auto validStatus = runSource("fn main(): i32 {\n    42\n}\n");
    CHECK_EQ(validStatus, 0, "valid program keeps the existing success status");
    CHECK_EQ(diagnosticsJson(), std::string("{\"diagnostics\":[]}"),
             "successful calls expose an empty diagnostics array");

    const auto compileStatus = runSource("fn main() {\n    let x: MissingType = 1;\n}\n");
    CHECK_EQ(compileStatus, 1, "compile failure keeps the existing status code");
    const auto compileJson = diagnosticsJson();
    CHECK(compileJson.find("\"diagnostics\":[{") != std::string::npos,
          "compile diagnostics are represented in the JSON array");
    CHECK(compileJson.find("\"severity\":\"error\"") != std::string::npos,
          "compile diagnostic includes severity");
    CHECK(compileJson.find("\"message\":") != std::string::npos,
          "compile diagnostic includes message");
    CHECK(compileJson.find("\"code\":") != std::string::npos,
          "compile diagnostic includes its numeric code");
    CHECK(compileJson.find("\"span\":{\"start\":") != std::string::npos &&
              compileJson.find("\"end\":") != std::string::npos,
          "compile diagnostic includes its source byte span");

    const auto trapStatus = runSource("fn main(): i32 {\n"
                                      "    let values: [1]i32 = [7];\n"
                                      "    let index: i32 = 2;\n"
                                      "    raw values[index]\n"
                                      "}\n");
    CHECK_EQ(trapStatus, 3, "runtime trap keeps the existing status code");
    const auto trapJson = diagnosticsJson();
    CHECK(trapJson.find("\"severity\":\"error\"") != std::string::npos &&
              trapJson.find("\"message\":") != std::string::npos,
          "runtime trap includes an error severity and message");

    const auto unsupportedStatus = runSource("fn main() {\n"
                                             "    let x = 10;\n"
                                             "    _ = *(&x);\n"
                                             "}\n");
    CHECK_EQ(unsupportedStatus, 5, "unsupported construct keeps the existing status code");
    const auto unsupportedJson = diagnosticsJson();
    CHECK(unsupportedJson.find("\"severity\":\"error\"") != std::string::npos &&
              unsupportedJson.find("\"message\":") != std::string::npos,
          "unsupported construct includes an error severity and message");
}

TEST_MAIN(wasm_playground_structured_diagnostics)

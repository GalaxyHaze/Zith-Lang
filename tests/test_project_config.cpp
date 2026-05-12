#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../impl/cli/project_config/project_config.hpp"

namespace {
void write_file(const std::filesystem::path &path, const std::string &content) {
    std::ofstream out(path);
    REQUIRE(out.good());
    out << content;
}
} // namespace

using namespace zith::cli::project_config;

TEST_CASE("project parser loads required fields and resolves entry path", "[cli][project]") {
    const auto root = std::filesystem::temp_directory_path() / "zith_project_test_valid";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "subdir");
    const auto file = root / "subdir" / "ZithProject.toml";
    write_file(file, "name='app'\nversion='1.2.3'\nentry='src/main.zith'\n");

    ZithProject proj;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    REQUIRE(try_load_project_from_path(proj, file.string(), &warnings, &errors));
    REQUIRE(errors.empty());
    REQUIRE(warnings.empty());
    REQUIRE(proj.name == "app");
    REQUIRE(proj.version == "1.2.3");
    REQUIRE(proj.entry == (file.parent_path() / "src/main.zith").lexically_normal().string());
}

TEST_CASE("project parser warns unknown keys", "[cli][project]") {
    const auto root = std::filesystem::temp_directory_path() / "zith_project_test_warn";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto file = root / "ZithProject.toml";
    write_file(file, "name='app'\nversion='0.1.0'\nentry='main.zith'\nmystery=1\n");

    ZithProject proj;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    REQUIRE(try_load_project_from_path(proj, file.string(), &warnings, &errors));
    REQUIRE(errors.empty());
    REQUIRE(warnings.size() == 1);
}

TEST_CASE("project parser rejects invalid and out-of-range fields", "[cli][project]") {
    const auto root = std::filesystem::temp_directory_path() / "zith_project_test_invalid";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto file = root / "ZithProject.toml";
    write_file(file,
               "name=1\nversion='0.1.0'\nentry='main.zith'\nopt_level=8\ndebug_level='high'\n");

    ZithProject proj;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    REQUIRE_FALSE(try_load_project_from_path(proj, file.string(), &warnings, &errors));
    REQUIRE(errors.size() >= 3);
}

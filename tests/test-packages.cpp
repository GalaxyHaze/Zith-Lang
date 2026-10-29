#include "packages/manifest.hpp"
#include "test-common.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <toml++/toml.hpp>

namespace {

namespace fs = std::filesystem;

struct TemporaryManifest {
    fs::path root = fs::temp_directory_path() /
                    ("zith-package-manifest-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    TemporaryManifest() {
        fs::create_directories(root);
    }

    ~TemporaryManifest() {
        fs::remove_all(root);
    }

    fs::path write(std::string_view contents) const {
        const auto path = root / "ZithProject.toml";
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << contents;
        return path;
    }
};

void test_manifest_parses_local_package_declaration() {
    TemporaryManifest temporary;
    const auto manifestPath = temporary.write("[project]\n"
                                              "name = \"application\"\n"
                                              "version = \"0.1.0\"\n"
                                              "\n"
                                              "[dependencies]\n"
                                              "logging = \"../vendor/logging\"\n"
                                              "\n"
                                              "[package]\n"
                                              "module_root = \"modules\"\n");

    const auto parsed = toml::parse_file(manifestPath.string());
    const auto result = zith::packages::parseManifest(parsed, manifestPath);

    CHECK(result.isOk(), "valid package manifest is accepted");
    if (!result)
        return;

    const auto &manifest = result.value();
    CHECK_EQ(manifest.name, std::string("application"), "manifest package name is parsed");
    CHECK_EQ(manifest.version, std::string("0.1.0"), "manifest package version is parsed");
    CHECK_EQ(manifest.moduleRoot, temporary.root / "modules",
             "module root is resolved relative to the manifest");
    CHECK_EQ(manifest.dependencies.size(), 1U, "one local dependency is parsed");
    if (!manifest.dependencies.empty()) {
        CHECK_EQ(manifest.dependencies[0].name, std::string("logging"),
                 "dependency name is preserved");
        CHECK_EQ(manifest.dependencies[0].path, temporary.root.parent_path() / "vendor" / "logging",
                 "dependency path is resolved relative to the declaring manifest");
    }
}

void test_manifest_defaults_module_root_to_src() {
    TemporaryManifest temporary;
    const auto manifestPath = temporary.write("[project]\n"
                                              "name = \"library\"\n"
                                              "version = \"1\"\n");

    const auto parsed = toml::parse_file(manifestPath.string());
    const auto result = zith::packages::parseManifest(parsed, manifestPath);

    CHECK(result.isOk(), "manifest without optional package fields is accepted");
    if (result) {
        CHECK_EQ(result.value().moduleRoot, temporary.root / "src",
                 "module root defaults to src relative to the manifest");
        CHECK(result.value().dependencies.empty(), "dependencies default to an empty list");
    }
}

void test_manifest_requires_non_empty_identity() {
    const auto manifestPath   = fs::path("/tmp/ZithProject.toml");
    const auto missingProject = toml::parse("[dependencies]\n"
                                            "logging = \"../logging\"\n");
    const auto emptyName      = toml::parse("[project]\n"
                                            "name = \"\"\n"
                                            "version = \"1\"\n");
    const auto emptyVersion   = toml::parse("[project]\n"
                                            "name = \"application\"\n"
                                            "version = \"\"\n");
    const auto nonStringName  = toml::parse("[project]\n"
                                            "name = 42\n"
                                            "version = \"1\"\n");

    CHECK(zith::packages::parseManifest(missingProject, manifestPath).isError(),
          "package identity requires a project table");
    CHECK(zith::packages::parseManifest(emptyName, manifestPath).isError(),
          "package name must not be empty");
    CHECK(zith::packages::parseManifest(emptyVersion, manifestPath).isError(),
          "package version must not be empty");
    CHECK(zith::packages::parseManifest(nonStringName, manifestPath).isError(),
          "package name must be a string");
}

void test_manifest_rejects_invalid_package_fields() {
    const auto manifestPath              = fs::path("/tmp/ZithProject.toml");
    const auto invalidDependencies       = toml::parse("dependencies = \"../logging\"\n"
                                                       "\n"
                                                       "[project]\n"
                                                       "name = \"application\"\n"
                                                       "version = \"1\"\n");
    const auto invalidModuleRoot         = toml::parse("[project]\n"
                                                       "name = \"application\"\n"
                                                       "version = \"1\"\n"
                                                       "\n"
                                                       "[package]\n"
                                                       "module_root = 42\n");
    const auto standardLibraryDependency = toml::parse("[project]\n"
                                                       "name = \"application\"\n"
                                                       "version = \"1\"\n"
                                                       "\n"
                                                       "[dependencies]\n"
                                                       "std = \"../std\"\n");
    const auto missingVersion            = toml::parse("[project]\n"
                                                       "name = \"application\"\n");
    const auto absoluteDependency        = toml::parse("[project]\n"
                                                       "name = \"application\"\n"
                                                       "version = \"1\"\n"
                                                       "\n"
                                                       "[dependencies]\n"
                                                       "logging = \"/vendor/logging\"\n");
    const auto structuredDependency      = toml::parse("[project]\n"
                                                       "name = \"application\"\n"
                                                       "version = \"1\"\n"
                                                       "\n"
                                                       "[dependencies.logging]\n"
                                                       "path = \"../logging\"\n");
    const auto absoluteModuleRoot        = toml::parse("[project]\n"
                                                       "name = \"application\"\n"
                                                       "version = \"1\"\n"
                                                       "\n"
                                                       "[package]\n"
                                                       "module_root = \"/modules\"\n");

    CHECK(zith::packages::parseManifest(invalidDependencies, manifestPath).isError(),
          "dependencies must be a table");
    CHECK(zith::packages::parseManifest(invalidModuleRoot, manifestPath).isError(),
          "module_root must be a path string");
    CHECK(zith::packages::parseManifest(standardLibraryDependency, manifestPath).isError(),
          "the bundled standard library cannot be declared as a package dependency");
    CHECK(zith::packages::parseManifest(missingVersion, manifestPath).isError(),
          "package version is required");
    CHECK(zith::packages::parseManifest(absoluteDependency, manifestPath).isError(),
          "dependency paths must be relative");
    CHECK(zith::packages::parseManifest(structuredDependency, manifestPath).isError(),
          "dependency aliases and structured declarations are unsupported");
    CHECK(zith::packages::parseManifest(absoluteModuleRoot, manifestPath).isError(),
          "module root must be relative");
}

void test_packages() {
    test_manifest_parses_local_package_declaration();
    test_manifest_defaults_module_root_to_src();
    test_manifest_requires_non_empty_identity();
    test_manifest_rejects_invalid_package_fields();
}

} // namespace

TEST_MAIN(packages)

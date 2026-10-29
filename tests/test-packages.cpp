#include "packages/lockfile.hpp"
#include "packages/manifest.hpp"
#include "packages/sha256.hpp"
#include "test-common.hpp"

#include <algorithm>
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

zith::packages::Lockfile sampleLockfile() {
    constexpr std::string_view fingerprint =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    return {
        "application",
        "0.1.0",
        std::string(fingerprint),
        {"logging"},
        {{"terminal", "2", "../vendor/terminal", "src", std::string(fingerprint), {}},
         {"logging", "1", "vendor/logging", "modules", std::string(fingerprint), {"terminal"}}}};
}

void test_lockfile_serialization_is_deterministic() {
    auto lockfile = sampleLockfile();
    auto first    = zith::packages::serializeLockfile(lockfile);
    std::reverse(lockfile.packages.begin(), lockfile.packages.end());
    std::reverse(lockfile.dependencies.begin(), lockfile.dependencies.end());
    auto second = zith::packages::serializeLockfile(lockfile);

    CHECK(first.isOk(), "valid lockfile serializes");
    CHECK(second.isOk(), "permuted valid lockfile serializes");
    if (first && second)
        CHECK_EQ(first.value(), second.value(),
                 "lockfile records are serialized in canonical order");
}

void test_lockfile_round_trips_versioned_graph() {
    const auto serialized = zith::packages::serializeLockfile(sampleLockfile());
    CHECK(serialized.isOk(), "sample lockfile serializes");
    if (!serialized)
        return;

    const auto parsed = toml::parse(serialized.value());
    const auto result = zith::packages::parseLockfile(parsed);
    CHECK(result.isOk(), "serialized lockfile parses and validates");
    if (!result)
        return;

    CHECK_EQ(result.value().projectName, std::string("application"),
             "project identity survives the round trip");
    CHECK_EQ(result.value().packages.size(), 2U, "the transitive graph is preserved");
    CHECK_EQ(result.value().packages[0].name, std::string("logging"),
             "serialized package records are sorted by name");
    CHECK_EQ(result.value().packages[0].dependencies[0], std::string("terminal"),
             "package dependency edges are preserved");

    auto emptyGraph = sampleLockfile();
    emptyGraph.dependencies.clear();
    emptyGraph.packages.clear();
    const auto emptySerialized = zith::packages::serializeLockfile(emptyGraph);
    CHECK(emptySerialized.isOk(), "an empty dependency graph serializes");
    if (emptySerialized) {
        const auto emptyParsed =
            zith::packages::parseLockfile(toml::parse(emptySerialized.value()));
        CHECK(emptyParsed.isOk(), "an empty dependency graph round-trips");
        if (emptyParsed)
            CHECK(emptyParsed.value().packages.empty(), "empty package records remain empty");
    }
}

void test_lockfile_rejects_invalid_metadata_and_edges() {
    constexpr std::string_view fingerprint =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    const auto unsupportedVersion  = toml::parse(std::string("[lock]\n"
                                                             "version = 2\n"
                                                             "\n"
                                                             "[project]\n"
                                                             "name = \"application\"\n"
                                                             "version = \"1\"\n"
                                                             "manifest_sha256 = \"") +
                                                 std::string(fingerprint) +
                                                 "\"\n"
                                                 "dependencies = []\n"
                                                 "package = []\n");
    const auto missingDependencies = toml::parse(std::string("[lock]\n"
                                                             "version = 1\n"
                                                             "\n"
                                                             "[project]\n"
                                                             "name = \"application\"\n"
                                                             "version = \"1\"\n"
                                                             "manifest_sha256 = \"") +
                                                 std::string(fingerprint) +
                                                 "\"\n"
                                                 "package = []\n");
    const auto absolutePackagePath = toml::parse(std::string("[lock]\n"
                                                             "version = 1\n"
                                                             "\n"
                                                             "[project]\n"
                                                             "name = \"application\"\n"
                                                             "version = \"1\"\n"
                                                             "manifest_sha256 = \"") +
                                                 std::string(fingerprint) +
                                                 "\"\n"
                                                 "dependencies = [\"logging\"]\n"
                                                 "\n"
                                                 "[[package]]\n"
                                                 "name = \"logging\"\n"
                                                 "version = \"1\"\n"
                                                 "path = \"/vendor/logging\"\n"
                                                 "module_root = \"src\"\n"
                                                 "manifest_sha256 = \"" +
                                                 std::string(fingerprint) +
                                                 "\"\n"
                                                 "dependencies = []\n");

    CHECK(zith::packages::parseLockfile(unsupportedVersion).isError(),
          "unsupported lockfile versions are rejected");
    CHECK(zith::packages::parseLockfile(missingDependencies).isError(),
          "required lockfile dependency arrays cannot be omitted");
    CHECK(zith::packages::parseLockfile(absolutePackagePath).isError(),
          "package paths in a lockfile must be project-relative");

    auto windowsAbsolutePath             = sampleLockfile();
    windowsAbsolutePath.packages[0].path = "C:/vendor/terminal";
    CHECK(zith::packages::serializeLockfile(windowsAbsolutePath).isError(),
          "Windows drive-prefixed paths are rejected on every host");

    const auto validLockfile = zith::packages::serializeLockfile(sampleLockfile());
    if (validLockfile) {
        auto nonNormalizedText = validLockfile.value();
        const auto pathOffset  = nonNormalizedText.find("vendor/logging");
        CHECK(pathOffset != std::string::npos, "serialized fixture contains the package path");
        if (pathOffset != std::string::npos) {
            nonNormalizedText.replace(pathOffset, std::string("vendor/logging").size(),
                                      "vendor//logging");
            CHECK(zith::packages::parseLockfile(toml::parse(nonNormalizedText)).isError(),
                  "lockfile paths must use normalized separators");
        }
    }

    auto duplicateName             = sampleLockfile();
    duplicateName.packages[1].name = duplicateName.packages[0].name;
    CHECK(zith::packages::serializeLockfile(duplicateName).isError(),
          "a package name cannot refer to multiple package records");

    auto duplicatePath             = sampleLockfile();
    duplicatePath.packages[1].path = duplicatePath.packages[0].path;
    CHECK(zith::packages::serializeLockfile(duplicatePath).isError(),
          "a package path cannot refer to multiple package records");

    auto projectNameCollision             = sampleLockfile();
    projectNameCollision.packages[1].name = projectNameCollision.projectName;
    CHECK(zith::packages::serializeLockfile(projectNameCollision).isError(),
          "a dependency cannot reuse the consumer project name");

    auto repeatedEdge = sampleLockfile();
    repeatedEdge.dependencies.push_back("logging");
    CHECK(zith::packages::serializeLockfile(repeatedEdge).isError(),
          "dependency edges cannot be duplicated");

    auto danglingEdge                     = sampleLockfile();
    danglingEdge.packages[0].dependencies = {"missing"};
    CHECK(zith::packages::serializeLockfile(danglingEdge).isError(),
          "lockfiles cannot refer to packages absent from the graph");

    auto cyclicGraph                     = sampleLockfile();
    cyclicGraph.packages[0].dependencies = {"logging"};
    CHECK(zith::packages::serializeLockfile(cyclicGraph).isError(),
          "lockfiles cannot contain dependency cycles");

    auto duplicateModuleRoot                   = sampleLockfile();
    duplicateModuleRoot.packages[0].path       = "vendor/logging/modules";
    duplicateModuleRoot.packages[0].moduleRoot = ".";
    CHECK(zith::packages::serializeLockfile(duplicateModuleRoot).isError(),
          "lockfiles cannot assign the same module root to multiple packages");

    auto invalidFingerprint                  = sampleLockfile();
    invalidFingerprint.projectManifestSha256 = "not-a-sha256";
    CHECK(zith::packages::serializeLockfile(invalidFingerprint).isError(),
          "manifest fingerprints must use lowercase SHA-256 hex");
}

void test_sha256_known_vectors() {
    CHECK_EQ(zith::packages::sha256Hex(""),
             std::string("e3b0c44298fc1c149afbf4c8996fb924"
                         "27ae41e4649b934ca495991b7852b855"),
             "SHA-256 hashes empty input");
    CHECK_EQ(zith::packages::sha256Hex("abc"),
             std::string("ba7816bf8f01cfea414140de5dae2223"
                         "b00361a396177a9cb410ff61f20015ad"),
             "SHA-256 hashes a short input");
    CHECK_EQ(zith::packages::sha256Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
             std::string("248d6a61d20638b8e5c026930c3e6039"
                         "a33ce45964ff2167f6ecedd419db06c1"),
             "SHA-256 hashes input crossing the one-block padding boundary");
    CHECK_EQ(zith::packages::sha256Hex(std::string(1'000'000U, 'a')),
             std::string("cdc76e5c9914fb9281a1c7e284d73e67"
                         "f1809a48a497200e046d39ccc7112cd0"),
             "SHA-256 hashes a long multi-block input");
}

void test_packages() {
    test_manifest_parses_local_package_declaration();
    test_manifest_defaults_module_root_to_src();
    test_manifest_requires_non_empty_identity();
    test_manifest_rejects_invalid_package_fields();
    test_sha256_known_vectors();
    test_lockfile_serialization_is_deterministic();
    test_lockfile_round_trips_versioned_graph();
    test_lockfile_rejects_invalid_metadata_and_edges();
}

} // namespace

TEST_MAIN(packages)

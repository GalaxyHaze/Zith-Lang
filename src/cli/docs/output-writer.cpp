#include "cli/docs/output-writer.hpp"

#include "cli/docs/markdown-renderer.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <map>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

namespace zith::cli::docs {

namespace {

namespace fs = std::filesystem;

struct OutputFile {
    fs::path path;
    std::string content;
    std::string owner;
};

struct StagedFile {
    OutputFile output;
    fs::path temporaryPath;
    fs::path backupPath;
    bool hasBackup = false;
    bool installed = false;
};

std::string codeSpan(const std::string_view text) {
    size_t longestRun = 0;
    size_t currentRun = 0;
    for (const char character : text) {
        if (character == '`') {
            ++currentRun;
            longestRun = std::max(longestRun, currentRun);
        } else {
            currentRun = 0;
        }
    }

    const std::string delimiter(longestRun + 1U, '`');
    const bool needsPadding = !text.empty() && (text.front() == '`' || text.back() == '`' ||
                                                (text.front() == ' ' && text.back() == ' '));
    return delimiter + (needsPadding ? " " : "") + std::string(text) + (needsPadding ? " " : "") +
           delimiter;
}

std::string safeModuleFileName(const std::string_view key) {
    std::string slug;
    bool previousDash = false;
    for (const char character : key) {
        const auto byte = static_cast<unsigned char>(character);
        const bool asciiAlphaNumeric =
            (byte >= static_cast<unsigned char>('a') && byte <= static_cast<unsigned char>('z')) ||
            (byte >= static_cast<unsigned char>('A') && byte <= static_cast<unsigned char>('Z')) ||
            (byte >= static_cast<unsigned char>('0') && byte <= static_cast<unsigned char>('9'));
        if (asciiAlphaNumeric) {
            char normalized = character;
            if (normalized >= 'A' && normalized <= 'Z')
                normalized = static_cast<char>(normalized - 'A' + 'a');
            slug += normalized;
            previousDash = false;
        } else if (!slug.empty() && !previousDash) {
            slug += '-';
            previousDash = true;
        }
    }
    while (!slug.empty() && slug.back() == '-')
        slug.pop_back();
    if (slug.empty())
        slug = "module";
    if (slug.size() > 96U)
        slug.resize(96U);
    return "module-" + slug + ".md";
}

std::vector<const DocModule *> orderedModules(const DocModel &model) {
    std::vector<const DocModule *> modules;
    modules.reserve(model.modules.size());
    for (const auto &module : model.modules)
        modules.push_back(&module);
    std::stable_sort(
        modules.begin(), modules.end(),
        [](const DocModule *left, const DocModule *right) { return left->key < right->key; });
    return modules;
}

std::string renderIndex(const DocModel &model,
                        const std::map<std::string, std::string, std::less<>> &pageNames,
                        std::string &error) {
    const auto modules = orderedModules(model);
    std::string output = "# API Reference\n\nEntry module: ";
    output += codeSpan(model.entryModule);
    output += "\n\n## Modules\n\n";

    for (const auto *module : modules) {
        const auto page = pageNames.find(module->key);
        if (page == pageNames.end()) {
            error = "no output page was planned for module '" + module->key + "'";
            return {};
        }
        output += "- [";
        output += codeSpan(module->key);
        output += "](modules/";
        output += page->second;
        output += ')';
        if (module->key == model.entryModule)
            output += " (entry)";
        else if (module->isDependency)
            output += " (dependency)";
        output += '\n';
    }

    struct Reexport {
        const DocModule *module = nullptr;
        const DocSymbol *symbol = nullptr;
    };
    std::vector<Reexport> reexports;
    for (const auto *module : modules)
        for (const auto &symbol : module->symbols)
            if (symbol.isReexport)
                reexports.push_back({module, &symbol});

    std::stable_sort(reexports.begin(), reexports.end(),
                     [](const Reexport &left, const Reexport &right) {
                         return std::tie(left.module->key, left.symbol->name,
                                         left.symbol->signature, left.symbol->originModule) <
                                std::tie(right.module->key, right.symbol->name,
                                         right.symbol->signature, right.symbol->originModule);
                     });
    if (!reexports.empty()) {
        output += "\n## Re-exported Symbols\n\n";
        for (const auto &reexport : reexports) {
            const auto origin = pageNames.find(reexport.symbol->originModule);
            if (origin == pageNames.end()) {
                error = "re-export '" + reexport.symbol->name + "' refers to missing module '" +
                        reexport.symbol->originModule + "'";
                return {};
            }
            const auto label = reexport.symbol->signature.empty()
                                   ? std::string_view(reexport.symbol->name)
                                   : std::string_view(reexport.symbol->signature);
            output += "- [";
            output += codeSpan(label);
            output += "](modules/";
            output += origin->second;
            output += ") (re-exported by ";
            output += codeSpan(reexport.module->key);
            output += ")\n";
        }
    }
    return output;
}

std::string modulePage(const DocModel &model, const DocModule &module) {
    DocModel page;
    page.entryModule = model.entryModule;
    page.modules.push_back(module);
    return renderMarkdown(page);
}

OutputWriteResult buildOutputSet(const fs::path &outputDirectory, const DocModel &model,
                                 const bool index, const std::string_view errorMarkdown,
                                 std::vector<OutputFile> &outputs) {
    if (!index) {
        auto content = renderMarkdown(model);
        content += errorMarkdown;
        outputs.push_back({outputDirectory / "API.md", std::move(content), "API.md"});
        return {true, {}};
    }

    std::map<std::string, std::string, std::less<>> pageNames;
    for (const auto *module : orderedModules(model)) {
        const auto name = safeModuleFileName(module->key);
        if (!pageNames.emplace(module->key, name).second) {
            return {false, "duplicate module key '" + module->key + "' in documentation model"};
        }
    }

    std::string indexError;
    auto indexText = renderIndex(model, pageNames, indexError);
    if (!indexError.empty())
        return {false, std::move(indexError)};
    indexText += errorMarkdown;
    outputs.push_back({outputDirectory / "README.md", std::move(indexText), "README.md"});

    for (const auto *module : orderedModules(model)) {
        const auto page = pageNames.find(module->key);
        if (page == pageNames.end())
            return {false, "no output page was planned for module '" + module->key + "'"};
        outputs.push_back(
            {outputDirectory / "modules" / page->second, modulePage(model, *module), module->key});
    }
    return {true, {}};
}

bool inspectPath(const fs::path &path, fs::file_status &status, std::string &error) {
    std::error_code ec;
    status = fs::symlink_status(path, ec);
    if (ec == std::errc::no_such_file_or_directory) {
        status = fs::file_status(fs::file_type::not_found);
        return true;
    }
    if (ec) {
        error = "cannot inspect '" + path.string() + "': " + ec.message();
        return false;
    }
    return true;
}

bool isPresent(const fs::file_status status) {
    return status.type() != fs::file_type::not_found && status.type() != fs::file_type::none;
}

fs::path sidecarPath(const fs::path &destination, const std::string_view kind,
                     const uint64_t identifier) {
    fs::path result = destination;
    result += ".zith-" + std::string(kind) + "-" + std::to_string(identifier);
    return result;
}

bool selectSidecarPath(const fs::path &destination, const std::string_view kind, fs::path &result,
                       std::string &error) {
    static std::atomic<uint64_t> nextIdentifier{0};
    for (size_t attempt = 0; attempt < 1000U; ++attempt) {
        const auto identifier = nextIdentifier.fetch_add(1U, std::memory_order_relaxed);
        auto candidate        = sidecarPath(destination, kind, identifier);
        fs::file_status status;
        if (!inspectPath(candidate, status, error))
            return false;
        if (!isPresent(status)) {
            result = std::move(candidate);
            return true;
        }
    }
    error = "cannot allocate a temporary filename beside '" + destination.string() + "'";
    return false;
}

void removePath(const fs::path &path) {
    if (path.empty())
        return;
    std::error_code ec;
    fs::remove(path, ec);
}

void cleanupStaged(std::vector<StagedFile> &staged) {
    for (auto &file : staged)
        removePath(file.temporaryPath);
}

bool writeTemporary(const fs::path &path, const std::string &content, std::string &error) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "cannot create temporary output '" + path.string() + "'";
        return false;
    }
    output << content;
    output.flush();
    if (!output) {
        error = "failed while writing temporary output '" + path.string() + "'";
        output.close();
        removePath(path);
        return false;
    }
    output.close();
    if (output.fail()) {
        error = "failed while closing temporary output '" + path.string() + "'";
        removePath(path);
        return false;
    }
    return true;
}

bool rollback(std::vector<StagedFile> &staged, std::string &error) {
    bool complete = true;
    for (auto it = staged.rbegin(); it != staged.rend(); ++it) {
        std::error_code ec;
        if (it->installed) {
            fs::remove(it->output.path, ec);
            if (ec)
                complete = false;
        }
        if (it->hasBackup) {
            ec.clear();
            fs::rename(it->backupPath, it->output.path, ec);
            if (ec)
                complete = false;
            else
                it->hasBackup = false;
        }
        removePath(it->temporaryPath);
    }
    if (!complete)
        error += " (rollback was incomplete)";
    return complete;
}

bool commitOutputs(std::vector<StagedFile> &staged, const bool force, std::string &error) {
    for (auto &file : staged) {
        fs::file_status status;
        if (!inspectPath(file.output.path, status, error)) {
            (void)rollback(staged, error);
            return false;
        }
        if (isPresent(status)) {
            if (fs::is_directory(status)) {
                error = "output destination is a directory: '" + file.output.path.string() + "'";
                (void)rollback(staged, error);
                return false;
            }
            if (!force) {
                error = "refusing to overwrite existing file '" + file.output.path.string() +
                        "' without --force";
                (void)rollback(staged, error);
                return false;
            }
            if (!selectSidecarPath(file.output.path, "backup", file.backupPath, error)) {
                (void)rollback(staged, error);
                return false;
            }
            std::error_code ec;
            fs::rename(file.output.path, file.backupPath, ec);
            if (ec) {
                error = "cannot preserve existing output '" + file.output.path.string() +
                        "': " + ec.message();
                (void)rollback(staged, error);
                return false;
            }
            file.hasBackup = true;
        }

        std::error_code ec;
        fs::rename(file.temporaryPath, file.output.path, ec);
        if (ec) {
            error = "cannot install output '" + file.output.path.string() + "': " + ec.message();
            (void)rollback(staged, error);
            return false;
        }
        file.installed = true;
        file.temporaryPath.clear();
    }

    for (auto &file : staged) {
        if (!file.hasBackup)
            continue;
        std::error_code ec;
        fs::remove(file.backupPath, ec);
        if (ec && error.empty())
            error = "output was generated but backup '" + file.backupPath.string() +
                    "' could not be removed: " + ec.message();
        else if (!ec)
            file.hasBackup = false;
    }
    return true;
}

} // namespace

fs::path resolveDocsOutputDirectory(const fs::path &inputPath, const fs::path &explicitOutputPath) {
    std::error_code ec;
    if (!explicitOutputPath.empty()) {
        auto output = fs::absolute(explicitOutputPath, ec);
        return ec ? explicitOutputPath.lexically_normal() : output.lexically_normal();
    }
    if (inputPath.empty())
        return {};

    auto input = fs::absolute(inputPath, ec);
    if (ec)
        input = inputPath.lexically_normal();

    ec.clear();
    const bool inputIsDirectory = fs::is_directory(input, ec) && !ec;
    auto current                = inputIsDirectory ? input : input.parent_path();
    while (!current.empty()) {
        ec.clear();
        const bool hasManifest = fs::is_regular_file(current / "ZithProject.toml", ec) && !ec;
        if (hasManifest)
            return (current / "docs").lexically_normal();
        const auto parent = current.parent_path();
        if (parent == current)
            break;
        current = parent;
    }

    return ((inputIsDirectory ? input : input.parent_path()) / "docs").lexically_normal();
}

OutputWriteResult writeDocsOutput(const fs::path &outputDirectory, const DocModel &model,
                                  const bool index, const bool force,
                                  const std::string_view errorMarkdown) {
    if (outputDirectory.empty())
        return {false, "documentation output directory is empty"};

    std::error_code ec;
    auto absoluteDirectory = fs::absolute(outputDirectory, ec);
    if (ec)
        return {false, "cannot resolve output directory '" + outputDirectory.string() +
                           "': " + ec.message()};
    absoluteDirectory = absoluteDirectory.lexically_normal();

    std::vector<OutputFile> outputs;
    auto buildResult = buildOutputSet(absoluteDirectory, model, index, errorMarkdown, outputs);
    if (!buildResult.ok)
        return buildResult;

    std::map<fs::path, std::string> plannedPaths;
    for (auto &output : outputs) {
        output.path                     = output.path.lexically_normal();
        const auto [existing, inserted] = plannedPaths.emplace(output.path, output.owner);
        if (!inserted) {
            return {false, "output filename collision between '" + existing->second + "' and '" +
                               output.owner + "' at '" + output.path.string() + "'"};
        }

        fs::file_status status;
        std::string inspectError;
        if (!inspectPath(output.path, status, inspectError))
            return {false, std::move(inspectError)};
        if (fs::is_directory(status))
            return {false, "output destination is a directory: '" + output.path.string() + "'"};
        if (isPresent(status) && !force)
            return {false, "refusing to overwrite existing file '" + output.path.string() +
                               "' without --force"};
    }

    std::vector<StagedFile> staged;
    staged.reserve(outputs.size());
    for (auto &output : outputs) {
        ec.clear();
        fs::create_directories(output.path.parent_path(), ec);
        if (ec) {
            cleanupStaged(staged);
            return {false, "cannot create output directory '" + output.path.parent_path().string() +
                               "': " + ec.message()};
        }

        StagedFile stagedFile;
        stagedFile.output = std::move(output);
        std::string stageError;
        if (!selectSidecarPath(stagedFile.output.path, "tmp", stagedFile.temporaryPath,
                               stageError)) {
            cleanupStaged(staged);
            return {false, std::move(stageError)};
        }
        if (!writeTemporary(stagedFile.temporaryPath, stagedFile.output.content, stageError)) {
            cleanupStaged(staged);
            return {false, std::move(stageError)};
        }
        staged.push_back(std::move(stagedFile));
    }

    std::string commitError;
    if (!commitOutputs(staged, force, commitError)) {
        cleanupStaged(staged);
        return {false, std::move(commitError)};
    }
    if (!commitError.empty())
        return {true, std::move(commitError)};
    return {true, {}};
}

} // namespace zith::cli::docs

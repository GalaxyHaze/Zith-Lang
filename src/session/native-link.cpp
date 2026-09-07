#include "compilation-session.hpp"

#include "cc/driver.hpp"
#include "memory/flat-set.hpp"

#ifdef ZITH_HAS_LLVM
#include <llvm/TargetParser/Host.h>
#endif

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#elif !defined(ZITH_IS_WASM)
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace zith::session {

namespace {

[[maybe_unused]] int runProgram(const std::vector<std::string> &arguments) {
    if (arguments.empty())
        return -1;

    std::vector<char *> argv;
    argv.reserve(arguments.size() + 1U);
    for (const auto &argument : arguments)
        argv.push_back(const_cast<char *>(argument.c_str()));
    argv.push_back(nullptr);

#ifdef _WIN32
    const auto spawned = _spawnvp(_P_WAIT, argv.front(), argv.data());
    if (spawned == -1)
        return -1;
    return static_cast<int>(spawned);
#elif defined(ZITH_IS_WASM)
    (void)argv;
    return -1;
#else
    const pid_t child = fork();
    if (child < 0)
        return -1;
    if (child == 0) {
        execvp(argv.front(), argv.data());
        _exit(127);
    }

    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR)
            return -1;
    }
    return status;
#endif
}

[[maybe_unused]] int captureProgram(const std::vector<std::string> &arguments,
                                    std::string &output) {
#if defined(_WIN32) || defined(ZITH_IS_WASM)
    (void)arguments;
    (void)output;
    return -1;
#else
    if (arguments.empty())
        return -1;

    int pipefd[2] = {-1, -1};
    if (pipe(pipefd) != 0)
        return -1;

    std::vector<char *> argv;
    argv.reserve(arguments.size() + 1U);
    for (const auto &argument : arguments)
        argv.push_back(const_cast<char *>(argument.c_str()));
    argv.push_back(nullptr);

    const pid_t child = fork();
    if (child < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    if (child == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execvp(argv.front(), argv.data());
        _exit(127);
    }

    close(pipefd[1]);
    char buffer[4096];
    ssize_t bytes = 0;
    while ((bytes = read(pipefd[0], buffer, sizeof(buffer))) != 0) {
        if (bytes < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        output.append(buffer, static_cast<size_t>(bytes));
    }
    close(pipefd[0]);

    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR)
            return -1;
    }
    return status;
#endif
}

[[maybe_unused]] int normalizeExitStatus(const int status) {
#ifdef _WIN32
    return status;
#else
    int waitStatus = status;
    if (WIFEXITED(waitStatus))
        return WEXITSTATUS(waitStatus);
    if (WIFSIGNALED(waitStatus))
        return 128 + WTERMSIG(waitStatus);
    return 1;
#endif
}

} // namespace

bool CompilationSession::prepareNativeLinkInputs() {
#ifndef ZITH_IS_WASM
    if (mPreparedNativeLinkInputs)
        return true;
    mPreparedNativeLinkInputs = true;

    namespace fs = std::filesystem;

    std::vector<std::string> configuredRoots;
    configuredRoots.reserve(mProjectConfig.cSourceDirs.size() + mOpts.get().cSourceDirs.size());
    for (const auto &root : mProjectConfig.cSourceDirs)
        configuredRoots.push_back(root);
    for (const auto &root : mOpts.get().cSourceDirs)
        configuredRoots.push_back(root);
    if (configuredRoots.empty())
        return true;

    std::vector<std::string> resolvedRoots;
    memory::FlatSet<std::string> seenRoots;
    for (const auto &root : configuredRoots) {
        fs::path path(root);
        if (path.is_relative())
            path = fs::path(mProjectRoot) / path;

        std::error_code error;
        if (!fs::exists(path, error) || !fs::is_directory(path, error)) {
            writeOutput("%s[error]%s C source root does not exist: %s\n", ansicolor("\033[31m"),
                        ansicolor("\033[0m"), path.lexically_normal().string().c_str());
            return false;
        }

        const std::string canonical = fs::weakly_canonical(path).string();
        if (seenRoots.insert(canonical))
            resolvedRoots.push_back(canonical);
    }

    const auto discovery = cc::discoverCSources(resolvedRoots);
    for (const auto &diag : discovery.diagnostics) {
        writeOutput("%s[error]%s %s\n", ansicolor("\033[31m"), ansicolor("\033[0m"),
                    diag.message.c_str());
    }
    if (!discovery.ok)
        return false;
    if (discovery.sourcePaths.empty())
        return true;

    std::vector<std::string> includeDirs;
    includeDirs.reserve(mProjectConfig.includeDirs.size() + mOpts.get().includeDirs.size());
    for (const auto &dir : mProjectConfig.includeDirs) {
        fs::path path(dir);
        if (path.is_relative())
            path = fs::path(mProjectRoot) / path;
        includeDirs.push_back(path.lexically_normal().string());
    }
    for (const auto &dir : mOpts.get().includeDirs) {
        fs::path path(dir);
        if (path.is_relative())
            path = fs::path(mProjectRoot) / path;
        includeDirs.push_back(path.lexically_normal().string());
    }

    std::vector<std::string> defines;
    defines.reserve(mProjectConfig.defines.size() + mOpts.get().defines.size());
    for (const auto &define : mProjectConfig.defines)
        defines.push_back(define);
    for (const auto &define : mOpts.get().defines)
        defines.push_back(define);

    std::string targetKey = mOpts.get().targetTriple;
    if (targetKey.empty()) {
#ifdef ZITH_HAS_LLVM
        targetKey = llvm::sys::getDefaultTargetTriple();
#else
        targetKey = "host";
#endif
    }
    std::replace(targetKey.begin(), targetKey.end(), '/', '_');

    const fs::path objectRoot = fs::path(mProjectRoot) / "cache" / "c-obj" / targetKey;
    fs::create_directories(objectRoot);

    for (size_t i = 0; i < discovery.sourcePaths.size(); ++i) {
        const auto &sourcePath = discovery.sourcePaths[i];
        const fs::path objectPath =
            objectRoot / (std::to_string(i) + "-" + fs::path(sourcePath).stem().string() + ".o");

        cc::CCompileRequest request;
        request.inputPath    = sourcePath;
        request.outputPath   = objectPath.string();
        request.targetTriple = mOpts.get().targetTriple;
        request.sysroot      = mOpts.get().sysroot;
        request.includeDirs  = includeDirs;
        request.defines      = defines;
        request.verbose      = mOpts.get().flags.verbose();

        const auto compileResult = cc::compileCSource(request);
        if (mOpts.get().flags.verbose() && !compileResult.commandDisplay.empty())
            writeOutput("  [c-compile] %s\n", compileResult.commandDisplay.c_str());
        if (!compileResult.ok) {
            for (const auto &diag : compileResult.diagnostics) {
                writeOutput("%s[error]%s %s\n", ansicolor("\033[31m"), ansicolor("\033[0m"),
                            diag.message.c_str());
            }
            if (!compileResult.toolOutput.empty())
                writeOutput("%s", compileResult.toolOutput.c_str());
            return false;
        }

        mExtraObjectPaths.push_back(objectPath.string());
    }

    return true;
#else
    return true;
#endif
}

bool CompilationSession::performLink(std::string &exePath, bool &isWasm) {
#ifndef ZITH_IS_WASM
    isWasm = false;
    if (mObjectPath.empty())
        return false;

    namespace fs = std::filesystem;
    if (!mOpts.get().outputFile.empty()) {
        exePath = mOpts.get().outputFile;
        fs::create_directories(fs::path(exePath).parent_path());
    } else {
        std::string binName;
        if (!mProjectConfig.name.empty())
            binName = mProjectConfig.name;
        else
            binName = fs::path(mFilePath).stem().string();

        std::string exeDir = (fs::path(mProjectRoot) / "target").string();
        auto &triple       = mOpts.get().targetTriple;
        if (!triple.empty()) {
#ifdef ZITH_HAS_LLVM
            std::string hostTriple = llvm::sys::getDefaultTargetTriple();
#else
            std::string hostTriple;
#endif
            if (triple != hostTriple)
                exeDir = (fs::path(mProjectRoot) / "target" / triple).string();
        }

        fs::create_directories(exeDir);
        exePath = exeDir + "/" + binName;
    }

    auto &triple = mOpts.get().targetTriple;
    isWasm =
        triple.find("wasm32") != std::string::npos || triple.find("wasm64") != std::string::npos;

    if (isWasm)
        exePath += ".wasm";
#ifdef _WIN32
    else if (mOpts.get().outputFile.empty())
        exePath += ".exe";
#endif

    if (mOpts.get().flags.verbose())
        writeOutput("  [link] %s -> %s\n", mObjectPath.c_str(), exePath.c_str());

    if (isWasm) {
        std::vector<std::string> link_args;
        bool isWasi = triple.find("wasi") != std::string::npos;
        link_args.emplace_back("wasm-ld");
        if (!isWasi)
            link_args.insert(link_args.end(), {"--no-entry", "--export-all"});
        link_args.insert(link_args.end(), {"-o", exePath, mObjectPath});
        if (!mOpts.get().sysroot.empty())
            link_args.push_back("--sysroot=" + mOpts.get().sysroot);
        if (mOpts.get().flags.verbose())
            writeOutput("  [link] %s\n", cc::displayCommand(link_args).c_str());
        const int linkResult = runProgram(link_args);
        if (linkResult != 0) {
            writeOutput("%s[error]%s linking failed (exit code %d)\n", ansicolor("\033[31m"),
                        ansicolor("\033[0m"), linkResult);
            return false;
        }
    } else {
        if (!prepareNativeLinkInputs())
            return false;

        cc::LinkRequest request;
        request.outputPath        = exePath;
        request.targetTriple      = mOpts.get().targetTriple;
        request.sysroot           = mOpts.get().sysroot;
        request.primaryObjectPath = mObjectPath;
        request.extraObjectPaths  = mExtraObjectPaths;
        request.verbose           = mOpts.get().flags.verbose();

        for (const auto &directory : mProjectConfig.libraryDirs)
            request.libraryDirs.push_back(
                (std::filesystem::path(mProjectRoot) / directory).lexically_normal().string());
        for (const auto &directory : mOpts.get().libraryDirs) {
            std::filesystem::path path(directory);
            if (path.is_relative())
                path = std::filesystem::path(mProjectRoot) / path;
            request.libraryDirs.push_back(path.lexically_normal().string());
        }
        for (const auto &library : mProjectConfig.libraries)
            request.libraries.push_back(library);
        for (const auto &library : mOpts.get().libraries)
            request.libraries.push_back(library);

        const auto linkResult = cc::linkNative(request);
        if (mOpts.get().flags.verbose() && !linkResult.commandDisplay.empty())
            writeOutput("  [link] %s\n", linkResult.commandDisplay.c_str());
        if (!linkResult.ok) {
            for (const auto &diag : linkResult.diagnostics) {
                writeOutput("%s[error]%s %s\n", ansicolor("\033[31m"), ansicolor("\033[0m"),
                            diag.message.c_str());
            }
            if (!linkResult.toolOutput.empty())
                writeOutput("%s", linkResult.toolOutput.c_str());
            return false;
        }
    }

    mExecutablePath = exePath;
    return true;
#else
    (void)exePath;
    (void)isWasm;
    (void)mObjectPath;
    writeOutput("%s[error]%s cannot link on WASM target\n", ansicolor("\033[31m"),
                ansicolor("\033[0m"));
    return false;
#endif
}

bool CompilationSession::link() {
    std::string exePath;
    bool isWasm = false;
    return performLink(exePath, isWasm);
}

bool CompilationSession::execAfterLink(const bool capture) {
#ifndef ZITH_IS_WASM
    std::string exePath;
    bool isWasm = false;
    if (!performLink(exePath, isWasm))
        return false;

    if (mOpts.get().flags.verbose())
        writeOutput("  [exec] %s\n", exePath.c_str());

    if (isWasm) {
        if (mOpts.get().flags.verbose())
            writeOutput("  [exec] skipped (cannot natively execute WASM)\n");
        mChildExitCode = 0;
        return true;
    }

    // Avoid duplicating any pending parent stdio buffers into the forked child.
    std::fflush(nullptr);
    const int execResult =
        capture ? captureProgram({exePath}, mChildOutput) : runProgram({exePath});
    if (execResult == -1) {
        writeOutput("%s[error]%s failed to launch executable\n", ansicolor("\033[31m"),
                    ansicolor("\033[0m"));
        return false;
    }

    mChildExitCode = normalizeExitStatus(execResult);
    return true;
#else
    (void)capture;
    writeOutput("%s[error]%s cannot execute on WASM target\n", ansicolor("\033[31m"),
                ansicolor("\033[0m"));
    return false;
#endif
}

bool CompilationSession::linkAndExec() {
    return execAfterLink(/*capture=*/true);
}

bool CompilationSession::linkAndExecDirect() {
    return execAfterLink(/*capture=*/false);
}

} // namespace zith::session

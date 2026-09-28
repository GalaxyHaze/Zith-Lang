#include <cstring>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "capi/zithc-capi.h"
#include "memory/arena.hpp"
#include "session/compilation-session.hpp"
#include "vm/hir-to-vm.hpp"
#include "vm/typed-ir.hpp"
#include "vm/vm-v2.hpp"
#include "wasm/abi-hir.hpp"

extern "C" {
// Import from JS: zith.host_write
__attribute__((import_module("zith"), import_name("host_write"))) void
host_write(int stream, const char *ptr, int len);
}

static std::string last_error;
static std::string last_output;
static std::string last_blob;
static std::vector<std::string> rendered_errors;
static int64_t last_exit_code = 0;
static std::vector<std::pair<std::string, std::string>> stdlib_sources;

#ifdef ZITH_IS_WASM
constexpr const char *kWasmStdioHeader = R"(#[discardable] pub extern fn getchar(): i32
#[discardable] pub extern fn putchar(c: i32): i32
#[discardable] pub extern fn sscanf(input: ?*char, format: *char, ...): i32
#[discardable] pub extern fn strncmp(left: ?*char, right: ?*char, count: u64): i32
)";

constexpr const char *kWasmStdlibHeader = R"(#[discardable] pub extern fn malloc(size: u64): raw opaque
#[discardable] pub extern fn calloc(count: u64, size: u64): raw opaque
#[discardable] pub extern fn realloc(ptr: raw opaque, size: u64): raw opaque
#[discardable] pub extern fn free(ptr: raw opaque)
)";

constexpr const char *kWasmStringHeader = R"(#[discardable] pub extern fn strlen(value: ?*char): u64
#[discardable] pub extern fn memcpy(destination: ?*char, source: ?*char, size: u64): raw opaque
#[discardable] pub extern fn snprintf(destination: *char, size: u64, format: *char, ...): i32
)";
#endif

extern "C" __attribute__((export_name("zith_alloc"))) void *zith_alloc(int size) {
    return new char[size];
}

extern "C" __attribute__((export_name("zith_free"))) void zith_free(void *ptr, int size) {
    delete[] static_cast<char *>(ptr);
}

extern "C" __attribute__((export_name("zith_last_error_ptr"))) const char *zith_last_error_ptr() {
    return last_error.data();
}

extern "C" __attribute__((export_name("zith_last_error_len"))) int zith_last_error_len() {
    return static_cast<int>(last_error.size());
}

extern "C" __attribute__((export_name("zith_last_output_ptr"))) const char *zith_last_output_ptr() {
    return last_output.data();
}

extern "C" __attribute__((export_name("zith_last_output_len"))) int zith_last_output_len() {
    return static_cast<int>(last_output.size());
}

extern "C" __attribute__((export_name("zith_last_buffer_ptr"))) const char *
zith_last_buffer_ptr() {
    return last_blob.data();
}

extern "C" __attribute__((export_name("zith_last_buffer_len"))) int zith_last_buffer_len() {
    return static_cast<int>(last_blob.size());
}

extern "C" __attribute__((export_name("zith_exit_code"))) int64_t zith_exit_code() {
    return last_exit_code;
}

extern "C" __attribute__((export_name("zith_error_count"))) unsigned zith_error_count() {
    return static_cast<unsigned>(rendered_errors.size());
}

extern "C" __attribute__((export_name("zith_error_at"))) const char *zith_error_at(unsigned index) {
    if (index >= rendered_errors.size())
        return nullptr;
    return rendered_errors[index].data();
}

extern "C" __attribute__((export_name("zith_compiler_version_ptr"))) const char *
zith_compiler_version_ptr() {
    return ZITH_VERSION;
}

extern "C" __attribute__((export_name("zith_compiler_version_len"))) int
zith_compiler_version_len() {
    return static_cast<int>(std::strlen(ZITH_VERSION));
}

namespace {

constexpr int kPlaygroundStatusOk            = 0;
constexpr int kPlaygroundStatusCompileFailed = 1;
constexpr int kPlaygroundStatusInvalidParam  = 2;
constexpr int kPlaygroundStatusTrap          = 3;
constexpr int kPlaygroundStatusOom           = 4;
constexpr int kPlaygroundStatusUnsupported   = 5;
constexpr std::string_view kStdlibPackMagic  = "ZSTDLIB2";
constexpr uint32_t kStdlibPackAbi            = 2;

constexpr bool isPlaygroundMode(int mode) {
    return mode == 0 || mode == 1;
}

constexpr bool isPlaygroundOptLevel(int opt_level) {
    return opt_level >= 0 && opt_level <= 3;
}

const char *severityName(zithc_severity severity) {
    switch (severity) {
    case ZITHC_SEVERITY_ERROR:
        return "error";
    case ZITHC_SEVERITY_WARNING:
        return "warning";
    case ZITHC_SEVERITY_NOTE:
        return "note";
    case ZITHC_SEVERITY_BUG:
        return "bug";
    }
    return "bug";
}

std::string renderDiagnostic(const zithc_diagnostic &diag) {
    std::string line;
    line += severityName(diag.severity);
    line += ": ";
    line += diag.message ? diag.message : "";
    line += "\n";
    return line;
}

void setErrorMessage(const std::string &message) {
    last_error = message;
    if (last_error.empty() || last_error.back() != '\n')
        last_error.push_back('\n');
    host_write(2, last_error.data(), last_error.size());
}

uint32_t readU32(const uint8_t *data, size_t size, size_t &offset, bool &ok) {
    if (offset > size || size - offset < 4U) {
        ok = false;
        return 0;
    }
    const uint32_t value = static_cast<uint32_t>(data[offset]) |
                           (static_cast<uint32_t>(data[offset + 1U]) << 8U) |
                           (static_cast<uint32_t>(data[offset + 2U]) << 16U) |
                           (static_cast<uint32_t>(data[offset + 3U]) << 24U);
    offset += 4U;
    return value;
}

bool isSafeStdlibPath(std::string_view path) {
    if (path.empty() || path.front() == '/' || path.find('\\') != std::string_view::npos)
        return false;
    size_t start = 0;
    while (start < path.size()) {
        const size_t end = path.find('/', start);
        const auto component = path.substr(start, end == std::string_view::npos
                                                   ? path.size() - start
                                                   : end - start);
        if (component.empty() || component == "." || component == "..")
            return false;
        start = end == std::string_view::npos ? path.size() : end + 1U;
    }
    return true;
}

bool registerStdlibPack(const char *ptr, int len) {
    stdlib_sources.clear();
    if (!ptr || len < static_cast<int>(kStdlibPackMagic.size() + 8U))
        return false;

    const auto *data = reinterpret_cast<const uint8_t *>(ptr);
    const size_t size = static_cast<size_t>(len);
    if (std::string_view(reinterpret_cast<const char *>(data), kStdlibPackMagic.size()) !=
        kStdlibPackMagic)
        return false;

    size_t offset = kStdlibPackMagic.size();
    bool ok = true;
    const uint32_t abi = readU32(data, size, offset, ok);
    const uint32_t count = readU32(data, size, offset, ok);
    if (!ok || abi != kStdlibPackAbi || count > 10000U)
        return false;

    stdlib_sources.reserve(count);
    for (uint32_t index = 0; index < count; ++index) {
        const uint32_t path_len = readU32(data, size, offset, ok);
        const uint32_t text_len = readU32(data, size, offset, ok);
        if (!ok || path_len == 0U || path_len > size || text_len > size ||
            offset > size - path_len || offset + path_len > size - text_len)
            return false;
        std::string path(reinterpret_cast<const char *>(data + offset), path_len);
        offset += path_len;
        std::string text(reinterpret_cast<const char *>(data + offset), text_len);
        offset += text_len;
        if (!isSafeStdlibPath(path))
            return false;
        for (const auto &[registered_path, registered_text] : stdlib_sources)
            if (registered_path == path)
                return false;
        stdlib_sources.emplace_back(std::move(path), std::move(text));
    }
    return offset == size;
}

void registerStdlibSources(zithc_session *session) {
    for (const auto &[path, text] : stdlib_sources) {
        const std::string virtual_path = "stdlib/" + path;
        zithc_session_register_virtual_source(session, virtual_path.c_str(), text.c_str());
    }
#ifdef ZITH_IS_WASM
    zithc_session_register_virtual_source(session, "stdlib/stdio.h.zith", kWasmStdioHeader);
    zithc_session_register_virtual_source(session, "stdlib/stdlib.h.zith", kWasmStdlibHeader);
    zithc_session_register_virtual_source(session, "stdlib/string.h.zith", kWasmStringHeader);
#endif
}

int runStatusToPlaygroundStatus(zith::vm::RunStatus status) {
    switch (status) {
    case zith::vm::RunStatus::Ok:
        return kPlaygroundStatusOk;
    case zith::vm::RunStatus::MissingMain:
    case zith::vm::RunStatus::Trap:
        return kPlaygroundStatusTrap;
    case zith::vm::RunStatus::Unsupported:
        return kPlaygroundStatusUnsupported;
    case zith::vm::RunStatus::Oom:
        return kPlaygroundStatusOom;
    }
    return kPlaygroundStatusTrap;
}

int runPlayground(const char *ptr, int len, bool is_compile, int mode = 0, int opt_level = 0,
                  int emit_mask = 0) {
    last_error.clear();
    last_output.clear();
    rendered_errors.clear();

    if (!ptr || len < 0)
        setErrorMessage("invalid source buffer");
    if (is_compile && !isPlaygroundMode(mode))
        setErrorMessage("invalid mode (must be 0 or 1)");
    if (is_compile && !isPlaygroundOptLevel(opt_level))
        setErrorMessage("invalid optimization level (must be 0-3)");
    if (last_error.empty() && (emit_mask & ~127) != 0)
        setErrorMessage("invalid emit_mask (bits 0-6 only)");
    if (!last_error.empty())
        return kPlaygroundStatusInvalidParam;

    auto *session =
        zithc_session_create_from_buffer("playground.zith", ptr, static_cast<size_t>(len));
    if (!session) {
        setErrorMessage("failed to create session");
        return kPlaygroundStatusCompileFailed;
    }

    zithc_session_set_mode(session, static_cast<uint8_t>(mode));
    zithc_session_set_opt_level(session, static_cast<uint8_t>(opt_level));
    zithc_session_set_emit_tokens(session, emit_mask & 1);
    zithc_session_set_emit_flags(session, emit_mask & 2, emit_mask & 4, emit_mask & 8,
                                 emit_mask & 16);
    zithc_session_set_emit_extra_flags(session, emit_mask & 32, emit_mask & 64);
    zithc_session_add_include_dir(session, "stdlib");
    registerStdlibSources(session);

    // The browser build has no LLVM backend, so emission stops at HIR lowering.
    const bool ok = zithc_run_to(session, ZITHC_STAGE_HIR_LOWERED);

    const char *buffered_out = zithc_session_flush_output(session);
    if (buffered_out && buffered_out[0] != '\0') {
        last_output = buffered_out;
        host_write(1, last_output.data(), last_output.size());
    }

    const size_t count = zithc_diag_count(session);
    for (size_t i = 0; i < count; ++i) {
        const zithc_diagnostic diag  = zithc_diag_get(session, i);
        const std::string render_str = renderDiagnostic(diag);
        rendered_errors.push_back(render_str);
        host_write(2, render_str.data(), render_str.size());
        last_error += render_str;
    }

    zithc_session_destroy(session);
    if (last_error.empty() && (emit_mask & (8 | 16)) != 0) {
        setErrorMessage("IR/ASM emission is not available in the WASM playground build");
        return kPlaygroundStatusCompileFailed;
    }
    return ok ? kPlaygroundStatusOk : kPlaygroundStatusCompileFailed;
}

} // namespace

extern "C" int zith_emit_hir(const char *ptr, int len);
extern "C" int zith_execute_hir(const char *ptr, int len);

extern "C" __attribute__((export_name("zith_compile_source"))) int
zith_compile_source(const char *ptr, int len, int mode, int opt_level, int emit_mask) {
    return runPlayground(ptr, len, true, mode, opt_level, emit_mask);
}

extern "C" __attribute__((export_name("zith_run_source"))) int zith_run_source(const char *ptr,
                                                                               int len) {
    const int compile_status = zith_emit_hir(ptr, len);
    if (compile_status != kPlaygroundStatusOk)
        return compile_status;
    return zith_execute_hir(last_blob.data(), static_cast<int>(last_blob.size()));
}

extern "C" __attribute__((export_name("zith_register_stdlib_pack"))) int
zith_register_stdlib_pack(const char *ptr, int len) {
    last_error.clear();
    last_output.clear();
    last_blob.clear();
    rendered_errors.clear();
    last_exit_code = 0;
    if (!registerStdlibPack(ptr, len)) {
        setErrorMessage("invalid stdlib pack");
        return kPlaygroundStatusInvalidParam;
    }
    return kPlaygroundStatusOk;
}

extern "C" __attribute__((export_name("zith_emit_hir"))) int zith_emit_hir(const char *ptr,
                                                                           int len) {
    last_error.clear();
    last_output.clear();
    last_blob.clear();
    rendered_errors.clear();
    last_exit_code = 0;

    if (!ptr || len < 0)
        setErrorMessage("invalid source buffer");
    if (!last_error.empty())
        return kPlaygroundStatusInvalidParam;

    auto *session =
        zithc_session_create_from_buffer("playground.zith", ptr, static_cast<size_t>(len));
    if (!session) {
        setErrorMessage("failed to create session");
        return kPlaygroundStatusCompileFailed;
    }

    zithc_session_set_mode(session, 1);
    zithc_session_set_opt_level(session, 0);
    zithc_session_add_include_dir(session, "stdlib");
    registerStdlibSources(session);
    const bool ok = zithc_run_to(session, ZITHC_STAGE_HIR_LOWERED);

    const char *buffered_out = zithc_session_flush_output(session);
    if (buffered_out && buffered_out[0] != '\0') {
        last_output = buffered_out;
        host_write(1, last_output.data(), last_output.size());
    }

    const size_t count = zithc_diag_count(session);
    for (size_t i = 0; i < count; ++i) {
        const zithc_diagnostic diag  = zithc_diag_get(session, i);
        const std::string render_str = renderDiagnostic(diag);
        rendered_errors.push_back(render_str);
        host_write(2, render_str.data(), render_str.size());
        last_error += render_str;
    }

    auto *compilerContext = static_cast<zith::session::CompilationSession *>(
        zithc_session_compiler_context(session));
    if (ok && last_error.empty()) {
        const auto encoded = zith::wasm::encodeHir(*compilerContext);
        if (!encoded.ok) {
            setErrorMessage(encoded.message.empty() ? "flat HIR encoding failed"
                                                    : encoded.message);
        } else {
            last_blob.assign(encoded.blob.bytes.begin(), encoded.blob.bytes.end());
        }
    }
    zithc_session_destroy(session);
    if (!ok || last_blob.empty()) {
        if (last_error.empty())
            setErrorMessage("compilation failed");
        return kPlaygroundStatusCompileFailed;
    }

    return kPlaygroundStatusOk;
}

extern "C" __attribute__((export_name("zith_execute_hir"))) int zith_execute_hir(const char *ptr,
                                                                                 int len) {
    last_error.clear();
    last_output.clear();
    last_exit_code = 0;

    if (!ptr || len < 0)
        setErrorMessage("invalid HIR buffer");
    if (!last_error.empty())
        return kPlaygroundStatusInvalidParam;

    zith::wasm::DecodedHir decoded;
    std::span<const uint8_t> bytes(reinterpret_cast<const uint8_t *>(ptr),
                                   static_cast<size_t>(len));
    if (!zith::wasm::decodeHir(bytes, decoded)) {
        setErrorMessage(decoded.message.empty() ? "invalid flat HIR blob" : decoded.message);
        return kPlaygroundStatusCompileFailed;
    }

    zith::memory::Arena vmArena;
    zith::vm::Module module(vmArena);
    const auto lowered =
        zith::vm::lowerModule(decoded.module, decoded.interner, decoded.types, vmArena, module);
    if (!lowered.ok) {
        setErrorMessage(lowered.message.empty() ? "unsupported program for VM v2"
                                                : lowered.message);
        return kPlaygroundStatusUnsupported;
    }

    zith::vm::Vm vm;
    const auto result = vm.runMain(module);
    last_exit_code    = result.exitCode;
    if (result.status == zith::vm::RunStatus::Ok) {
        if (!result.output.empty()) {
            last_output = result.output;
            host_write(1, last_output.data(), last_output.size());
        }
        return kPlaygroundStatusOk;
    }

    setErrorMessage(result.message.empty() ? "VM v2 could not execute the program"
                                           : result.message);
    return runStatusToPlaygroundStatus(result.status);
}

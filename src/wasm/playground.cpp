#include <cstring>
#include <string>
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

#ifdef ZITH_IS_WASM
constexpr const char *kWasmConsoleModule = R"(pub trait Formatable {
    fn format(self, dest: lend FormatBuffer): IoError;
}

pub extern fn puts(msg: *char): i32

pub enum IoError {
    Ok = 0,
}

pub struct FormatBuffer {
    data: ?*char = null,
    capacity: u64 = 0,
    length: u64 = 0,
}

#[discardable]
pub fn println(msg: []char, values: [...]dyn Formatable): IoError {
    if (@lengthOf(msg) > 0) {
        _ = puts(@ptrOf(msg));
    }
    return IoError.Ok;
}
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
    if (last_error.empty() && (emit_mask & ~31) != 0)
        setErrorMessage("invalid emit_mask (bits 0-4 only)");
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
    zithc_session_add_include_dir(session, "stdlib");
#ifdef ZITH_IS_WASM
    zithc_session_register_virtual_source(session, "stdlib/std/io/console.zith",
                                          kWasmConsoleModule);
#endif

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

extern "C" __attribute__((export_name("zith_compile_source"))) int
zith_compile_source(const char *ptr, int len, int mode, int opt_level, int emit_mask) {
    return runPlayground(ptr, len, true, mode, opt_level, emit_mask);
}

extern "C" __attribute__((export_name("zith_run_source"))) int zith_run_source(const char *ptr,
                                                                               int len) {
    // Run is documented as check + HIR emission; the browser does not execute the program.
    return runPlayground(ptr, len, false, 1, 0, 0);
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
    zithc_session_register_virtual_source(session, "stdlib/std/io/console.zith",
                                          kWasmConsoleModule);
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

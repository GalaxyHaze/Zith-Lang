// impl/diagnostics/diagnostics.hpp — C++ Diagnostic Manager
//
// Provides three tiers of API:
//   1. New C++ API: zith::diag::DiagnosticBag + DiagnosticBuilder (fluent)
//   2. Legacy DiagManager wrapper (still works, delegates to DiagnosticBag)
//   3. C ABI bridge (ZithDiagBag wrapper)
#pragma once

#include <zith/diagnostics.h>

#include "diagnostics/builder.hpp"
#include "diagnostics/diagnostic_bag.hpp"
#include "diagnostics/emitter.hpp"
#include "diagnostics/heuristic_engine.hpp"
#include "diagnostics/span.hpp"

#ifdef __cplusplus
struct ZithArena;

// ============================================================================
// C ABI implementation — ZithDiagBag wraps zith::diag::DiagnosticBag
// ============================================================================

struct ZithDiagBag {
    zith::diag::DiagnosticBag bag;
    zith::diag::SourceMap source_map;
    zith::diag::TerminalEmitter emitter;
    zith::diag::HeuristicEngine heuristic;
};

// ============================================================================
// DiagManager — Legacy C++ wrapper (delegates to DiagnosticBag)
// ============================================================================

class DiagManager {
public:
    DiagManager();

    // Legacy API (unchanged interface)
    void error(ZithSourceLoc loc, const char *msg);
    void warning(ZithSourceLoc loc, const char *msg);
    void note(ZithSourceLoc loc, const char *msg);
    static void info(const char *msg);

    void print_all(const char *source, size_t source_len,
                   const char *filename = "<input>") const;
    void print_summary(const char *filename = "<input>") const;

    [[nodiscard]] bool had_error() const { return bag_.had_errors(); }
    [[nodiscard]] const ZithDiagList &list() const { return legacy_list_; }

    void set_arena(ZithArena *arena) { arena_ = arena; }

    // New API accessors
    zith::diag::DiagnosticBag& bag() { return bag_; }
    const zith::diag::DiagnosticBag& bag() const { return bag_; }

    zith::diag::SourceMap& source_map() { return source_map_; }
    const zith::diag::SourceMap& source_map() const { return source_map_; }

    zith::diag::TerminalEmitter& emitter() { return emitter_; }
    const zith::diag::TerminalEmitter& emitter() const { return emitter_; }

    zith::diag::HeuristicEngine& heuristic() { return heuristic_; }

    // Convenience: create a new-style DiagnosticBuilder
    zith::diag::DiagnosticBuilder build(zith::diag::DiagLevel level,
                                         zith::diag::DiagCode code) {
        return zith::diag::DiagnosticBuilder(level, code);
    }

    // Bridge: add new-style diagnostic directly to bag
    void emit_diagnostic(zith::diag::Diagnostic diag) {
        bag_.emit(std::move(diag));
    }

private:
    zith::diag::DiagnosticBag bag_;
    zith::diag::SourceMap source_map_;
    mutable zith::diag::TerminalEmitter emitter_;
    zith::diag::HeuristicEngine heuristic_;
    ZithArena *arena_ = nullptr;

    // Legacy flat list — built on-demand from bag_ for C ABI compatibility
    mutable ZithDiagList legacy_list_;
    mutable bool legacy_stale_ = true;
    mutable std::vector<ZithDiagnostic> legacy_storage_;

    void rebuild_legacy_list() const;
    void emit(ZithSourceLoc loc, ZithDiagSeverity severity, const char *msg);
};

// Convenience macros (unchanged)
#define DIAG_ERROR(dm, loc, msg)   (dm).error(loc, msg)
#define DIAG_WARN(dm, loc, msg)    (dm).warning(loc, msg)
#define DIAG_NOTE(dm, loc, msg)    (dm).note(loc, msg)
#define DIAG_INFO(dm, msg)         (dm).info(msg)

#endif

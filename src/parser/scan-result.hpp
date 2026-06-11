#pragma once

#include "ast/ast-ids.hpp"
#include "memory/dyn-array.hpp"
#include "memory/span.hpp"

#include <string_view>

namespace zith::parser {

struct ScanEntry {
    std::string_view name;
    memory::Span span;
    ast::ExprId body_node;
};

struct ScanResult {
    memory::DynArray<ScanEntry> fns;
    memory::DynArray<ScanEntry> structs;
    memory::DynArray<ScanEntry> unions;
    memory::DynArray<ScanEntry> enums;
    memory::DynArray<ScanEntry> components;

    explicit ScanResult(memory::Arena &arena)
        : fns(arena), structs(arena), unions(arena), enums(arena), components(arena) {}
};

} // namespace zith::parser

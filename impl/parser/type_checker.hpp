#pragma once

#include "memory/arena.hpp"
#include "zith/parser.h"
#include "zith/typesystem.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace zith {

struct VarEntry {
    Type type;
    ZithBindingKind binding = ZITH_BINDING_LET;
    bool is_dirty = false;
};

struct StructField {
    std::string name;
    Type type;
};

struct StructDef {
    std::string name;
    std::vector<StructField> fields;
};

class TypeContext {
public:
    std::unordered_map<std::string, std::vector<ZithFuncPayload *>> functions;
    std::unordered_map<std::string, StructDef> structs;

    Type lookup_struct(const std::string& name) const {
        auto it = structs.find(name);
        if (it != structs.end()) {
            return {SemaType::Struct, false, false, ZITH_OWN_DEFAULT, it->second.name.c_str(), nullptr};
        }
        return {};
    }

    const StructDef* get_struct_def(const std::string& name) const {
        auto it = structs.find(name);
        return it != structs.end() ? &it->second : nullptr;
    }
};

class TypeChecker {
public:
    explicit TypeChecker(TypeContext& ctx, DiagManager* diag_mgr);

    Type get_type(ZithNode* node, Type expected_return = {});

    void check_struct_literal(ZithNode* expr, const Type& struct_type);

    void check_assignment(const std::string& lhs_name, const Type& lhs_type,
                         ZithNode* rhs_expr, Type rhs_type, ZithSourceLoc loc);

    bool is_assignable(const Type& dst, const Type& src);
    bool is_coercible(const Type& dst, const Type& src);

    static const char* base_type_name(SemaType t);

private:
    TypeContext& ctx_;
    DiagManager* diag_;

    static bool is_exact_match(const Type& lhs, const Type& rhs);

    Type type_from_node(const ZithNode* n);
    void emit_error(ZithSourceLoc loc, DiagCode code, const char* msg);
};

inline const char* TypeChecker::base_type_name(SemaType t) {
    switch (t) {
    case SemaType::Void:    return "void";
    case SemaType::Char:    return "char";
    case SemaType::Int:     return "int";
    case SemaType::Float:   return "float";
    case SemaType::String:  return "string";
    case SemaType::Bool:    return "bool";
    case SemaType::Opaque:   return "opaque";
    case SemaType::Invalid: return "invalid";
    case SemaType::VoidPtr: return "voidPtr";
    case SemaType::Module:  return "module";
    case SemaType::Struct:  return "struct";
    case SemaType::Array:   return "array";
    case SemaType::Slice:   return "slice";
    default:                return "unknown";
    }
}

} // namespace zith
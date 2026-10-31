#pragma once

#include "frontend/frontend.hpp"
#include "memory/arena.hpp"
#include "memory/flat-map.hpp"
#include "sema/modern-types.hpp"
#include "session/frontend-context.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zith::comptime {

struct InstantiationInstance {
    session::ModuleKey module;
    frontend::DeclId decl{};
    std::vector<sema::modern::TypeId> args;
    std::string mangled;
};

struct GenericCallBinding {
    session::ModuleKey module;
    frontend::ExprId callee{};
    size_t instance = ~size_t{0};
};

enum class GenericResolveStatus : uint8_t {
    Ok,
    Arity,
    CannotInfer,
    Explosion,
};

class GenericInstantiationPass {
public:
    GenericInstantiationPass(const session::CompilationSnapshot &snapshot,
                             sema::modern::TypeTable &type_table);

    [[nodiscard]] const GenericCallBinding *callBinding(const session::ModuleKey &module,
                                                        frontend::ExprId call) const noexcept;
    [[nodiscard]] const InstantiationInstance *at(size_t index) const noexcept;
    [[nodiscard]] size_t instanceCount() const noexcept {
        return instances_.size();
    }

    [[nodiscard]] GenericResolveStatus
    resolveArgs(const sema::modern::FunctionType &fn, size_t degree, uint32_t decl_id,
                const std::vector<sema::modern::TypeId> &explicit_args,
                const std::vector<sema::modern::TypeId> &argument_types,
                sema::modern::TypeId expected_result,
                const std::vector<bool> &weak_args,
                std::vector<sema::modern::TypeId> &out_args) const;

    /// Deduces struct-literal generic arguments from the declared field types
    /// that surround each initializer. Unlike function calls, inconsistent
    /// inferred values are left to the later field coercion so the normal
    /// TypeMismatch diagnostics win over GenericStructInfer.
    [[nodiscard]] GenericResolveStatus
    resolveStruct(size_t degree, uint32_t template_decl_id,
                  const std::vector<sema::modern::TypeId> &explicit_args,
                  const std::vector<sema::modern::TypeId> &declared_field_types,
                  const std::vector<sema::modern::TypeId> &argument_types,
                  std::vector<sema::modern::TypeId> &out_args) const;

    size_t bindCall(const session::ModuleKey &module, frontend::ExprId callee,
                    const session::ModuleKey &target_module, frontend::DeclId decl,
                    std::vector<sema::modern::TypeId> args);

    /// Ensures a concrete instance exists without touching any call-site
    /// binding. HIR lowering uses this to concretize a nested generic call
    /// whose template arguments are the enclosing function's type parameters.
    /// Returns the instance index, or `~size_t{0}` on explosion.
    size_t ensureInstance(const session::ModuleKey &target_module, frontend::DeclId decl,
                          std::vector<sema::modern::TypeId> args);

    /// Walks every concrete instance body and ensures the concrete instances
    /// its nested generic calls need, running to a fixpoint. A call bound inside
    /// `HashMap<K, V>` as `reserve<K, V>` therefore materializes
    /// `reserve<i64, i64>` before HIR lowering emits symbols. Without this the
    /// abstract `reserve<T, T>` template would be emitted and fail to link.
    void concretizeNestedInstances();

    /// True when `instance.args` still mentions a generic parameter. Such an
    /// instance is a template artifact, not a callable symbol, and must not be
    /// emitted as a standalone HIR function.
    [[nodiscard]] bool instanceIsAbstract(const InstantiationInstance &instance) const;

    [[nodiscard]] sema::modern::TypeId
    substituteType(sema::modern::TypeId type, const std::vector<sema::modern::TypeId> &args) const;

    /// True when `type` mentions any generic parameter, directly or through a
    /// nested struct/union/function shape. Used to decide whether a nested
    /// call's recorded arguments still need concretization.
    [[nodiscard]] bool typeMentionsGeneric(sema::modern::TypeId type) const;

    /// Substitutes the concrete arguments of an enclosing generic instance into
    /// the recorded type arguments of a nested call. This is what turns a call
    /// bound as `reserve<K, V>` inside `HashMap<K, V>` into `reserve<i64, i64>`
    /// once the enclosing function is instantiated.
    [[nodiscard]] std::vector<sema::modern::TypeId>
    substituteArgs(const std::vector<sema::modern::TypeId> &args,
                   const std::vector<sema::modern::TypeId> &enclosing) const;

    [[nodiscard]] sema::modern::TypeId
    substituteFunction(const sema::modern::FunctionType &fn,
                       const std::vector<sema::modern::TypeId> &args) const;

    [[nodiscard]] std::string mangledName(const session::ModuleKey &module, frontend::DeclId decl,
                                          const std::vector<sema::modern::TypeId> &args) const;

    [[nodiscard]] bool hasErrors() const noexcept {
        return has_errors_;
    }

private:
    static uint64_t callKey(const session::ModuleKey &module, frontend::ExprId call) noexcept;
    static constexpr size_t kMaxInstances = 1024;

    /// Shared recursive generic unification used by function calls and struct
    /// literals. `strict` keeps the legacy call behavior: a conflict after a
    /// generic parameter has been bound is a CannotInfer failure.
    [[nodiscard]] GenericResolveStatus
    resolveTypes(size_t degree, uint32_t decl_id,
                 const std::vector<sema::modern::TypeId> &explicit_args,
                 const std::vector<sema::modern::TypeId> &declared_types,
                 const std::vector<sema::modern::TypeId> &argument_types, bool strict,
                 sema::modern::TypeId declared_result, sema::modern::TypeId expected_result,
                 const std::vector<bool> &weak_args,
                 std::vector<sema::modern::TypeId> &out_args) const;

    const session::CompilationSnapshot &snapshot_;
    sema::modern::TypeTable &type_table_;
    bool has_errors_ = false;
    memory::FlatMap<uint64_t, GenericCallBinding> calls_;
    std::vector<InstantiationInstance> instances_;
};

} // namespace zith::comptime

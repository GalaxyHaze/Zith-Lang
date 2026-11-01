#include "comptime/generic-instantiate.hpp"
#include <functional>
#include <limits>
#include <string>

namespace zith::comptime {

namespace {

const frontend::Declaration *declarationFor(const session::CompilationSnapshot &snapshot,
                                            const session::ModuleKey &module,
                                            frontend::DeclId id) noexcept {
    if (!id)
        return nullptr;
    if (const auto *artifact = snapshot.findModule(module);
        artifact != nullptr && artifact->frontend != nullptr &&
        id.value <= artifact->frontend->declarations().size()) {
        return &artifact->frontend->declarations()[id.value - 1U];
    }
    // A call can name a declaration from another module; fall back to a linear
    // scan only in the uncommon import case so the mangled name stays stable.
    for (const auto &artifact_ptr : snapshot.modules()) {
        const auto *artifact = artifact_ptr.get();
        if (artifact == nullptr || artifact->frontend == nullptr)
            continue;
        if (artifact->key != module || id.value > artifact->frontend->declarations().size())
            continue;
        return &artifact->frontend->declarations()[id.value - 1U];
    }
    return nullptr;
}

const frontend::Declaration *declarationById(const session::CompilationSnapshot &snapshot,
                                             uint32_t decl_id) noexcept {
    for (const auto &artifact_ptr : snapshot.modules()) {
        const auto *artifact = artifact_ptr.get();
        if (artifact == nullptr || artifact->frontend == nullptr)
            continue;
        const auto &decls = artifact->frontend->declarations();
        if (decl_id >= 1U && decl_id <= decls.size() && decls[decl_id - 1U].id.value == decl_id)
            return &decls[decl_id - 1U];
    }
    return nullptr;
}

std::string_view baseTypeName(std::string_view name) noexcept {
    if (const size_t angle = name.find('<'); angle != std::string_view::npos)
        name = name.substr(0, angle);
    return name;
}

std::string concreteStructName(std::string_view base,
                               const memory::DynArray<sema::modern::TypeId> &fields,
                               const sema::modern::TypeTable &type_table) {
    std::string result(base);
    result += "<";
    bool first = true;
    for (const auto substituted : fields) {
        if (!first)
            result += ",";
        first = false;
        result += type_table.typeToString(substituted);
    }
    if (fields.empty()) {
        result += "?";
    }
    result += ">";
    return result;
}

std::string concreteTypeName(std::string_view base, const std::vector<sema::modern::TypeId> &args,
                             const sema::modern::TypeTable &type_table) {
    std::string result(base);
    result += "<";
    for (size_t i = 0; i < args.size(); ++i) {
        if (i != 0)
            result += ",";
        result += type_table.typeToString(args[i]);
    }
    result += ">";
    return result;
}

/// Collects the callee expression of every `Call` reachable from `root`. The
/// generic call-binding map is keyed by the callee `ExprId`, not the call
/// `ExprId`, so the walker records `operands[0]` for each call it finds.
void collectCallCallees(const frontend::FrontendSnapshot &frontend, const frontend::ExprId root,
                        std::vector<frontend::ExprId> &out, std::vector<uint8_t> &visited) {
    if (!root || root.value > frontend.expressions().size())
        return;
    if (visited[root.value] != 0U)
        return;
    visited[root.value] = 1U;
    const auto &expr    = frontend.expressions()[root.value - 1U];
    if (expr.kind == frontend::ExprKind::Call && !expr.operands.empty())
        out.push_back(expr.operands[0]);
    for (const auto operand : expr.operands)
        collectCallCallees(frontend, operand, out, visited);
    for (const auto condition : expr.conditions)
        collectCallCallees(frontend, condition, out, visited);
    if (expr.expansion)
        collectCallCallees(frontend, expr.expansion, out, visited);
    for (const auto stmt_id : expr.statements) {
        if (!stmt_id || stmt_id.value > frontend.statements().size())
            continue;
        const auto &stmt = frontend.statements()[stmt_id.value - 1U];
        collectCallCallees(frontend, stmt.expression, out, visited);
        collectCallCallees(frontend, stmt.binding.initializer, out, visited);
        for (const auto arg : stmt.arguments)
            collectCallCallees(frontend, arg, out, visited);
    }
}

} // namespace

GenericInstantiationPass::GenericInstantiationPass(const session::CompilationSnapshot &snapshot,
                                                   sema::modern::TypeTable &type_table)
    : snapshot_(snapshot), type_table_(type_table) {
    // Reserve the full cap up front so `InstantiationInstance *` returned by
    // `at()` stays valid even as nested calls append new instances while HIR
    // lowering walks a body. kMaxInstances is small enough that this is cheap.
    instances_.reserve(kMaxInstances);
}

const GenericCallBinding *
GenericInstantiationPass::callBinding(const session::ModuleKey &module,
                                      frontend::ExprId call) const noexcept {
    return calls_.get(callKey(module, call));
}

const InstantiationInstance *GenericInstantiationPass::at(size_t index) const noexcept {
    return index < instances_.size() ? &instances_[index] : nullptr;
}

uint64_t GenericInstantiationPass::callKey(const session::ModuleKey &module,
                                           frontend::ExprId call) noexcept {
    const uint32_t module_hash = static_cast<uint32_t>(std::hash<std::string>{}(module));
    return (static_cast<uint64_t>(call.value) << 32U) | module_hash;
}

GenericResolveStatus
GenericInstantiationPass::resolveArgs(const sema::modern::FunctionType &fn, const size_t degree,
                                      const uint32_t decl_id,
                                      const std::vector<sema::modern::TypeId> &explicit_args,
                                      const std::vector<sema::modern::TypeId> &argument_types,
                                      const sema::modern::TypeId expected_result,
                                      const std::vector<bool> &weak_args,
                                      std::vector<sema::modern::TypeId> &out_args) const {
    std::vector<sema::modern::TypeId> declared_types;
    declared_types.reserve(fn.params.size());
    for (const auto param : fn.params)
        declared_types.push_back(param);
    return resolveTypes(degree, decl_id, explicit_args, declared_types, argument_types, true,
                        fn.result, expected_result, weak_args, out_args);
}

GenericResolveStatus GenericInstantiationPass::resolveStruct(
    const size_t degree, const uint32_t template_decl_id,
    const std::vector<sema::modern::TypeId> &explicit_args,
    const std::vector<sema::modern::TypeId> &declared_field_types,
    const std::vector<sema::modern::TypeId> &argument_types,
    std::vector<sema::modern::TypeId> &out_args) const {
    return resolveTypes(degree, template_decl_id, explicit_args, declared_field_types,
                        argument_types, false, sema::modern::TypeId{}, sema::modern::TypeId{},
                        {}, out_args);
}

GenericResolveStatus
GenericInstantiationPass::resolveTypes(const size_t degree, const uint32_t decl_id,
                                       const std::vector<sema::modern::TypeId> &explicit_args,
                                       const std::vector<sema::modern::TypeId> &declared_types,
                                       const std::vector<sema::modern::TypeId> &argument_types,
                                       const bool strict,
                                       const sema::modern::TypeId declared_result,
                                       const sema::modern::TypeId expected_result,
                                       const std::vector<bool> &weak_args,
                                       std::vector<sema::modern::TypeId> &out_args) const {
    if (degree == 0)
        return GenericResolveStatus::Arity;
    if (!explicit_args.empty() && explicit_args.size() != degree)
        return GenericResolveStatus::Arity;
    if (strict && explicit_args.size() == degree) {
        for (const auto arg : explicit_args)
            if (!arg)
                return GenericResolveStatus::CannotInfer;
        out_args = explicit_args;
        return GenericResolveStatus::Ok;
    }

    std::vector<sema::modern::TypeId> resolved(degree, sema::modern::TypeId{});
    for (size_t i = 0; i < degree; ++i)
        if (i < explicit_args.size())
            resolved[i] = explicit_args[i];

    // Inference is not limited to direct `T` parameters: `first<T>(p: Pair<T>)`
    // and generic method receivers put the parameter inside a named struct or
    // pointer type. The recursive walk below matches the declared shape against
    // the concrete argument and binds any usable generic origin.
    const auto inferredIndex = [&](uint32_t origin_decl, uint32_t origin_index) -> size_t {
        if (origin_decl == decl_id && origin_index < degree)
            return origin_index;
        const auto *call_decl = declarationById(snapshot_, decl_id);
        const auto *origin    = declarationById(snapshot_, origin_decl);
        if (call_decl == nullptr || origin == nullptr ||
            origin_index >= origin->genericParams.size())
            return ~size_t{0};
        const std::string_view origin_name = origin->genericParams[origin_index].name;
        for (size_t index = 0; index < call_decl->genericParams.size(); ++index) {
            if (call_decl->genericParams[index].name == origin_name)
                return index;
        }
        return ~size_t{0};
    };

    bool failed      = false;
    // Only parameters that mention a generic parameter carry inference
    // information. A concrete parameter such as `n: i64` or `a: dyn Allocator`
    // must not fail inference just because the argument widens or coerces to
    // it (an `i32` literal for an `i64`, a struct for a `dyn` interface); that
    // coercion is validated separately at the call site.
    const auto mentionsGeneric = [&](auto &&self, sema::modern::TypeId type,
                                     unsigned depth = 0U) -> bool {
        if (!type || depth >= 16U)
            return false;
        type = type_table_.stripQualifiers(type);
        if (type_table_.kindOf(type) == sema::modern::TypeKind::GenericParam)
            return true;
        if (const auto *alias = type_table_.alias(type))
            return self(self, alias->target, depth + 1U);
        if (const auto *nominal = type_table_.nominal(type))
            return self(self, nominal->target, depth + 1U);
        if (const auto *opt = type_table_.optional(type))
            return self(self, opt->inner, depth + 1U);
        if (const auto *ptr = type_table_.pointer(type))
            return self(self, ptr->pointee, depth + 1U);
        if (const auto *slice = type_table_.slice(type))
            return self(self, slice->element, depth + 1U);
        if (const auto *array = type_table_.array(type))
            return self(self, array->element, depth + 1U);
        if (const auto *nested = type_table_.function(type)) {
            for (const auto param : nested->params)
                if (self(self, param, depth + 1U))
                    return true;
            return self(self, nested->result, depth + 1U);
        }
        if (const auto *failable = type_table_.failable(type))
            return self(self, failable->inner, depth + 1U);
        if (const auto *dyn = type_table_.dyn_type(type))
            return self(self, dyn->target, depth + 1U);
        if (const auto *st = type_table_.struct_type(type)) {
            for (const auto arg : st->args)
                if (self(self, arg, depth + 1U))
                    return true;
            for (const auto field : st->fields)
                if (self(self, field, depth + 1U))
                    return true;
            return false;
        }
        if (const auto *ut = type_table_.union_type(type)) {
            for (const auto member : ut->members)
                if (self(self, member, depth + 1U))
                    return true;
            return false;
        }
        if (const auto *sum = type_table_.sum(type)) {
            for (const auto member : sum->members)
                if (self(self, member, depth + 1U))
                    return true;
            return false;
        }
        return false;
    };
    // Parameters bound from a weak argument (an unsuffixed numeric literal that
    // the call-site coercion would adapt anyway) may be overridden by a later
    // strong binding, such as the expected result type of a contextual call.
    std::vector<bool> weak_resolved(degree, false);
    const auto unify = [&](auto &&self, sema::modern::TypeId param, sema::modern::TypeId arg,
                           const bool weak) -> void {
        if ((strict && failed) || !param || !arg)
            return;
        // A parameter with no generic occurrence cannot bind anything; leave
        // its type checking to the ordinary call-site coercion.
        if (!mentionsGeneric(mentionsGeneric, param))
            return;
        // A borrow parameter is represented as `*lend T` in the sema ABI. For
        // generic inference the call-site annotation carries the pointee type,
        // so a raw value can be unified against the declared pointer's pointee.
        if (const auto *ptr = type_table_.pointer(type_table_.stripQualifiers(param));
            ptr != nullptr) {
            const auto *pointee_qual = type_table_.qualified(type_table_.canonical(ptr->pointee));
            if (pointee_qual != nullptr &&
                (pointee_qual->ownership == types::OwnershipKind::Lend ||
                 pointee_qual->ownership == types::OwnershipKind::View)) {
                if (type_table_.kindOf(type_table_.stripQualifiers(arg)) ==
                    sema::modern::TypeKind::Pointer) {
                    if (const auto *arg_ptr = type_table_.pointer(type_table_.stripQualifiers(arg));
                        arg_ptr != nullptr)
                        arg = type_table_.stripQualifiers(arg_ptr->pointee);
                } else {
                    // `view self` and `lend self` are lowered to a borrow
                    // pointer in the declared ABI, but expression inference
                    // keeps the pointee type for the argument (`Entry<K>`
                    // rather than `*view Entry<K>`). Unify directly with that
                    // pointee so a generic callee can bind its own `K` to the
                    // caller's `K`.
                }
                param = ptr->pointee;
            }
        }
        param = type_table_.stripQualifiers(param);
        arg   = type_table_.stripQualifiers(arg);

        uint32_t origin_decl = 0;
        uint32_t origin_idx  = 0;
        type_table_.genericParamOrigin(param, &origin_decl, &origin_idx);
        if (origin_decl != 0 && origin_idx < degree) {
            const size_t target = inferredIndex(origin_decl, origin_idx);
            if (target == ~size_t{0} || target >= degree) {
                failed = true;
                return;
            }
            if (resolved[target]) {
                const bool same =
                    resolved[target] == arg ||
                    type_table_.stripQualifiers(resolved[target]) ==
                        type_table_.stripQualifiers(arg);
                if (same)
                    return;
                if (!weak && weak_resolved[target]) {
                    // A strong binding replaces an earlier weak literal binding.
                    resolved[target]      = arg;
                    weak_resolved[target] = false;
                    return;
                }
                if (strict && !weak)
                    failed = true;
                return;
            }
            resolved[target]      = arg;
            weak_resolved[target] = weak;
            return;
        }

        if (param == arg)
            return;

        if (const auto *alias = type_table_.alias(param)) {
            self(self, alias->target, arg, weak);
            return;
        }
        if (const auto *nominal = type_table_.nominal(param)) {
            if (const auto *arg_nominal = type_table_.nominal(arg)) {
                self(self, nominal->target, arg_nominal->target, weak);
                return;
            }
            self(self, nominal->target, arg, weak);
            return;
        }

        const sema::modern::TypeKind p_kind = type_table_.kindOf(param);
        const sema::modern::TypeKind a_kind = type_table_.kindOf(arg);
        if (p_kind != a_kind) {
            if (strict && p_kind == sema::modern::TypeKind::Optional) {
                const auto *coercive_opt = type_table_.optional(param);
                if (coercive_opt != nullptr) {
                    // `i32` is a normal argument for `?T`, `?T` is a normal
                    // argument for `??T`, and `?i32` is a normal argument for
                    // `??T`. Wrap the argument in the declared optional layers
                    // before binding `T`, so generic inference matches the
                    // coercion the call site will lower. Only optional
                    // coercion is eligible; other coercions stay exact.
                    std::vector<sema::modern::TypeId> probe(degree, sema::modern::TypeId{});
                    for (size_t i = 0; i < degree; ++i)
                        probe[i] = resolved[i];
                    bool probe_failed      = false;
                    const auto probe_unify = [&](auto &&probe_self,
                                                 sema::modern::TypeId probe_param,
                                                 sema::modern::TypeId probe_arg) -> void {
                        if (probe_failed || !probe_param || !probe_arg)
                            return;
                        probe_param = type_table_.stripQualifiers(probe_param);
                        probe_arg   = type_table_.stripQualifiers(probe_arg);

                        uint32_t probe_origin_decl = 0;
                        uint32_t probe_origin_idx  = 0;
                        type_table_.genericParamOrigin(probe_param, &probe_origin_decl,
                                                       &probe_origin_idx);
                        if (probe_origin_decl != 0 && probe_origin_idx < degree) {
                            const size_t target =
                                inferredIndex(probe_origin_decl, probe_origin_idx);
                            if (target == ~size_t{0} || target >= degree) {
                                probe_failed = true;
                                return;
                            }
                            if (probe[target] && probe[target] != probe_arg &&
                                type_table_.stripQualifiers(probe[target]) !=
                                    type_table_.stripQualifiers(probe_arg)) {
                                probe_failed = true;
                                return;
                            }
                            probe[target] = probe_arg;
                            return;
                        }
                        if (probe_param == probe_arg)
                            return;
                        if (const auto *probe_alias = type_table_.alias(probe_param)) {
                            probe_self(probe_self, probe_alias->target, probe_arg);
                            return;
                        }
                        const sema::modern::TypeKind probe_p_kind = type_table_.kindOf(probe_param);
                        const sema::modern::TypeKind probe_a_kind = type_table_.kindOf(probe_arg);
                        if (probe_p_kind != probe_a_kind) {
                            probe_failed = true;
                            return;
                        }
                        switch (probe_p_kind) {
                        case sema::modern::TypeKind::Pointer:
                            if (const auto *pp = type_table_.pointer(probe_param);
                                pp != nullptr && type_table_.pointer(probe_arg) != nullptr)
                                probe_self(probe_self, pp->pointee,
                                           type_table_.pointer(probe_arg)->pointee);
                            else
                                probe_failed = true;
                            break;
                        case sema::modern::TypeKind::Optional:
                            if (const auto *ppo = type_table_.optional(probe_param);
                                ppo != nullptr && type_table_.optional(probe_arg) != nullptr)
                                probe_self(probe_self, ppo->inner,
                                           type_table_.optional(probe_arg)->inner);
                            else
                                probe_failed = true;
                            break;
                        case sema::modern::TypeKind::Array:
                            if (const auto *pa = type_table_.array(probe_param);
                                pa != nullptr && type_table_.array(probe_arg) != nullptr &&
                                pa->size == type_table_.array(probe_arg)->size)
                                probe_self(probe_self, pa->element,
                                           type_table_.array(probe_arg)->element);
                            else
                                probe_failed = true;
                            break;
                        case sema::modern::TypeKind::Slice:
                            if (const auto *ps = type_table_.slice(probe_param);
                                ps != nullptr && type_table_.slice(probe_arg) != nullptr)
                                probe_self(probe_self, ps->element,
                                           type_table_.slice(probe_arg)->element);
                            else
                                probe_failed = true;
                            break;
                        case sema::modern::TypeKind::Failable:
                            if (const auto *pf = type_table_.failable(probe_param);
                                pf != nullptr && type_table_.failable(probe_arg) != nullptr)
                                probe_self(probe_self, pf->inner,
                                           type_table_.failable(probe_arg)->inner);
                            else
                                probe_failed = true;
                            break;
                        case sema::modern::TypeKind::Struct: {
                            const auto *ps  = type_table_.struct_type(probe_param);
                            const auto *as_ = type_table_.struct_type(probe_arg);
                            if (ps != nullptr && as_ != nullptr &&
                                baseTypeName(ps->name) == baseTypeName(as_->name) &&
                                ps->fields.size() == as_->fields.size()) {
                                // Generic structs carry their concrete arguments
                                // as metadata. Prefer those over field scans so
                                // `Entry<K>` from different callers keeps its own
                                // K binding while still mapping by index.
                                if (ps->args.size() == as_->args.size() && !ps->args.empty()) {
                                    for (size_t i = 0; i < ps->args.size(); ++i)
                                        probe_self(probe_self, ps->args[i], as_->args[i]);
                                } else {
                                    for (size_t i = 0; i < ps->fields.size(); ++i)
                                        probe_self(probe_self, ps->fields[i], as_->fields[i]);
                                }
                            } else {
                                probe_failed = true;
                            }
                            break;
                        }
                        case sema::modern::TypeKind::Union: {
                            const auto *pu = type_table_.union_type(probe_param);
                            const auto *au = type_table_.union_type(probe_arg);
                            if (pu != nullptr && au != nullptr &&
                                baseTypeName(pu->name) == baseTypeName(au->name) &&
                                pu->members.size() == au->members.size()) {
                                for (size_t i = 0; i < pu->members.size(); ++i)
                                    probe_self(probe_self, pu->members[i], au->members[i]);
                            } else {
                                probe_failed = true;
                            }
                            break;
                        }
                        default:
                            probe_failed = true;
                            break;
                        }
                    };

                    sema::modern::TypeId peeled_param = coercive_opt->inner;
                    while (type_table_.kindOf(type_table_.stripQualifiers(peeled_param)) ==
                           sema::modern::TypeKind::Optional) {
                        const auto *peel =
                            type_table_.optional(type_table_.stripQualifiers(peeled_param));
                        if (peel == nullptr)
                            break;
                        peeled_param = peel->inner;
                    }
                    probe_unify(probe_unify, peeled_param, arg);
                    if (!probe_failed) {
                        for (size_t i = 0; i < degree; ++i)
                            resolved[i] = probe[i];
                        return;
                    }
                }
                failed = true;
                return;
            }
            if (strict)
                failed = true;
            return;
        }
        switch (p_kind) {
        case sema::modern::TypeKind::Pointer:
            if (const auto *pp = type_table_.pointer(param);
                pp != nullptr && type_table_.pointer(arg) != nullptr)
                self(self, pp->pointee, type_table_.pointer(arg)->pointee, weak);
            else if (strict)
                failed = true;
            break;
        case sema::modern::TypeKind::Optional:
            if (const auto *po = type_table_.optional(param);
                po != nullptr && type_table_.optional(arg) != nullptr)
                self(self, po->inner, type_table_.optional(arg)->inner, weak);
            else if (strict)
                failed = true;
            break;
        case sema::modern::TypeKind::Array:
            if (const auto *pa = type_table_.array(param);
                pa != nullptr && type_table_.array(arg) != nullptr) {
                const auto *aa = type_table_.array(arg);
                if (pa->size != aa->size && strict)
                    failed = true;
                else if (pa->size == aa->size)
                    self(self, pa->element, aa->element, weak);
            } else {
                if (strict)
                    failed = true;
            }
            break;
        case sema::modern::TypeKind::Slice:
            if (const auto *ps = type_table_.slice(param);
                ps != nullptr && type_table_.slice(arg) != nullptr)
                self(self, ps->element, type_table_.slice(arg)->element, weak);
            else if (strict)
                failed = true;
            break;
        case sema::modern::TypeKind::Failable:
            if (const auto *pf = type_table_.failable(param);
                pf != nullptr && type_table_.failable(arg) != nullptr)
                self(self, pf->inner, type_table_.failable(arg)->inner, weak);
            else if (strict)
                failed = true;
            break;
        case sema::modern::TypeKind::Function: {
            const auto *pf = type_table_.function(param);
            const auto *af = type_table_.function(arg);
            if (pf != nullptr && af != nullptr && pf->params.size() == af->params.size()) {
                for (size_t i = 0; i < pf->params.size(); ++i)
                    self(self, pf->params[i], af->params[i], weak);
                self(self, pf->result, af->result, weak);
            } else {
                if (strict)
                    failed = true;
            }
            break;
        }
        case sema::modern::TypeKind::Sum: {
            const auto *ps  = type_table_.sum(param);
            const auto *as_ = type_table_.sum(arg);
            if (ps != nullptr && as_ != nullptr && ps->members.size() == as_->members.size()) {
                for (size_t i = 0; i < ps->members.size(); ++i)
                    self(self, ps->members[i], as_->members[i], weak);
            } else {
                if (strict)
                    failed = true;
            }
            break;
        }
        case sema::modern::TypeKind::Struct: {
            const auto *ps  = type_table_.struct_type(param);
            const auto *as_ = type_table_.struct_type(arg);
            if (ps != nullptr && as_ != nullptr &&
                baseTypeName(ps->name) == baseTypeName(as_->name) &&
                ps->fields.size() == as_->fields.size()) {
                if (ps->args.size() == as_->args.size() && !ps->args.empty()) {
                    for (size_t i = 0; i < ps->args.size(); ++i)
                        self(self, ps->args[i], as_->args[i], weak);
                } else {
                    for (size_t i = 0; i < ps->fields.size(); ++i)
                        self(self, ps->fields[i], as_->fields[i], weak);
                }
            } else {
                if (strict)
                    failed = true;
            }
            break;
        }
        case sema::modern::TypeKind::Union: {
            const auto *pu = type_table_.union_type(param);
            const auto *au = type_table_.union_type(arg);
            if (pu != nullptr && au != nullptr &&
                baseTypeName(pu->name) == baseTypeName(au->name) &&
                pu->members.size() == au->members.size()) {
                for (size_t i = 0; i < pu->members.size(); ++i)
                    self(self, pu->members[i], au->members[i], weak);
            } else {
                if (strict)
                    failed = true;
            }
            break;
        }
        case sema::modern::TypeKind::Qualified:
        case sema::modern::TypeKind::Alias:
        case sema::modern::TypeKind::Nominal:
            // Handled above after stripping; reaching here is an interned shape
            // the concrete pass does not currently match structurally.
            if (strict)
                failed = true;
            break;
        default:
            if (strict)
                failed = true;
            break;
        }
    };

    for (size_t p = 0; p < declared_types.size() && p < argument_types.size(); ++p)
        unify(unify, declared_types[p], argument_types[p],
              p < weak_args.size() && weak_args[p]);
    if (strict && failed)
        return GenericResolveStatus::CannotInfer;

    // Contextual inference: when the call's result type is used in a position
    // with a known expected type (a binding annotation, a declared return
    // type, or a typed argument), unify the declared result shape against that
    // expectation so result-only parameters such as `fn Ok<T,E>(): Result<T,E>`
    // or `fn new<T>(): ?*T` bind from the context instead of requiring explicit
    // type arguments. Arguments that already bound a parameter keep priority:
    // the unify below only fills parameters still unbound or checks agreement.
    if (declared_result && expected_result)
        unify(unify, declared_result, expected_result, false);
    if (strict && failed)
        return GenericResolveStatus::CannotInfer;

    for (const auto arg : resolved) {
        if (!arg)
            return GenericResolveStatus::CannotInfer;
    }
    out_args = std::move(resolved);
    return GenericResolveStatus::Ok;
}

size_t GenericInstantiationPass::bindCall(const session::ModuleKey &module, frontend::ExprId callee,
                                          const session::ModuleKey &target_module,
                                          frontend::DeclId decl,
                                          std::vector<sema::modern::TypeId> args) {
    const size_t index = ensureInstance(target_module, decl, std::move(args));
    if (index == ~size_t{0})
        return index;
    calls_.insert(callKey(module, callee),
                  GenericCallBinding{module, callee, index});
    return index;
}

size_t GenericInstantiationPass::ensureInstance(const session::ModuleKey &target_module,
                                                frontend::DeclId decl,
                                                std::vector<sema::modern::TypeId> args) {
    for (size_t i = 0; i < instances_.size(); ++i) {
        const auto &existing = instances_[i];
        if (existing.module == target_module && existing.decl == decl && existing.args == args) {
            return i;
        }
    }
    if (instances_.size() >= kMaxInstances)
        return ~size_t{0};
    InstantiationInstance instance;
    instance.module  = target_module;
    instance.decl    = decl;
    instance.args    = std::move(args);
    instance.mangled = mangledName(target_module, decl, instance.args);
    instances_.push_back(std::move(instance));
    return instances_.size() - 1U;
}

sema::modern::TypeId
GenericInstantiationPass::substituteType(const sema::modern::TypeId type,
                                         const std::vector<sema::modern::TypeId> &args) const {
    if (!type)
        return type;

    // Preserve memory qualifiers while substituting their inner type. The
    // switch below intentionally reaches the bare entry after peeling aliases
    // and nominals that may hide a generic parameter.
    if (const auto *qualified = type_table_.qualified(type))
        return type_table_.internQualified(substituteType(qualified->inner, args),
                                           qualified->ownership, qualified->isMut);
    if (const auto *alias = type_table_.alias(type)) {
        if (const auto named = type_table_.namedTypeName(type); !named.empty()) {
            const sema::modern::TypeId substituted = substituteType(alias->target, args);
            const std::string concrete             = concreteTypeName(named, args, type_table_);
            const sema::modern::TypeId existing    = type_table_.lookupNamed(concrete);
            if (existing)
                return sema::modern::TypeId{existing};
            const sema::modern::TypeId reified = type_table_.internAlias(concrete, substituted);
            type_table_.setDefiningModule(reified, type_table_.definingModule(type));
            type_table_.registerNamed(concrete, reified);
            return reified;
        }
        return type_table_.internAlias(substituteType(alias->target, args));
    }
    if (const auto *nominal = type_table_.nominal(type)) {
        if (const auto named = type_table_.namedTypeName(type); !named.empty()) {
            const sema::modern::TypeId substituted = substituteType(nominal->target, args);
            const std::string concrete             = concreteTypeName(named, args, type_table_);
            const sema::modern::TypeId existing    = type_table_.lookupNamed(concrete);
            if (existing)
                return sema::modern::TypeId{existing};
            const sema::modern::TypeId reified = type_table_.internNominal(concrete, substituted);
            type_table_.setDefiningModule(reified, type_table_.definingModule(type));
            type_table_.registerNamed(concrete, reified);
            return reified;
        }
        return type_table_.internNominal(nominal->name, substituteType(nominal->target, args));
    }

    const sema::modern::TypeId resolved = type_table_.stripQualifiers(type);
    uint32_t origin_decl                = 0;
    uint32_t origin_index               = 0;
    type_table_.genericParamOrigin(resolved, &origin_decl, &origin_index);
    if (origin_decl != 0 && origin_index < args.size())
        return args[origin_index];

    switch (type_table_.kindOf(resolved)) {
    case sema::modern::TypeKind::Pointer:
        if (const auto *ptr = type_table_.pointer(resolved))
            return type_table_.internPointer(substituteType(ptr->pointee, args));
        break;
    case sema::modern::TypeKind::Optional:
        if (const auto *opt = type_table_.optional(resolved))
            return type_table_.internOptional(substituteType(opt->inner, args));
        break;
    case sema::modern::TypeKind::Array:
        if (const auto *array = type_table_.array(resolved))
            return type_table_.internArray(substituteType(array->element, args), array->size);
        break;
    case sema::modern::TypeKind::Slice:
        if (const auto *slice = type_table_.slice(resolved))
            return type_table_.internSlice(substituteType(slice->element, args));
        break;
    case sema::modern::TypeKind::Function: {
        const auto *fn = type_table_.function(resolved);
        if (fn != nullptr) {
            auto &params = type_table_.makeTypeStorage();
            for (const auto param : fn->params)
                params.push(substituteType(param, args));
            return type_table_.internFunction(params, substituteType(fn->result, args));
        }
        break;
    }
    case sema::modern::TypeKind::Sum: {
        if (const auto *sum = type_table_.sum(resolved)) {
            auto &members = type_table_.makeTypeStorage();
            for (const auto member : sum->members)
                members.push(substituteType(member, args));
            return type_table_.internSum(members);
        }
        break;
    }
    case sema::modern::TypeKind::Struct: {
        if (const auto *st = type_table_.struct_type(resolved)) {
            auto &fields = type_table_.makeTypeStorage();
            auto &meta   = type_table_.makeFieldMetaStorage();
            for (const auto field : st->fields)
                fields.push(substituteType(field, args));
            if (st->field_meta.size() >= fields.size()) {
                for (size_t field_index = 0; field_index < fields.size(); ++field_index)
                    meta.push(sema::modern::FieldMeta{st->field_meta[field_index].visibility,
                                                      st->field_meta[field_index].modDepth,
                                                      st->field_meta[field_index].owner});
            }
            const std::string_view base = baseTypeName(st->name);
            const std::string concrete  = concreteTypeName(base, args, type_table_);
            if (const sema::modern::TypeId existing =
                    type_table_.lookupReifiedStruct(concrete, args))
                return existing;
            const sema::modern::TypeId reified =
                type_table_.internStruct(concrete, fields, &st->field_names, &meta, &args,
                                         st->explicit_alignment, st->isComponent);
            type_table_.setDefiningModule(reified, type_table_.definingModule(resolved));
            type_table_.registerNamed(concrete, reified);
            return reified;
        }
        break;
    }
    case sema::modern::TypeKind::Enum: {
        if (const auto *et = type_table_.enum_type(resolved)) {
            const std::string concrete =
                concreteTypeName(baseTypeName(et->name), args, type_table_);
            if (const sema::modern::TypeId existing = type_table_.lookupNamed(concrete))
                return existing;
            auto &variant_names = type_table_.makeStringStorage();
            for (const auto name_view : et->variant_names)
                variant_names.push(name_view);
            auto &discs = type_table_.makeDiscStorage();
            for (const auto disc : et->discriminants)
                discs.push(disc);
            const sema::modern::TypeId reified = type_table_.internEnum(
                concrete, substituteType(et->underlying, args), variant_names, discs);
            type_table_.setDefiningModule(reified, type_table_.definingModule(resolved));
            type_table_.registerNamed(concrete, reified);
            return reified;
        }
        break;
    }
    case sema::modern::TypeKind::Union: {
        if (const auto *ut = type_table_.union_type(resolved)) {
            auto &members = type_table_.makeTypeStorage();
            for (const auto member : ut->members)
                members.push(substituteType(member, args));
            // The concrete name must come from the substituted members, not the
            // template's `T`/`E` spellings, or every instance would collapse to
            // `Result<T,E>` and share one (wrong) concrete union.
            const std::string concrete =
                concreteStructName(baseTypeName(ut->name), members, type_table_);
            if (const sema::modern::TypeId existing = type_table_.lookupNamed(concrete))
                return existing;
            const sema::modern::TypeId reified =
                type_table_.internUnion(concrete, members, ut->is_tagged);
            type_table_.setDefiningModule(reified, type_table_.definingModule(resolved));
            type_table_.registerNamed(concrete, reified);
            return reified;
        }
        break;
    }
    case sema::modern::TypeKind::Failable:
        if (const auto *failable = type_table_.failable(resolved))
            return type_table_.internFailable(substituteType(failable->inner, args));
        break;
    default:
        break;
    }
    return type;
}

sema::modern::TypeId
GenericInstantiationPass::substituteFunction(const sema::modern::FunctionType &fn,
                                             const std::vector<sema::modern::TypeId> &args) const {
    auto &params = type_table_.makeTypeStorage();
    for (const auto param : fn.params)
        params.push(substituteType(param, args));
    return type_table_.internFunction(params, substituteType(fn.result, args));
}

bool GenericInstantiationPass::typeMentionsGeneric(const sema::modern::TypeId type) const {
    const auto visit = [&](auto &&self, sema::modern::TypeId current,
                           unsigned depth) -> bool {
        if (!current || depth >= 16U)
            return false;
        current = type_table_.stripQualifiers(current);
        if (type_table_.kindOf(current) == sema::modern::TypeKind::GenericParam)
            return true;
        if (const auto *alias = type_table_.alias(current))
            return self(self, alias->target, depth + 1U);
        if (const auto *nominal = type_table_.nominal(current))
            return self(self, nominal->target, depth + 1U);
        if (const auto *opt = type_table_.optional(current))
            return self(self, opt->inner, depth + 1U);
        if (const auto *ptr = type_table_.pointer(current))
            return self(self, ptr->pointee, depth + 1U);
        if (const auto *slice = type_table_.slice(current))
            return self(self, slice->element, depth + 1U);
        if (const auto *array = type_table_.array(current))
            return self(self, array->element, depth + 1U);
        if (const auto *nested = type_table_.function(current)) {
            for (const auto param : nested->params)
                if (self(self, param, depth + 1U))
                    return true;
            return self(self, nested->result, depth + 1U);
        }
        if (const auto *failable = type_table_.failable(current))
            return self(self, failable->inner, depth + 1U);
        if (const auto *dyn = type_table_.dyn_type(current))
            return self(self, dyn->target, depth + 1U);
        if (const auto *st = type_table_.struct_type(current)) {
            for (const auto arg : st->args)
                if (self(self, arg, depth + 1U))
                    return true;
            for (const auto field : st->fields)
                if (self(self, field, depth + 1U))
                    return true;
            return false;
        }
        if (const auto *ut = type_table_.union_type(current)) {
            for (const auto member : ut->members)
                if (self(self, member, depth + 1U))
                    return true;
            return false;
        }
        if (const auto *sum = type_table_.sum(current)) {
            for (const auto member : sum->members)
                if (self(self, member, depth + 1U))
                    return true;
            return false;
        }
        return false;
    };
    return visit(visit, type, 0U);
}

std::vector<sema::modern::TypeId>
GenericInstantiationPass::substituteArgs(const std::vector<sema::modern::TypeId> &args,
                                         const std::vector<sema::modern::TypeId> &enclosing) const {
    std::vector<sema::modern::TypeId> result;
    result.reserve(args.size());
    for (const auto arg : args)
        result.push_back(substituteType(arg, enclosing));
    return result;
}

bool GenericInstantiationPass::instanceIsAbstract(
    const InstantiationInstance &instance) const {
    for (const auto arg : instance.args)
        if (typeMentionsGeneric(arg))
            return true;
    return false;
}

void GenericInstantiationPass::concretizeNestedInstances() {
    // A generic body's nested generic calls are recorded once, against the
    // template's own type parameters (for example `reserve<K, V>` inside
    // `HashMap<K, V>`). Those bindings never mention the concrete arguments,
    // so each concrete instance of the enclosing function needs its own
    // concrete instances of the callees. Walk every concrete instance's body,
    // substitute the enclosing arguments into each bound call, and materialize
    // the result. Repeat until no new instance appears so transitive chains
    // (`outer<T>` -> `mid<T>` -> `inner<T>`) are fully closed.
    std::vector<uint8_t> visited;
    std::vector<frontend::ExprId> callees;
    for (size_t cursor = 0; cursor < instances_.size(); ++cursor) {
        const InstantiationInstance source = instances_[cursor];
        if (instanceIsAbstract(source))
            continue;
        const auto *module = snapshot_.findModule(source.module);
        if (module == nullptr || module->frontend == nullptr)
            continue;
        const auto *decl = declarationFor(snapshot_, source.module, source.decl);
        if (decl == nullptr || !decl->body)
            continue;

        const auto &frontend = *module->frontend;
        visited.assign(frontend.expressions().size() + 1U, 0U);
        callees.clear();
        collectCallCallees(frontend, decl->body, callees, visited);
        for (const auto callee : callees) {
            const auto *binding = calls_.get(callKey(source.module, callee));
            if (binding == nullptr)
                continue;
            const auto *bound = at(binding->instance);
            if (bound == nullptr || !instanceIsAbstract(*bound))
                continue;
            std::vector<sema::modern::TypeId> concrete =
                substituteArgs(bound->args, source.args);
            bool still_abstract = false;
            for (const auto arg : concrete) {
                if (typeMentionsGeneric(arg)) {
                    still_abstract = true;
                    break;
                }
            }
            if (still_abstract)
                continue;
            (void)ensureInstance(bound->module, bound->decl, std::move(concrete));
        }
    }
}

std::string
GenericInstantiationPass::mangledName(const session::ModuleKey &module, frontend::DeclId decl,
                                      const std::vector<sema::modern::TypeId> &args) const {
    std::string name;
    if (const auto *declaration = declarationFor(snapshot_, module, decl);
        declaration != nullptr && !declaration->name.empty()) {
        name = declaration->name;
    }
    std::string result = name + "<";
    for (size_t i = 0; i < args.size(); ++i) {
        if (i != 0)
            result += ",";
        result += type_table_.typeToString(args[i]);
    }
    result += ">";
    return result;
}

} // namespace zith::comptime

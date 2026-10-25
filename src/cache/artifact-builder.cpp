#include "artifact-builder.hpp"

#include "common/overloaded.hpp"
#include "zirl/zirl-header.hpp"

#include <algorithm>
#include <sstream>

namespace zith::cache {

namespace {

std::vector<std::string> genericArgsFromName(std::string_view mangled) {
    std::vector<std::string> args;
    const auto open = mangled.find('<');
    if (open == std::string_view::npos || mangled.empty() || mangled.back() != '>')
        return args;

    size_t start      = open + 1U;
    size_t depth      = 0;
    const size_t last = mangled.size() - 1U;
    for (size_t i = start; i < last; ++i) {
        const char current = mangled[i];
        if (current == '<') {
            ++depth;
        } else if (current == '>') {
            if (depth > 0)
                --depth;
        } else if (current == ',' && depth == 0) {
            args.emplace_back(mangled.substr(start, i - start));
            start = i + 1U;
        }
    }
    if (start < last)
        args.emplace_back(mangled.substr(start, last - start));
    return args;
}

CompactSymKind mapDeclKind(hir::HirDeclarationKind kind) {
    switch (kind) {
    case hir::HirDeclarationKind::Function:
        return CompactSymKind::Fn;
    case hir::HirDeclarationKind::Struct:
        return CompactSymKind::Struct;
    case hir::HirDeclarationKind::Trait:
        return CompactSymKind::Trait;
    case hir::HirDeclarationKind::Interface:
        return CompactSymKind::Interface;
    case hir::HirDeclarationKind::Enum:
        return CompactSymKind::Enum;
    case hir::HirDeclarationKind::Alias:
        return CompactSymKind::Alias;
    case hir::HirDeclarationKind::Variable:
        return CompactSymKind::Variable;
    case hir::HirDeclarationKind::Module:
        return CompactSymKind::Module;
    case hir::HirDeclarationKind::Component:
        return CompactSymKind::Component;
    case hir::HirDeclarationKind::Union:
        return CompactSymKind::Union;
    case hir::HirDeclarationKind::Asset:
        return CompactSymKind::Asset;
    case hir::HirDeclarationKind::Word:
        return CompactSymKind::Word;
    case hir::HirDeclarationKind::Context:
        return CompactSymKind::Context;
    }
    return CompactSymKind::Variable;
}

CompactVisibility mapVisibility(hir::HirVisibility visibility) {
    switch (visibility) {
    case hir::HirVisibility::Private:
        return CompactVisibility::Private;
    case hir::HirVisibility::Public:
        return CompactVisibility::Public;
    case hir::HirVisibility::Module:
        return CompactVisibility::Module;
    }
    return CompactVisibility::Private;
}

} // namespace

ArtifactBuilder::ArtifactBuilder(const types::TypeIntern &types, const hir::HirModule &hir,
                                 const memory::StringInterner &interner,
                                 ContentFingerprint source_fingerprint,
                                 FrontendIdentity frontend_identity,
                                 SemanticCompatibilityProfile semantic_profile)
    : types_(types), hir_(hir), interner_(interner),
      source_fingerprint_(source_fingerprint), frontend_identity_(std::move(frontend_identity)),
      semantic_profile_(std::move(semantic_profile)) {}

uint32_t ArtifactBuilder::internString(std::string_view s) {
    if (s.empty())
        return 0;
    if (const auto *existing = string_index_.get(s))
        return *existing;
    const uint32_t id = static_cast<uint32_t>(strings_.size());
    strings_.emplace_back(s);
    string_index_[s] = id;
    return id;
}

CompactType ArtifactBuilder::convertType(types::TypeId id) {
    CompactType out;
    if (id >= types_.count())
        return out;
    const auto &data = types_.lookup(id);
    out.kind         = std::visit(
        common::overloaded{
            [&](const types::TypeError &) { return CompactTypeKind::Error; },
            [&](const types::TypeNever &) { return CompactTypeKind::Never; },
            [&](const types::TypeVoid &) { return CompactTypeKind::Void; },
            [&](const types::TypeBool &) { return CompactTypeKind::Bool; },
            [&](const types::TypeChar &) { return CompactTypeKind::Char; },
            [&](const types::TypeInt &t) {
                out.int_width = static_cast<uint8_t>(t.width);
                return CompactTypeKind::Int;
            },
            [&](const types::TypeFloat &t) {
                out.int_width = static_cast<uint8_t>(t.width);
                return CompactTypeKind::Float;
            },
            [&](const types::TypePtr &t) {
                out.ref0  = internType(t.pointee);
                out.flags = t.is_mut ? 1 : 0;
                return CompactTypeKind::Ptr;
            },
            [&](const types::TypeArray &t) {
                out.ref0 = internType(t.elem);
                out.ref1 = t.count;
                return CompactTypeKind::Array;
            },
            [&](const types::TypeStruct &t) {
                out.ref0 = t.def_id;
                return CompactTypeKind::Struct;
            },
            [&](const types::TypeFn &t) {
                out.ref0 = internType(t.ret);
                for (size_t i = 0; i < t.param_count; ++i)
                    out.args.push_back(internType(t.params[i]));
                return CompactTypeKind::Fn;
            },
            [&](const types::TypeTypeVar &t) {
                out.ref0 = t.id;
                return CompactTypeKind::TypeVar;
            },
            [&](const types::TypeOptional &t) {
                out.ref0 = internType(t.inner);
                return CompactTypeKind::Optional;
            },
            [&](const types::TypeFailable &t) {
                out.ref0 = internType(t.inner);
                return CompactTypeKind::Failable;
            },
            [&](const types::TypeAlias &t) {
                out = convertType(t.target);
                return out.kind;
            },
            [&](const types::TypeNominal &t) {
                out = convertType(t.target);
                return out.kind;
            },
            [&](const types::TypeTrait &t) {
                out.ref0 = internString(interner_.lookup(t.name));
                return CompactTypeKind::Trait;
            },
            [&](const types::TypeOpaque &) { return CompactTypeKind::Opaque; },
            [&](const types::TypeOpaqueTagged &) { return CompactTypeKind::OpaqueTagged; },
            [&](const types::TypeUnknown &) { return CompactTypeKind::Error; },
            [&](const types::TypeQualified &t) {
                out = convertType(t.inner);
                return out.kind;
            },
            [&](const types::TypeSlice &t) {
                out.ref0 = internType(t.elem);
                return CompactTypeKind::Slice;
            },
            [&](const types::TypeEnum &t) {
                out.ref0 = t.def_id;
                return CompactTypeKind::Enum;
            },
            [&](const types::TypeUnion &t) {
                out.ref0 = t.def_id;
                return CompactTypeKind::Union;
            },
            [&](const types::TypePack &t) {
                for (size_t i = 0; i < t.count; ++i)
                    out.args.push_back(internType(t.members[i]));
                for (size_t i = 0; i < t.count; ++i)
                    out.arg_names.push_back(internString(interner_.lookup(t.names[i])));
                return CompactTypeKind::Pack;
            },
            [&](const types::TypeDyn &t) {
                out.ref0 = internType(t.target);
                out.ref1 = static_cast<uint32_t>(t.method_count);
                return CompactTypeKind::Dyn;
            },
            [&](const types::TypeSum &t) {
                for (size_t i = 0; i < t.count; ++i)
                    out.args.push_back(internType(t.members[i]));
                return CompactTypeKind::Union;
            },
            [&](const types::TypeGenericParam &t) {
                out.ref0 = internString(types_.genericParamOwnerName(t.owner_name));
                out.ref1 = t.param_index;
                return CompactTypeKind::GenericParam;
            },
            [&](const types::TypeIncomplete &t) {
                out.ref0 = internType(t.base);
                for (size_t i = 0; i < t.arg_count; ++i)
                    out.args.push_back(internType(t.args[i]));
                return CompactTypeKind::Incomplete;
            },
        },
        data);
    return out;
}

uint32_t ArtifactBuilder::internType(types::TypeId id) {
    if (id == types::kInvalidType)
        return 0;
    if (const auto *existing = type_index_.get(id))
        return *existing;

    // Intern dependencies first. convertType() emits plain CompactTypes whose
    // refs must point at already-written compact ids; deferring them until the
    // parent is appended lets a child claim the same compact id and corrupts
    // the table (a `?*C` is written as `?C` when `*C` itself is being interned).
    const auto &data = types_.lookup(id);
    std::visit(common::overloaded{
                   [&](const types::TypePtr &t) { (void)internType(t.pointee); },
                   [&](const types::TypeArray &t) { (void)internType(t.elem); },
                   [&](const types::TypeFn &t) {
                       (void)internType(t.ret);
                       for (size_t i = 0; i < t.param_count; ++i)
                           (void)internType(t.params[i]);
                   },
                   [&](const types::TypeOptional &t) { (void)internType(t.inner); },
                   [&](const types::TypeFailable &t) { (void)internType(t.inner); },
                   [&](const types::TypeAlias &t) { (void)internType(t.target); },
                   [&](const types::TypeNominal &t) { (void)internType(t.target); },
                   [&](const types::TypeQualified &t) { (void)internType(t.inner); },
                   [&](const types::TypeSlice &t) { (void)internType(t.elem); },
                   [&](const types::TypePack &t) {
                       for (size_t i = 0; i < t.count; ++i)
                           (void)internType(t.members[i]);
                   },
                   [&](const types::TypeDyn &t) { (void)internType(t.target); },
                   [&](const types::TypeSum &t) {
                       for (size_t i = 0; i < t.count; ++i)
                           (void)internType(t.members[i]);
                   },
                   [&](const types::TypeIncomplete &t) {
                       (void)internType(t.base);
                       for (size_t i = 0; i < t.arg_count; ++i)
                           (void)internType(t.args[i]);
                   },
                   [](const auto &) {},
               },
               data);

    const uint32_t compact_id = static_cast<uint32_t>(compact_types_.size());
    compact_types_.push_back(convertType(id));
    type_index_[id] = compact_id;
    return compact_id;
}

uint32_t ArtifactBuilder::internFunctionSignature(const hir::HirFunction &function) {
    CompactType signature;
    signature.kind = CompactTypeKind::Fn;
    signature.ref0 = internType(function.return_type);
    if (function.isVariadic)
        signature.flags |= kFunctionSignatureVariadic;
    if (function.isState)
        signature.flags |= kFunctionSignatureState;
    if (function.isForeignC)
        signature.flags |= kFunctionSignatureForeignC;
    if (function.usesTailCC)
        signature.flags |= kFunctionSignatureTailCC;
    for (const auto parameter_type : function.params)
        signature.args.push_back(internType(parameter_type));

    const auto signature_id = static_cast<uint32_t>(compact_types_.size());
    compact_types_.push_back(std::move(signature));
    return signature_id;
}

CompactExpr ArtifactBuilder::convertExpr(hir::HirExprId id) {
    CompactExpr out;
    const auto &expr = hir_.getExpr(id);
    hir::visitExpr(expr,
                   common::overloaded{
                       [&](const hir::HirLiteral &lit) {
                           out.kind    = CompactExprKind::Literal;
                           out.type_id = internType(lit.type);
                           // HirLiteral stores the value in a union; the active member is
                           // recovered from the type below.
                           switch (types_.kindOf(lit.type)) {
                           case types::TypeKind::Bool:
                               out.flags   = 2; // bool
                               out.int_val = lit.b ? 1 : 0;
                               break;
                           case types::TypeKind::Float:
                               out.flags   = 1; // float
                               out.flt_val = lit.f;
                               break;
                           case types::TypeKind::Ptr:
                               out.flags   = 3; // string/ptr literal
                               out.name_id = internString(interner_.lookup(lit.str_val));
                               break;
                           default:
                               out.flags   = 0; // int/char
                               out.int_val = lit.i;
                               break;
                           }
                       },
                       [&](const hir::HirBinary &bin) {
                           out.kind    = CompactExprKind::Binary;
                           out.type_id = internType(bin.type);
                           out.ref_a   = bin.lhs;
                           out.ref_b   = bin.rhs;
                           out.op      = static_cast<uint8_t>(bin.op);
                           out.ref_e   = internType(bin.operand_type);
                       },
                       [&](const hir::HirUnary &un) {
                           out.kind    = CompactExprKind::Unary;
                           out.type_id = internType(un.type);
                           out.ref_a   = un.operand;
                           out.op      = static_cast<uint8_t>(un.op);
                       },
                       [&](const hir::HirLet &let) {
                           out.kind    = CompactExprKind::Let;
                           out.name_id = internString(interner_.lookup(let.name));
                           out.type_id = internType(let.type);
                           out.ref_a   = let.init;
                       },
                       [&](const hir::HirVar &var) {
                           out.kind    = CompactExprKind::Var;
                           out.name_id = internString(interner_.lookup(var.name));
                           out.ref_c   = var.version;
                       },
                       [&](const hir::HirCall &call) {
                           out.kind  = CompactExprKind::Call;
                           out.ref_a = call.callee;
                           if (call.resolved_fn < hir_.getFnCount()) {
                               out.resolved_function_name_id = internString(interner_.lookup(
                                   hir_.getFn(call.resolved_fn).name));
                               if (call.resolved_fn < function_signature_type_ids_.size())
                                   out.resolved_function_signature_type_id =
                                       function_signature_type_ids_[call.resolved_fn];
                           }
                           out.ref_e = internType(call.fn_type);
                           out.flags = (call.musttail ? 1U : 0U) | (call.usesTailCC ? 2U : 0U);
                           out.variadic_slice_param = call.variadicSliceParam;
                           out.is_variadic_slice = call.isVariadicSlice;
                           out.auto_collect_tail = call.autoCollectTail;
                           for (auto arg : call.args)
                               out.args.push_back(arg);
                           for (auto arg_type : call.argument_types)
                               out.arg_types.push_back(internType(arg_type));
                       },
                       [&](const hir::HirRet &ret) {
                           out.kind  = CompactExprKind::Ret;
                           out.ref_a = ret.value;
                       },
                       [&](const hir::HirBranch &branch) {
                           out.kind  = CompactExprKind::Branch;
                           out.ref_a = branch.cond;
                           out.ref_c = branch.then_block;
                           out.ref_d = branch.else_block;
                       },
                       [&](const hir::HirJump &jump) {
                           out.kind  = CompactExprKind::Jump;
                           out.ref_c = jump.target;
                       },
                       [&](const hir::HirPhi &phi) {
                           out.kind = CompactExprKind::Phi;
                           for (auto in : phi.incoming)
                               out.args.push_back(in);
                       },
                       [&](const hir::HirAssign &assign) {
                           out.kind  = CompactExprKind::Assign;
                           out.ref_a = assign.target;
                           out.ref_b = assign.value;
                       },
                       [&](const hir::HirIndex &idx) {
                           out.kind    = CompactExprKind::Index;
                           out.type_id = internType(idx.type);
                           out.ref_a   = idx.object;
                           out.ref_b   = idx.index;
                           out.ref_e   = internType(idx.obj_type);
                           out.flags   = idx.is_array ? 1 : 0;
                           out.safety  = static_cast<CompactSafetyMode>(idx.safety);
                       },
                       [&](const hir::HirField &field) {
                           out.kind    = CompactExprKind::Field;
                           out.type_id = internType(field.type);
                           out.ref_a   = field.object;
                           out.ref_c   = field.index;
                           out.ref_e   = internType(field.object_type);
                           out.safety  = static_cast<CompactSafetyMode>(field.safety);
                       },
                       [&](const hir::HirStructLiteral &lit) {
                           out.kind    = CompactExprKind::StructLiteral;
                           out.type_id = internType(lit.type);
                           for (auto v : lit.values)
                               out.args.push_back(v);
                       },
                       [&](const hir::HirArrayLiteral &lit) {
                           out.kind    = CompactExprKind::ArrayLiteral;
                           out.type_id = internType(lit.type);
                           for (auto e : lit.elements)
                               out.args.push_back(e);
                       },
                       [&](const hir::HirEnumValue &ev) {
                           out.kind    = CompactExprKind::EnumValue;
                           out.type_id = internType(ev.type);
                           out.int_val = ev.value;
                       },
                       [&](const hir::HirSlotAlloca &sa) {
                           out.kind    = CompactExprKind::SlotAlloca;
                           out.ref_a   = sa.slot;
                           out.type_id = internType(sa.type);
                       },
                       [&](const hir::HirSlotStore &ss) {
                           out.kind  = CompactExprKind::SlotStore;
                           out.ref_a = ss.slot;
                           out.ref_b = ss.value;
                       },
                       [&](const hir::HirSlotLoad &sl) {
                           out.kind    = CompactExprKind::SlotLoad;
                           out.ref_a   = sl.slot;
                           out.type_id = internType(sl.type);
                       },
                       [&](const hir::HirSlotAddr &sa) {
                           out.kind    = CompactExprKind::SlotAddr;
                           out.ref_a   = sa.slot;
                           out.type_id = internType(sa.type);
                       },
                       [&](const hir::HirMakeNone &mn) {
                           out.kind    = CompactExprKind::MakeNone;
                           out.type_id = internType(mn.type);
                       },
                       [&](const hir::HirMakeSome &ms) {
                           out.kind    = CompactExprKind::MakeSome;
                           out.type_id = internType(ms.type);
                           out.ref_a   = ms.value;
                       },
                       [&](const hir::HirMakeSlice &slice) {
                           out.kind    = CompactExprKind::MakeSlice;
                           out.type_id = internType(slice.type);
                           out.ref_a   = slice.object;
                           out.ref_b   = slice.lo;
                           out.ref_c   = slice.hi;
                           out.ref_e   = internType(slice.object_type);
                           out.ref_f   = internType(slice.bound_type);
                           out.flags   = (slice.is_array ? 1U : 0U) |
                                       (slice.is_pointer ? 4U : 0U);
                           out.safety  = static_cast<CompactSafetyMode>(slice.safety);
                       },
                       [&](const hir::HirUnionCheck &check) {
                           out.kind    = CompactExprKind::UnionCheck;
                           out.type_id = internType(types::kBoolType);
                           out.ref_a   = check.value;
                           out.ref_b   = internType(check.union_type);
                           out.ref_c   = check.member_index;
                       },
                       [&](const hir::HirUnionCast &uc) {
                           out.kind  = CompactExprKind::Cast;
                           out.ref_a = uc.value;
                           out.ref_e = internType(uc.from);
                           out.ref_b = internType(uc.to);
                           out.ref_c = uc.member_index;
                           out.safety = static_cast<CompactSafetyMode>(uc.safety);
                       },
                       [&](const hir::HirCast &cast) {
                           out.kind  = CompactExprKind::Cast;
                           out.ref_a = cast.value;
                           out.ref_e = internType(cast.from);
                           out.ref_b = internType(cast.to);
                           out.ref_c = ~0U;
                           out.flags = 0U;
                       },
                       [&](const hir::HirLayoutIntrinsic &li) {
                           out.kind    = CompactExprKind::LayoutIntrinsic;
                           out.type_id = internType(li.type);
                           out.ref_e   = static_cast<uint32_t>(li.which);
                           out.ref_f   = li.field_index;
                           out.ref_a   = li.operand;
                           out.ref_b   = internType(li.operand_type);
                           out.int_val = static_cast<int64_t>(li.string_length);
                       },
                       [&](const hir::HirStateTailCall &tail) {
                           out.kind  = CompactExprKind::StateTailCall;
                           out.ref_a = tail.call.callee;
                           if (tail.call.resolved_fn < hir_.getFnCount()) {
                               out.resolved_function_name_id = internString(interner_.lookup(
                                   hir_.getFn(tail.call.resolved_fn).name));
                               if (tail.call.resolved_fn < function_signature_type_ids_.size())
                                   out.resolved_function_signature_type_id =
                                       function_signature_type_ids_[tail.call.resolved_fn];
                           }
                           out.ref_e = internType(tail.call.fn_type);
                           out.flags = tail.call.usesTailCC ? 2U : 0U;
                           out.variadic_slice_param = tail.call.variadicSliceParam;
                           out.is_variadic_slice = tail.call.isVariadicSlice;
                           out.auto_collect_tail = tail.call.autoCollectTail;
                           for (auto arg : tail.call.args)
                               out.args.push_back(arg);
                           for (auto arg_type : tail.call.argument_types)
                               out.arg_types.push_back(internType(arg_type));
                       },
                       [&](const hir::HirCleanup &cleanup) {
                           out.kind = CompactExprKind::Cleanup;
                           for (auto expr_id : cleanup.exprs)
                               out.args.push_back(expr_id);
                       },
                       [&](const hir::HirGlobalConstLoad &load) {
                           out.kind    = CompactExprKind::GlobalConstLoad;
                           out.name_id = internString(interner_.lookup(load.name));
                           out.type_id = internType(load.type);
                       },
                       [&](const hir::HirExternalGlobalAddr &address) {
                           out.kind    = CompactExprKind::ExternalGlobalAddr;
                           out.name_id = internString(interner_.lookup(address.name));
                           out.type_id = internType(address.type);
                       },
                       [&](const hir::HirMakeDyn &m) {
                           out.kind    = CompactExprKind::MakeDyn;
                           out.ref_a   = m.value;
                           out.ref_b   = internType(m.source_type);
                           out.type_id = internType(m.dyn_type);
                           out.name_id = internString(interner_.lookup(m.vtable_name));
                           out.flags   = m.value_is_place ? 1U : 0U;
                       },
                       [&](const hir::HirDynCall &call) {
                           out.kind    = CompactExprKind::DynCall;
                           out.ref_a   = call.receiver;
                           out.name_id = internString(interner_.lookup(call.vtable_name));
                           out.ref_c   = call.slot_index;
                           out.type_id = internType(call.result_type);
                           out.ref_e   = internType(call.fn_type);
                           for (auto arg : call.args)
                               out.args.push_back(arg);
                           for (auto arg_type : call.arg_types)
                               out.arg_types.push_back(internType(arg_type));
                           out.flags = call.has_receiver ? 1U : 0U;
                       },
                       [&](const hir::HirMakeOpaque &m) {
                           out.kind    = CompactExprKind::MakeOpaque;
                           out.ref_a   = m.value;
                           out.ref_b   = internType(m.source_type);
                           out.type_id = internType(m.opaque_type);
                           out.ref_c   = m.type_id;
                           out.ints    = {m.canonical_id.hi, m.canonical_id.lo};
                       },
                       [&](const hir::HirOpaqueCast &cast) {
                           out.kind    = CompactExprKind::OpaqueCast;
                           out.ref_a   = cast.value;
                           out.ref_b   = internType(cast.from);
                           out.ref_c   = internType(cast.to);
                           out.ref_d   = internType(cast.opaque_type);
                           out.type_id = internType(cast.result_type);
                           out.ref_e   = cast.type_id;
                           out.flags   = cast.returns_ptr ? 2U : 0U;
                           out.safety  = static_cast<CompactSafetyMode>(cast.safety);
                           out.ints    = {cast.canonical_id.hi, cast.canonical_id.lo};
                       },
                       [&](const hir::HirOpaqueCheck &check) {
                           out.kind    = CompactExprKind::OpaqueCheck;
                           out.ref_a   = check.value;
                           out.type_id = internType(check.opaque_type);
                           out.ref_e   = check.type_id;
                           out.ints    = {check.canonical_id.hi, check.canonical_id.lo};
                       },
                       [&](const hir::HirRuntimePanic &panic) {
                           out.kind    = CompactExprKind::RuntimePanic;
                           out.int_val = static_cast<int64_t>(panic.code);
                       },
                       [&](const hir::HirCanonicalType &canonical) {
                           out.kind    = CompactExprKind::CanonicalType;
                           out.type_id = internType(canonical.type);
                           out.ints    = {canonical.canonical_id.hi, canonical.canonical_id.lo};
                       },
                       [&](const hir::HirPipe &pipe) {
                           out.kind     = pipe.is_effect ? CompactExprKind::PipeDo
                                                         : CompactExprKind::Pipe;
                           out.ref_a    = pipe.stage;
                           out.ref_b    = pipe.source_slot;
                           out.type_id  = internType(pipe.type);
                           out.ref_e    = internType(pipe.source_type);
                       },
                       [&](const hir::HirPipeCurrent &current) {
                           out.kind    = CompactExprKind::PipeCurrent;
                           out.ref_a   = current.slot;
                           out.type_id = internType(current.type);
                       },
                   });
    return out;
}

uint64_t ArtifactBuilder::computePublicAbiHash() const {
    std::ostringstream primary;
    std::ostringstream secondary;
    for (size_t id = 0; id < hir_.declarationCount(); ++id) {
        const auto &decl = hir_.getDeclaration(id);
        if (decl.visibility != hir::HirVisibility::Public &&
            decl.visibility != hir::HirVisibility::Module)
            continue;
        const auto name = interner_.lookup(decl.name);
        const auto kind = static_cast<int>(mapDeclKind(decl.kind));
        const auto visibility = static_cast<int>(mapVisibility(decl.visibility));
        primary << name << '\x1f' << kind << '\x1f' << visibility << '\x1f'
                << decl.module_depth << '\x1f' << (decl.discardable ? 1 : 0) << '\n';
        secondary << decl.module_depth << '\x1f' << visibility << '\x1f' << kind << '\x1f'
                  << name << '\x1f' << (decl.discardable ? 1 : 0) << '\n';
    }
    const uint32_t hi = zith::zirl::fnv1a32(primary.str());
    const uint32_t lo = zith::zirl::fnv1a32(secondary.str());
    return (static_cast<uint64_t>(hi) << 32u) | lo;
}

Artifact ArtifactBuilder::build(std::string_view canonical_path, std::string_view module_name,
                                const std::vector<DependencyRecord> &deps) {
    Artifact art;
    art.canonical_path = std::string(canonical_path);
    art.module_identity.path = art.canonical_path;
    art.module_name    = std::string(module_name);
    art.frontend_identity = frontend_identity_;
    art.semantic_profile = semantic_profile_;
    art.hir_schema_version = kHirSchemaVersion;
    art.source_fingerprint = source_fingerprint_;
    for (const auto &mapping : types_.canonicalTagSnapshot()) {
        art.canonical_mappings.push_back(
            CompactCanonicalMapping{mapping.id.hi, mapping.id.lo, mapping.tag});
    }
    const uint64_t abi = computePublicAbiHash();
    art.public_abi_hi  = static_cast<uint32_t>(abi >> 32u);
    art.public_abi_lo  = static_cast<uint32_t>(abi & 0xFFFFFFFFu);

    art.deps = deps;

    // Persist all declarations so hydration rebuilds the complete HIR module;
    // public ABI consumers still filter by visibility.
    std::vector<uint32_t> hir_decl_to_artifact(hir_.declarationCount(), ~uint32_t{0});
    std::vector<hir::HirDeclarationId> artifact_to_hir_decl;
    for (hir::HirDeclarationId id = 0; id < hir_.declarationCount(); ++id) {
        const auto &hir_decl = hir_.getDeclaration(id);

        DeclRecord decl;
        const auto name = interner_.lookup(hir_decl.name);
        decl.name = std::string(name);
        decl.name_id = internString(name);
        decl.kind = mapDeclKind(hir_decl.kind);
        decl.visibility = mapVisibility(hir_decl.visibility);
        decl.mod_depth = hir_decl.module_depth;
        decl.is_extern = hir_decl.is_extern;
        decl.discardable = hir_decl.discardable;
        decl.template_index = hir_decl.template_id;
        if (hir_decl.body_function != hir::kInvalidHirFunction)
            decl.body_fn_index = hir_decl.body_function;
        hir_decl_to_artifact[id] = static_cast<uint32_t>(art.decls.size());
        artifact_to_hir_decl.push_back(id);
        art.decls.push_back(std::move(decl));
        auto &out_decl = art.decls.back();

        const auto type_id = hir_decl.type != types::kInvalidType
                                 ? hir_decl.type
                                 : types_.lookupNamedType(name);
        if (type_id != types::kErrorType && type_id != types::kInvalidType)
            out_decl.type_id = internType(type_id);

        if (hir_decl.kind == hir::HirDeclarationKind::Struct ||
            hir_decl.kind == hir::HirDeclarationKind::Union ||
            hir_decl.kind == hir::HirDeclarationKind::Component) {
            uint32_t def_id = ~uint32_t{0};
            if (type_id != types::kErrorType && type_id != types::kInvalidType) {
                if (const auto *td = std::get_if<types::TypeStruct>(&types_.lookup(type_id)))
                    def_id = td->def_id;
                else if (const auto *ud = std::get_if<types::TypeUnion>(&types_.lookup(type_id)))
                    def_id = ud->def_id;
            }
            if (def_id != ~uint32_t{0}) {
                if (const auto *sd = types_.lookupStructDef(def_id)) {
                    for (const auto &field : sd->fields) {
                        out_decl.field_name_ids.push_back(
                            internString(interner_.lookup(field.name)));
                        out_decl.field_type_ids.push_back(internType(field.type));
                    }
                } else if (const auto *ud = types_.lookupUnionDef(def_id)) {
                    for (const auto member : ud->members)
                        out_decl.field_type_ids.push_back(internType(member));
                }
            }
        }

        if (hir_decl.kind == hir::HirDeclarationKind::Enum) {
            const auto *ed = std::get_if<types::TypeEnum>(&types_.lookup(type_id));
            if (ed != nullptr) {
                const auto *def = types_.lookupEnumDef(ed->def_id);
                if (def != nullptr)
                    for (const auto &variant : def->variants)
                        out_decl.field_name_ids.push_back(
                            internString(interner_.lookup(variant.name)));
            }
        }
    }

    // Template and function relations are persisted as explicit module-local
    // indices; neither is reconstructed from declaration names.
    for (hir::HirTemplateId id = 0; id < hir_.templateCount(); ++id) {
        const auto &hir_template = hir_.getTemplate(id);
        TemplateBlueprint blueprint;
        blueprint.kind = mapDeclKind(hir_template.kind);
        blueprint.is_extern = hir_template.is_extern;
        blueprint.is_variadic = hir_template.variadic;
        blueprint.is_state = hir_template.is_state;
        blueprint.is_foreign_c = hir_template.is_foreign_c;
        blueprint.uses_tailcc = hir_template.uses_tail_cc;
        blueprint.name = std::string(interner_.lookup(hir_template.name));
        blueprint.name_id = internString(blueprint.name);
        blueprint.declaration_index =
            hir_template.declaration_id < hir_decl_to_artifact.size()
                ? hir_decl_to_artifact[hir_template.declaration_id]
                : ~uint32_t{0};
        if (hir_template.function_id != hir::kInvalidHirFunction)
            blueprint.function_index = hir_template.function_id;
        if (hir_template.return_type != types::kInvalidType)
            blueprint.return_type_id = internType(hir_template.return_type);
        for (const auto &param : hir_template.params) {
            GenericParamRecord out_param;
            out_param.name_id = internString(interner_.lookup(param.name));
            out_param.name = std::string(interner_.lookup(param.name));
            for (const auto bound : param.bounds)
                out_param.bound_type_ids.push_back(internType(bound));
            blueprint.params.push_back(std::move(out_param));
        }
        for (size_t index = 0; index < hir_template.parameter_types.size(); ++index)
            blueprint.param_type_ids.push_back(internType(hir_template.parameter_types[index]));
        for (const auto name : hir_template.parameter_names)
            blueprint.param_name_ids.push_back(internString(interner_.lookup(name)));
        for (const auto name : hir_template.field_names)
            blueprint.field_name_ids.push_back(internString(interner_.lookup(name)));
        for (const auto type : hir_template.field_types)
            blueprint.field_type_ids.push_back(internType(type));
        for (const auto index : hir_template.canonical_field_order)
            blueprint.canonical_field_order.push_back(index);
        for (const auto method : hir_template.method_declarations) {
            if (method < hir_decl_to_artifact.size() &&
                hir_decl_to_artifact[method] != ~uint32_t{0})
                blueprint.method_decl_indices.push_back(hir_decl_to_artifact[method]);
        }
        for (const auto trait : hir_template.trait_types)
            blueprint.trait_type_ids.push_back(internType(trait));
        art.templates.push_back(std::move(blueprint));
    }

    // Resolve method references to artifact declaration indices after the full
    // declaration table has been collected, so cache consumers never have to
    // guess at session-local SymbolTable ids.
    for (size_t di = 0; di < art.decls.size(); ++di) {
        const auto &decl = art.decls[di];
        if (decl.kind != CompactSymKind::Struct && decl.kind != CompactSymKind::Union &&
            decl.kind != CompactSymKind::Component)
            continue;
        auto &out_decl = art.decls[di];
        const auto &hir_decl = hir_.getDeclaration(artifact_to_hir_decl[di]);
        for (const auto member_id : hir_decl.method_declarations) {
            if (member_id < hir_decl_to_artifact.size() &&
                hir_decl_to_artifact[member_id] != ~uint32_t{0})
                out_decl.method_decl_indices.push_back(hir_decl_to_artifact[member_id]);
        }
    }

    // Extract concrete HIR functions into the code section.
    function_signature_type_ids_.clear();
    function_signature_type_ids_.reserve(hir_.getFnCount());
    for (size_t fi = 0; fi < hir_.getFnCount(); ++fi) {
        const auto &fn = hir_.getFn(fi);
        CompactFunction cfn;
        cfn.name                   = std::string(interner_.lookup(fn.name));
        cfn.name_id                = internString(interner_.lookup(fn.name));
        cfn.is_extern              = !fn.hasBody;
        cfn.is_foreign_c           = fn.isForeignC;
        cfn.is_variadic            = fn.isVariadic;
        cfn.is_state               = fn.isState;
        cfn.uses_tailcc            = fn.usesTailCC;
        if (const auto *attrs = hir_.attrs().tryFn(fi))
            cfn.discardable = attrs->discardable;
        cfn.variadic_slice_param   = fn.variadicSliceParam == ~static_cast<size_t>(0)
                                         ? ~uint32_t{0}
                                         : static_cast<uint32_t>(fn.variadicSliceParam);
        if (fn.foreignAdapterName != memory::kInvalidInternedId)
            cfn.foreign_adapter_name_id =
                internString(interner_.lookup(fn.foreignAdapterName));
        cfn.machine_id             = fn.machineId;
        cfn.return_type_id         = internType(fn.return_type);
        cfn.machine_return_type_id = fn.machineReturnType != types::kInvalidType
                                         ? internType(fn.machineReturnType)
                                         : cfn.return_type_id;
        const auto angle           = cfn.name.find('<');
        if (angle != std::string::npos) {
            InstantiationRecord rec;
            rec.canonical_module = art.canonical_path;
            rec.mangled          = cfn.name;
            rec.template_name    = cfn.name.substr(0, angle);
            rec.arg_types      = genericArgsFromName(cfn.name);
            cfn.instance_index = static_cast<uint32_t>(art.instantiations.size());
            art.instantiations.push_back(std::move(rec));
        }
        for (size_t pi = 0; pi < fn.params.size(); ++pi) {
            cfn.param_type_ids.push_back(internType(fn.params[pi]));
            cfn.param_name_ids.push_back(internString(interner_.lookup(fn.param_names[pi])));
            cfn.param_slot_ids.push_back(pi < fn.param_slots.size()
                                             ? static_cast<uint32_t>(fn.param_slots[pi])
                                             : hir::kInvalidHirSlot);
        }
        cfn.signature_type_id = internFunctionSignature(fn);
        function_signature_type_ids_.push_back(cfn.signature_type_id);
        for (size_t bi = 0; bi < fn.blocks.size(); ++bi) {
            const auto &blk = fn.blocks[bi];
            CompactBasicBlock cblk;
            for (auto eid : blk.insts)
                cblk.insts.push_back(eid);
            cblk.terminator = blk.terminator;
            cfn.blocks.push_back(std::move(cblk));
        }
        art.functions.push_back(std::move(cfn));
    }

    // HIR expression ids are module-global and shared by all functions and const
    // globals. Serialize them once at module level so hydration can restore the
    // pool even for modules that only contain constants.
    art.exprs.reserve(hir_.exprCount());
    for (hir::HirExprId eid = 0; eid < hir_.exprCount(); ++eid)
        art.exprs.push_back(convertExpr(eid));

    for (size_t gi = 0; gi < hir_.getGlobalConstCount(); ++gi) {
        const auto &global = hir_.getGlobalConst(gi);
        CompactGlobalConst cg;
        cg.name_id   = internString(interner_.lookup(global.name));
        cg.type_id   = internType(global.type);
        cg.init_expr = global.init;
        art.globals.push_back(std::move(cg));
    }
    for (size_t vi = 0; vi < hir_.getVTableCount(); ++vi) {
        const auto &vtable = hir_.getVTable(vi);
        CompactVTable cv;
        cv.name_id = internString(interner_.lookup(vtable.name));
        for (const auto function_id : vtable.slots) {
            if (function_id < hir_.getFnCount()) {
                cv.slot_function_name_ids.push_back(
                    internString(interner_.lookup(hir_.getFn(function_id).name)));
                cv.slot_function_signature_type_ids.push_back(
                    function_signature_type_ids_[function_id]);
            } else {
                cv.slot_function_name_ids.push_back(~uint32_t{0});
                cv.slot_function_signature_type_ids.push_back(~uint32_t{0});
            }
        }
        art.vtables.push_back(std::move(cv));
    }

    for (size_t slot = 0; slot < hir_.attrs().slotCount(); ++slot) {
        const auto attrs = hir_.attrs().trySlot(static_cast<hir::HirSlotId>(slot));
        if (attrs == nullptr || !attrs->hasResidualFacts())
            continue;
        HirSlotAttrsRecord rec;
        rec.slot      = static_cast<uint32_t>(slot);
        rec.ownership = static_cast<uint8_t>(attrs->ownership);
        rec.consumed  = static_cast<uint8_t>(attrs->consumed);
        rec.nonNull   = attrs->nonNull;
        rec.volatileSlot = attrs->volatileSlot;
        art.attrs_slots.push_back(std::move(rec));
    }
    for (hir::HirExprId call_id = 0; call_id < hir_.exprCount(); ++call_id) {
        const auto *attrs = hir_.attrs().tryCall(call_id);
        if (attrs == nullptr || !attrs->hasResidualFacts())
            continue;
        HirCallAttrsRecord rec;
        rec.expr_id     = call_id;
        rec.returns_arg = attrs->returnsArg;
        for (const auto &arg : attrs->args)
            rec.arg_escapes.push_back(static_cast<uint32_t>(arg.escape));
        art.attrs_calls.push_back(std::move(rec));
    }
    for (size_t fn_index = 0; fn_index < hir_.attrs().fnCount(); ++fn_index) {
        const auto *attrs = hir_.attrs().tryFn(fn_index);
        if (attrs == nullptr || !attrs->hasResidualFacts())
            continue;
        HirFnAttrsRecord rec;
        rec.fn_index        = static_cast<uint32_t>(fn_index);
        rec.return_consumed = static_cast<uint8_t>(attrs->returnConsumed);
        rec.nonNull         = attrs->nonNull;
        rec.noAlias         = attrs->noAlias;
        rec.readOnly        = attrs->readOnly;
        rec.noCapture       = attrs->noCapture;
        rec.discardable     = attrs->discardable;
        art.attrs_fns.push_back(std::move(rec));
    }
    for (hir::HirExprId expr_id = 0; expr_id < hir_.exprCount(); ++expr_id) {
        const auto *proof = hir_.attrs().trySafetyProof(expr_id);
        if (proof == nullptr || !proof->present)
            continue;
        HirSafetyProofRecord rec;
        rec.expr_id   = expr_id;
        rec.kind      = static_cast<CompactSafetyProofKind>(proof->kind);
        rec.operand_a = proof->operand_a;
        rec.operand_b = proof->operand_b;
        rec.operand_c = proof->operand_c;
        art.attrs_safety_proofs.push_back(std::move(rec));
    }

    // Composite definitions are needed to hydrate private/internal named types
    // that appear in HIR but are absent from the exported DeclRecord surface.
    // Enum variants also need their non-index discriminants for exact codegen.
    for (size_t def_id = 0; def_id < types_.structDefCount(); ++def_id) {
        const auto *sd = types_.lookupStructDef(static_cast<uint32_t>(def_id));
        if (sd == nullptr)
            continue;
        CompactStructDef cdef;
        const auto name            = interner_.lookup(sd->name);
        cdef.name                  = std::string(name);
        cdef.name_id               = internString(name);
        cdef.hasForeignLayout      = sd->hasForeignLayout;
        cdef.foreignAbiIsSingleI64 = sd->foreignAbiIsSingleI64;
        cdef.foreignSizeBytes      = sd->foreignSizeBytes;
        cdef.foreignAlignBytes     = sd->foreignAlignBytes;
        for (const auto &field : sd->fields) {
            cdef.field_name_ids.push_back(internString(interner_.lookup(field.name)));
            cdef.field_type_ids.push_back(internType(field.type));
            cdef.foreign_field_offsets.push_back(field.foreignOffsetBytes);
        }
        for (const auto &arg : sd->args)
            cdef.type_arg_ids.push_back(internType(arg));
        art.struct_defs.push_back(std::move(cdef));
    }
    for (size_t def_id = 0; def_id < types_.enumDefCount(); ++def_id) {
        const auto *ed = types_.lookupEnumDef(static_cast<uint32_t>(def_id));
        if (ed == nullptr)
            continue;
        CompactEnumDef cdef;
        const auto name = interner_.lookup(ed->name);
        cdef.name       = std::string(name);
        cdef.name_id    = internString(name);
        cdef.underlying_id =
            ed->underlying != types::kErrorType ? internType(ed->underlying) : ~uint32_t{0};
        for (const auto &variant : ed->variants) {
            CompactEnumVariant cvariant;
            const auto vname      = interner_.lookup(variant.name);
            cvariant.name         = std::string(vname);
            cvariant.name_id      = internString(vname);
            cvariant.discriminant = variant.discriminant;
            cdef.variants.push_back(std::move(cvariant));
        }
        art.enum_defs.push_back(std::move(cdef));
    }
    for (size_t def_id = 0; def_id < types_.unionDefCount(); ++def_id) {
        const auto *ud = types_.lookupUnionDef(static_cast<uint32_t>(def_id));
        if (ud == nullptr)
            continue;
        CompactUnionDef cdef;
        const auto name = interner_.lookup(ud->name);
        cdef.name       = std::string(name);
        cdef.name_id    = internString(name);
        cdef.is_raw     = !ud->is_tagged;
        for (const auto member : ud->members)
            cdef.member_type_ids.push_back(internType(member));
        art.union_defs.push_back(std::move(cdef));
    }

    // Commit string and type tables.
    art.strings = std::move(strings_);
    art.types   = std::move(compact_types_);
    return art;
}

} // namespace zith::cache

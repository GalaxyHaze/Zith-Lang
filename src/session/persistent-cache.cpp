#include "compilation-session.hpp"

#include "cache/artifact-builder.hpp"
#include "cache/cache-paths.hpp"
#include "common/ast-ids.hpp"
#include "memory/flat-set.hpp"
#include "types/type-kind.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace zith::session {

bool CompilationSession::tryLoadPersistentCache() {
    if (mCacheStore == nullptr)
        return false;

    mCanonicalPath = SourceCatalog::canonicalPath(mFilePath);

    std::string source_text;
    if (!mContentOverride.empty()) {
        source_text = mContentOverride;
    } else {
        std::ifstream input(mFilePath, std::ios::binary);
        if (!input)
            return false;
        source_text.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }
    mSourceFingerprint = ContentFingerprint::fromText(source_text);

    mHydratedEntry = mCacheStore->loadEntry(mCanonicalPath, mSourceFingerprint);
    if (!mHydratedEntry) {
        if (mOpts.get().flags.verbose())
            writeOutput("  [cache] miss for %s\n", mCanonicalPath.c_str());
        return false;
    }

    hydrateFromArtifact(mHydratedEntry->artifact);
    mCacheHydrated = true;

    if (mOpts.get().flags.verbose())
        writeOutput("  [cache] hit for %s (%zu decls, %zu fns)\n", mCanonicalPath.c_str(),
                    mHydratedEntry->artifact.decls.size(),
                    mHydratedEntry->artifact.functions.size());

    return true;
}

void CompilationSession::writePersistentCache() {
    if (mOpts.get().noCache)
        return;
    if (mCacheStore == nullptr || mCacheHydrated)
        return;
    if (mDiags.hasErrors())
        return;

    namespace fs          = std::filesystem;
    const auto cache_root = (fs::path(mProjectRoot) / cache::kPersistentCacheDirName).string();
    if (!mCacheStore || mCacheStore->root() != cache_root) {
        mCacheStore = std::make_unique<cache::Store>(
            cache_root,
            mFrontendContext ? mFrontendContext->config().cacheKey() : session::CacheKey{});
    }

    cache::ArtifactBuilder builder(mSyms, mTypes, mHirModule, *mInterner, mSourceFingerprint,
                                   mFrontendContext ? mFrontendContext->config().cacheKey()
                                                    : session::CacheKey{});
    std::vector<cache::DependencyRecord> deps;
    memory::FlatSet<std::string> seen_deps;
    auto append_dep = [&](std::string canonical_path, std::string import_key) {
        if (canonical_path.empty() || !seen_deps.insert(canonical_path))
            return;
        cache::DependencyRecord dep;
        dep.canonical_path = std::move(canonical_path);
        dep.import_key     = std::move(import_key);
        if (const auto entry = mCacheStore->manifestEntry(dep.canonical_path)) {
            dep.public_abi_hi = entry->public_abi_hi;
            dep.public_abi_lo = entry->public_abi_lo;
        }
        deps.push_back(std::move(dep));
    };
    for (const auto &edge : mSnapshot->importGraph()) {
        if (edge.importer != mCanonicalPath || edge.targetKind != session::ImportTargetKind::Zith) {
            continue;
        }
        for (const auto &target : edge.targets)
            append_dep(SourceCatalog::canonicalPath(target), target);
    }

    std::string module_name = fs::path(mFilePath).stem().string();
    auto artifact           = builder.build(mCanonicalPath, module_name, deps);
    mCacheStore->store(artifact);

    if (mOpts.get().flags.verbose())
        writeOutput("  [cache] wrote artifact for %s\n", mCanonicalPath.c_str());
}

void CompilationSession::hydrateFromArtifact(const cache::Artifact &art) {
    // Recreate the exported surface first so downstream lookup (including
    // methods that point at Fn declarations) sees stable symbol ids.
    mTypes.setCurrentModule(art.canonical_path);
    for (const auto &mapping : art.canonical_mappings) {
        mTypes.setCanonicalTag(types::TypeCanonicalId{mapping.hi, mapping.lo}, mapping.runtime_id);
    }
    std::vector<symbols::SymId> decl_sym_ids;
    decl_sym_ids.reserve(art.decls.size());
    for (const auto &decl : art.decls) {
        decl_sym_ids.push_back(mSyms.declare(decl.name, decl.visibility, decl.mod_depth,
                                             static_cast<symbols::SymKind>(decl.kind),
                                             ast::kInvalidDecl, {}, {}, {}));
    }

    for (size_t di = 0; di < art.decls.size(); ++di) {
        const auto &decl = art.decls[di];
        if (decl.kind != cache::CompactSymKind::Struct &&
            decl.kind != cache::CompactSymKind::Union &&
            decl.kind != cache::CompactSymKind::Component)
            continue;
        auto &owner = mSyms.get(decl_sym_ids[di]);
        for (const auto method_index : decl.method_decl_indices) {
            if (method_index < decl_sym_ids.size())
                owner.members.push(decl_sym_ids[method_index]);
        }
    }

    // Compact types are emitted in dependency order: refs always have a lower
    // compact id, so a single forward pass can restore the type table.
    std::vector<types::TypeId> compact_type_ids(art.types.size(), types::kErrorType);
    std::vector<types::TypeId> struct_tids(art.struct_defs.size(), types::kErrorType);
    std::vector<types::TypeId> enum_tids(art.enum_defs.size(), types::kErrorType);
    std::vector<types::TypeId> union_tids(art.union_defs.size(), types::kErrorType);

    auto compactType = [&](uint32_t id) -> types::TypeId {
        return id < compact_type_ids.size() ? compact_type_ids[id] : types::kErrorType;
    };

    // Restore composite definitions by compact def id before resolving the
    // compact type table, so private/internal types referenced by HIR are
    // recreated with the same names, fields, variants, and discriminants.
    for (size_t si = 0; si < art.struct_defs.size(); ++si)
        struct_tids[si] = mTypes.defineStruct(art.struct_defs[si].name);
    for (size_t ei = 0; ei < art.enum_defs.size(); ++ei)
        enum_tids[ei] = mTypes.defineEnum(art.enum_defs[ei].name, types::kErrorType);
    for (size_t ui = 0; ui < art.union_defs.size(); ++ui)
        union_tids[ui] = mTypes.defineUnion(art.union_defs[ui].name, !art.union_defs[ui].is_raw);

    for (size_t i = 0; i < art.types.size(); ++i) {
        const auto &ct = art.types[i];
        switch (ct.kind) {
        case cache::CompactTypeKind::Error:
            compact_type_ids[i] = types::kErrorType;
            break;
        case cache::CompactTypeKind::Never:
            compact_type_ids[i] = types::kNeverType;
            break;
        case cache::CompactTypeKind::Void:
            compact_type_ids[i] = types::kVoidType;
            break;
        case cache::CompactTypeKind::Bool:
            compact_type_ids[i] = types::kBoolType;
            break;
        case cache::CompactTypeKind::Char:
            compact_type_ids[i] = types::kCharType;
            break;
        case cache::CompactTypeKind::Int:
            compact_type_ids[i] = mTypes.internInt(static_cast<types::IntWidth>(ct.int_width));
            break;
        case cache::CompactTypeKind::Float:
            compact_type_ids[i] = mTypes.internFloat(static_cast<types::FloatWidth>(ct.int_width));
            break;
        case cache::CompactTypeKind::Ptr:
            compact_type_ids[i] = mTypes.internPtr(compactType(ct.ref0), (ct.flags & 1U) != 0);
            break;
        case cache::CompactTypeKind::Array:
            compact_type_ids[i] = mTypes.internArray(compactType(ct.ref0), ct.ref1);
            break;
        case cache::CompactTypeKind::Struct: {
            compact_type_ids[i] =
                ct.ref0 < struct_tids.size() ? struct_tids[ct.ref0] : types::kErrorType;
            break;
        }
        case cache::CompactTypeKind::Fn: {
            std::vector<types::TypeId> params;
            params.reserve(ct.args.size());
            for (auto id : ct.args)
                params.push_back(compactType(id));
            compact_type_ids[i] = mTypes.internFn(params, compactType(ct.ref0));
            break;
        }
        case cache::CompactTypeKind::Optional:
            compact_type_ids[i] = mTypes.internOptional(compactType(ct.ref0));
            break;
        case cache::CompactTypeKind::Failable:
            compact_type_ids[i] = mTypes.internFailable(compactType(ct.ref0));
            break;
        case cache::CompactTypeKind::Pack: {
            std::vector<types::TypeId> members;
            members.reserve(ct.args.size());
            for (const auto id : ct.args)
                members.push_back(compactType(id));
            std::vector<std::string_view> names;
            names.reserve(ct.arg_names.size());
            for (const auto id : ct.arg_names)
                names.push_back(art.strings[id]);
            compact_type_ids[i] = mTypes.internPack(members, names);
            break;
        }
        case cache::CompactTypeKind::Dyn: {
            const auto target   = compactType(ct.ref0);
            compact_type_ids[i] = mTypes.internDyn(target, ct.ref1);
            break;
        }
        case cache::CompactTypeKind::Slice:
            compact_type_ids[i] = mTypes.internSlice(compactType(ct.ref0));
            break;
        case cache::CompactTypeKind::Enum: {
            compact_type_ids[i] =
                ct.ref0 < enum_tids.size() ? enum_tids[ct.ref0] : types::kErrorType;
            break;
        }
        case cache::CompactTypeKind::Union: {
            compact_type_ids[i] =
                ct.ref0 < union_tids.size() ? union_tids[ct.ref0] : types::kErrorType;
            break;
        }
        case cache::CompactTypeKind::TypeVar:
            compact_type_ids[i] = mTypes.internTypeVar();
            break;
        case cache::CompactTypeKind::GenericParam:
            compact_type_ids[i] = mTypes.internGenericParam(ct.ref0, ct.ref1);
            break;
        case cache::CompactTypeKind::Incomplete: {
            std::vector<types::TypeId> args;
            args.reserve(ct.args.size());
            for (auto id : ct.args)
                args.push_back(compactType(id));
            compact_type_ids[i] = mTypes.internIncomplete(compactType(ct.ref0), args);
            break;
        }
        case cache::CompactTypeKind::Opaque:
            compact_type_ids[i] = mTypes.internUnknown();
            break;
        case cache::CompactTypeKind::OpaqueTagged:
            compact_type_ids[i] = mTypes.internOpaqueTagged();
            break;
        }
    }

    for (size_t si = 0; si < art.struct_defs.size(); ++si) {
        const auto &s  = art.struct_defs[si];
        const auto tid = struct_tids[si];
        if (s.hasForeignLayout)
            mTypes.setForeignLayout(tid, s.foreignSizeBytes, s.foreignAlignBytes,
                                    s.foreignAbiIsSingleI64);
        if (!s.type_arg_ids.empty()) {
            std::vector<types::TypeId> args;
            args.reserve(s.type_arg_ids.size());
            for (const auto arg_id : s.type_arg_ids)
                args.push_back(compactType(arg_id));
            mTypes.setTypeArgs(tid, args);
        }
        for (size_t fi = 0; fi < s.field_name_ids.size() && fi < s.field_type_ids.size(); ++fi) {
            const auto &name = art.strings[s.field_name_ids[fi]];
            mTypes.addField(tid, name, compactType(s.field_type_ids[fi]));
        }
    }
    for (size_t ei = 0; ei < art.enum_defs.size(); ++ei) {
        const auto &e  = art.enum_defs[ei];
        const auto tid = enum_tids[ei];
        if (e.underlying_id != ~uint32_t{0})
            mTypes.setEnumUnderlying(tid, compactType(e.underlying_id));
        for (const auto &v : e.variants)
            mTypes.addEnumVariant(tid, v.name, v.discriminant);
    }
    for (size_t ui = 0; ui < art.union_defs.size(); ++ui) {
        const auto &u  = art.union_defs[ui];
        const auto tid = union_tids[ui];
        for (auto member_type_id : u.member_type_ids)
            mTypes.addUnionMember(tid, compactType(member_type_id));
    }

    // Rebuild the module-level expression pool. Function blocks and const global
    // initializers reference these global HirExprIds.
    for (const auto &ce : art.exprs) {
        hir::HirExpr expr;
        switch (ce.kind) {
        case cache::CompactExprKind::Literal: {
            hir::HirLiteral lit;
            lit.type = compactType(ce.type_id);
            if (ce.flags == 1) {
                lit.f = ce.flt_val;
            } else if (ce.flags == 2) {
                lit.b = ce.int_val != 0;
            } else if (ce.flags == 3) {
                const auto text = art.strings[ce.name_id];
                lit.str_val     = mInterner->intern(text);
            } else {
                lit.i = ce.int_val;
            }
            expr = lit;
            break;
        }
        case cache::CompactExprKind::Binary: {
            hir::HirBinary bin;
            bin.lhs          = ce.ref_a;
            bin.rhs          = ce.ref_b;
            bin.op           = static_cast<hir::HirBinaryOp>(ce.op);
            bin.type         = compactType(ce.type_id);
            bin.operand_type = compactType(ce.ref_e);
            expr             = bin;
            break;
        }
        case cache::CompactExprKind::Unary: {
            hir::HirUnary un;
            un.op      = static_cast<hir::HirUnaryOp>(ce.op);
            un.operand = ce.ref_a;
            un.type    = compactType(ce.type_id);
            expr       = un;
            break;
        }
        case cache::CompactExprKind::Let: {
            hir::HirLet let;
            let.name = mInterner->intern(art.strings[ce.name_id]);
            let.type = compactType(ce.type_id);
            let.init = ce.ref_a;
            expr     = let;
            break;
        }
        case cache::CompactExprKind::Var: {
            hir::HirVar var;
            var.name    = mInterner->intern(art.strings[ce.name_id]);
            var.version = ce.ref_c;
            expr        = var;
            break;
        }
        case cache::CompactExprKind::GlobalConstLoad: {
            hir::HirGlobalConstLoad load;
            load.name = mInterner->intern(art.strings[ce.name_id]);
            load.type = compactType(ce.type_id);
            expr      = load;
            break;
        }
        case cache::CompactExprKind::Call: {
            memory::DynArray<hir::HirExprId> args(mHirArena);
            for (auto id : ce.args)
                args.push(id);
            memory::DynArray<types::TypeId> arg_types(mHirArena);
            for (auto id : ce.arg_types)
                arg_types.push(compactType(id));
            hir::HirCall call(ce.ref_a, std::move(args), std::move(arg_types));
            call.resolved_fn = ce.ref_b;
            call.fn_type     = compactType(ce.ref_e);
            call.musttail    = (ce.flags & 1U) != 0U;
            call.usesTailCC  = (ce.flags & 2U) != 0U;
            expr             = std::move(call);
            break;
        }
        case cache::CompactExprKind::StateTailCall: {
            memory::DynArray<hir::HirExprId> args(mHirArena);
            for (auto id : ce.args)
                args.push(id);
            memory::DynArray<types::TypeId> arg_types(mHirArena);
            for (auto id : ce.arg_types)
                arg_types.push(compactType(id));
            hir::HirCall call(ce.ref_a, std::move(args), std::move(arg_types));
            call.resolved_fn = ce.ref_b;
            call.fn_type     = compactType(ce.ref_e);
            call.musttail    = true;
            call.usesTailCC  = (ce.flags & 2U) != 0U;
            hir::HirStateTailCall tail(mHirArena);
            tail.call = std::move(call);
            expr      = std::move(tail);
            break;
        }
        case cache::CompactExprKind::Ret: {
            hir::HirRet ret;
            ret.value = ce.ref_a;
            expr      = ret;
            break;
        }
        case cache::CompactExprKind::Branch: {
            hir::HirBranch branch;
            branch.cond       = ce.ref_a;
            branch.then_block = ce.ref_c;
            branch.else_block = ce.ref_d;
            expr              = branch;
            break;
        }
        case cache::CompactExprKind::Jump: {
            hir::HirJump jump;
            jump.target = ce.ref_c;
            expr        = jump;
            break;
        }
        case cache::CompactExprKind::Phi: {
            memory::DynArray<hir::HirExprId> incoming(mHirArena);
            for (auto id : ce.args)
                incoming.push(id);
            hir::HirPhi phi(mHirArena);
            phi.incoming = std::move(incoming);
            expr         = std::move(phi);
            break;
        }
        case cache::CompactExprKind::Assign: {
            hir::HirAssign assign;
            assign.target = ce.ref_a;
            assign.value  = ce.ref_b;
            expr          = assign;
            break;
        }
        case cache::CompactExprKind::Index: {
            hir::HirIndex idx;
            idx.object   = ce.ref_a;
            idx.index    = ce.ref_b;
            idx.type     = compactType(ce.type_id);
            idx.obj_type = compactType(ce.ref_e);
            idx.is_array = (ce.flags & 1U) != 0;
            expr         = idx;
            break;
        }
        case cache::CompactExprKind::Field: {
            hir::HirField field;
            field.object      = ce.ref_a;
            field.index       = ce.ref_c;
            field.type        = compactType(ce.type_id);
            field.object_type = compactType(ce.ref_e);
            expr              = field;
            break;
        }
        case cache::CompactExprKind::StructLiteral: {
            memory::DynArray<hir::HirExprId> values(mHirArena);
            for (auto id : ce.args)
                values.push(id);
            hir::HirStructLiteral lit(mHirArena);
            lit.values = std::move(values);
            lit.type   = compactType(ce.type_id);
            expr       = std::move(lit);
            break;
        }
        case cache::CompactExprKind::ArrayLiteral: {
            memory::DynArray<hir::HirExprId> elements(mHirArena);
            for (auto id : ce.args)
                elements.push(id);
            hir::HirArrayLiteral lit(mHirArena);
            lit.elements = std::move(elements);
            lit.type     = compactType(ce.type_id);
            expr         = std::move(lit);
            break;
        }
        case cache::CompactExprKind::EnumValue: {
            hir::HirEnumValue ev;
            ev.value = ce.int_val;
            ev.type  = compactType(ce.type_id);
            expr     = ev;
            break;
        }
        case cache::CompactExprKind::SlotAlloca: {
            hir::HirSlotAlloca sa;
            sa.slot = ce.ref_a;
            sa.type = compactType(ce.type_id);
            expr    = sa;
            break;
        }
        case cache::CompactExprKind::SlotStore: {
            hir::HirSlotStore ss;
            ss.slot  = ce.ref_a;
            ss.value = ce.ref_b;
            expr     = ss;
            break;
        }
        case cache::CompactExprKind::SlotLoad: {
            hir::HirSlotLoad sl;
            sl.slot = ce.ref_a;
            sl.type = compactType(ce.type_id);
            expr    = sl;
            break;
        }
        case cache::CompactExprKind::SlotAddr: {
            hir::HirSlotAddr sa;
            sa.slot = ce.ref_a;
            sa.type = compactType(ce.type_id);
            expr    = sa;
            break;
        }
        case cache::CompactExprKind::MakeNone: {
            hir::HirMakeNone mn;
            mn.type = compactType(ce.type_id);
            expr    = mn;
            break;
        }
        case cache::CompactExprKind::MakeSome: {
            hir::HirMakeSome ms;
            ms.value = ce.ref_a;
            ms.type  = compactType(ce.type_id);
            expr     = ms;
            break;
        }
        case cache::CompactExprKind::MakeSlice: {
            hir::HirMakeSlice slice;
            slice.object      = ce.ref_a;
            slice.lo          = ce.ref_b;
            slice.hi          = ce.ref_c;
            slice.type        = compactType(ce.type_id);
            slice.object_type = compactType(ce.ref_e);
            slice.bound_type  = compactType(ce.ref_f);
            slice.is_array    = (ce.flags & 1U) != 0;
            slice.is_pointer  = (ce.flags & 4U) != 0;
            slice.checked     = (ce.flags & 2U) != 0;
            expr              = slice;
            break;
        }
        case cache::CompactExprKind::UnionCheck: {
            hir::HirUnionCheck check;
            check.value        = ce.ref_a;
            check.union_type   = compactType(ce.ref_b);
            check.member_index = ce.ref_c;
            expr               = check;
            break;
        }
        case cache::CompactExprKind::Cast: {
            const auto to   = compactType(ce.ref_b);
            const auto from = compactType(ce.ref_e);
            if (from == types::kInvalidType || to == types::kInvalidType) {
                hir::HirCast cast;
                cast.value = ce.ref_a;
                cast.from  = from;
                cast.to    = to;
                expr       = cast;
            } else if (mTypes.kindOf(from) == types::TypeKind::Union ||
                       mTypes.kindOf(to) == types::TypeKind::Union) {
                hir::HirUnionCast cast;
                cast.value        = ce.ref_a;
                cast.from         = from;
                cast.to           = to;
                cast.member_index = ce.ref_c;
                cast.checked      = (ce.flags & 1U) != 0;
                expr              = cast;
            } else {
                hir::HirCast cast;
                cast.value = ce.ref_a;
                cast.from  = from;
                cast.to    = to;
                expr       = cast;
            }
            break;
        }
        case cache::CompactExprKind::LayoutIntrinsic: {
            hir::HirLayoutIntrinsic li;
            li.which         = static_cast<hir::HirLayoutIntrinsic::Which>(ce.ref_e);
            li.type          = compactType(ce.type_id);
            li.field_index   = ce.ref_f;
            li.operand       = ce.ref_a;
            li.operand_type  = compactType(ce.ref_b);
            li.string_length = static_cast<uint64_t>(ce.int_val);
            expr             = li;
            break;
        }
        case cache::CompactExprKind::Cleanup: {
            memory::DynArray<hir::HirExprId> exprs(mHirArena);
            for (auto id : ce.args)
                exprs.push(id);
            hir::HirCleanup cleanup(mHirArena);
            cleanup.exprs = std::move(exprs);
            expr          = std::move(cleanup);
            break;
        }
        case cache::CompactExprKind::MakeDyn: {
            hir::HirMakeDyn make;
            make.value       = ce.ref_a;
            make.source_type = compactType(ce.ref_b);
            make.dyn_type    = compactType(ce.type_id);
            make.vtable_name = mInterner->intern(art.strings[ce.name_id]);
            expr             = std::move(make);
            break;
        }
        case cache::CompactExprKind::DynCall: {
            memory::DynArray<hir::HirExprId> args(mHirArena);
            for (auto id : ce.args)
                args.push(id);
            memory::DynArray<types::TypeId> arg_types(mHirArena);
            for (auto id : ce.arg_types)
                arg_types.push(compactType(id));
            hir::HirDynCall call(mHirArena);
            call.receiver     = ce.ref_a;
            call.vtable_name  = mInterner->intern(art.strings[ce.name_id]);
            call.slot_index   = ce.ref_c;
            call.result_type  = compactType(ce.type_id);
            call.fn_type      = compactType(ce.ref_e);
            call.args         = std::move(args);
            call.arg_types    = std::move(arg_types);
            call.has_receiver = (ce.flags & 1U) != 0;
            expr              = std::move(call);
            break;
        }
        case cache::CompactExprKind::MakeOpaque: {
            hir::HirMakeOpaque make;
            make.value        = ce.ref_a;
            make.source_type  = compactType(ce.ref_b);
            make.opaque_type  = compactType(ce.type_id);
            make.type_id      = ce.ref_c;
            make.canonical_id = ce.ints.size() >= 2U
                                    ? types::TypeCanonicalId{ce.ints[0], ce.ints[1]}
                                    : types::kInvalidCanonicalId;
            expr              = std::move(make);
            break;
        }
        case cache::CompactExprKind::OpaqueCast: {
            hir::HirOpaqueCast cast;
            cast.value        = ce.ref_a;
            cast.from         = compactType(ce.ref_b);
            cast.to           = compactType(ce.ref_c);
            cast.opaque_type  = compactType(ce.ref_d);
            cast.result_type  = compactType(ce.type_id);
            cast.type_id      = ce.ref_e;
            cast.canonical_id = ce.ints.size() >= 2U
                                    ? types::TypeCanonicalId{ce.ints[0], ce.ints[1]}
                                    : types::kInvalidCanonicalId;
            cast.checked      = (ce.flags & 1U) != 0;
            cast.returns_ptr  = (ce.flags & 2U) != 0;
            expr              = std::move(cast);
            break;
        }
        case cache::CompactExprKind::OpaqueCheck: {
            hir::HirOpaqueCheck check;
            check.value        = ce.ref_a;
            check.opaque_type  = compactType(ce.type_id);
            check.type_id      = ce.ref_e;
            check.canonical_id = ce.ints.size() >= 2U
                                     ? types::TypeCanonicalId{ce.ints[0], ce.ints[1]}
                                     : types::kInvalidCanonicalId;
            expr               = std::move(check);
            break;
        }
        case cache::CompactExprKind::RuntimePanic: {
            hir::HirRuntimePanic panic;
            panic.code = static_cast<uint32_t>(ce.int_val);
            expr       = std::move(panic);
            break;
        }
        case cache::CompactExprKind::CanonicalType: {
            hir::HirCanonicalType canonical;
            canonical.type         = compactType(ce.type_id);
            canonical.canonical_id = ce.ints.size() >= 2U
                                         ? types::TypeCanonicalId{ce.ints[0], ce.ints[1]}
                                         : types::kInvalidCanonicalId;
            expr                   = std::move(canonical);
            break;
        }
        }
        mHirModule.addExpr(std::move(expr));
    }

    for (const auto &cv : art.vtables) {
        auto &vtable = mHirModule.addVTable(mInterner->intern(art.strings[cv.name_id]));
        for (const auto sym : cv.slot_sym_ids)
            vtable.slots.push(static_cast<symbols::SymId>(sym));
    }

    symbols::SymId next_sym = 1;
    for (size_t fi = 0; fi < art.functions.size(); ++fi) {
        const auto &cfn                 = art.functions[fi];
        const std::string_view cfn_name = cfn.name.empty() && cfn.name_id < art.strings.size()
                                              ? art.strings[cfn.name_id]
                                              : cfn.name;
        auto &fn                        = mHirModule.addFn(mInterner->intern(cfn_name));
        fn.return_type                  = compactType(cfn.return_type_id);
        fn.isForeignC                   = cfn.is_foreign_c;
        fn.isVariadic                   = cfn.is_variadic;
        fn.isState                      = cfn.is_state;
        fn.usesTailCC                   = cfn.uses_tailcc;
        fn.variadicSliceParam = cfn.variadic_slice_param != ~uint32_t{0} ? cfn.variadic_slice_param
                                                                         : ~static_cast<size_t>(0);
        fn.machineId          = cfn.machine_id;
        fn.machineReturnType  = cfn.machine_return_type_id != 0
                                    ? compactType(cfn.machine_return_type_id)
                                    : fn.return_type;
        fn.sym_id             = next_sym++;
        for (size_t pi = 0; pi < cfn.param_type_ids.size() && pi < cfn.param_name_ids.size();
             ++pi) {
            fn.params.push(compactType(cfn.param_type_ids[pi]));
            fn.param_names.push(mInterner->intern(art.strings[cfn.param_name_ids[pi]]));
            fn.param_slots.push(pi < cfn.param_slot_ids.size()
                                    ? static_cast<hir::HirSlotId>(cfn.param_slot_ids[pi])
                                    : hir::kInvalidHirSlot);
        }
        for (const auto &cblk : cfn.blocks) {
            auto &blk = fn.blocks.emplace(mHirArena);
            for (auto id : cblk.insts)
                blk.insts.push(id);
            blk.terminator = cblk.terminator;
        }
    }

    for (const auto &cg : art.globals) {
        if (cg.name_id >= art.strings.size())
            continue;
        auto &global = mHirModule.addGlobalConst();
        global.name  = mInterner->intern(art.strings[cg.name_id]);
        global.type  = compactType(cg.type_id);
        global.init  = cg.init_expr;
    }

    for (const auto &slot : art.attrs_slots) {
        auto &attrs     = mHirModule.attrs().slot(static_cast<hir::HirSlotId>(slot.slot));
        attrs.ownership = static_cast<hir::HirOwnership>(slot.ownership);
        attrs.consumed  = static_cast<hir::HirConsumedState>(slot.consumed);
        attrs.nonNull   = slot.nonNull;
    }
    for (const auto &call : art.attrs_calls) {
        auto &attrs      = mHirModule.attrs().call(call.expr_id);
        attrs.returnsArg = call.returns_arg;
        for (auto escape : call.arg_escapes)
            attrs.args.emplace(hir::HirCallArgAttr{static_cast<hir::HirCallEscape>(escape)});
    }
    for (const auto &fn_attrs : art.attrs_fns) {
        auto &attrs          = mHirModule.attrs().fn(fn_attrs.fn_index);
        attrs.returnConsumed = static_cast<hir::HirConsumedState>(fn_attrs.return_consumed);
        attrs.nonNull        = fn_attrs.nonNull;
        attrs.noAlias        = fn_attrs.noAlias;
        attrs.readOnly       = fn_attrs.readOnly;
        attrs.noCapture      = fn_attrs.noCapture;
    }
}

} // namespace zith::session

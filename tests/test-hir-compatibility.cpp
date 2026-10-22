#include "diagnostics/diagnostic-engine.hpp"
#include "hir/hir-verify.hpp"
#include "memory/arena.hpp"
#include "memory/string-interner.hpp"
#include "test-common.hpp"
#include "types/type-intern.hpp"

#include <utility>

using namespace zith;

namespace {

auto verify(hir::HirModule &module, const types::TypeIntern &types,
            diagnostics::DiagnosticEngine &diags) -> bool {
    return hir::HirVerifier(module, types, diags).verify();
}

auto addDefinition(hir::HirModule &module, memory::StringInterner &interner) -> hir::HirFunction & {
    auto &fn   = module.addFn(interner.intern("main"));
    fn.hasBody = true;
    return fn;
}

void test_valid_minimal_module() {
    memory::Arena arena;
    memory::StringInterner interner(arena);
    types::TypeIntern types(arena, interner);
    diagnostics::DiagnosticEngine diags(arena);
    hir::HirModule module(arena);

    const auto i32 = types.internInt(types::IntWidth::I32);
    auto &fn       = addDefinition(module, interner);
    fn.return_type = i32;
    fn.blocks.emplace(arena);
    hir::HirLiteral literal;
    literal.type     = i32;
    literal.i        = 1;
    const auto value = module.addExpr(literal);
    fn.blocks[0].insts.push(value);
    fn.blocks[0].terminator = module.addExpr(hir::HirRet{value});

    CHECK(verify(module, types, diags), "verifier accepts a well-formed function");
    CHECK_EQ(diags.errorCount(), 0u, "valid HIR produces no diagnostics");
}

void test_invalid_expression_reference() {
    memory::Arena arena;
    memory::StringInterner interner(arena);
    types::TypeIntern types(arena, interner);
    diagnostics::DiagnosticEngine diags(arena);
    hir::HirModule module(arena);

    const auto i32 = types.internInt(types::IntWidth::I32);
    auto &fn       = addDefinition(module, interner);
    fn.return_type = i32;
    fn.blocks.emplace(arena);
    hir::HirUnary unary;
    unary.operand = 99;
    unary.type    = i32;
    fn.blocks[0].insts.push(module.addExpr(unary));
    fn.blocks[0].terminator = module.addExpr(hir::HirRet{hir::kInvalidHirExpr});

    CHECK(!verify(module, types, diags), "verifier rejects an invalid expression reference");
    CHECK(diags.hasErrors(), "invalid expression reference emits a diagnostic");
}

void test_invalid_block_target() {
    memory::Arena arena;
    memory::StringInterner interner(arena);
    types::TypeIntern types(arena, interner);
    diagnostics::DiagnosticEngine diags(arena);
    hir::HirModule module(arena);

    auto &fn       = addDefinition(module, interner);
    fn.return_type = types::kVoidType;
    fn.blocks.emplace(arena);
    hir::HirLiteral condition;
    condition.type          = types::kBoolType;
    const auto condition_id = module.addExpr(condition);
    hir::HirBranch branch;
    branch.cond             = condition_id;
    branch.then_block       = 1;
    branch.else_block       = 0;
    fn.blocks[0].terminator = module.addExpr(branch);

    CHECK(!verify(module, types, diags), "verifier rejects a branch outside its function");
}

void test_invalid_function_and_vtable_references() {
    memory::Arena arena;
    memory::StringInterner interner(arena);
    types::TypeIntern types(arena, interner);
    diagnostics::DiagnosticEngine diags(arena);
    hir::HirModule module(arena);

    auto &fn       = addDefinition(module, interner);
    fn.return_type = types::kVoidType;
    fn.blocks.emplace(arena);
    hir::HirCall call(arena);
    call.resolved_fn = 17;
    fn.blocks[0].insts.push(module.addExpr(std::move(call)));
    fn.blocks[0].terminator = module.addExpr(hir::HirRet{hir::kInvalidHirExpr});
    auto &vtable            = module.addVTable(interner.intern("invalid"));
    vtable.slots.push(17);

    CHECK(!verify(module, types, diags),
          "verifier rejects invalid direct-call and vtable function ids");
}

void test_missing_slot_definition() {
    memory::Arena arena;
    memory::StringInterner interner(arena);
    types::TypeIntern types(arena, interner);
    diagnostics::DiagnosticEngine diags(arena);
    hir::HirModule module(arena);

    auto &fn       = addDefinition(module, interner);
    fn.return_type = types::kVoidType;
    fn.blocks.emplace(arena);
    hir::HirSlotLoad load;
    load.slot = 4;
    load.type = types.internInt(types::IntWidth::I32);
    fn.blocks[0].insts.push(module.addExpr(load));
    fn.blocks[0].terminator = module.addExpr(hir::HirRet{hir::kInvalidHirExpr});

    CHECK(!verify(module, types, diags), "verifier rejects a slot use without a definition");
}

void test_call_facts_allow_variadic_argument_coalescing() {
    memory::Arena arena;
    memory::StringInterner interner(arena);
    types::TypeIntern types(arena, interner);
    diagnostics::DiagnosticEngine diags(arena);
    hir::HirModule module(arena);

    const auto i32 = types.internInt(types::IntWidth::I32);
    auto &fn       = addDefinition(module, interner);
    fn.return_type = types::kVoidType;
    fn.blocks.emplace(arena);
    hir::HirLiteral arg;
    arg.type          = i32;
    const auto arg_id = module.addExpr(arg);
    hir::HirCall call(arena);
    call.args.push(arg_id);
    call.argument_types.push(i32);
    const auto call_id = module.addExpr(std::move(call));
    fn.blocks[0].insts.push(arg_id);
    fn.blocks[0].insts.push(call_id);
    fn.blocks[0].terminator = module.addExpr(hir::HirRet{hir::kInvalidHirExpr});

    auto &attrs = module.attrs().call(call_id);
    attrs.args.emplace(hir::HirCallArgAttr{hir::HirCallEscape::Borrow});
    attrs.args.emplace(hir::HirCallArgAttr{hir::HirCallEscape::Escape});

    CHECK(attrs.hasResidualFacts(), "call argument escape facts count as residual facts");
    CHECK(verify(module, types, diags), "source call facts may outnumber normalized HIR operands");
}

void test_checked_operation_without_proof_attributes() {
    memory::Arena arena;
    memory::StringInterner interner(arena);
    types::TypeIntern types(arena, interner);
    diagnostics::DiagnosticEngine diags(arena);
    hir::HirModule module(arena);

    const auto i32          = types.internInt(types::IntWidth::I32);
    const auto opaque       = types.internOpaqueTagged();
    const auto optional_i32 = types.internOptional(i32);
    auto &fn                = addDefinition(module, interner);
    fn.return_type          = optional_i32;
    fn.blocks.emplace(arena);
    hir::HirLiteral payload;
    payload.type          = opaque;
    const auto payload_id = module.addExpr(payload);
    hir::HirOpaqueCast cast;
    cast.value         = payload_id;
    cast.from          = opaque;
    cast.to            = i32;
    cast.opaque_type   = opaque;
    cast.result_type   = optional_i32;
    cast.safety        = hir::HirSafetyMode::RequiredCheck;
    const auto cast_id = module.addExpr(cast);
    fn.blocks[0].insts.push(payload_id);
    fn.blocks[0].insts.push(cast_id);
    fn.blocks[0].terminator = module.addExpr(hir::HirRet{cast_id});

    CHECK(verify(module, types, diags),
          "a checked HIR operation remains valid without optional proof attributes");
    CHECK_EQ(diags.errorCount(), 0u, "missing proof attributes do not imply safety or fail HIR");
}

void test_matching_and_mismatched_safety_proofs() {
    memory::Arena arena;
    memory::StringInterner interner(arena);
    types::TypeIntern types(arena, interner);
    diagnostics::DiagnosticEngine diags(arena);
    hir::HirModule module(arena);

    const auto i32        = types.internInt(types::IntWidth::I32);
    const auto array_type = types.internArray(i32, 4);
    auto &fn              = addDefinition(module, interner);
    fn.return_type        = i32;
    fn.blocks.emplace(arena);
    hir::HirLiteral object;
    object.type          = array_type;
    const auto object_id = module.addExpr(object);
    hir::HirLiteral index;
    index.type          = i32;
    index.i             = 1;
    const auto index_id = module.addExpr(index);
    hir::HirIndex access;
    access.object        = object_id;
    access.index         = index_id;
    access.type          = i32;
    access.obj_type      = array_type;
    access.is_array      = true;
    access.safety        = hir::HirSafetyMode::RequiredCheck;
    const auto access_id = module.addExpr(access);
    fn.blocks[0].insts.push(access_id);
    fn.blocks[0].terminator = module.addExpr(hir::HirRet{access_id});

    auto &proof     = module.attrs().safetyProof(access_id);
    proof.kind      = hir::HirSafetyProofKind::BoundsValid;
    proof.operand_a = object_id;
    proof.operand_b = index_id;
    proof.present   = true;
    CHECK(verify(module, types, diags),
          "matching proof facts validate without changing the required-check mode");

    proof.operand_b = object_id;
    CHECK(!verify(module, types, diags), "verifier rejects proof facts for different operands");
}

void test_guarded_optional_payload_requires_dominating_true_edge() {
    const auto run = [](const bool use_true_edge, const bool merge_paths,
                        const bool use_matching_slot) {
        memory::Arena arena;
        memory::StringInterner interner(arena);
        types::TypeIntern types(arena, interner);
        diagnostics::DiagnosticEngine diags(arena);
        hir::HirModule module(arena);

        const auto i32          = types.internInt(types::IntWidth::I32);
        const auto optional_i32 = types.internOptional(i32);
        auto &fn                = addDefinition(module, interner);
        fn.return_type          = types::kVoidType;
        fn.blocks.emplace(arena);
        fn.blocks.emplace(arena);
        fn.blocks.emplace(arena);
        if (merge_paths)
            fn.blocks.emplace(arena);

        fn.blocks[0].insts.push(module.addExpr(hir::HirSlotAlloca{0, optional_i32}));
        if (!use_matching_slot)
            fn.blocks[0].insts.push(module.addExpr(hir::HirSlotAlloca{1, optional_i32}));
        const auto tag_object = module.addExpr(hir::HirSlotAddr{0, optional_i32});
        const auto tag = module.addExpr(hir::HirField{tag_object, 1U, types::kBoolType,
                                                      optional_i32, hir::HirSafetyMode::Intrinsic});
        hir::HirBranch branch;
        branch.cond             = tag;
        branch.then_block       = 1;
        branch.else_block       = 2;
        fn.blocks[0].terminator = module.addExpr(branch);

        const size_t use_block = merge_paths ? 3U : (use_true_edge ? 1U : 2U);
        if (merge_paths) {
            fn.blocks[1].terminator =
                module.addExpr(hir::HirJump{static_cast<hir::HirBlockId>(use_block)});
            fn.blocks[2].terminator =
                module.addExpr(hir::HirJump{static_cast<hir::HirBlockId>(use_block)});
        }
        const auto payload_object =
            module.addExpr(hir::HirSlotAddr{use_matching_slot ? 0U : 1U, optional_i32});
        const auto payload = module.addExpr(
            hir::HirField{payload_object, 0U, i32, optional_i32, hir::HirSafetyMode::Guarded});
        fn.blocks[use_block].insts.push(payload);
        fn.blocks[use_block].terminator = module.addExpr(hir::HirRet{hir::kInvalidHirExpr});
        if (!merge_paths && use_block == 1U)
            fn.blocks[2].terminator = module.addExpr(hir::HirRet{hir::kInvalidHirExpr});

        return verify(module, types, diags);
    };

    CHECK(run(true, false, true), "matching optional guard accepts payload on its true edge");
    CHECK(!run(false, false, true), "optional guard does not authorize its false edge");
    CHECK(!run(true, true, true), "a guard on only one incoming path does not dominate a merge");
    CHECK(!run(true, false, false), "a guard for another optional value is not accepted");
}

void test_guarded_slice_index_requires_matching_bounds_edge() {
    const auto run = [](const bool use_true_edge, const bool use_matching_index_slot) {
        memory::Arena arena;
        memory::StringInterner interner(arena);
        types::TypeIntern types(arena, interner);
        diagnostics::DiagnosticEngine diags(arena);
        hir::HirModule module(arena);

        const auto i32          = types.internInt(types::IntWidth::I32);
        const auto i64          = types.internInt(types::IntWidth::I64);
        const auto slice_type   = types.internSlice(i32);
        const auto optional_i32 = types.internOptional(i32);
        auto &fn                = addDefinition(module, interner);
        fn.return_type          = optional_i32;
        fn.blocks.emplace(arena);
        fn.blocks.emplace(arena);
        fn.blocks.emplace(arena);

        fn.blocks[0].insts.push(module.addExpr(hir::HirSlotAlloca{0, slice_type}));
        fn.blocks[0].insts.push(module.addExpr(hir::HirSlotAlloca{1, i32}));
        if (!use_matching_index_slot)
            fn.blocks[0].insts.push(module.addExpr(hir::HirSlotAlloca{2, i32}));

        const auto checked_index = module.addExpr(hir::HirSlotLoad{1, i32});
        const auto zero          = module.addExpr(hir::HirLiteral{.i = 0, .type = i32});
        const auto lower_check   = module.addExpr(
            hir::HirBinary{checked_index, zero, hir::HirBinaryOp::Ge, types::kBoolType, i32});
        const auto slice_load   = module.addExpr(hir::HirSlotLoad{0, slice_type});
        const auto length_field = module.addExpr(
            hir::HirField{slice_load, 1U, i64, slice_type, hir::HirSafetyMode::Intrinsic});
        const auto length      = module.addExpr(hir::HirCast{length_field, i64, i32});
        const auto upper_check = module.addExpr(
            hir::HirBinary{checked_index, length, hir::HirBinaryOp::Lt, types::kBoolType, i32});
        const auto bounds_check = module.addExpr(hir::HirBinary{
            lower_check, upper_check, hir::HirBinaryOp::And, types::kBoolType, types::kBoolType});

        hir::HirBranch branch;
        branch.cond             = bounds_check;
        branch.then_block       = 1;
        branch.else_block       = 2;
        fn.blocks[0].terminator = module.addExpr(branch);

        const auto index_slot = use_matching_index_slot ? 1U : 2U;
        hir::HirIndex index;
        index.object         = module.addExpr(hir::HirSlotLoad{0, slice_type});
        index.index          = module.addExpr(hir::HirSlotLoad{index_slot, i32});
        index.type           = i32;
        index.obj_type       = slice_type;
        index.safety         = hir::HirSafetyMode::Guarded;
        const auto index_id  = module.addExpr(index);
        const auto some_id   = module.addExpr(hir::HirMakeSome{index_id, optional_i32});
        const auto use_block = use_true_edge ? 1U : 2U;
        fn.blocks[use_block].insts.push(some_id);
        fn.blocks[use_block].terminator = module.addExpr(hir::HirRet{some_id});
        fn.blocks[use_true_edge ? 2U : 1U].terminator =
            module.addExpr(hir::HirRet{hir::kInvalidHirExpr});

        return verify(module, types, diags);
    };

    CHECK(run(true, true), "slice index accepts its matching bounds check, including length casts");
    CHECK(!run(false, true), "slice index rejects a bounds check on the false edge");
    CHECK(!run(true, false), "slice index rejects bounds for a different index slot");
}

void test_guarded_slice_requires_matching_bounds_edge() {
    const auto run = [](const bool use_true_edge, const bool use_matching_bounds) {
        memory::Arena arena;
        memory::StringInterner interner(arena);
        types::TypeIntern types(arena, interner);
        diagnostics::DiagnosticEngine diags(arena);
        hir::HirModule module(arena);

        const auto i32        = types.internInt(types::IntWidth::I32);
        const auto i64        = types.internInt(types::IntWidth::I64);
        const auto slice_type = types.internSlice(i32);
        auto &fn              = addDefinition(module, interner);
        fn.return_type        = slice_type;
        fn.blocks.emplace(arena);
        fn.blocks.emplace(arena);
        fn.blocks.emplace(arena);

        fn.blocks[0].insts.push(module.addExpr(hir::HirSlotAlloca{0, slice_type}));
        fn.blocks[0].insts.push(module.addExpr(hir::HirSlotAlloca{1, i32}));
        fn.blocks[0].insts.push(module.addExpr(hir::HirSlotAlloca{2, i32}));
        if (!use_matching_bounds)
            fn.blocks[0].insts.push(module.addExpr(hir::HirSlotAlloca{3, i32}));

        const auto lower       = module.addExpr(hir::HirSlotLoad{1, i32});
        const auto upper       = module.addExpr(hir::HirSlotLoad{2, i32});
        const auto zero        = module.addExpr(hir::HirLiteral{.i = 0, .type = i32});
        const auto nonnegative = module.addExpr(
            hir::HirBinary{lower, zero, hir::HirBinaryOp::Ge, types::kBoolType, i32});
        const auto slice_load   = module.addExpr(hir::HirSlotLoad{0, slice_type});
        const auto length_field = module.addExpr(
            hir::HirField{slice_load, 1U, i64, slice_type, hir::HirSafetyMode::Intrinsic});
        const auto length        = module.addExpr(hir::HirCast{length_field, i64, i32});
        const auto within_length = module.addExpr(
            hir::HirBinary{upper, length, hir::HirBinaryOp::Le, types::kBoolType, i32});
        const auto ordered = module.addExpr(
            hir::HirBinary{lower, upper, hir::HirBinaryOp::Le, types::kBoolType, i32});
        const auto first_pair   = module.addExpr(hir::HirBinary{
            nonnegative, within_length, hir::HirBinaryOp::And, types::kBoolType, types::kBoolType});
        const auto bounds_check = module.addExpr(hir::HirBinary{
            first_pair, ordered, hir::HirBinaryOp::And, types::kBoolType, types::kBoolType});

        hir::HirBranch branch;
        branch.cond             = bounds_check;
        branch.then_block       = 1;
        branch.else_block       = 2;
        fn.blocks[0].terminator = module.addExpr(branch);

        const auto hi_slot = use_matching_bounds ? 2U : 3U;
        hir::HirMakeSlice slice;
        slice.object         = module.addExpr(hir::HirSlotLoad{0, slice_type});
        slice.lo             = module.addExpr(hir::HirSlotLoad{1, i32});
        slice.hi             = module.addExpr(hir::HirSlotLoad{hi_slot, i32});
        slice.type           = slice_type;
        slice.object_type    = slice_type;
        slice.bound_type     = i32;
        slice.safety         = hir::HirSafetyMode::Guarded;
        const auto slice_id  = module.addExpr(slice);
        const auto use_block = use_true_edge ? 1U : 2U;
        fn.blocks[use_block].insts.push(slice_id);
        fn.blocks[use_block].terminator = module.addExpr(hir::HirRet{slice_id});
        fn.blocks[use_true_edge ? 2U : 1U].terminator =
            module.addExpr(hir::HirRet{hir::kInvalidHirExpr});

        return verify(module, types, diags);
    };

    CHECK(run(true, true), "slice construction accepts its matching bounds check");
    CHECK(!run(false, true), "slice construction rejects a bounds check on the false edge");
    CHECK(!run(true, false), "slice construction rejects bounds for a different upper-bound slot");
}

void test_guarded_union_and_opaque_casts_require_matching_checks() {
    const auto run_union = [](const bool use_true_edge, const bool check_matching_member) {
        memory::Arena arena;
        memory::StringInterner interner(arena);
        types::TypeIntern types(arena, interner);
        diagnostics::DiagnosticEngine diags(arena);
        hir::HirModule module(arena);

        const auto i32        = types.internInt(types::IntWidth::I32);
        const auto i64        = types.internInt(types::IntWidth::I64);
        const auto union_type = types.defineUnion("GuardedUnion", true);
        types.addUnionMember(union_type, i32);
        types.addUnionMember(union_type, i64);
        const auto tag_type = types.internInt(types::IntWidth::U8);
        auto &fn            = addDefinition(module, interner);
        fn.return_type      = types::kVoidType;
        fn.blocks.emplace(arena);
        fn.blocks.emplace(arena);
        fn.blocks.emplace(arena);
        fn.blocks[0].insts.push(module.addExpr(hir::HirSlotAlloca{0, union_type}));

        const auto checked_value = module.addExpr(hir::HirSlotLoad{0, union_type});
        const auto tag           = module.addExpr(
            hir::HirField{checked_value, 1U, tag_type, union_type, hir::HirSafetyMode::Intrinsic});
        hir::HirUnionCheck check;
        check.value        = tag;
        check.union_type   = union_type;
        check.member_index = check_matching_member ? 0U : 1U;
        hir::HirBranch branch;
        branch.cond             = module.addExpr(check);
        branch.then_block       = 1;
        branch.else_block       = 2;
        fn.blocks[0].terminator = module.addExpr(branch);

        hir::HirUnionCast cast;
        cast.value           = module.addExpr(hir::HirSlotLoad{0, union_type});
        cast.from            = union_type;
        cast.to              = i32;
        cast.member_index    = 0;
        cast.safety          = hir::HirSafetyMode::Guarded;
        const auto cast_id   = module.addExpr(cast);
        const auto use_block = use_true_edge ? 1U : 2U;
        fn.blocks[use_block].insts.push(cast_id);
        fn.blocks[use_block].terminator = module.addExpr(hir::HirRet{hir::kInvalidHirExpr});
        fn.blocks[use_true_edge ? 2U : 1U].terminator =
            module.addExpr(hir::HirRet{hir::kInvalidHirExpr});

        return verify(module, types, diags);
    };

    const auto run_opaque = [](const bool use_true_edge, const bool matching_type_id) {
        memory::Arena arena;
        memory::StringInterner interner(arena);
        types::TypeIntern types(arena, interner);
        diagnostics::DiagnosticEngine diags(arena);
        hir::HirModule module(arena);

        const auto i32         = types.internInt(types::IntWidth::I32);
        const auto opaque_type = types.internOpaqueTagged();
        auto &fn               = addDefinition(module, interner);
        fn.return_type         = types::kVoidType;
        fn.blocks.emplace(arena);
        fn.blocks.emplace(arena);
        fn.blocks.emplace(arena);
        fn.blocks[0].insts.push(module.addExpr(hir::HirSlotAlloca{0, opaque_type}));

        hir::HirOpaqueCheck check;
        check.value       = module.addExpr(hir::HirSlotLoad{0, opaque_type});
        check.opaque_type = opaque_type;
        check.type_id     = 17;
        hir::HirBranch branch;
        branch.cond             = module.addExpr(check);
        branch.then_block       = 1;
        branch.else_block       = 2;
        fn.blocks[0].terminator = module.addExpr(branch);

        hir::HirOpaqueCast cast;
        cast.value           = module.addExpr(hir::HirSlotLoad{0, opaque_type});
        cast.from            = opaque_type;
        cast.to              = i32;
        cast.opaque_type     = opaque_type;
        cast.result_type     = i32;
        cast.type_id         = matching_type_id ? 17U : 18U;
        cast.safety          = hir::HirSafetyMode::Guarded;
        const auto cast_id   = module.addExpr(cast);
        const auto use_block = use_true_edge ? 1U : 2U;
        fn.blocks[use_block].insts.push(cast_id);
        fn.blocks[use_block].terminator = module.addExpr(hir::HirRet{hir::kInvalidHirExpr});
        fn.blocks[use_true_edge ? 2U : 1U].terminator =
            module.addExpr(hir::HirRet{hir::kInvalidHirExpr});

        return verify(module, types, diags);
    };

    CHECK(run_union(true, true), "union cast accepts the matching member check");
    CHECK(!run_union(false, true), "union cast rejects a check on the false edge");
    CHECK(!run_union(true, false), "union cast rejects a check for a different member");
    CHECK(run_opaque(true, true), "opaque cast accepts the matching type check");
    CHECK(!run_opaque(false, true), "opaque cast rejects a check on the false edge");
    CHECK(!run_opaque(true, false), "opaque cast rejects a check for a different type id");
}

void test_safety_proof_on_unsupported_expression_is_rejected() {
    memory::Arena arena;
    memory::StringInterner interner(arena);
    types::TypeIntern types(arena, interner);
    diagnostics::DiagnosticEngine diags(arena);
    hir::HirModule module(arena);

    auto &fn       = addDefinition(module, interner);
    fn.return_type = types::kVoidType;
    fn.blocks.emplace(arena);
    hir::HirLiteral literal;
    literal.type          = types::kBoolType;
    const auto literal_id = module.addExpr(literal);
    fn.blocks[0].insts.push(literal_id);
    fn.blocks[0].terminator = module.addExpr(hir::HirRet{hir::kInvalidHirExpr});
    module.attrs().safetyProof(literal_id).present = true;

    CHECK(!verify(module, types, diags),
          "verifier rejects proof attributes attached to expression kinds without checks");
}

void test_function_body_classification_is_explicit() {
    memory::Arena arena;
    memory::StringInterner interner(arena);
    types::TypeIntern types(arena, interner);
    diagnostics::DiagnosticEngine declaration_diags(arena);
    hir::HirModule declaration_module(arena);

    auto &declaration       = declaration_module.addFn(interner.intern("external"));
    declaration.return_type = types::kVoidType;
    CHECK(verify(declaration_module, types, declaration_diags),
          "a declaration with no body is valid");

    diagnostics::DiagnosticEngine mismatch_diags(arena);
    hir::HirModule mismatch_module(arena);
    auto &definition       = mismatch_module.addFn(interner.intern("defined"));
    definition.return_type = types::kVoidType;
    definition.blocks.emplace(arena);
    CHECK(!verify(mismatch_module, types, mismatch_diags),
          "blocks without explicit definition classification are rejected");
}

} // namespace

static void test_hir_compatibility() {
    test_valid_minimal_module();
    test_invalid_expression_reference();
    test_invalid_block_target();
    test_invalid_function_and_vtable_references();
    test_missing_slot_definition();
    test_call_facts_allow_variadic_argument_coalescing();
    test_checked_operation_without_proof_attributes();
    test_matching_and_mismatched_safety_proofs();
    test_guarded_optional_payload_requires_dominating_true_edge();
    test_guarded_slice_index_requires_matching_bounds_edge();
    test_guarded_slice_requires_matching_bounds_edge();
    test_guarded_union_and_opaque_casts_require_matching_checks();
    test_safety_proof_on_unsupported_expression_is_rejected();
    test_function_body_classification_is_explicit();
}

TEST_MAIN(hir_compatibility);

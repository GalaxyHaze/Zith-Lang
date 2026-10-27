#include "sema/hir-lower-modern.hpp"

#include "common/test-log-protocol.hpp"
#include "diagnostics/error-codes.hpp"
#include "sema/hir-lower-utils.hpp"
#include "types/type-kind.hpp"

#include <string>

namespace zith::sema {
namespace modern {

hir::HirExprId HirLowerModern::emitI32Literal(const int64_t value) {
    hir::HirLiteral literal;
    literal.type = types_.internInt(types::IntWidth::I32);
    literal.i    = value;
    return addExpr(std::move(literal));
}

hir::HirFunctionId HirLowerModern::ensureForeignFunction(std::string_view name,
                                                         const std::vector<types::TypeId> &params,
                                                         const types::TypeId result,
                                                         const bool variadic) {
    const auto interned = interner_.intern(name);
    if (const auto *existing = foreign_fn_by_name_.get(interned))
        return *existing;

    auto &fn          = hir_.addFn(interned);
    fn.return_type    = result;
    fn.isVariadic     = variadic;
    fn.isForeignC     = false;
    fn.hasBody        = false;
    for (const auto param : params) {
        fn.params.push(param);
        fn.param_names.push({});
    }
    const auto id = static_cast<hir::HirFunctionId>(hir_.getFnCount() - 1U);
    foreign_fn_by_name_.insert(interned, id);
    // `addFn` may have reallocated the function array, so the pointer cached
    // while lowering a body must be refreshed before it is used again.
    if (current_fn_ != nullptr)
        current_fn_ = &hir_.getFn(current_fn_index_);
    return id;
}

/// Emits `putchar(byte)` for every byte of `text` into the current block.
void HirLowerModern::emitPutcharLiteral(std::string_view text) {
    const auto putchar_id =
        ensureForeignFunction("putchar", {types_.internInt(types::IntWidth::I32)},
                              types_.internInt(types::IntWidth::I32));
    for (const char byte : text) {
        memory::DynArray<hir::HirExprId> args(arena_);
        memory::DynArray<types::TypeId> arg_types(arena_);
        args.push(emitI32Literal(static_cast<unsigned char>(byte)));
        arg_types.push(types_.internInt(types::IntWidth::I32));
        hir::HirCall call{hir::kInvalidHirExpr, std::move(args), std::move(arg_types)};
        call.resolved_fn = putchar_id;
        current_fn_->blocks[current_block_].insts.push(addExpr(std::move(call)));
    }
}

/// Prints a non-negative `i32` in decimal using only `putchar`. The value is
/// spilled, the largest power of ten not exceeding it is found, and then digits
/// are emitted most-significant first.
void HirLowerModern::emitPrintI32(const hir::HirExprId value) {
    const auto putchar_id =
        ensureForeignFunction("putchar", {types_.internInt(types::IntWidth::I32)},
                              types_.internInt(types::IntWidth::I32));
    const auto i32        = types_.internInt(types::IntWidth::I32);
    const auto value_slot = next_slot_++;
    const auto scale_slot = next_slot_++;
    current_fn_->blocks[current_block_].insts.push(emitSlotAlloca(value_slot, i32));
    current_fn_->blocks[current_block_].insts.push(emitSlotStore(value_slot, value));
    current_fn_->blocks[current_block_].insts.push(emitSlotAlloca(scale_slot, i32));

    const auto emit_putchar = [&](const hir::HirExprId arg) {
        memory::DynArray<hir::HirExprId> args(arena_);
        memory::DynArray<types::TypeId> arg_types(arena_);
        args.push(arg);
        arg_types.push(i32);
        hir::HirCall call{hir::kInvalidHirExpr, std::move(args), std::move(arg_types)};
        call.resolved_fn = putchar_id;
        current_fn_->blocks[current_block_].insts.push(addExpr(std::move(call)));
    };
    const auto i32_binary = [&](const hir::HirExprId lhs, const hir::HirExprId rhs,
                                const hir::HirBinaryOp op) {
        hir::HirBinary binary;
        binary.lhs          = lhs;
        binary.rhs          = rhs;
        binary.op           = op;
        binary.type         = i32;
        binary.operand_type = i32;
        return addExpr(std::move(binary));
    };

    const auto zero_block  = newBlock();
    const auto scale_setup = newBlock();
    const auto scale_head  = newBlock();
    const auto scale_body  = newBlock();
    const auto digit_setup = newBlock();
    const auto digit_head  = newBlock();
    const auto done_block  = newBlock();

    // Zero needs a single digit; the scaling loop below would otherwise emit
    // nothing for it.
    hir::HirBranch is_zero;
    is_zero.cond = i32_binary(emitSlotLoad(value_slot, i32), emitI32Literal(0),
                              hir::HirBinaryOp::Eq);
    is_zero.then_block = static_cast<hir::HirBlockId>(zero_block);
    is_zero.else_block = static_cast<hir::HirBlockId>(scale_setup);
    setTerminator(addExpr(std::move(is_zero)));

    setCurrentBlock(zero_block);
    current_fn_->blocks[zero_block].insts = memory::DynArray<hir::HirExprId>(arena_);
    emit_putchar(emitI32Literal('0'));
    emitJump(done_block);

    // scale = 1; while (scale * 10 <= value) scale = scale * 10;
    setCurrentBlock(scale_setup);
    current_fn_->blocks[scale_setup].insts = memory::DynArray<hir::HirExprId>(arena_);
    current_fn_->blocks[scale_setup].insts.push(emitSlotStore(scale_slot, emitI32Literal(1)));
    emitJump(scale_head);

    setCurrentBlock(scale_head);
    current_fn_->blocks[scale_head].insts = memory::DynArray<hir::HirExprId>(arena_);
    {
        const auto next =
            i32_binary(emitSlotLoad(scale_slot, i32), emitI32Literal(10), hir::HirBinaryOp::Mul);
        const auto next_slot = next_slot_++;
        current_fn_->blocks[scale_head].insts.push(emitSlotAlloca(next_slot, i32));
        current_fn_->blocks[scale_head].insts.push(emitSlotStore(next_slot, next));
        hir::HirBranch more;
        more.cond = i32_binary(emitSlotLoad(next_slot, i32), emitSlotLoad(value_slot, i32),
                               hir::HirBinaryOp::Le);
        more.then_block = static_cast<hir::HirBlockId>(scale_body);
        more.else_block = static_cast<hir::HirBlockId>(digit_setup);
        setTerminator(addExpr(std::move(more)));
        // The body reads the freshly computed scale from `next_slot`.
        setCurrentBlock(scale_body);
        current_fn_->blocks[scale_body].insts = memory::DynArray<hir::HirExprId>(arena_);
        current_fn_->blocks[scale_body].insts.push(
            emitSlotStore(scale_slot, emitSlotLoad(next_slot, i32)));
        emitJump(scale_head);
    }

    setCurrentBlock(digit_setup);
    current_fn_->blocks[digit_setup].insts = memory::DynArray<hir::HirExprId>(arena_);
    emitJump(digit_head);

    setCurrentBlock(digit_head);
    current_fn_->blocks[digit_head].insts = memory::DynArray<hir::HirExprId>(arena_);
    {
        const auto digit = i32_binary(emitSlotLoad(value_slot, i32), emitSlotLoad(scale_slot, i32),
                                      hir::HirBinaryOp::Div);
        emit_putchar(i32_binary(emitI32Literal('0'), digit, hir::HirBinaryOp::Add));
        const auto remainder = i32_binary(emitSlotLoad(value_slot, i32),
                                          emitSlotLoad(scale_slot, i32), hir::HirBinaryOp::Rem);
        current_fn_->blocks[digit_head].insts.push(emitSlotStore(value_slot, remainder));
        const auto next_scale =
            i32_binary(emitSlotLoad(scale_slot, i32), emitI32Literal(10), hir::HirBinaryOp::Div);
        current_fn_->blocks[digit_head].insts.push(emitSlotStore(scale_slot, next_scale));
        hir::HirBranch digit_more;
        digit_more.cond = i32_binary(emitSlotLoad(scale_slot, i32), emitI32Literal(0),
                                     hir::HirBinaryOp::Gt);
        digit_more.then_block = static_cast<hir::HirBlockId>(digit_head);
        digit_more.else_block = static_cast<hir::HirBlockId>(done_block);
        setTerminator(addExpr(std::move(digit_more)));
    }

    setCurrentBlock(done_block);
    current_fn_->blocks[done_block].insts = memory::DynArray<hir::HirExprId>(arena_);
}

hir::HirExprId HirLowerModern::lowerTestAssert(const frontend::Expression &expr) {
    if (expr.operands.empty())
        return hir::kInvalidHirExpr;

    if (!current_fn_is_test_) {
        // Outside a test, `@assert(false)` is a hard runtime failure.
        const auto cond = lowerCondition(expr.operands[0]);
        if (cond == hir::kInvalidHirExpr)
            return hir::kInvalidHirExpr;
        const auto pass_block = newBlock();
        const auto fail_block = newBlock();
        hir::HirBranch branch;
        branch.cond       = cond;
        branch.then_block = static_cast<hir::HirBlockId>(pass_block);
        branch.else_block = static_cast<hir::HirBlockId>(fail_block);
        setTerminator(addExpr(std::move(branch)));

        setCurrentBlock(fail_block);
        current_fn_->blocks[fail_block].insts = memory::DynArray<hir::HirExprId>(arena_);
        hir::HirRuntimePanic panic;
        panic.code = diagnostics::err::Panic;
        setTerminator(addExpr(std::move(panic)));

        setCurrentBlock(pass_block);
        current_fn_->blocks[pass_block].insts = memory::DynArray<hir::HirExprId>(arena_);
        return hir::kInvalidHirExpr;
    }

    // Inside a test: on failure record the static assert id and return it.
    const auto assert_id = ++current_test_assert_id_;
    TestAssertSite site;
    site.test_name = current_test_name_;
    site.assert_id = assert_id;
    site.span      = memory::Span{current_module_ != nullptr ? current_module_->fileId : 0U,
                                  expr.span.start, expr.span.end};
    test_assert_sites_.push_back(site);

    const auto cond = lowerCondition(expr.operands[0]);
    if (cond == hir::kInvalidHirExpr)
        return hir::kInvalidHirExpr;

    const auto pass_block = newBlock();
    const auto fail_block = newBlock();
    hir::HirBranch branch;
    branch.cond       = cond;
    branch.then_block = static_cast<hir::HirBlockId>(pass_block);
    branch.else_block = static_cast<hir::HirBlockId>(fail_block);
    setTerminator(addExpr(std::move(branch)));

    setCurrentBlock(fail_block);
    current_fn_->blocks[fail_block].insts = memory::DynArray<hir::HirExprId>(arena_);
    emitCleanupFrom(0);
    hir::HirRet fail_ret;
    fail_ret.value = emitI32Literal(static_cast<int64_t>(assert_id));
    setTerminator(addExpr(std::move(fail_ret)));

    setCurrentBlock(pass_block);
    current_fn_->blocks[pass_block].insts = memory::DynArray<hir::HirExprId>(arena_);
    return hir::kInvalidHirExpr;
}

hir::HirExprId HirLowerModern::lowerTestLog(const frontend::Expression &expr) {
    if (expr.operands.empty())
        return hir::kInvalidHirExpr;
    // `@log(msg)` writes `msg` to stdout via `puts`. A string literal is
    // materialised as a `*char` literal directly, because a slice-typed
    // literal is not representable in the VM and interpreter lowering. Any
    // other `[]char` value projects its data pointer with `@ptrOf`.
    const auto char_ptr = types_.internPtr(types::kCharType);

    const frontend::Expression *operand_expr = nullptr;
    if (current_module_ != nullptr &&
        expr.operands[0].value <= current_module_->frontend->expressions().size())
        operand_expr =
            &current_module_->frontend->expressions()[expr.operands[0].value - 1U];
    const bool literal_string =
        operand_expr != nullptr && operand_expr->kind == frontend::ExprKind::Literal &&
        operand_expr->text.size() >= 2U && operand_expr->text.front() == '"' &&
        operand_expr->text.back() == '"';

    hir::HirExprId pointer = hir::kInvalidHirExpr;
    if (literal_string) {
        std::string decoded;
        if (!decodeEscapes(
                std::string_view(operand_expr->text.data() + 1U, operand_expr->text.size() - 2U),
                decoded))
            return hir::kInvalidHirExpr;
        hir::HirLiteral literal;
        literal.type    = char_ptr;
        literal.str_val = interner_.intern(std::string_view(decoded));
        pointer         = addExpr(std::move(literal));
    } else {
        const auto message = lowerExpr(expr.operands[0]);
        if (message == hir::kInvalidHirExpr)
            return hir::kInvalidHirExpr;
        const auto message_type = typeOfExpr(expr.operands[0]);
        pointer                 = message;
        if (types_.kindOf(message_type) == types::TypeKind::Slice) {
            hir::HirLayoutIntrinsic ptr_intrinsic;
            ptr_intrinsic.which        = hir::HirLayoutIntrinsic::Which::PtrOf;
            ptr_intrinsic.type         = char_ptr;
            ptr_intrinsic.operand      = message;
            ptr_intrinsic.operand_type = message_type;
            pointer                    = addExpr(std::move(ptr_intrinsic));
        }
    }

    const auto puts_id  = ensureForeignFunction("puts", {char_ptr},
                                                types_.internInt(types::IntWidth::I32));
    if (!test_module_.empty()) {
        const std::string report_name =
            current_test_name_.empty() ? std::string{}
                                       : test_module_ + "::" + current_test_name_;
        emitPutcharLiteral(std::string(common::kTestLogBeginPrefix) + report_name + "\n");
    }

    memory::DynArray<hir::HirExprId> args(arena_);
    memory::DynArray<types::TypeId> arg_types(arena_);
    args.push(pointer);
    arg_types.push(char_ptr);
    hir::HirCall call{hir::kInvalidHirExpr, std::move(args), std::move(arg_types)};
    call.resolved_fn = puts_id;
    current_fn_->blocks[current_block_].insts.push(addExpr(std::move(call)));
    if (!test_module_.empty())
        emitPutcharLiteral(common::kTestLogEndMarker);
    return hir::kInvalidHirExpr;
}

bool HirLowerModern::buildTestDriver() {
    if (test_driver_built_ || test_fn_ids_.empty())
        return true;
    test_driver_built_ = true;

    std::vector<size_t> selected_tests;
    for (size_t index = 0; index < test_fn_ids_.size(); ++index) {
        std::string report_name = test_names_[index];
        if (!test_module_.empty())
            report_name = test_module_ + "::" + report_name;
        if (!test_filter_.empty() && report_name.find(test_filter_) == std::string::npos)
            continue;
        selected_tests.push_back(index);
        test_report_names_.push_back(std::move(report_name));
    }

    auto &fn       = hir_.addFn(interner_.intern("main"));
    fn.return_type = types_.internInt(types::IntWidth::I32);
    fn.hasBody     = true;

    current_fn_             = &hir_.getFn(hir_.getFnCount() - 1U);
    current_fn_index_       = hir_.getFnCount() - 1U;
    current_block_          = 0;
    next_slot_              = 0;
    current_fn_is_test_     = false;
    current_test_name_.clear();
    current_test_assert_id_ = 0;

    fn.blocks.emplace(arena_);
    fn.blocks[0].insts = memory::DynArray<hir::HirExprId>(arena_);

    const auto i32 = types_.internInt(types::IntWidth::I32);
    std::vector<hir::HirSlotId> result_slots;
    result_slots.reserve(selected_tests.size());
    for (size_t selected_index = 0; selected_index < selected_tests.size(); ++selected_index) {
        const size_t index = selected_tests[selected_index];
        if (!test_module_.empty()) {
            emitPutcharLiteral(std::string(common::kTestLogContextPrefix) +
                               test_report_names_[selected_index] + "\n");
        }
        // Call the test function and capture its result slot: 0 on success or
        // the id of the first failing assert.
        memory::DynArray<hir::HirExprId> args(arena_);
        memory::DynArray<types::TypeId> arg_types(arena_);
        hir::HirCall call{hir::kInvalidHirExpr, std::move(args), std::move(arg_types)};
        call.resolved_fn = test_fn_ids_[index];
        const auto result = next_slot_++;
        current_fn_->blocks[current_block_].insts.push(emitSlotAlloca(result, i32));
        current_fn_->blocks[current_block_].insts.push(
            emitSlotStore(result, addExpr(std::move(call))));
        result_slots.push_back(result);
    }

    emitPutcharLiteral("zith-test-results-begin\n");
    for (size_t selected_index = 0; selected_index < selected_tests.size(); ++selected_index) {
        const auto result = result_slots[selected_index];
        emitPutcharLiteral("zith-test ");
        emitPutcharLiteral(test_report_names_[selected_index]);
        emitPutcharLiteral(" ");
        emitPrintI32(emitSlotLoad(result, i32));
        emitPutcharLiteral("\n");
    }
    emitPutcharLiteral("zith-test-results-end\n");

    hir::HirRet ret;
    ret.value = emitI32Literal(0);
    setTerminator(addExpr(std::move(ret)));

    current_fn_       = nullptr;
    current_fn_index_ = 0;
    return true;
}

} // namespace modern
} // namespace zith::sema

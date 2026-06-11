#include "../test-common.hpp"
#include "ast/ast-builder.hpp"
#include "ast/ast-ids.hpp"
#include "ast/ast-nodes.hpp"
#include "memory/arena.hpp"

using namespace zith::ast;
using zith::memory::Arena;
using zith::memory::DynArray;

static void test_struct_decl() {
    Arena arena;
    AstBuilder builder(arena);

    DynArray<std::pair<std::string_view, uint32_t>> fields(arena);
    fields.push({"x", 0u});
    fields.push({"y", 0u});
    auto decl = builder.structDecl("Point", std::move(fields));
    CHECK(decl != kInvalidDecl, "structDecl returns valid DeclId");

    auto &node = builder.getDecl(decl);
    CHECK(std::holds_alternative<StructDeclNode>(node), "structDecl node is StructDeclNode");
    if (!std::holds_alternative<StructDeclNode>(node))
        return;

    auto &s = std::get<StructDeclNode>(node);
    CHECK(s.name == "Point", "struct name is Point");
    CHECK_EQ(s.fields.size(), size_t(2), "struct has 2 fields");
}

static void test_if_expr() {
    Arena arena;
    AstBuilder builder(arena);

    auto cond   = builder.ident("true");
    auto then_b = builder.block(DynArray<StmtId>(arena));
    auto e      = builder.ifExpr(cond, then_b);
    CHECK(e != kInvalidExpr, "ifExpr returns valid ExprId");

    auto &node = builder.getExpr(e);
    CHECK(std::holds_alternative<IfNode>(node), "ifExpr node is IfNode");
    if (!std::holds_alternative<IfNode>(node))
        return;

    auto &iff = std::get<IfNode>(node);
    CHECK(iff.cond != kInvalidExpr, "if has condition");
    CHECK(iff.then_branch != kInvalidExpr, "if has then branch");
    CHECK_EQ(iff.else_branch, kInvalidExpr, "if has no else branch");
}

static void test_if_else_expr() {
    Arena arena;
    AstBuilder builder(arena);

    auto cond   = builder.ident("cond");
    auto then_b = builder.block(DynArray<StmtId>(arena));
    auto else_b = builder.block(DynArray<StmtId>(arena));
    auto e      = builder.ifExpr(cond, then_b, else_b);
    CHECK(e != kInvalidExpr, "ifExpr with else returns valid ExprId");

    auto &node = builder.getExpr(e);
    CHECK(std::holds_alternative<IfNode>(node), "if-else node is IfNode");
    auto &iff = std::get<IfNode>(node);
    CHECK(iff.else_branch != kInvalidExpr, "if-else has else branch");
}

static void test_call_expr() {
    Arena arena;
    AstBuilder builder(arena);

    auto callee = builder.ident("foo");
    auto arg    = builder.litExpr(LitKind::Int, "1");
    DynArray<ExprId> args(arena);
    args.push(arg);
    auto e = builder.call(callee, std::move(args));
    CHECK(e != kInvalidExpr, "call returns valid ExprId");

    auto &node = builder.getExpr(e);
    CHECK(std::holds_alternative<CallNode>(node), "call node is CallNode");
    if (!std::holds_alternative<CallNode>(node))
        return;

    auto &call = std::get<CallNode>(node);
    CHECK_EQ(call.args.size(), size_t(1), "call has 1 arg");
}

static void test_block_with_stmts() {
    Arena arena;
    AstBuilder builder(arena);

    auto s1 = builder.letStmt("a", false, builder.litExpr(LitKind::Int, "1"));
    auto s2 = builder.retStmt(builder.ident("a"));
    DynArray<StmtId> stmts(arena);
    stmts.push(s1);
    stmts.push(s2);
    auto block = builder.block(std::move(stmts));
    CHECK(block != kInvalidExpr, "block returns valid ExprId");

    auto &node = builder.getExpr(block);
    CHECK(std::holds_alternative<BlockNode>(node), "block node is BlockNode");
    if (!std::holds_alternative<BlockNode>(node))
        return;

    auto &b = std::get<BlockNode>(node);
    CHECK_EQ(b.stmts.size(), size_t(2), "block has 2 stmts");
    CHECK_EQ(b.trailing, kInvalidExpr, "block has no trailing expr");
}

static void test_assign_stmt() {
    Arena arena;
    AstBuilder builder(arena);

    auto target = builder.ident("x");
    auto value  = builder.litExpr(LitKind::Int, "5");
    auto s      = builder.assign(target, value);
    CHECK(s != kInvalidStmt, "assign returns valid StmtId");

    auto &node = builder.getStmt(s);
    CHECK(std::holds_alternative<AssignNode>(node), "assign node is AssignNode");
}

int main() {
    std::printf("parser-stmt tests\n");
    std::printf("====================\n\n");

    test_struct_decl();
    test_if_expr();
    test_if_else_expr();
    test_call_expr();
    test_block_with_stmts();
    test_assign_stmt();

    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}

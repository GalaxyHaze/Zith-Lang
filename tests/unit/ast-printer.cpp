#include "../test-common.hpp"
#include "ast/ast-builder.hpp"
#include "ast/ast-ids.hpp"
#include "ast/ast-nodes.hpp"
#include "ast/ast-printer.hpp"
#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"

#include <cstdio>

using namespace zith::ast;
using zith::memory::Arena;
using zith::memory::DynArray;

// Helper: open /dev/null FILE* for printer output
static FILE *devnull() {
#ifdef _WIN32
    FILE *f = fopen("NUL", "w");
#else
    FILE *f = fopen("/dev/null", "w");
#endif
    CHECK(f != nullptr, "fopen null device succeeded");
    return f;
}

// ── Print each expression node type ─────────────────────────────

static void test_print_expr_literal() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);
    ProgramNode program(arena);
    (void)program;

    // Build: a literal expression
    auto lit = builder.litExpr(LitKind::Int, "42");
    // Wrap in a fn so it's part of a program
    auto body = builder.block(DynArray<StmtId>(arena), lit);
    auto fn   = builder.fnDecl("test_fn", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with literal expression does not crash");
}

static void test_print_expr_ident() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto ident = builder.ident("x");
    auto body  = builder.block(DynArray<StmtId>(arena), ident);
    auto fn    = builder.fnDecl("test_fn", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with ident expression does not crash");
}

static void test_print_expr_binary() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto lhs = builder.litExpr(LitKind::Int, "1");
    auto rhs = builder.litExpr(LitKind::Int, "2");
    auto bin = builder.binary(lhs, BinaryOp::Add, rhs);
    auto body = builder.block(DynArray<StmtId>(arena), bin);
    auto fn   = builder.fnDecl("test_fn", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with binary expression does not crash");
}

static void test_print_expr_unary() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto operand = builder.litExpr(LitKind::Int, "5");
    auto un = builder.unary(UnaryOp::Neg, operand);
    auto body = builder.block(DynArray<StmtId>(arena), un);
    auto fn   = builder.fnDecl("test_fn", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with unary expression does not crash");
}

static void test_print_expr_call() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<ExprId> args(arena);
    args.push(builder.litExpr(LitKind::Int, "1"));
    args.push(builder.litExpr(LitKind::Int, "2"));
    auto callee = builder.ident("add");
    auto call   = builder.call(callee, std::move(args));
    auto body   = builder.block(DynArray<StmtId>(arena), call);
    auto fn     = builder.fnDecl("test_fn", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with call expression does not crash");
}

static void test_print_expr_field() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto obj   = builder.ident("point");
    auto f     = builder.field(obj, "x");
    auto body  = builder.block(DynArray<StmtId>(arena), f);
    auto fn    = builder.fnDecl("test_fn", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with field expression does not crash");
}

static void test_print_expr_index() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto obj   = builder.ident("arr");
    auto idx   = builder.litExpr(LitKind::Int, "0");
    auto index = builder.index(obj, idx);
    auto body  = builder.block(DynArray<StmtId>(arena), index);
    auto fn    = builder.fnDecl("test_fn", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with index expression does not crash");
}

static void test_print_expr_block() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto blk = builder.block(DynArray<StmtId>(arena));
    auto fn  = builder.fnDecl("test_fn", DynArray<std::string_view>(arena), blk);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with block expression does not crash");
}

static void test_print_expr_if() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto cond  = builder.litExpr(LitKind::Bool, "true");
    auto then_ = builder.litExpr(LitKind::Int, "1");
    auto else_ = builder.litExpr(LitKind::Int, "2");
    auto ife   = builder.ifExpr(cond, then_, else_);
    auto body  = builder.block(DynArray<StmtId>(arena), ife);
    auto fn    = builder.fnDecl("test_fn", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with if expression does not crash");
}

static void test_print_expr_while() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto cond  = builder.litExpr(LitKind::Bool, "true");
    auto body  = builder.block(DynArray<StmtId>(arena));
    auto wh    = builder.whileExpr(cond, body);
    auto fn    = builder.fnDecl("test_fn", DynArray<std::string_view>(arena), wh);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with while expression does not crash");
}

// ── Print each statement node type ──────────────────────────────

static void test_print_stmt_let() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto init = builder.litExpr(LitKind::Int, "42");
    auto stmt = builder.letStmt("x", false, init);
    DynArray<StmtId> stmts(arena);
    stmts.push(stmt);
    auto body = builder.block(std::move(stmts));
    auto fn   = builder.fnDecl("test_fn", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with let statement does not crash");
}

static void test_print_stmt_assign() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto target = builder.ident("x");
    auto value  = builder.litExpr(LitKind::Int, "5");
    auto stmt   = builder.assign(target, value);
    DynArray<StmtId> stmts(arena);
    stmts.push(stmt);
    auto body = builder.block(std::move(stmts));
    auto fn   = builder.fnDecl("test_fn", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with assign statement does not crash");
}

static void test_print_stmt_ret() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto value = builder.litExpr(LitKind::Int, "7");
    auto stmt  = builder.retStmt(value);
    DynArray<StmtId> stmts(arena);
    stmts.push(stmt);
    auto body = builder.block(std::move(stmts));
    auto fn   = builder.fnDecl("test_fn", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with return statement does not crash");
}

static void test_print_stmt_ret_void() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto stmt = builder.retStmt();
    DynArray<StmtId> stmts(arena);
    stmts.push(stmt);
    auto body = builder.block(std::move(stmts));
    auto fn   = builder.fnDecl("test_fn", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with void return statement does not crash");
}

// ── Print each declaration node type ────────────────────────────

static void test_print_decl_fn() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<std::string_view> params(arena);
    params.push("a");
    params.push("b");
    auto body = builder.block(DynArray<StmtId>(arena));
    auto fn   = builder.fnDecl("my_fn", std::move(params), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with fn declaration does not crash");
}

static void test_print_decl_struct() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<StructField> fields(arena);
    fields.push(StructField{"x", 0});
    fields.push(StructField{"y", 0});
    auto s = builder.structDecl("Point", std::move(fields));
    decls.push(s);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with struct declaration does not crash");
}

static void test_print_decl_enum() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<EnumVariant> variants(arena);
    DynArray<StructField> empty_fields(arena);
    variants.push(EnumVariant{"VariantA", DynArray<StructField>(arena), -1});
    variants.push(EnumVariant{"VariantB", DynArray<StructField>(arena), 0});
    auto e = builder.enumDecl("MyEnum", std::move(variants));
    decls.push(e);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with enum declaration does not crash");
}

static void test_print_decl_union() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<UnionVariant> variants(arena);
    variants.push(UnionVariant{"IntVal", 0});
    variants.push(UnionVariant{"FloatVal", 0});
    auto u = builder.unionDecl("MyUnion", std::move(variants));
    decls.push(u);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with union declaration does not crash");
}

static void test_print_decl_component() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto comp = builder.componentDecl("MyComponent");
    decls.push(comp);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with component declaration does not crash");
}

static void test_print_decl_trait() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<TraitMethod> methods(arena);
    DynArray<std::string_view> params(arena);
    params.push("x");
    methods.push(TraitMethod{"method_a", std::move(params)});
    auto t = builder.traitDecl("MyTrait", std::move(methods));
    decls.push(t);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with trait declaration does not crash");
}

static void test_print_decl_interface() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<TraitMethod> methods(arena);
    methods.push(TraitMethod{"doSomething", DynArray<std::string_view>(arena)});
    auto iface = builder.interfaceDecl("MyInterface", std::move(methods));
    decls.push(iface);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with interface declaration does not crash");
}

static void test_print_decl_import() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<std::string_view> path(arena);
    path.push("std");
    path.push("io");
    path.push("console");
    auto imp = builder.importDecl(std::move(path), "con", false, false, 1);
    decls.push(imp);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with import declaration does not crash");
}

// ── Edge cases ──────────────────────────────────────────────────

static void test_print_empty_program() {
    Arena arena;
    AstBuilder builder(arena);
    ProgramNode prog(arena);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with empty program does not crash");
}

static void test_print_invalid_expr_ref() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    // Build a fn that references kInvalidExpr as body (edge case)
    auto body = builder.block(DynArray<StmtId>(arena), kInvalidExpr);
    auto fn   = builder.fnDecl("test_fn", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FILE *out = devnull();
    printAST(prog, builder, out);
    fclose(out);
    CHECK(true, "printAST with kInvalidExpr reference does not crash");
}

int main() {
    std::printf("ast-printer tests\n");
    std::printf("===================\n\n");

    // Expression nodes
    test_print_expr_literal();
    test_print_expr_ident();
    test_print_expr_binary();
    test_print_expr_unary();
    test_print_expr_call();
    test_print_expr_field();
    test_print_expr_index();
    test_print_expr_block();
    test_print_expr_if();
    test_print_expr_while();

    // Statement nodes
    test_print_stmt_let();
    test_print_stmt_assign();
    test_print_stmt_ret();
    test_print_stmt_ret_void();

    // Declaration nodes
    test_print_decl_fn();
    test_print_decl_struct();
    test_print_decl_enum();
    test_print_decl_union();
    test_print_decl_component();
    test_print_decl_trait();
    test_print_decl_interface();
    test_print_decl_import();

    // Edge cases
    test_print_empty_program();
    test_print_invalid_expr_ref();

    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}

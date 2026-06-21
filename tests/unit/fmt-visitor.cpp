#include "formatter/fmt-visitor.hpp"
#include "../test-common.hpp"
#include "ast/ast-builder.hpp"
#include "ast/ast-ids.hpp"
#include "ast/ast-nodes.hpp"
#include "ast/type-expr.hpp"
#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"

using namespace zith::ast;
using namespace zith::formatter;
using zith::memory::Arena;
using zith::memory::DynArray;

// ── Fn declarations ───────────────────────────────────────────

static void test_fmt_fn_no_body() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto fn = builder.fnDecl("foo", DynArray<std::string_view>(arena), kInvalidExpr);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn foo();\n\n";
    CHECK_EQ(out, expected, "fmt: fn decl without body");
}

static void test_fmt_fn_with_body() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<std::string_view> params(arena);
    params.push("x");
    auto lit = builder.litExpr(LitKind::Int, "1");
    auto body = builder.block(DynArray<StmtId>(arena), lit);
    auto fn = builder.fnDecl("foo", std::move(params), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn foo(x) {\n    1\n}\n\n";
    CHECK_EQ(out, expected, "fmt: fn decl with body and param");
}

static void test_fmt_fn_with_return_type() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto ret_type = builder.builtinExpr(BuiltinType::I32);
    DynArray<FnParam> params(arena);
    params.push(FnParam{"x", builder.builtinExpr(BuiltinType::I32)});
    auto lit = builder.litExpr(LitKind::Int, "0");
    auto body = builder.block(DynArray<StmtId>(arena), lit);
    auto fn = builder.fnDecl("add", DynArray<GenericParam>(arena), std::move(params), ret_type, body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn add(x: i32): i32 {\n    0\n}\n\n";
    CHECK_EQ(out, expected, "fmt: fn decl with return type");
}

static void test_fmt_fn_generic() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<TypeExprId> bounds(arena);
    bounds.push(builder.builtinExpr(BuiltinType::I32));
    DynArray<GenericParam> generics(arena);
    generics.push(GenericParam{"T", std::move(bounds)});

    DynArray<FnParam> params(arena);
    auto path_segs = DynArray<std::string_view>(arena);
    path_segs.push("T");
    params.push(FnParam{"x", builder.pathExpr(std::move(path_segs))});
    auto body = builder.block(DynArray<StmtId>(arena), builder.litExpr(LitKind::Int, "0"));
    auto fn = builder.fnDecl("id", std::move(generics), std::move(params), kInvalidTypeExpr, body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn id<T: i32>(x: T) {\n    0\n}\n\n";
    CHECK_EQ(out, expected, "fmt: fn decl with generic params");
}

// ── Struct / Enum / Union declarations ────────────────────────

static void test_fmt_struct() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<StructField> fields(arena);
    fields.push(StructField{"x", builder.builtinExpr(BuiltinType::I32)});
    fields.push(StructField{"y", builder.builtinExpr(BuiltinType::F64)});
    auto s = builder.structDecl("Point", std::move(fields));
    decls.push(s);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "struct Point {\n    x: i32,\n    y: f64,\n}\n\n";
    CHECK_EQ(out, expected, "fmt: struct with fields");
}

static void test_fmt_struct_empty() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto s = builder.structDecl("Empty", DynArray<StructField>(arena));
    decls.push(s);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "struct Empty;\n\n";
    CHECK_EQ(out, expected, "fmt: empty struct");
}

static void test_fmt_enum() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<EnumVariant> variants(arena);
    variants.push(EnumVariant{"None", DynArray<StructField>(arena), 0});
    DynArray<StructField> coord_fields(arena);
    coord_fields.push(StructField{"x", builder.builtinExpr(BuiltinType::I32)});
    coord_fields.push(StructField{"y", builder.builtinExpr(BuiltinType::I32)});
    variants.push(EnumVariant{"Coord", std::move(coord_fields), -1});
    auto e = builder.enumDecl("Shape", std::move(variants));
    decls.push(e);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "enum Shape {\n    None = 0,\n    Coord(x: i32, y: i32),\n}\n\n";
    CHECK_EQ(out, expected, "fmt: enum with variants");
}

static void test_fmt_enum_empty() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto e = builder.enumDecl("Empty", DynArray<EnumVariant>(arena));
    decls.push(e);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "enum Empty;\n\n";
    CHECK_EQ(out, expected, "fmt: empty enum");
}

static void test_fmt_union() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<UnionVariant> variants(arena);
    variants.push(UnionVariant{"IntVal", builder.builtinExpr(BuiltinType::I64)});
    variants.push(UnionVariant{"FloatVal", builder.builtinExpr(BuiltinType::F64)});
    auto u = builder.unionDecl("Num", std::move(variants));
    decls.push(u);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "union Num {\n    IntVal: i64,\n    FloatVal: f64,\n}\n\n";
    CHECK_EQ(out, expected, "fmt: union with variants");
}

static void test_fmt_union_empty() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto u = builder.unionDecl("Empty", DynArray<UnionVariant>(arena));
    decls.push(u);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "union Empty;\n\n";
    CHECK_EQ(out, expected, "fmt: empty union");
}

// ── Trait / Interface / Component ─────────────────────────────

static void test_fmt_trait() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<TraitMethod> methods(arena);
    DynArray<std::string_view> tparams(arena);
    tparams.push("self");
    methods.push(TraitMethod{"do_thing", std::move(tparams)});
    auto t = builder.traitDecl("MyTrait", std::move(methods));
    decls.push(t);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "trait MyTrait {\n    fn do_thing(self);\n}\n\n";
    CHECK_EQ(out, expected, "fmt: trait with methods");
}

static void test_fmt_trait_empty() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto t = builder.traitDecl("Empty", DynArray<TraitMethod>(arena));
    decls.push(t);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "trait Empty;\n\n";
    CHECK_EQ(out, expected, "fmt: empty trait");
}

static void test_fmt_interface() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<TraitMethod> methods(arena);
    DynArray<std::string_view> iparams(arena);
    iparams.push("x");
    iparams.push("y");
    methods.push(TraitMethod{"add", std::move(iparams)});
    auto iface = builder.interfaceDecl("Adder", std::move(methods));
    decls.push(iface);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "interface Adder {\n    fn add(x, y);\n}\n\n";
    CHECK_EQ(out, expected, "fmt: interface with methods");
}

static void test_fmt_component() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto c = builder.componentDecl("MyComp");
    decls.push(c);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "component MyComp;\n\n";
    CHECK_EQ(out, expected, "fmt: component decl");
}

// ── Imports ───────────────────────────────────────────────────

static void test_fmt_import() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<std::string_view> path(arena);
    path.push("std");
    path.push("io");
    auto imp = builder.importDecl(std::move(path));
    decls.push(imp);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "import std::io;\n\n";
    CHECK_EQ(out, expected, "fmt: import decl");
}

static void test_fmt_from_import() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<std::string_view> path(arena);
    path.push("std");
    path.push("io");
    auto imp = builder.importDecl(std::move(path), "IO", true, false);
    decls.push(imp);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "from std::io import IO;\n\n";
    CHECK_EQ(out, expected, "fmt: from ... import decl");
}

static void test_fmt_import_export() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<std::string_view> path(arena);
    path.push("foo");
    auto imp = builder.importDecl(std::move(path), {}, false, true);
    decls.push(imp);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "import foo export;\n\n";
    CHECK_EQ(out, expected, "fmt: import export decl");
}

// ── Statements ────────────────────────────────────────────────

static void test_fmt_let() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<StmtId> stmts(arena);
    stmts.push(builder.letStmt("x", false, builder.litExpr(LitKind::Int, "42")));
    auto body = builder.block(std::move(stmts));
    auto fn = builder.fnDecl("main", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn main() {\n    let x = 42;\n}\n\n";
    CHECK_EQ(out, expected, "fmt: let statement");
}

static void test_fmt_var() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<StmtId> stmts(arena);
    stmts.push(builder.letStmt("x", true, builder.litExpr(LitKind::Int, "0")));
    auto body = builder.block(std::move(stmts));
    auto fn = builder.fnDecl("main", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn main() {\n    var x = 0;\n}\n\n";
    CHECK_EQ(out, expected, "fmt: var statement");
}

static void test_fmt_let_typed() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<StmtId> stmts(arena);
    auto name_arr = DynArray<std::string_view>(arena);
    name_arr.push("name");
    stmts.push(builder.letStmt(std::move(name_arr), false,
                               builder.builtinExpr(BuiltinType::I32), kInvalidExpr));
    auto body = builder.block(std::move(stmts));
    auto fn = builder.fnDecl("main", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn main() {\n    let name: i32;\n}\n\n";
    CHECK_EQ(out, expected, "fmt: let with type annotation");
}

static void test_fmt_assign() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<StmtId> stmts(arena);
    auto target = builder.ident("x");
    stmts.push(builder.assign(target, builder.litExpr(LitKind::Int, "10")));
    auto body = builder.block(std::move(stmts));
    auto fn = builder.fnDecl("main", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn main() {\n    x = 10;\n}\n\n";
    CHECK_EQ(out, expected, "fmt: assign statement");
}

static void test_fmt_return() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<StmtId> stmts(arena);
    stmts.push(builder.retStmt(builder.litExpr(LitKind::Int, "99")));
    auto body = builder.block(std::move(stmts));
    auto fn = builder.fnDecl("main", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn main() {\n    return 99;\n}\n\n";
    CHECK_EQ(out, expected, "fmt: return with value");
}

static void test_fmt_return_void() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<StmtId> stmts(arena);
    stmts.push(builder.retStmt());
    auto body = builder.block(std::move(stmts));
    auto fn = builder.fnDecl("main", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn main() {\n    return;\n}\n\n";
    CHECK_EQ(out, expected, "fmt: return void");
}

// ── Expressions: binary / unary / precedence ──────────────────

static void test_fmt_binary_add() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto lhs = builder.litExpr(LitKind::Int, "1");
    auto rhs = builder.litExpr(LitKind::Int, "2");
    auto bin = builder.binary(lhs, BinaryOp::Add, rhs);
    auto body = builder.block(DynArray<StmtId>(arena), bin);
    auto fn = builder.fnDecl("main", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn main() {\n    1 + 2\n}\n\n";
    CHECK_EQ(out, expected, "fmt: binary add");
}

static void test_fmt_binary_precedence_keep() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    // (1 + 2) * 3  → parens needed because + has lower prec than *
    auto add = builder.binary(builder.litExpr(LitKind::Int, "1"), BinaryOp::Add,
                              builder.litExpr(LitKind::Int, "2"));
    auto mul = builder.binary(add, BinaryOp::Mul, builder.litExpr(LitKind::Int, "3"));
    auto body = builder.block(DynArray<StmtId>(arena), mul);
    auto fn = builder.fnDecl("main", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn main() {\n    (1 + 2) * 3\n}\n\n";
    CHECK_EQ(out, expected, "fmt: (a + b) * c preserves parens");
}

static void test_fmt_binary_precedence_no_parens() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    // 1 + 2 * 3  → no parens needed because * has higher prec
    auto mul = builder.binary(builder.litExpr(LitKind::Int, "2"), BinaryOp::Mul,
                              builder.litExpr(LitKind::Int, "3"));
    auto add = builder.binary(builder.litExpr(LitKind::Int, "1"), BinaryOp::Add, mul);
    auto body = builder.block(DynArray<StmtId>(arena), add);
    auto fn = builder.fnDecl("main", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn main() {\n    1 + 2 * 3\n}\n\n";
    CHECK_EQ(out, expected, "fmt: a + b * c omits parens");
}

static void test_fmt_binary_or_and() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    // (a or b) and c  → parens needed because or has lower prec than and
    auto a = builder.ident("a");
    auto b = builder.ident("b");
    auto c = builder.ident("c");
    auto or_expr = builder.binary(a, BinaryOp::Or, b);
    auto and_expr = builder.binary(or_expr, BinaryOp::And, c);
    auto body = builder.block(DynArray<StmtId>(arena), and_expr);
    auto fn = builder.fnDecl("main", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn main() {\n    (a or b) and c\n}\n\n";
    CHECK_EQ(out, expected, "fmt: (a or b) and c preserves parens");
}

static void test_fmt_unary_neg_binary() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    // -(1 + 2)  → parens needed because unary has higher prec
    auto add = builder.binary(builder.litExpr(LitKind::Int, "1"), BinaryOp::Add,
                              builder.litExpr(LitKind::Int, "2"));
    auto neg = builder.unary(UnaryOp::Neg, add);
    auto body = builder.block(DynArray<StmtId>(arena), neg);
    auto fn = builder.fnDecl("main", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn main() {\n    -(1 + 2)\n}\n\n";
    CHECK_EQ(out, expected, "fmt: -(a + b) preserves parens");
}

static void test_fmt_not_precedence() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    // not a == b → should be (not a) == b since not has higher prec
    // Actually, let's test: not (a == b), which needs parens
    auto a = builder.ident("a");
    auto b = builder.ident("b");
    auto eq = builder.binary(a, BinaryOp::Eq, b);
    auto not_expr = builder.unary(UnaryOp::Not, eq);
    auto body = builder.block(DynArray<StmtId>(arena), not_expr);
    auto fn = builder.fnDecl("main", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn main() {\n    not (a == b)\n}\n\n";
    CHECK_EQ(out, expected, "fmt: not (a == b) preserves parens");
}

static void test_fmt_comparison_non_assoc() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    // (a == b) == c  → parens for non-assoc comparison
    auto a = builder.ident("a");
    auto b = builder.ident("b");
    auto c = builder.ident("c");
    auto inner = builder.binary(a, BinaryOp::Eq, b);
    auto outer = builder.binary(inner, BinaryOp::Eq, c);
    auto body = builder.block(DynArray<StmtId>(arena), outer);
    auto fn = builder.fnDecl("main", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn main() {\n    (a == b) == c\n}\n\n";
    CHECK_EQ(out, expected, "fmt: chain of == adds parens (non-assoc)");
}

// ── Expressions: call, field, index, if, while, block ─────────

static void test_fmt_call() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<ExprId> args(arena);
    args.push(builder.litExpr(LitKind::Int, "1"));
    args.push(builder.litExpr(LitKind::Int, "2"));
    auto callee = builder.ident("add");
    auto call = builder.call(callee, std::move(args));
    auto body = builder.block(DynArray<StmtId>(arena), call);
    auto fn = builder.fnDecl("main", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn main() {\n    add(1, 2)\n}\n\n";
    CHECK_EQ(out, expected, "fmt: call expression");
}

static void test_fmt_field() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto obj = builder.ident("point");
    auto f = builder.field(obj, "x");
    auto body = builder.block(DynArray<StmtId>(arena), f);
    auto fn = builder.fnDecl("main", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn main() {\n    point.x\n}\n\n";
    CHECK_EQ(out, expected, "fmt: field access");
}

static void test_fmt_index() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto arr = builder.ident("arr");
    auto idx = builder.index(arr, builder.litExpr(LitKind::Int, "0"));
    auto body = builder.block(DynArray<StmtId>(arena), idx);
    auto fn = builder.fnDecl("main", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn main() {\n    arr[0]\n}\n\n";
    CHECK_EQ(out, expected, "fmt: index expression");
}

static void test_fmt_if() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto cond = builder.binary(builder.ident("x"), BinaryOp::Gt, builder.litExpr(LitKind::Int, "0"));
    auto then_body = builder.block(DynArray<StmtId>(arena), builder.litExpr(LitKind::Int, "1"));
    auto if_expr = builder.ifExpr(cond, then_body);
    auto body = builder.block(DynArray<StmtId>(arena), if_expr);
    auto fn = builder.fnDecl("main", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn main() {\n    if (x > 0) {\n        1\n    }\n}\n\n";
    CHECK_EQ(out, expected, "fmt: if expression");
}

static void test_fmt_if_else() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto cond = builder.ident("flag");
    auto then_body = builder.litExpr(LitKind::Int, "1");
    auto else_body = builder.litExpr(LitKind::Int, "2");
    auto if_expr = builder.ifExpr(cond, then_body, else_body);
    auto body = builder.block(DynArray<StmtId>(arena), if_expr);
    auto fn = builder.fnDecl("main", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn main() {\n    if (flag) 1 else 2\n}\n\n";
    CHECK_EQ(out, expected, "fmt: if-else expression");
}

static void test_fmt_while() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto cond = builder.binary(builder.ident("x"), BinaryOp::Lt, builder.litExpr(LitKind::Int, "10"));
    auto wbody = builder.block(DynArray<StmtId>(arena), builder.litExpr(LitKind::Int, "0"));
    auto w = builder.whileExpr(cond, wbody);
    auto body = builder.block(DynArray<StmtId>(arena), w);
    auto fn = builder.fnDecl("main", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn main() {\n    while (x < 10) {\n        0\n    }\n}\n\n";
    CHECK_EQ(out, expected, "fmt: while expression");
}

static void test_fmt_range() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto range = builder.range(builder.litExpr(LitKind::Int, "0"), builder.litExpr(LitKind::Int, "10"));
    auto body = builder.block(DynArray<StmtId>(arena), range);
    auto fn = builder.fnDecl("main", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn main() {\n    0..10\n}\n\n";
    CHECK_EQ(out, expected, "fmt: range expression");
}

// ── Edge cases ────────────────────────────────────────────────

static void test_fmt_empty_program() {
    Arena arena;
    AstBuilder builder(arena);
    ProgramNode prog(arena);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    CHECK_EQ(out, "\n", "fmt: empty program produces newline");
}

static void test_fmt_multiple_decls() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    auto foo = builder.fnDecl("foo", DynArray<std::string_view>(arena), kInvalidExpr);
    auto bar = builder.fnDecl("bar", DynArray<std::string_view>(arena), kInvalidExpr);
    decls.push(foo);
    decls.push(bar);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn foo();\n\nfn bar();\n\n";
    CHECK_EQ(out, expected, "fmt: multiple declarations separated by blank line");
}

static void test_fmt_block_with_stmts_and_trailing() {
    Arena arena;
    AstBuilder builder(arena);
    DynArray<DeclId> decls(arena);

    DynArray<StmtId> stmts(arena);
    stmts.push(builder.letStmt("x", false, builder.litExpr(LitKind::Int, "1")));
    auto body = builder.block(std::move(stmts), builder.ident("x"));
    auto fn = builder.fnDecl("main", DynArray<std::string_view>(arena), body);
    decls.push(fn);
    ProgramNode prog(arena);
    prog.decls = std::move(decls);

    FmtVisitor visitor(builder, prog);
    visitor.format();
    std::string out = visitor.result();

    std::string expected = "fn main() {\n    let x = 1;\n    x\n}\n\n";
    CHECK_EQ(out, expected, "fmt: block with stmts and trailing expr");
}

// ── Runner ────────────────────────────────────────────────────

static void test_fmt_visitor() {
    test_fmt_fn_no_body();
    test_fmt_fn_with_body();
    test_fmt_fn_with_return_type();
    test_fmt_fn_generic();

    test_fmt_struct();
    test_fmt_struct_empty();
    test_fmt_enum();
    test_fmt_enum_empty();
    test_fmt_union();
    test_fmt_union_empty();

    test_fmt_trait();
    test_fmt_trait_empty();
    test_fmt_interface();
    test_fmt_component();

    test_fmt_import();
    test_fmt_from_import();
    test_fmt_import_export();

    test_fmt_let();
    test_fmt_var();
    test_fmt_let_typed();
    test_fmt_assign();
    test_fmt_return();
    test_fmt_return_void();

    test_fmt_binary_add();
    test_fmt_binary_precedence_keep();
    test_fmt_binary_precedence_no_parens();
    test_fmt_binary_or_and();
    test_fmt_unary_neg_binary();
    test_fmt_not_precedence();
    test_fmt_comparison_non_assoc();

    test_fmt_call();
    test_fmt_field();
    test_fmt_index();
    test_fmt_if();
    test_fmt_if_else();
    test_fmt_while();
    test_fmt_range();

    test_fmt_empty_program();
    test_fmt_multiple_decls();
    test_fmt_block_with_stmts_and_trailing();
}

TEST_MAIN(fmt_visitor)

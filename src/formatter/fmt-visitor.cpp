#include "formatter/fmt-visitor.hpp"
#include "common/overloaded.hpp"

#include <cstdio>

namespace zith::formatter {

FmtVisitor::FmtVisitor(const ast::AstBuilder &builder, const ast::ProgramNode &program)
    : builder_(builder), program_(program) {}

void FmtVisitor::format() {
    for (size_t i = 0; i < program_.decls.size(); i++) {
        visitDecl(program_.decls[i]);
        if (i + 1 < program_.decls.size())
            newline();
    }
    newline();
}

// ── Indent / emit helpers ─────────────────────────────────────

void FmtVisitor::indent() {
    for (int i = 0; i < indent_; i++) {
        out_ += "    ";
    }
}

void FmtVisitor::newline() {
    out_ += '\n';
}

void FmtVisitor::emit(std::string_view text) {
    out_ += text;
}

// ── Declarations ──────────────────────────────────────────────

void FmtVisitor::visitDecl(ast::DeclId id) {
    const auto &node = builder_.getDecl(id);
    std::visit(common::overloaded{
                   [&](const ast::FnDeclNode &n) { emitFnDecl(n); },
                   [&](const ast::StructDeclNode &n) { emitStructDecl(n); },
                   [&](const ast::EnumDeclNode &n) { emitEnumDecl(n); },
                   [&](const ast::UnionDeclNode &n) { emitUnionDecl(n); },
                   [&](const ast::TraitDeclNode &n) { emitTraitDecl(n); },
                   [&](const ast::InterfaceDeclNode &n) { emitInterfaceDecl(n); },
                   [&](const ast::ComponentDeclNode &n) { emitComponentDecl(n); },
                   [&](const ast::ImportNode &n) { emitImport(n); },
                   [&](const ast::TypeAliasDeclNode &n) { emitTypeAlias(n); },
                   [&](const ast::GlobalDeclNode &n) { emitGlobalDecl(n); },
               },
               node);
}

void FmtVisitor::emitGenericParams(const memory::DynArray<ast::GenericParam> &params) {
    if (params.size() == 0)
        return;
    emit("<");
    for (size_t i = 0; i < params.size(); i++) {
        if (i > 0)
            emit(", ");
        emit(params[i].name);
        if (params[i].bounds.size() > 0) {
            emit(": ");
            for (size_t j = 0; j < params[i].bounds.size(); j++) {
                if (j > 0)
                    emit(" + ");
                emitType(params[i].bounds[j]);
            }
        }
    }
    emit(">");
}

void FmtVisitor::emitFnDecl(const ast::FnDeclNode &node) {
    indent();
    if (node.is_extern)
        emit("extern ");
    emit("fn ");
    emit(node.name);
    emitGenericParams(node.generic_params);

    emit("(");
    emitCommaList(node.params, [&](const ast::FnParam &p) {
        emit(p.name);
        if (p.type != ast::kInvalidTypeExpr) {
            emit(": ");
            emitType(p.type);
        }
    });
    emit(")");

    // Return type (not populated by parser yet, but ready)
    if (node.return_type != ast::kInvalidTypeExpr) {
        emit(": ");
        emitType(node.return_type);
    }

    if (node.body != ast::kInvalidExpr) {
        emit(" ");
        visitExpr(node.body);
    } else {
        emit(";");
    }
    newline();
}

void FmtVisitor::emitStructDecl(const ast::StructDeclNode &node) {
    indent();
    emit("struct ");
    emit(node.name);
    emitGenericParams(node.generic_params);
    if (node.fields.size() == 0) {
        emit(";");
        newline();
        return;
    }
    emitBraceBlock([&] {
        for (size_t i = 0; i < node.fields.size(); i++) {
            indent();
            emit(node.fields[i].name);
            if (node.fields[i].type_expr != ast::kInvalidTypeExpr) {
                emit(": ");
                emitType(node.fields[i].type_expr);
            }
            emit(",");
            newline();
        }
    });
}

void FmtVisitor::emitEnumDecl(const ast::EnumDeclNode &node) {
    indent();
    emit("enum ");
    emit(node.name);
    emitGenericParams(node.generic_params);
    if (node.variants.size() == 0) {
        emit(";");
        newline();
        return;
    }
    emitBraceBlock([&] {
        for (size_t i = 0; i < node.variants.size(); i++) {
            indent();
            emit(node.variants[i].name);
            if (node.variants[i].fields.size() > 0) {
                emit("(");
                emitCommaList(node.variants[i].fields, [&](const ast::StructField &f) {
                    emit(f.name);
                    if (f.type_expr != ast::kInvalidTypeExpr) {
                        emit(": ");
                        emitType(f.type_expr);
                    }
                });
                emit(")");
            }
            if (node.variants[i].discriminant != ast::kInvalidExpr) {
                emit(" = ...");
            }
            emit(",");
            newline();
        }
    });
}

void FmtVisitor::emitUnionDecl(const ast::UnionDeclNode &node) {
    indent();
    emit("union ");
    emit(node.name);
    emitGenericParams(node.generic_params);
    if (node.variants.size() == 0) {
        emit(";");
        newline();
        return;
    }
    emitBraceBlock([&] {
        for (size_t i = 0; i < node.variants.size(); i++) {
            indent();
            emit(node.variants[i].name);
            emit(": ");
            if (node.variants[i].type_expr != ast::kInvalidTypeExpr)
                emitType(node.variants[i].type_expr);
            emit(",");
            newline();
        }
    });
}

void FmtVisitor::emitTraitDecl(const ast::TraitDeclNode &node) {
    indent();
    emit("trait ");
    emit(node.name);
    emitGenericParams(node.generic_params);
    if (node.methods.size() == 0) {
        emit(";");
        newline();
        return;
    }
    emitBraceBlock([&] {
        for (size_t i = 0; i < node.methods.size(); i++) {
            indent();
            emit("fn ");
            emit(node.methods[i].name);
            emit("(");
            emitCommaList(node.methods[i].params, [&](std::string_view p) { emit(p); });
            emit(");");
            newline();
        }
    });
}

void FmtVisitor::emitInterfaceDecl(const ast::InterfaceDeclNode &node) {
    indent();
    emit("interface ");
    emit(node.name);
    emitGenericParams(node.generic_params);
    if (node.methods.size() == 0) {
        emit(";");
        newline();
        return;
    }
    emitBraceBlock([&] {
        for (size_t i = 0; i < node.methods.size(); i++) {
            indent();
            emit("fn ");
            emit(node.methods[i].name);
            emit("(");
            emitCommaList(node.methods[i].params, [&](std::string_view p) { emit(p); });
            emit(");");
            newline();
        }
    });
}

void FmtVisitor::emitComponentDecl(const ast::ComponentDeclNode &node) {
    indent();
    emit("component ");
    emit(node.name);
    emitGenericParams(node.generic_params);
    emit(";");
    newline();
}

void FmtVisitor::emitImport(const ast::ImportNode &node) {
    indent();
    if (node.is_export)
        emit("export ");
    else if (node.is_from)
        emit("from ");
    else
        emit("import ");
    for (size_t i = 0; i < node.path.size(); i++) {
        if (i > 0)
            emit("/");
        emit(node.path[i]);
    }
    if (!node.symbols.empty()) {
        emit(" {");
        for (size_t i = 0; i < node.symbols.size(); i++) {
            if (i > 0)
                emit(",");
            emit(" ");
            emit(node.symbols[i].name);
            if (!node.symbols[i].alias.empty()) {
                emit(" as ");
                emit(node.symbols[i].alias);
            }
        }
        emit(" }");
    }
    if (node.is_from) {
        if (!node.alias.empty()) {
            emit(" as ");
            emit(node.alias);
        }
    } else {
        if (!node.alias.empty()) {
            emit(" as ");
            emit(node.alias);
        }
    }
    newline();
}

void FmtVisitor::emitTypeAlias(const ast::TypeAliasDeclNode &node) {
    indent();
    emit("alias ");
    emit(node.name);
    emitGenericParams(node.generic_params);
    emit(" = ");
    emitType(node.target_type);
    newline();
}

void FmtVisitor::emitGlobalDecl(const ast::GlobalDeclNode &node) {
    indent();
    emit(node.is_const ? "const " : "global ");
    emit(node.name);
    if (node.type_annot != ast::kInvalidTypeExpr) {
        emit(": ");
        emitType(node.type_annot);
    }
    emit(" = ");
    visitExpr(node.init);
    emit(";");
    newline();
}

// ── Statements ────────────────────────────────────────────────

void FmtVisitor::visitStmt(ast::StmtId id) {
    const auto &node = builder_.getStmt(id);
    switch (ast::stmtKind(node)) {
    case ast::StmtKind::Let:
        emitLet(std::get<ast::LetNode>(node));
        break;
    case ast::StmtKind::Assign:
        emitAssign(std::get<ast::AssignNode>(node));
        break;
    case ast::StmtKind::Return:
        emitRet(std::get<ast::RetNode>(node));
        break;
    case ast::StmtKind::Goto: {
        const auto &n = std::get<ast::GotoNode>(node);
        indent();
        emit("jump ");
        emit(n.target);
        emit(";\n");
        break;
    }
    case ast::StmtKind::Marker: {
        const auto &n = std::get<ast::MarkerNode>(node);
        indent();
        emit("marker ");
        emit(n.name);
        emit(" {\n");
        for (auto stmt_id : n.body)
            visitStmt(stmt_id);
        indent();
        emit("}\n");
        break;
    }
    case ast::StmtKind::Expr:
        indent();
        visitExpr(std::get<ast::ExprStmtNode>(node).expr);
        emit(";");
        newline();
        break;
    }
}

void FmtVisitor::emitLet(const ast::LetNode &node) {
    indent();
    if (node.mut)
        emit("var ");
    else
        emit("let ");
    emitCommaList(node.names, [&](std::string_view n) { emit(n); });
    if (node.type_annot != ast::kInvalidTypeExpr) {
        emit(": ");
        emitType(node.type_annot);
    }
    if (node.init != ast::kInvalidExpr) {
        emit(" = ");
        visitExpr(node.init);
    }
    emit(";");
    newline();
}

void FmtVisitor::emitAssign(const ast::AssignNode &node) {
    indent();
    visitExpr(node.target);
    emit(" = ");
    visitExpr(node.value);
    emit(";");
    newline();
}

void FmtVisitor::emitRet(const ast::RetNode &node) {
    indent();
    if (node.value != ast::kInvalidExpr) {
        emit("return ");
        visitExpr(node.value);
    } else {
        emit("return");
    }
    emit(";");
    newline();
}

// ── Expressions ───────────────────────────────────────────────

void FmtVisitor::visitExpr(ast::ExprId id, int parent_prec) {
    if (id == ast::kInvalidExpr) {
        emit("_");
        return;
    }
    const auto &node = builder_.getExpr(id);
    switch (ast::exprKind(node)) {
    case ast::ExprKind::Literal:
        emitLit(std::get<ast::LitValue>(node));
        break;
    case ast::ExprKind::Identifier:
        emitIdent(std::get<ast::IdentNode>(node));
        break;
    case ast::ExprKind::Binary: {
        const auto &n = std::get<ast::BinaryNode>(node);
        int prec      = binopPrecedence(n.op);
        bool need_paren = parent_prec > prec || (parent_prec == prec && !isLeftAssoc(n.op));
        if (need_paren)
            emit("(");
        emitBinary(n);
        if (need_paren)
            emit(")");
        break;
    }
    case ast::ExprKind::Unary: {
        const auto &n = std::get<ast::UnaryNode>(node);
        int prec      = 8;
        if (parent_prec > prec)
            emit("(");
        emitUnary(n);
        if (parent_prec > prec)
            emit(")");
        break;
    }
    case ast::ExprKind::Call:
        emitCall(std::get<ast::CallNode>(node));
        break;
    case ast::ExprKind::Block:
        emitBlock(std::get<ast::BlockNode>(node));
        break;
    case ast::ExprKind::If:
        emitIf(std::get<ast::IfNode>(node));
        break;
    case ast::ExprKind::While:
        emitWhile(std::get<ast::WhileNode>(node));
        break;
    case ast::ExprKind::Field:
        emitField(std::get<ast::FieldNode>(node));
        break;
    case ast::ExprKind::Index:
        emitIndex(std::get<ast::IndexNode>(node));
        break;
    case ast::ExprKind::Range:
        emitRange(std::get<ast::RangeNode>(node));
        break;
    case ast::ExprKind::Unbody:
        break;
    case ast::ExprKind::Intrinsic:
        emitIntrinsic(std::get<ast::IntrinsicNode>(node));
        break;
    case ast::ExprKind::MacroCall:
        emitMacroCall(std::get<ast::MacroCallNode>(node));
        break;
    }
}

void FmtVisitor::emitLit(const ast::LitValue &node) {
    emit(node.raw);
}

void FmtVisitor::emitIdent(const ast::IdentNode &node) {
    emit(node.name);
}

void FmtVisitor::emitBinary(const ast::BinaryNode &node) {
    int prec = binopPrecedence(node.op);

    visitExpr(node.lhs, prec);
    emit(" ");
    emit(binopText(node.op));
    emit(" ");
    visitExpr(node.rhs, prec + 1);
}

void FmtVisitor::emitUnary(const ast::UnaryNode &node) {
    emit(unaryopText(node.op));
    if (node.op == ast::UnaryOp::Not)
        emit(" ");
    visitExpr(node.operand, 8);
}

void FmtVisitor::emitCall(const ast::CallNode &node) {
    visitExpr(node.callee);
    emit("(");
    emitCommaList(node.args, [&](ast::ExprId e) { visitExpr(e); });
    emit(")");
}

void FmtVisitor::emitBlock(const ast::BlockNode &node) {
    if (node.stmts.size() == 0 && node.trailing == ast::kInvalidExpr) {
        emit("{}");
        return;
    }
    emit("{");
    newline();
    indent_++;
    for (size_t i = 0; i < node.stmts.size(); i++) {
        visitStmt(node.stmts[i]);
    }
    if (node.trailing != ast::kInvalidExpr) {
        indent();
        visitExpr(node.trailing);
        newline();
    }
    indent_--;
    indent();
    emit("}");
}

void FmtVisitor::emitIf(const ast::IfNode &node) {
    emit("if (");
    visitExpr(node.cond);
    emit(") ");
    visitExpr(node.then_branch);
    if (node.else_branch != ast::kInvalidExpr) {
        emit(" else ");
        visitExpr(node.else_branch);
    }
}

void FmtVisitor::emitWhile(const ast::WhileNode &node) {
    // while is planned for removal — emit a minimal form for existing code
    emit("while (");
    visitExpr(node.cond);
    emit(") ");
    visitExpr(node.body);
}

void FmtVisitor::emitField(const ast::FieldNode &node) {
    visitExpr(node.object);
    emit(".");
    emit(node.field);
}

void FmtVisitor::emitIndex(const ast::IndexNode &node) {
    visitExpr(node.object);
    emit("[");
    visitExpr(node.index);
    emit("]");
}

void FmtVisitor::emitRange(const ast::RangeNode &node) {
    visitExpr(node.lhs);
    emit("..");
    visitExpr(node.rhs);
}

void FmtVisitor::emitIntrinsic(const ast::IntrinsicNode &node) {
    emit("@");
    // Map kind back to name — keep it simple for now
    switch (node.kind) {
    case ast::IntrinsicKind::Fields:
        emit("fields");
        break;
    case ast::IntrinsicKind::SizeOf:
        emit("sizeOf");
        break;
    case ast::IntrinsicKind::AlignOf:
        emit("alignOf");
        break;
    case ast::IntrinsicKind::HasTrait:
        emit("hasTrait");
        break;
    case ast::IntrinsicKind::Struct:
        emit("struct");
        break;
    case ast::IntrinsicKind::Component:
        emit("component");
        break;
    case ast::IntrinsicKind::Union:
        emit("union");
        break;
    case ast::IntrinsicKind::Enum:
        emit("enum");
        break;
    case ast::IntrinsicKind::Nullable:
        emit("nullable");
        break;
    case ast::IntrinsicKind::Primitive:
        emit("primitive");
        break;
    case ast::IntrinsicKind::Allocate:
        emit("allocate");
        break;
    case ast::IntrinsicKind::Pack:
        emit("pack");
        break;
    case ast::IntrinsicKind::ToStruct:
        emit("toStruct");
        break;
    case ast::IntrinsicKind::ToPack:
        emit("toPack");
        break;
    case ast::IntrinsicKind::AppendField:
        emit("appendField");
        break;
    case ast::IntrinsicKind::RemoveField:
        emit("removeField");
        break;
    case ast::IntrinsicKind::AppendMethod:
        emit("appendMethod");
        break;
    case ast::IntrinsicKind::File:
        emit("file");
        break;
    case ast::IntrinsicKind::Line:
        emit("line");
        break;
    case ast::IntrinsicKind::FnName:
        emit("fnName");
        break;
    case ast::IntrinsicKind::Location:
        emit("location");
        break;
    case ast::IntrinsicKind::Ok:
        emit("ok");
        break;
    case ast::IntrinsicKind::Err:
        emit("err");
        break;
    case ast::IntrinsicKind::OffsetOf:
        emit("offsetOf");
        break;
    }
    if (node.args.size() > 0) {
        emit(" ");
        emitCommaList(node.args, [&](ast::ExprId e) { visitExpr(e); });
    }
}

void FmtVisitor::emitMacroCall(const ast::MacroCallNode &node) {
    emit("@");
    emit(node.name);
    emit("(");
    emitCommaList(node.args, [&](ast::ExprId e) { visitExpr(e); });
    emit(")");
}

// ── Type expressions ──────────────────────────────────────────

void FmtVisitor::emitType(ast::TypeExprId id) {
    if (id == ast::kInvalidTypeExpr) {
        emit("_");
        return;
    }
    const auto &node = builder_.getTypeExpr(id);
    std::visit(common::overloaded{
                   [&](const ast::TypePath &n) {
                       for (size_t i = 0; i < n.segments.size(); i++) {
                           if (i > 0)
                               emit("::");
                           emit(n.segments[i]);
                       }
                   },
                   [&](const ast::TypeBuiltin &n) { emitBuiltinType(n.kind); },
                   [&](const ast::TypePtrExpr &n) {
                       emit("*");
                       if (n.is_mut)
                           emit("mut ");
                       switch (n.ownership) {
                       case ast::OwnershipKw::Unique:
                           emit("unique ");
                           break;
                       case ast::OwnershipKw::Share:
                           emit("share ");
                           break;
                       case ast::OwnershipKw::Lend:
                           emit("lend ");
                           break;
                       case ast::OwnershipKw::View:
                           emit("view ");
                           break;
                       case ast::OwnershipKw::Belong:
                           emit("belong ");
                           break;
                       default:
                           break;
                       }
                       emitType(n.pointee);
                   },
                   [&](const ast::TypeSlice &n) {
                       emit("[]");
                       emitType(n.elem);
                   },
                   [&](const ast::TypeArray &n) {
                       emit("[");
                       emitType(n.count);
                       emit("]");
                       emitType(n.elem);
                   },
                   [&](const ast::TypeFnExpr &n) {
                       emit("fn(");
                       emitCommaList(n.params, [&](ast::TypeExprId t) { emitType(t); });
                       emit(")");
                       if (n.ret != ast::kInvalidTypeExpr) {
                           emit(": ");
                           emitType(n.ret);
                       }
                   },
                   [&](const ast::TypeOptional &n) {
                       emit("?");
                       emitType(n.inner);
                   },
                   [&](const ast::TypeFailable &n) {
                       emitType(n.inner);
                       emit("!");
                       if (n.error != ast::kInvalidTypeExpr)
                           emitType(n.error);
                   },
                   [&](const ast::TypeApp &n) {
                       emitType(n.base);
                       emit("<");
                       emitCommaList(n.args, [&](ast::TypeExprId t) { emitType(t); });
                       emit(">");
                   },
                   [&](const ast::TypePack &n) {
                       emit("{ ");
                       emitCommaList(n.members, [&](const ast::TypePackMember &m) {
                           emit(m.name);
                           emit(": ");
                           emitType(m.type);
                       });
                       emit(" }");
                   },
                   [&](const ast::TypeSum &n) {
                       for (size_t i = 0; i < n.members.size(); i++) {
                           if (i > 0)
                               emit(" | ");
                           emitType(n.members[i]);
                       }
                   },
                   [&](const ast::TypeInfer &) { emit("_"); },
                   [&](const ast::TypeMut &n) {
                       emit("mut ");
                       emitType(n.inner);
                   },
                   [&](const ast::TypeGenericParamRef &n) { emit(n.name); },
               },
               node);
}

void FmtVisitor::emitBuiltinType(ast::BuiltinType kind) {
    switch (kind) {
    case ast::BuiltinType::I8:
        emit("i8");
        break;
    case ast::BuiltinType::I16:
        emit("i16");
        break;
    case ast::BuiltinType::I32:
        emit("i32");
        break;
    case ast::BuiltinType::I64:
        emit("i64");
        break;
    case ast::BuiltinType::I128:
        emit("i128");
        break;
    case ast::BuiltinType::U8:
        emit("u8");
        break;
    case ast::BuiltinType::U16:
        emit("u16");
        break;
    case ast::BuiltinType::U32:
        emit("u32");
        break;
    case ast::BuiltinType::U64:
        emit("u64");
        break;
    case ast::BuiltinType::U128:
        emit("u128");
        break;
    case ast::BuiltinType::F32:
        emit("f32");
        break;
    case ast::BuiltinType::F64:
        emit("f64");
        break;
    case ast::BuiltinType::Bool:
        emit("bool");
        break;
    case ast::BuiltinType::Char:
        emit("char");
        break;
    case ast::BuiltinType::Void:
        emit("void");
        break;
    case ast::BuiltinType::Never:
        emit("noreturn");
        break;
    case ast::BuiltinType::Unknown:
        emit("unknown");
        break;
    case ast::BuiltinType::Invalid:
        emit("invalid");
        break;
    case ast::BuiltinType::Opaque:
        emit("opaque");
        break;
    }
}

// ── Precedence / helpers ──────────────────────────────────────

int FmtVisitor::binopPrecedence(ast::BinaryOp op) {
    switch (op) {
    case ast::BinaryOp::Or:
        return 1;
    case ast::BinaryOp::And:
        return 2;
    case ast::BinaryOp::Xor:
        return 3;
    case ast::BinaryOp::Eq:
    case ast::BinaryOp::Ne:
    case ast::BinaryOp::Lt:
    case ast::BinaryOp::Le:
    case ast::BinaryOp::Gt:
    case ast::BinaryOp::Ge:
        return 4;
    case ast::BinaryOp::Shl:
    case ast::BinaryOp::Shr:
        return 5;
    case ast::BinaryOp::Add:
    case ast::BinaryOp::Sub:
        return 6;
    case ast::BinaryOp::Mul:
    case ast::BinaryOp::Div:
    case ast::BinaryOp::Rest:
        return 7;
    }
    return 0;
}

const char *FmtVisitor::binopText(ast::BinaryOp op) {
    switch (op) {
    case ast::BinaryOp::Add:
        return "+";
    case ast::BinaryOp::Sub:
        return "-";
    case ast::BinaryOp::Mul:
        return "*";
    case ast::BinaryOp::Div:
        return "/";
    case ast::BinaryOp::Rest:
        return "%";
    case ast::BinaryOp::Eq:
        return "==";
    case ast::BinaryOp::Ne:
        return "!=";
    case ast::BinaryOp::Lt:
        return "<";
    case ast::BinaryOp::Le:
        return "<=";
    case ast::BinaryOp::Gt:
        return ">";
    case ast::BinaryOp::Ge:
        return ">=";
    case ast::BinaryOp::And:
        return "and";
    case ast::BinaryOp::Or:
        return "or";
    case ast::BinaryOp::Xor:
        return "xor";
    case ast::BinaryOp::Shl:
        return "<<";
    case ast::BinaryOp::Shr:
        return ">>";
    }
    return "?";
}

const char *FmtVisitor::unaryopText(ast::UnaryOp op) {
    switch (op) {
    case ast::UnaryOp::Neg:
        return "-";
    case ast::UnaryOp::Not:
        return "not";
    case ast::UnaryOp::Ref:
        return "&";
    case ast::UnaryOp::Deref:
        return "*";
    }
    return "?";
}

bool FmtVisitor::isLeftAssoc(ast::BinaryOp op) {
    switch (op) {
    case ast::BinaryOp::Eq:
    case ast::BinaryOp::Ne:
    case ast::BinaryOp::Lt:
    case ast::BinaryOp::Le:
    case ast::BinaryOp::Gt:
    case ast::BinaryOp::Ge:
        return false;
    default:
        return true;
    }
}

} // namespace zith::formatter

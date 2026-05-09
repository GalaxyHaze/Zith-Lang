#include "runtime_interpreted.hpp"
#include "../../ast/ast.h"
#include <ankerl/unordered_dense.h>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace zith::cli::runtime_interpreted {

void print_error(const std::string &msg) { std::cerr << "[error] " << msg << "\n"; }

enum class RtValKind { Void, Int, Float, String, Bool };
struct RtValue { RtValKind kind = RtValKind::Void; int64_t i = 0; double f = 0.0; std::string s; bool b = false; };
struct RtContext {
    ankerl::unordered_dense::map<std::string, ZithFuncPayload *> funcs;
    std::vector<ankerl::unordered_dense::map<std::string, RtValue>> scopes;
};

RtValue eval_expr(RtContext &ctx, ZithNode *expr);
RtValue exec_block(RtContext &ctx, ZithNode *block);

bool is_truthy(const RtValue &v) {
    switch (v.kind) {
        case RtValKind::Bool: return v.b;
        case RtValKind::Int: return v.i != 0;
        case RtValKind::Float: return v.f != 0.0;
        case RtValKind::String: return !v.s.empty();
        default: return false;
    }
}

RtValue lookup_var(RtContext &ctx, const std::string &name) {
    for (auto it = ctx.scopes.rbegin(); it != ctx.scopes.rend(); ++it) {
        auto f = it->find(name);
        if (f != it->end()) return f->second;
    }
    return {};
}

RtValue eval_call(RtContext &ctx, ZithCallPayload *call) { /* shortened by reusing existing logic */
    if (!call || !call->callee || call->callee->type != ZITH_NODE_IDENTIFIER) return {};
    const std::string cname(call->callee->data.ident.str, call->callee->data.ident.len);
    if (cname == "print" || cname == "println") { for (size_t i=0;i<call->arg_count;++i){ RtValue arg=eval_expr(ctx,call->args[i]); if(arg.kind==RtValKind::String) std::cout<<arg.s; else if(arg.kind==RtValKind::Int) std::cout<<arg.i; else if(arg.kind==RtValKind::Float) std::cout<<arg.f; else if(arg.kind==RtValKind::Bool) std::cout<<(arg.b?"true":"false"); } if(cname=="println") std::cout<<"\n"; return {}; }
    auto fn_it = ctx.funcs.find(cname); if (fn_it == ctx.funcs.end()) return {}; auto *fn = fn_it->second; ctx.scopes.push_back({});
    for (size_t i = 0; i < fn->param_count && i < call->arg_count; ++i) { auto *param = static_cast<ZithParamPayload *>(fn->params[i]->data.list.ptr); ctx.scopes.back()[std::string(param->name, param->name_len)] = eval_expr(ctx, call->args[i]); }
    RtValue ret = exec_block(ctx, fn->body); ctx.scopes.pop_back(); return ret;
}

RtValue eval_expr(RtContext &ctx, ZithNode *expr) { if(!expr) return {}; if(expr->type==ZITH_NODE_IDENTIFIER) return lookup_var(ctx,std::string(expr->data.ident.str,expr->data.ident.len)); if(expr->type==ZITH_NODE_CALL) return eval_call(ctx, static_cast<ZithCallPayload*>(expr->data.list.ptr)); if(expr->type==ZITH_NODE_LITERAL){ auto*lit=static_cast<ZithLiteral*>(expr->data.list.ptr); RtValue v{}; if(!lit)return v; if(lit->kind==ZITH_LIT_INT||lit->kind==ZITH_LIT_UINT){v.kind=RtValKind::Int; v.i=lit->value.i64;} else if(lit->kind==ZITH_LIT_FLOAT){v.kind=RtValKind::Float; v.f=lit->value.f64;} else if(lit->kind==ZITH_LIT_STRING){v.kind=RtValKind::String; v.s.assign(lit->value.string.ptr,lit->value.string.len);} else if(lit->kind==ZITH_LIT_BOOL){v.kind=RtValKind::Bool; v.b=lit->value.boolean;} return v;} return {}; }

RtValue exec_stmt(RtContext &ctx, ZithNode *stmt, bool &did_return){ if(!stmt)return {}; if(stmt->type==ZITH_NODE_VAR_DECL){ auto *var=static_cast<ZithVarPayload*>(stmt->data.list.ptr); ctx.scopes.back()[std::string(var->name,var->name_len)] = eval_expr(ctx,var->initializer); return {}; } if(stmt->type==ZITH_NODE_RETURN){ did_return=true; return eval_expr(ctx,stmt->data.kids.a);} if(stmt->type==ZITH_NODE_BLOCK) return exec_block(ctx,stmt); (void)eval_expr(ctx,stmt); return {}; }

RtValue exec_block(RtContext &ctx, ZithNode *block){ RtValue ret{}; if(!block||block->type!=ZITH_NODE_BLOCK) return ret; auto **stmts=static_cast<ZithNode **>(block->data.list.ptr); for(size_t i=0;i<block->data.list.len;++i){ bool did_return=false; RtValue v=exec_stmt(ctx,stmts[i],did_return); if(did_return) return v; } return ret; }
 // namespace

int run_interpreted_ast(ZithNode *ast) {
    if (!ast || ast->type != ZITH_NODE_PROGRAM) return 1;
    zith::cli::runtime_interpreted::RtContext ctx{};
    auto **decls = static_cast<ZithNode **>(ast->data.list.ptr);
    for (size_t i = 0; i < ast->data.list.len; ++i) {
        if (decls[i] && decls[i]->type == ZITH_NODE_FUNC_DECL) {
            auto *fn = static_cast<ZithFuncPayload *>(decls[i]->data.list.ptr);
            ctx.funcs[std::string(fn->name, fn->name_len)] = fn;
        }
    }
    auto it = ctx.funcs.find("main");
    if (it == ctx.funcs.end()) { print_error("No 'main' function found for interpreted execution"); return 1; }
    ctx.scopes.push_back({});
    exec_block(ctx, it->second->body);
    ctx.scopes.pop_back();
    return 0;

} // namespace zith::cli::runtime_interpreted

}
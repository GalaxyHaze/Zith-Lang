#include "runtime_interpreted.hpp"
#include "ast/ast.h"
#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace zith::cli::runtime_interpreted {

static void print_error(const std::string &msg) {
    std::cerr << "[error] " << msg << "\n";
}

enum class RtValKind { Void, Int, Float, String, Bool };

struct RtValue {
    RtValKind kind = RtValKind::Void;
    int64_t i      = 0;
    double f       = 0.0;
    std::string s;
    bool b = false;
};

struct RtContext {
    std::unordered_map<std::string, ZithFuncPayload *> funcs;
    std::vector<std::unordered_map<std::string, RtValue>> scopes;
};

// Forward declarations
static RtValue eval_expr(RtContext &ctx, ZithNode *expr);
static RtValue exec_block(RtContext &ctx, ZithNode *block);
static RtValue exec_stmt(RtContext &ctx, ZithNode *stmt, bool &did_return);

static bool is_truthy(const RtValue &v) {
    switch (v.kind) {
    case RtValKind::Bool:
        return v.b;
    case RtValKind::Int:
        return v.i != 0;
    case RtValKind::Float:
        return v.f != 0.0;
    case RtValKind::String:
        return !v.s.empty();
    default:
        return false;
    }
}

static RtValue lookup_var(RtContext &ctx, const std::string &name) {
    for (auto it = ctx.scopes.rbegin(); it != ctx.scopes.rend(); ++it) {
        auto f = it->find(name);
        if (f != it->end())
            return f->second;
    }
    return {};
}

static RtValue eval_call(RtContext &ctx, ZithCallPayload *call) {
    if (!call || !call->callee || call->callee->type != ZITH_NODE_IDENTIFIER)
        return {};

    const std::string cname(call->callee->data.ident.str, call->callee->data.ident.len);

    // Handle built-in print functions
    if (cname == "print" || cname == "println") {
        for (size_t i = 0; i < call->arg_count; ++i) {
            RtValue arg = eval_expr(ctx, call->args[i]);
            if (arg.kind == RtValKind::String)
                std::cout << arg.s;
            else if (arg.kind == RtValKind::Int)
                std::cout << arg.i;
            else if (arg.kind == RtValKind::Float)
                std::cout << arg.f;
            else if (arg.kind == RtValKind::Bool)
                std::cout << (arg.b ? "true" : "false");
        }
        if (cname == "println")
            std::cout << "\n";
        return {};
    }

    // Handle user defined functions
    auto fn_it = ctx.funcs.find(cname);
    if (fn_it == ctx.funcs.end())
        return {};

    auto *fn = fn_it->second;
    ctx.scopes.push_back({}); // New scope

    // Bind arguments
    for (size_t i = 0; i < fn->param_count && i < call->arg_count; ++i) {
        auto *param = static_cast<ZithParamPayload *>(fn->params[i]->data.list.ptr);
        ctx.scopes.back()[std::string(param->name, param->name_len)] =
            eval_expr(ctx, call->args[i]);
    }

    RtValue ret = exec_block(ctx, fn->body);
    ctx.scopes.pop_back(); // Restore scope
    return ret;
}

static RtValue eval_expr(RtContext &ctx, ZithNode *expr) {
    if (!expr)
        return {};

    switch (expr->type) {
    case ZITH_NODE_LITERAL: {
        auto *lit = static_cast<ZithLiteral *>(expr->data.list.ptr);
        RtValue v{};
        if (!lit)
            return v;
        if (lit->kind == ZITH_LIT_INT || lit->kind == ZITH_LIT_UINT) {
            v.kind = RtValKind::Int;
            v.i    = lit->value.i64;
        } else if (lit->kind == ZITH_LIT_FLOAT) {
            v.kind = RtValKind::Float;
            v.f    = lit->value.f64;
        } else if (lit->kind == ZITH_LIT_STRING) {
            v.kind = RtValKind::String;
            v.s.assign(lit->value.string.ptr, lit->value.string.len);
        } else if (lit->kind == ZITH_LIT_BOOL) {
            v.kind = RtValKind::Bool;
            v.b    = lit->value.boolean;
        }
        return v;
    }
    case ZITH_NODE_IDENTIFIER:
        return lookup_var(ctx, std::string(expr->data.ident.str, expr->data.ident.len));
    case ZITH_NODE_CALL:
        return eval_call(ctx, static_cast<ZithCallPayload *>(expr->data.list.ptr));
    case ZITH_NODE_BINARY_OP: {
        RtValue a = eval_expr(ctx, expr->data.kids.a);
        RtValue b = eval_expr(ctx, expr->data.kids.c);
        RtValue out{};

        // String concatenation
        if (expr->data.list.len == ZITH_TOKEN_PLUS && a.kind == RtValKind::String &&
            b.kind == RtValKind::String) {
            out.kind = RtValKind::String;
            out.s    = a.s + b.s;
            return out;
        }

        // Int operations
        if (a.kind == RtValKind::Int && b.kind == RtValKind::Int) {
            out.kind = RtValKind::Int;
            switch (expr->data.list.len) {
            case ZITH_TOKEN_PLUS:
                out.i = a.i + b.i;
                break;
            case ZITH_TOKEN_MINUS:
                out.i = a.i - b.i;
                break;
            case ZITH_TOKEN_MULTIPLY:
                out.i = a.i * b.i;
                break;
            case ZITH_TOKEN_DIVIDE:
                out.i = (b.i == 0 ? 0 : a.i / b.i);
                break;
            case ZITH_TOKEN_EQUAL:
                out.kind = RtValKind::Bool;
                out.b    = (a.i == b.i);
                break;
            case ZITH_TOKEN_NOT_EQUAL:
                out.kind = RtValKind::Bool;
                out.b    = (a.i != b.i);
                break;
            case ZITH_TOKEN_LESS_THAN:
                out.kind = RtValKind::Bool;
                out.b    = (a.i < b.i);
                break;
            case ZITH_TOKEN_LESS_THAN_OR_EQUAL:
                out.kind = RtValKind::Bool;
                out.b    = (a.i <= b.i);
                break;
            case ZITH_TOKEN_GREATER_THAN:
                out.kind = RtValKind::Bool;
                out.b    = (a.i > b.i);
                break;
            case ZITH_TOKEN_GREATER_THAN_OR_EQUAL:
                out.kind = RtValKind::Bool;
                out.b    = (a.i >= b.i);
                break;
            default:
                break;
            }
        }
        return out;
    }
    case ZITH_NODE_UNARY_OP: {
        RtValue val = eval_expr(ctx, expr->data.kids.a);
        if (expr->data.list.len == ZITH_TOKEN_BANG) {
            RtValue out{};
            out.kind = RtValKind::Bool;
            out.b    = !is_truthy(val);
            return out;
        }
        return val;
    }
    default:
        return {};
    }
}

static RtValue exec_stmt(RtContext &ctx, ZithNode *stmt, bool &did_return) {
    if (!stmt)
        return {};

    switch (stmt->type) {
    case ZITH_NODE_VAR_DECL: {
        auto *var = static_cast<ZithVarPayload *>(stmt->data.list.ptr);
        ctx.scopes.back()[std::string(var->name, var->name_len)] = eval_expr(ctx, var->initializer);
        return {};
    }
    case ZITH_NODE_RETURN:
        did_return = true;
        return eval_expr(ctx, stmt->data.kids.a);
    case ZITH_NODE_IF: {
        RtValue cond = eval_expr(ctx, stmt->data.kids.a);
        if (is_truthy(cond))
            return exec_block(ctx, stmt->data.kids.b);
        if (stmt->data.kids.c)
            return exec_block(ctx, stmt->data.kids.c);
        return {};
    }
    case ZITH_NODE_FOR: {
        auto *payload = static_cast<ZithForPayload *>(stmt->data.list.ptr);
        while (true) {
            if (payload && payload->condition) {
                RtValue cond = eval_expr(ctx, payload->condition);
                if (!is_truthy(cond))
                    break;
            }

            // Note: exec_block might return a value if a 'return' is inside the loop
            RtValue loop_ret = exec_block(ctx, payload ? payload->body : nullptr);

            // Check if we returned inside the loop
            if (loop_ret.kind != RtValKind::Void) {
                did_return = true;
                return loop_ret;
            }

            if (!payload || !payload->condition)
                break; // Infinite loop guard if condition is null (though unlikely in for)

            // Note: 'step' logic is missing in AST access here, assuming standard C-style for loop
            // structure or simplified If payload->step exists, it should be evaluated here.
        }
        return {};
    }
    case ZITH_NODE_CALL:
    case ZITH_NODE_BINARY_OP:
    case ZITH_NODE_UNARY_OP:
    case ZITH_NODE_IDENTIFIER:
    case ZITH_NODE_LITERAL:
        // Expression statements
        (void)eval_expr(ctx, stmt);
        return {};
    case ZITH_NODE_BLOCK:
        return exec_block(ctx, stmt);
    default:
        return {};
    }
}

static RtValue exec_block(RtContext &ctx, ZithNode *block) {
    RtValue ret{};
    if (!block || block->type != ZITH_NODE_BLOCK)
        return ret;

    auto **stmts = static_cast<ZithNode **>(block->data.list.ptr);
    for (size_t i = 0; i < block->data.list.len; ++i) {
        bool did_return = false;
        RtValue v       = exec_stmt(ctx, stmts[i], did_return);
        if (did_return)
            return v;
    }
    return ret;
}

int run_interpreted_ast(ZithNode *ast) {
    if (!ast || ast->type != ZITH_NODE_PROGRAM)
        return 1;

    RtContext ctx{};

    // Register functions
    auto **decls = static_cast<ZithNode **>(ast->data.list.ptr);
    for (size_t i = 0; i < ast->data.list.len; ++i) {
        if (decls[i] && decls[i]->type == ZITH_NODE_FUNC_DECL) {
            auto *fn = static_cast<ZithFuncPayload *>(decls[i]->data.list.ptr);
            ctx.funcs[std::string(fn->name, fn->name_len)] = fn;
        }
    }

    // Find and execute main
    auto it = ctx.funcs.find("main");
    if (it == ctx.funcs.end()) {
        print_error("No 'main' function found for interpreted execution");
        return 1;
    }

    ctx.scopes.push_back({});
    exec_block(ctx, it->second->body);
    ctx.scopes.pop_back();

    return 0;
}

} // namespace zith::cli::runtime_interpreted
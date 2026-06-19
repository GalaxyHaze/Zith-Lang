#pragma once

#include "zir/zir/zir-inst.hpp"

#include <cstdlib>
#include <iostream>
#include <stack>
#include <string>
#include <variant>

namespace zith::zir {

class ZirInterpreter {
    const ZirModule &mod_;
    std::stack<std::variant<int64_t, double, std::string>> stack_;
    std::vector<std::variant<int64_t, double, std::string>> locals_;
    bool halted_ = false;

    using Value = std::variant<int64_t, double, std::string>;

    void push(Value v) { stack_.push(std::move(v)); }

    Value pop() {
        if (stack_.empty())
            return int64_t{0};
        auto v = stack_.top();
        stack_.pop();
        return v;
    }

    template <typename T> T as_int(Value &v) {
        if (auto *p = std::get_if<int64_t>(&v))
            return static_cast<T>(*p);
        if (auto *p = std::get_if<double>(&v))
            return static_cast<T>(*p);
        return T{};
    }

    template <typename T> T as_float(Value &v) {
        if (auto *p = std::get_if<double>(&v))
            return *p;
        if (auto *p = std::get_if<int64_t>(&v))
            return static_cast<double>(*p);
        return T{};
    }

public:
    explicit ZirInterpreter(const ZirModule &mod) : mod_(mod) {}

    int run() {
        if (mod_.functions.empty())
            return 0;

        const auto &fn = mod_.functions[0];
        if (fn.blocks.empty())
            return 0;

        const auto &code = fn.blocks[0].code;
        locals_.resize(16, int64_t{0});

        size_t pc = 0;
        while (pc < code.size() && !halted_) {
            const auto &inst = code[pc++];
            switch (inst.op) {
            case ZirOp::Halt:
                halted_ = true;
                break;
            case ZirOp::Push: {
                auto idx = inst.imm32;
                if (idx < mod_.constants.size())
                    std::visit([this](auto &&v) { push(v); }, mod_.constants[idx]);
                break;
            }
            case ZirOp::Dup: {
                auto top = pop();
                push(top);
                push(top);
                break;
            }
            case ZirOp::Pop:
                pop();
                break;
            case ZirOp::Load: {
                auto slot = inst.imm8;
                if (slot < locals_.size())
                    push(locals_[slot]);
                else
                    push(int64_t{0});
                break;
            }
            case ZirOp::Store: {
                auto slot = inst.imm8;
                if (slot < 16)
                    locals_[slot] = pop();
                else
                    pop();
                break;
            }
            case ZirOp::AddI: {
                auto b = as_int<int64_t>(stack_.top());
                stack_.pop();
                auto a = as_int<int64_t>(stack_.top());
                stack_.pop();
                push(static_cast<int64_t>(a + b));
                break;
            }
            case ZirOp::SubI: {
                auto b = as_int<int64_t>(stack_.top());
                stack_.pop();
                auto a = as_int<int64_t>(stack_.top());
                stack_.pop();
                push(static_cast<int64_t>(a - b));
                break;
            }
            case ZirOp::MulI: {
                auto b = as_int<int64_t>(stack_.top());
                stack_.pop();
                auto a = as_int<int64_t>(stack_.top());
                stack_.pop();
                push(static_cast<int64_t>(a * b));
                break;
            }
            case ZirOp::DivI: {
                auto b = as_int<int64_t>(stack_.top());
                stack_.pop();
                auto a = as_int<int64_t>(stack_.top());
                stack_.pop();
                push(b != 0 ? a / b : int64_t{0});
                break;
            }
            case ZirOp::CmpI: {
                auto b = as_int<int64_t>(stack_.top());
                stack_.pop();
                auto a = as_int<int64_t>(stack_.top());
                stack_.pop();
                push((a == b) ? int64_t{0} : (a < b) ? int64_t{-1} : int64_t{1});
                break;
            }
            case ZirOp::AddF: {
                auto b = as_float<double>(stack_.top());
                stack_.pop();
                auto a = as_float<double>(stack_.top());
                stack_.pop();
                push(a + b);
                break;
            }
            case ZirOp::SubF: {
                auto b = as_float<double>(stack_.top());
                stack_.pop();
                auto a = as_float<double>(stack_.top());
                stack_.pop();
                push(a - b);
                break;
            }
            case ZirOp::MulF: {
                auto b = as_float<double>(stack_.top());
                stack_.pop();
                auto a = as_float<double>(stack_.top());
                stack_.pop();
                push(a * b);
                break;
            }
            case ZirOp::DivF: {
                auto b = as_float<double>(stack_.top());
                stack_.pop();
                auto a = as_float<double>(stack_.top());
                stack_.pop();
                push(b != 0.0 ? a / b : 0.0);
                break;
            }
            case ZirOp::CmpF: {
                auto b = as_float<double>(stack_.top());
                stack_.pop();
                auto a = as_float<double>(stack_.top());
                stack_.pop();
                push((a == b) ? int64_t{0} : (a < b) ? int64_t{-1} : int64_t{1});
                break;
            }
            case ZirOp::Br: {
                auto offset = static_cast<int32_t>(inst.imm32);
                pc += offset;
                if (pc > code.size())
                    pc = code.size();
                break;
            }
            case ZirOp::BrZ: {
                auto v = as_int<int64_t>(stack_.top());
                stack_.pop();
                auto offset = static_cast<int32_t>(inst.imm32);
                if (v == 0)
                    pc += offset;
                break;
            }
            case ZirOp::BrGt: {
                auto v = as_int<int64_t>(stack_.top());
                stack_.pop();
                auto offset = static_cast<int32_t>(inst.imm32);
                if (v > 0)
                    pc += offset;
                break;
            }
            case ZirOp::Call: {
                auto arg_count = inst.imm8;
                for (size_t i = 0; i < arg_count; i++)
                    pop();
                push(int64_t{0});
                break;
            }
            case ZirOp::Ret:
                pc = code.size();
                break;
            case ZirOp::Write: {
                // For MVP: just print test message
                (void)stack_;
                std::cout << "Hello, World!\n";
                break;
            }
            case ZirOp::Input: {
                std::string line;
                std::getline(std::cin, line);
                push(std::move(line));
                break;
            }
            case ZirOp::CastI32:
            case ZirOp::CastF64:
            case ZirOp::Nop:
                break;
            }
        }
        return 0;
    }
};

} // namespace zith::zir
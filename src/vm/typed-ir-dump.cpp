#include "vm/typed-ir-dump.hpp"

#include <iomanip>
#include <sstream>

namespace zith::vm {
namespace {

const char *valueTypeName(ValueType type) {
    switch (type) {
    case ValueType::I32:
        return "i32";
    case ValueType::I64:
        return "i64";
    case ValueType::F32:
        return "f32";
    case ValueType::F64:
        return "f64";
    case ValueType::Ptr:
        return "ptr";
    case ValueType::Slice:
        return "slice";
    case ValueType::FnRef:
        return "fnref";
    case ValueType::ExternRef:
        return "externref";
    case ValueType::Void:
        return "void";
    }
    return "unknown";
}

const char *opName(Op op) {
#define ZITH_VM_OP_NAME(name)                                                                      \
    case Op::name:                                                                                 \
        return #name
    switch (op) {
        ZITH_VM_OP_NAME(LoadConstI32);
        ZITH_VM_OP_NAME(LoadConstI64);
        ZITH_VM_OP_NAME(LoadConstF32);
        ZITH_VM_OP_NAME(LoadConstF64);
        ZITH_VM_OP_NAME(LoadString);
        ZITH_VM_OP_NAME(LoadFnRef);
        ZITH_VM_OP_NAME(LoadExternRef);
        ZITH_VM_OP_NAME(Move);
        ZITH_VM_OP_NAME(AllocBytes);
        ZITH_VM_OP_NAME(MallocBytes);
        ZITH_VM_OP_NAME(StoreBytes);
        ZITH_VM_OP_NAME(StoreI64);
        ZITH_VM_OP_NAME(LoadBytes);
        ZITH_VM_OP_NAME(LoadI64);
        ZITH_VM_OP_NAME(IndexLoad);
        ZITH_VM_OP_NAME(FieldPtr);
        ZITH_VM_OP_NAME(MakeSlice);
        ZITH_VM_OP_NAME(SlicePtr);
        ZITH_VM_OP_NAME(SliceLen);
        ZITH_VM_OP_NAME(MemCopy);
        ZITH_VM_OP_NAME(Add);
        ZITH_VM_OP_NAME(Sub);
        ZITH_VM_OP_NAME(Mul);
        ZITH_VM_OP_NAME(Div);
        ZITH_VM_OP_NAME(Rem);
        ZITH_VM_OP_NAME(BitAnd);
        ZITH_VM_OP_NAME(BitOr);
        ZITH_VM_OP_NAME(BitXor);
        ZITH_VM_OP_NAME(Shl);
        ZITH_VM_OP_NAME(Shr);
        ZITH_VM_OP_NAME(Neg);
        ZITH_VM_OP_NAME(Not);
        ZITH_VM_OP_NAME(BitNot);
        ZITH_VM_OP_NAME(Eq);
        ZITH_VM_OP_NAME(Ne);
        ZITH_VM_OP_NAME(Lt);
        ZITH_VM_OP_NAME(Le);
        ZITH_VM_OP_NAME(Gt);
        ZITH_VM_OP_NAME(Ge);
        ZITH_VM_OP_NAME(CallFn);
        ZITH_VM_OP_NAME(CallRange);
        ZITH_VM_OP_NAME(CallFnRef);
        ZITH_VM_OP_NAME(CallExtern);
        ZITH_VM_OP_NAME(CallExternRange);
        ZITH_VM_OP_NAME(CallExternRef);
        ZITH_VM_OP_NAME(Ret);
        ZITH_VM_OP_NAME(Branch);
        ZITH_VM_OP_NAME(Branch2);
        ZITH_VM_OP_NAME(Jump);
        ZITH_VM_OP_NAME(Trap);
    }
#undef ZITH_VM_OP_NAME
    return "Unknown";
}

void appendQuoted(std::ostringstream &out, std::string_view text) {
    out << '"';
    for (const char ch : text) {
        if (ch == '"' || ch == '\\')
            out << '\\';
        out << ch;
    }
    out << '"';
}

} // namespace

std::string dump(const Module &module) {
    std::ostringstream out;
    out << "--- VIR ---\n";
    out << "strings (" << module.strings.size() << "):\n";
    for (size_t i = 0; i < module.strings.size(); ++i) {
        out << "  [" << i << "] ";
        appendQuoted(out, module.strings[i]);
        out << '\n';
    }
    out << "i64_constants (" << module.i64Constants.size() << "):\n";
    for (size_t i = 0; i < module.i64Constants.size(); ++i)
        out << "  [" << i << "] " << module.i64Constants[i] << '\n';
    out << "f64_constants (" << module.f64Constants.size() << "):\n";
    out << std::setprecision(17);
    for (size_t i = 0; i < module.f64Constants.size(); ++i)
        out << "  [" << i << "] " << module.f64Constants[i] << '\n';
    out << "externs (" << module.externs.size() << "):\n";
    for (size_t i = 0; i < module.externs.size(); ++i) {
        out << "  [" << i << "] ";
        appendQuoted(out, module.externs[i]);
        out << '\n';
    }
    out << "functions (" << module.functions.size() << "):\n";
    for (size_t fi = 0; fi < module.functions.size(); ++fi) {
        const auto &fn = module.functions[fi];
        out << "fn " << fn.name << " -> " << valueTypeName(fn.returnType)
            << " (params=" << fn.paramCount << ", regs=" << fn.regCount << ")\n";
        out << "  reg_types:";
        for (size_t ri = 0; ri < fn.regTypes.size(); ++ri)
            out << " r" << ri << ":" << valueTypeName(fn.regTypes[ri]);
        out << '\n';
        for (size_t pc = 0; pc < fn.body.size(); ++pc) {
            const auto &instr = fn.body[pc];
            out << "  " << std::setw(4) << pc << ": " << opName(instr.op) << " a=r" << instr.a
                << " b=r" << instr.b << " c=r" << instr.c << " imm=" << instr.imm
                << " d=" << instr.d << " e=" << instr.e << '\n';
        }
    }
    out << "---\n";
    return out.str();
}

} // namespace zith::vm

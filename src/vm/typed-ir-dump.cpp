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

void appendInstruction(std::ostringstream &out, const Instr &instr, size_t pc) {
    out << "  " << std::setw(4) << pc << ": " << opName(instr.op) << ' ';
    switch (instr.op) {
    case Op::LoadConstI32:
    case Op::LoadConstI64:
        out << "dst=r" << instr.a << " value=" << instr.imm;
        break;
    case Op::LoadConstF32:
    case Op::LoadConstF64:
        out << "dst=r" << instr.a << " float_constant=#" << instr.imm;
        break;
    case Op::LoadString:
        out << "dst=r" << instr.a << " string=#" << instr.imm;
        break;
    case Op::LoadFnRef:
        out << "dst=r" << instr.a << " function=#" << instr.imm;
        break;
    case Op::LoadExternRef:
        out << "dst=r" << instr.a << " extern=#" << instr.imm;
        break;
    case Op::Move:
        out << "dst=r" << instr.a << " src=r" << instr.b;
        break;
    case Op::AllocBytes:
    case Op::MallocBytes:
        out << "dst=r" << instr.a << " size=r" << instr.b << " alignment=r" << instr.c;
        break;
    case Op::StoreBytes:
        out << "address=r" << instr.a << " string=#" << instr.imm;
        break;
    case Op::StoreI64:
        out << "address=r" << instr.a << " value=r" << instr.b;
        break;
    case Op::LoadBytes:
        out << "dst=r" << instr.a << " address=r" << instr.b << " count=r" << instr.c;
        break;
    case Op::LoadI64:
        out << "dst=r" << instr.a << " address=r" << instr.b;
        break;
    case Op::IndexLoad:
        out << "dst=r" << instr.a << " base=r" << instr.b << " index=r" << instr.c
            << " element_size=" << instr.imm;
        if (instr.e == 1U)
            out << " length=r" << instr.d;
        else
            out << " length=" << instr.d;
        break;
    case Op::FieldPtr:
        out << "dst=r" << instr.a << " base=r" << instr.b << " byte_offset=" << instr.imm;
        break;
    case Op::MakeSlice:
        out << "ptr_dst=r" << instr.a << " len_dst=r" << (instr.a + 1U) << " ptr=r" << instr.b
            << " len=r" << instr.c;
        break;
    case Op::SlicePtr:
        out << "dst=r" << instr.a << " slice=r" << instr.b;
        break;
    case Op::SliceLen:
        out << "dst=r" << instr.a << " slice=r" << instr.b;
        break;
    case Op::MemCopy:
        out << "destination=r" << instr.a << " source=r" << instr.b << " count=r" << instr.c;
        break;
    case Op::Add:
    case Op::Sub:
    case Op::Mul:
    case Op::Div:
    case Op::Rem:
    case Op::BitAnd:
    case Op::BitOr:
    case Op::BitXor:
    case Op::Shl:
    case Op::Shr:
    case Op::Eq:
    case Op::Ne:
    case Op::Lt:
    case Op::Le:
    case Op::Gt:
    case Op::Ge:
        out << "dst=r" << instr.a << " lhs=r" << instr.b << " rhs=r" << instr.c;
        break;
    case Op::Neg:
    case Op::Not:
    case Op::BitNot:
        out << "dst=r" << instr.a << " value=r" << instr.b;
        break;
    case Op::CallFn:
        out << "dst=r" << instr.a << " arg0=r" << instr.b << " arg1=r" << instr.c << " function=#"
            << instr.imm;
        break;
    case Op::CallRange:
        out << "dst=r" << instr.a << " arg_base=r" << instr.b << " arg_count=" << instr.c
            << " function=#" << instr.imm;
        break;
    case Op::CallExtern:
        out << "dst=r" << instr.a << " arg0=r" << instr.b << " arg1=r" << instr.c << " arg2=r"
            << instr.d << " arg3=r" << instr.e << " extern=#" << instr.imm;
        break;
    case Op::CallExternRange:
        out << "dst=r" << instr.a << " arg_base=r" << instr.b << " arg_count=" << instr.c
            << " extern=#" << instr.imm;
        break;
    case Op::CallFnRef:
        out << "dst=r" << instr.a << " function_ref=r" << instr.b << " arg0=r" << instr.c
            << " arg1=r" << instr.imm;
        break;
    case Op::CallExternRef:
        out << "dst=r" << instr.a << " extern_ref=r" << instr.b << " arg0=r" << instr.c << " arg1=r"
            << instr.imm << " arg2=r" << instr.d << " arg3=r" << instr.e;
        break;
    case Op::Ret:
        out << "value=r" << instr.a;
        break;
    case Op::Branch:
        out << "condition=r" << instr.b << " if_true=pc" << instr.imm << " if_false=pc"
            << (pc + 1U);
        break;
    case Op::Branch2:
        out << "condition=r" << instr.b << " if_true=pc" << instr.imm << " if_false=pc" << instr.d;
        break;
    case Op::Jump:
        out << "target=pc" << instr.imm;
        break;
    case Op::Trap:
        out << "reason=explicit";
        break;
    }
    out << '\n';
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
        for (size_t pc = 0; pc < fn.body.size(); ++pc)
            appendInstruction(out, fn.body[pc], pc);
    }
    out << "---\n";
    return out.str();
}

} // namespace zith::vm

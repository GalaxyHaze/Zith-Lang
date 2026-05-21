// impl/llvm/backend.cpp — LLVM IR → Zith IR converter
#include "zith/vm.hpp"
#include "LLVM/LLVM.hpp"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instructions.h>
#include <unordered_map>
#include <vector>

namespace zith::LLVM {

// ============================================================================
// Type Mapping: llvm::Type* → ZithTypeId
// ============================================================================

static ZithTypeId llvmTypeToZith(llvm::Type* ty) {
    switch (ty->getTypeID()) {
        case llvm::Type::VoidTyID:    return ZITH_TYPE_VOID;
        case llvm::Type::IntegerTyID: {
            auto bits = cast<llvm::IntegerType>(ty)->getBitWidth();
            switch (bits) {
                case 1:   return ZITH_TYPE_I1;
                case 8:   return ZITH_TYPE_I8;
                case 16:  return ZITH_TYPE_I16;
                case 32:  return ZITH_TYPE_I32;
                case 64:  return ZITH_TYPE_I64;
                case 128: return ZITH_TYPE_I128;
                default:  return ZITH_TYPE_I64;
            }
        }
        case llvm::Type::HalfTyID:     return ZITH_TYPE_F16;
        case llvm::Type::FloatTyID:    return ZITH_TYPE_F32;
        case llvm::Type::DoubleTyID:   return ZITH_TYPE_F64;
        case llvm::Type::PointerTyID:  return ZITH_TYPE_PTR;
        case llvm::Type::StructTyID:   return ZITH_TYPE_STRUCT;
        case llvm::Type::ArrayTyID:    return ZITH_TYPE_ARRAY;
        case llvm::Type::FixedVectorTyID: return ZITH_TYPE_VECTOR;
        default: return ZITH_TYPE_I64;
    }
}

static ZithLinkage llvmLinkageToZith(llvm::GlobalValue::LinkageTypes linkage) {
    switch (linkage) {
        case llvm::GlobalValue::PrivateLinkage:              return ZITH_LINKAGE_PRIVATE;
        case llvm::GlobalValue::InternalLinkage:             return ZITH_LINKAGE_INTERNAL;
        case llvm::GlobalValue::ExternalLinkage:             return ZITH_LINKAGE_EXTERNAL;
        case llvm::GlobalValue::LinkOnceAnyLinkage:          return ZITH_LINKAGE_LINKONCE;
        case llvm::GlobalValue::LinkOnceODRLinkage:          return ZITH_LINKAGE_LINKONCE_ODR;
        case llvm::GlobalValue::WeakAnyLinkage:              return ZITH_LINKAGE_WEAK;
        case llvm::GlobalValue::WeakODRLinkage:              return ZITH_LINKAGE_WEAK_ODR;
        case llvm::GlobalValue::AvailableExternallyLinkage:  return ZITH_LINKAGE_AVAILABLE_EXTERNALLY;
        case llvm::GlobalValue::CommonLinkage:               return ZITH_LINKAGE_COMMON;
        case llvm::GlobalValue::AppendingLinkage:            return ZITH_LINKAGE_APPENDING;
        case llvm::GlobalValue::ExternalWeakLinkage:         return ZITH_LINKAGE_EXTERN_WEAK;
        default:                                             return ZITH_LINKAGE_EXTERNAL;
    }
}

// ============================================================================
// Backend: converts an llvm::Module into a DecodedModule (ZBC)
// ============================================================================

class ZithBackend {
    Module* llvm_module;
    DecodedModule zith_module;

    // llvm::Value* → ValueId mapping per function
    std::unordered_map<const llvm::Value*, uint32_t> value_to_id;
    // llvm::BasicBlock* → BlockId mapping per function
    std::unordered_map<const llvm::BasicBlock*, uint32_t> block_to_id;

    uint32_t next_value_id;
    uint32_t next_block_id;

    // String interning
    std::unordered_map<std::string_view, uint32_t> string_map;

    uint32_t internString(std::string_view s) {
        auto it = string_map.find(s);
        if (it != string_map.end()) return it->second;
        uint32_t id = static_cast<uint32_t>(zith_module.strings.size());
        zith_module.strings.push_back(s);
        string_map[s] = id;
        return id;
    }

    uint32_t addType(ZithTypeId base) {
        uint32_t id = static_cast<uint32_t>(zith_module.types.size());
        zith_module.types.push_back(TypeEntry{base});
        return id;
    }

public:
    ZithBackend(Module* mod) : llvm_module(mod), next_value_id(0), next_block_id(0) {}

    DecodedModule convert() {
        // Collect all types used in the module
        collectTypes();

        // Convert global variables
        for (auto& gv : llvm_module->globals()) {
            convertGlobal(gv);
        }

        // Convert functions
        for (auto& fn : *llvm_module) {
            convertFunction(fn);
        }

        return std::move(zith_module);
    }

private:
    void collectTypes() {
        // Scan all types used in the module and populate the type table
        // This is a simplified version — full implementation would recursively
        // collect all struct types, function types, etc.
        addType(ZITH_TYPE_VOID);
        addType(ZITH_TYPE_I1);
        addType(ZITH_TYPE_I8);
        addType(ZITH_TYPE_I16);
        addType(ZITH_TYPE_I32);
        addType(ZITH_TYPE_I64);
        addType(ZITH_TYPE_PTR);
        addType(ZITH_TYPE_F64);
    }

    void convertGlobal(const llvm::GlobalVariable& gv) {
        GlobalEntry entry{};
        entry.name_string_id = internString(gv.getName());
        entry.type_id = static_cast<uint32_t>(llvmTypeToZith(gv.getValueType()));
        entry.linkage = llvmLinkageToZith(gv.getLinkage());
        if (gv.getAlignment()) {
            entry.align_log2 = static_cast<uint8_t>(
                llvm::Log2_64(gv.getAlignment().value())
            );
        }
        if (gv.isConstant()) entry.flags |= 1;
        zith_module.globals.push_back(entry);
    }

    void convertFunction(const llvm::Function& fn) {
        FunctionEntry entry{};
        entry.name_string_id = internString(fn.getName());
        entry.linkage = llvmLinkageToZith(fn.getLinkage());

        // Extract function attributes
        if (fn.hasFnAttribute(llvm::Attribute::NoUnwind))
            entry.attr_flags |= ZITH_FN_ATTR_NOUNWIND;
        if (fn.hasFnAttribute(llvm::Attribute::ReadOnly))
            entry.attr_flags |= ZITH_FN_ATTR_READONLY;
        if (fn.hasFnAttribute(llvm::Attribute::ReadNone))
            entry.attr_flags |= ZITH_FN_ATTR_READNONE;
        if (fn.hasFnAttribute(llvm::Attribute::NoReturn))
            entry.attr_flags |= ZITH_FN_ATTR_NORETURN;

        entry.n_params = fn.arg_size();
        entry.n_blocks = fn.size();

        zith_module.functions.push_back(entry);

        // Assign block IDs
        next_block_id = 0;
        for (auto& bb : fn) {
            block_to_id[&bb] = next_block_id++;
        }

        // Assign value IDs to instructions
        next_value_id = 0;

        // Function parameters get first value IDs
        for (auto& arg : fn.args()) {
            value_to_id[&arg] = next_value_id++;
        }

        // Then all instructions
        for (auto& bb : fn) {
            for (auto& inst : bb) {
                if (!inst.getType()->isVoidTy()) {
                    value_to_id[&inst] = next_value_id++;
                }
            }
        }
    }

    // Full implementation would emit bytecode for each instruction.
    // Skeleton for key instruction patterns:

    void emitBinaryOp(Opcode op, const llvm::BinaryOperator& inst) {
        // [opcode][type_byte][overflow_flags][result_vid][lhs_vid][rhs_vid]
        uint8_t type_byte = static_cast<uint8_t>(llvmTypeToZith(inst.getType()));
        uint8_t overflow = 0;
        if (inst.hasNoSignedWrap()) overflow |= ZITH_OVERFLOW_NSW;
        if (inst.hasNoUnsignedWrap()) overflow |= ZITH_OVERFLOW_NUW;
        // Emit bytes...
    }

    void emitLoad(const llvm::LoadInst& inst) {
        // [opcode][type_byte][result_vid][addr_vid][align_log2][ordering]
        uint8_t type_byte = static_cast<uint8_t>(llvmTypeToZith(inst.getType()));
        uint8_t align_log2 = inst.getAlign() ?
            static_cast<uint8_t>(llvm::Log2_64(inst.getAlign().value())) : 0;
        // Emit bytes...
    }

    void emitPhi(const llvm::PHINode& phi, const llvm::BasicBlock& current_bb) {
        // [opcode][type_byte][result_vid][n_incoming][vid_0][block_id_0]...
        uint8_t type_byte = static_cast<uint8_t>(llvmTypeToZith(phi.getType()));
        uint32_t n_incoming = phi.getNumIncomingValues();
        // Emit bytes...
    }

    void emitRet(const llvm::ReturnInst& ret) {
        // [opcode][n_return_values][ret_type_0][ret_vid_0]...
        if (ret.getNumOperands() == 0) {
            // void return: [OP_RET][0]
        } else {
            // [OP_RET][1][type][vid]
        }
    }

    void emitBr(const llvm::BranchInst& br) {
        if (br.isUnconditional()) {
            // [OP_BR][target_block_id]
        } else {
            // [OP_COND_BR][cond_vid][true_block_id][false_block_id]
        }
    }
};

DecodedModule LLVMToZith(Module* llvm_mod) {
    ZithBackend backend(llvm_mod);
    return backend.convert();
}

} // namespace zith::LLVM

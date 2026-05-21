// include/zith/zbcBuilder.h — ZBC v2 IR builder (SSA-based, LLVM-round-trippable)
#pragma once
#include "zith/vm.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

namespace zith {

using Zith::DecodedModule;
using Zith::FunctionEntry;
using Zith::GlobalEntry;
using Zith::ModuleHeader;
using Zith::Opcode;
using Zith::StringEntry;
using Zith::TypeEntry;
using Zith::ZBC_VERSION;
using Zith::ZBC_MAGIC;

class ZbcBuilder {
private:
    std::vector<uint8_t> buffer;
    uint32_t next_value_id = 0;
    uint32_t next_block_id = 0;
    uint32_t current_block_id = 0;

    std::vector<TypeEntry> types;
    std::vector<std::string> string_storage;
    std::vector<GlobalEntry> globals;
    std::vector<FunctionEntry> functions;

    uint8_t* emit_u8(uint8_t v) {
        buffer.push_back(v);
        return &buffer.back();
    }

    uint8_t* emit_u32(uint32_t v) {
        buffer.push_back(v & 0xFF);
        buffer.push_back((v >> 8) & 0xFF);
        buffer.push_back((v >> 16) & 0xFF);
        buffer.push_back((v >> 24) & 0xFF);
        return &buffer[buffer.size() - 4];
    }

    uint8_t* emit_uleb(uint64_t v) {
        uint8_t tmp[10];
        int n = zith_uleb128_encode_uint64(tmp, v);
        for (int i = 0; i < n; i++) buffer.push_back(tmp[i]);
        return &buffer[buffer.size() - n];
    }

    uint8_t* emit_sleb(int64_t v) {
        uint8_t tmp[10];
        int n = zith_sleb128_encode_int64(tmp, v);
        for (int i = 0; i < n; i++) buffer.push_back(tmp[i]);
        return &buffer[buffer.size() - n];
    }

    uint8_t* emit_f64_bits(double v) {
        uint64_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        for (int i = 0; i < 8; i++) buffer.push_back((bits >> (i * 8)) & 0xFF);
        return &buffer[buffer.size() - 8];
    }

    uint32_t internString(std::string_view s) {
        for (size_t i = 0; i < string_storage.size(); i++) {
            if (string_storage[i] == s) return static_cast<uint32_t>(i);
        }
        string_storage.emplace_back(s);
        return static_cast<uint32_t>(string_storage.size() - 1);
    }

public:
    ZbcBuilder() = default;

    uint32_t newValueId() { return next_value_id++; }
    uint32_t currentBlockId() const { return current_block_id; }
    uint32_t newBlockId() { return next_block_id++; }

    size_t currentOffset() const { return buffer.size(); }

    // ==========================================================================
    // Type Table
    // ==========================================================================
    uint32_t addType(ZithTypeId base) {
        uint32_t id = static_cast<uint32_t>(types.size());
        types.push_back(TypeEntry{base});
        return id;
    }

    // ==========================================================================
    // Globals
    // ==========================================================================
    void addGlobal(std::string_view name, uint32_t type_id, ZithLinkage linkage,
                   uint8_t align_log2 = 0, bool is_const = false) {
        GlobalEntry entry{};
        entry.name_string_id = internString(name);
        entry.type_id = type_id;
        entry.linkage = linkage;
        entry.align_log2 = align_log2;
        if (is_const) entry.flags |= 1;
        globals.push_back(entry);
    }

    // ==========================================================================
    // Functions
    // ==========================================================================
    void beginFunction(std::string_view name, uint32_t type_id, ZithLinkage linkage,
                       uint32_t attr_flags = 0, ZithCallingConv cc = ZITH_CC_C,
                       uint32_t n_params = 0) {
        FunctionEntry entry{};
        entry.name_string_id = internString(name);
        entry.type_id = type_id;
        entry.linkage = linkage;
        entry.attr_flags = attr_flags;
        entry.calling_conv = cc;
        entry.n_params = n_params;
        entry.n_blocks = 0;
        functions.push_back(entry);
        next_value_id = 0;
        next_block_id = 0;
    }

    void beginBlock() {
        current_block_id = next_block_id++;
        if (!functions.empty()) {
            functions.back().n_blocks++;
        }
    }

    // ==========================================================================
    // Integer ALU — [opcode][type][flags][result][lhs][rhs]
    // ==========================================================================
    uint32_t emitAdd(ZithTypeId ty, uint32_t lhs, uint32_t rhs, uint8_t flags = 0) {
        uint32_t res = newValueId();
        emit_u8(OP_ADD);
        emit_u8(static_cast<uint8_t>(ty));
        emit_u8(flags);
        emit_uleb(res);
        emit_uleb(lhs);
        emit_uleb(rhs);
        return res;
    }

    uint32_t emitSub(ZithTypeId ty, uint32_t lhs, uint32_t rhs, uint8_t flags = 0) {
        uint32_t res = newValueId();
        emit_u8(OP_SUB);
        emit_u8(static_cast<uint8_t>(ty));
        emit_u8(flags);
        emit_uleb(res);
        emit_uleb(lhs);
        emit_uleb(rhs);
        return res;
    }

    uint32_t emitMul(ZithTypeId ty, uint32_t lhs, uint32_t rhs, uint8_t flags = 0) {
        uint32_t res = newValueId();
        emit_u8(OP_MUL);
        emit_u8(static_cast<uint8_t>(ty));
        emit_u8(flags);
        emit_uleb(res);
        emit_uleb(lhs);
        emit_uleb(rhs);
        return res;
    }

    uint32_t emitSDiv(ZithTypeId ty, uint32_t lhs, uint32_t rhs, uint8_t flags = 0) {
        uint32_t res = newValueId();
        emit_u8(OP_SDIV);
        emit_u8(static_cast<uint8_t>(ty));
        emit_u8(flags);
        emit_uleb(res);
        emit_uleb(lhs);
        emit_uleb(rhs);
        return res;
    }

    uint32_t emitUDiv(ZithTypeId ty, uint32_t lhs, uint32_t rhs, uint8_t flags = 0) {
        uint32_t res = newValueId();
        emit_u8(OP_UDIV);
        emit_u8(static_cast<uint8_t>(ty));
        emit_u8(flags);
        emit_uleb(res);
        emit_uleb(lhs);
        emit_uleb(rhs);
        return res;
    }

    uint32_t emitAnd(ZithTypeId ty, uint32_t lhs, uint32_t rhs) {
        uint32_t res = newValueId();
        emit_u8(OP_AND);
        emit_u8(static_cast<uint8_t>(ty));
        emit_uleb(res);
        emit_uleb(lhs);
        emit_uleb(rhs);
        return res;
    }

    uint32_t emitOr(ZithTypeId ty, uint32_t lhs, uint32_t rhs) {
        uint32_t res = newValueId();
        emit_u8(OP_OR);
        emit_u8(static_cast<uint8_t>(ty));
        emit_uleb(res);
        emit_uleb(lhs);
        emit_uleb(rhs);
        return res;
    }

    uint32_t emitXor(ZithTypeId ty, uint32_t lhs, uint32_t rhs) {
        uint32_t res = newValueId();
        emit_u8(OP_XOR);
        emit_u8(static_cast<uint8_t>(ty));
        emit_uleb(res);
        emit_uleb(lhs);
        emit_uleb(rhs);
        return res;
    }

    uint32_t emitShl(ZithTypeId ty, uint32_t lhs, uint32_t rhs) {
        uint32_t res = newValueId();
        emit_u8(OP_SHL);
        emit_u8(static_cast<uint8_t>(ty));
        emit_uleb(res);
        emit_uleb(lhs);
        emit_uleb(rhs);
        return res;
    }

    uint32_t emitAShr(ZithTypeId ty, uint32_t lhs, uint32_t rhs) {
        uint32_t res = newValueId();
        emit_u8(OP_ASHR);
        emit_u8(static_cast<uint8_t>(ty));
        emit_uleb(res);
        emit_uleb(lhs);
        emit_uleb(rhs);
        return res;
    }

    uint32_t emitLShr(ZithTypeId ty, uint32_t lhs, uint32_t rhs) {
        uint32_t res = newValueId();
        emit_u8(OP_LSHR);
        emit_u8(static_cast<uint8_t>(ty));
        emit_uleb(res);
        emit_uleb(lhs);
        emit_uleb(rhs);
        return res;
    }

    // ==========================================================================
    // Float ALU — [opcode][type][result][lhs][rhs]
    // ==========================================================================
    uint32_t emitFAdd(ZithTypeId ty, uint32_t lhs, uint32_t rhs) {
        uint32_t res = newValueId();
        emit_u8(OP_FADD);
        emit_u8(static_cast<uint8_t>(ty));
        emit_uleb(res);
        emit_uleb(lhs);
        emit_uleb(rhs);
        return res;
    }

    uint32_t emitFSub(ZithTypeId ty, uint32_t lhs, uint32_t rhs) {
        uint32_t res = newValueId();
        emit_u8(OP_FSUB);
        emit_u8(static_cast<uint8_t>(ty));
        emit_uleb(res);
        emit_uleb(lhs);
        emit_uleb(rhs);
        return res;
    }

    uint32_t emitFMul(ZithTypeId ty, uint32_t lhs, uint32_t rhs) {
        uint32_t res = newValueId();
        emit_u8(OP_FMUL);
        emit_u8(static_cast<uint8_t>(ty));
        emit_uleb(res);
        emit_uleb(lhs);
        emit_uleb(rhs);
        return res;
    }

    uint32_t emitFDiv(ZithTypeId ty, uint32_t lhs, uint32_t rhs) {
        uint32_t res = newValueId();
        emit_u8(OP_FDIV);
        emit_u8(static_cast<uint8_t>(ty));
        emit_uleb(res);
        emit_uleb(lhs);
        emit_uleb(rhs);
        return res;
    }

    // ==========================================================================
    // Comparisons — [opcode][i1][result][pred][lhs][rhs]
    // ==========================================================================
    uint32_t emitICmp(ZithICmpPred pred, uint32_t lhs, uint32_t rhs) {
        uint32_t res = newValueId();
        emit_u8(OP_ICMP);
        emit_u8(ZITH_TYPE_I1);
        emit_uleb(res);
        emit_u8(static_cast<uint8_t>(pred));
        emit_uleb(lhs);
        emit_uleb(rhs);
        return res;
    }

    uint32_t emitFCmp(ZithFCmpPred pred, uint32_t lhs, uint32_t rhs) {
        uint32_t res = newValueId();
        emit_u8(OP_FCMP);
        emit_u8(ZITH_TYPE_I1);
        emit_uleb(res);
        emit_u8(static_cast<uint8_t>(pred));
        emit_uleb(lhs);
        emit_uleb(rhs);
        return res;
    }

    // ==========================================================================
    // Memory — [opcode][type][result][addr][align][ordering]
    // ==========================================================================
    uint32_t emitLoad(ZithTypeId ty, uint32_t addr, uint8_t align_log2 = 0,
                      uint8_t ordering = ZITH_ORDER_SEQ_CST) {
        uint32_t res = newValueId();
        emit_u8(OP_LOAD);
        emit_u8(static_cast<uint8_t>(ty));
        emit_uleb(res);
        emit_uleb(addr);
        emit_u8(align_log2);
        emit_u8(ordering);
        return res;
    }

    void emitStore(ZithTypeId ty, uint32_t value, uint32_t addr, uint8_t align_log2 = 0,
                   uint8_t ordering = ZITH_ORDER_SEQ_CST) {
        emit_u8(OP_STORE);
        emit_u8(static_cast<uint8_t>(ty));
        emit_uleb(value);
        emit_uleb(addr);
        emit_u8(align_log2);
        emit_u8(ordering);
    }

    uint32_t emitAlloca(ZithTypeId ty, uint32_t alloc_type_id, uint8_t align_log2 = 0) {
        uint32_t res = newValueId();
        emit_u8(OP_ALLOCA);
        emit_u8(static_cast<uint8_t>(ty));
        emit_uleb(res);
        emit_uleb(alloc_type_id);
        emit_u8(align_log2);
        return res;
    }

    // ==========================================================================
    // GEP — [opcode][ptr][result][base][n_indices][idx_type][idx_val]...
    // ==========================================================================
    uint32_t emitGep(uint32_t base_ptr, const std::vector<std::pair<uint8_t, uint32_t>>& indices) {
        uint32_t res = newValueId();
        emit_u8(OP_GEP);
        emit_u8(ZITH_TYPE_PTR);
        emit_uleb(res);
        emit_uleb(base_ptr);
        emit_uleb(indices.size());
        for (auto& [ty, val] : indices) {
            emit_u8(ty);
            emit_uleb(val);
        }
        return res;
    }

    // ==========================================================================
    // Conversions — [opcode][result_type][result][src]
    // ==========================================================================
    uint32_t emitTrunc(ZithTypeId dst_ty, uint32_t src) {
        uint32_t res = newValueId();
        emit_u8(OP_TRUNC);
        emit_u8(static_cast<uint8_t>(dst_ty));
        emit_uleb(res);
        emit_uleb(src);
        return res;
    }

    uint32_t emitZExt(ZithTypeId dst_ty, uint32_t src) {
        uint32_t res = newValueId();
        emit_u8(OP_ZEXT);
        emit_u8(static_cast<uint8_t>(dst_ty));
        emit_uleb(res);
        emit_uleb(src);
        return res;
    }

    uint32_t emitSExt(ZithTypeId dst_ty, uint32_t src) {
        uint32_t res = newValueId();
        emit_u8(OP_SEXT);
        emit_u8(static_cast<uint8_t>(dst_ty));
        emit_uleb(res);
        emit_uleb(src);
        return res;
    }

    uint32_t emitBitCast(ZithTypeId dst_ty, uint32_t src) {
        uint32_t res = newValueId();
        emit_u8(OP_BITCAST);
        emit_u8(static_cast<uint8_t>(dst_ty));
        emit_uleb(res);
        emit_uleb(src);
        return res;
    }

    uint32_t emitPtrToInt(ZithTypeId dst_ty, uint32_t src) {
        uint32_t res = newValueId();
        emit_u8(OP_PTR2INT);
        emit_u8(static_cast<uint8_t>(dst_ty));
        emit_uleb(res);
        emit_uleb(src);
        return res;
    }

    uint32_t emitIntToPtr(uint32_t src) {
        uint32_t res = newValueId();
        emit_u8(OP_INT2PTR);
        emit_u8(ZITH_TYPE_PTR);
        emit_uleb(res);
        emit_uleb(src);
        return res;
    }

    // ==========================================================================
    // SSA — PHI, Freeze
    // ==========================================================================
    uint32_t emitPhi(ZithTypeId ty, const std::vector<std::pair<uint32_t, uint32_t>>& incoming) {
        uint32_t res = newValueId();
        emit_u8(OP_PHI);
        emit_u8(static_cast<uint8_t>(ty));
        emit_uleb(res);
        emit_uleb(incoming.size());
        for (auto& [vid, bid] : incoming) {
            emit_uleb(vid);
            emit_uleb(bid);
        }
        return res;
    }

    uint32_t emitFreeze(ZithTypeId ty, uint32_t src) {
        uint32_t res = newValueId();
        emit_u8(OP_FREEZE);
        emit_u8(static_cast<uint8_t>(ty));
        emit_uleb(res);
        emit_uleb(src);
        return res;
    }

    // ==========================================================================
    // Select — [opcode][type][result][cond][true][false]
    // ==========================================================================
    uint32_t emitSelect(ZithTypeId ty, uint32_t cond, uint32_t true_val, uint32_t false_val) {
        uint32_t res = newValueId();
        emit_u8(OP_SELECT);
        emit_u8(static_cast<uint8_t>(ty));
        emit_uleb(res);
        emit_uleb(cond);
        emit_uleb(true_val);
        emit_uleb(false_val);
        return res;
    }

    // ==========================================================================
    // Calls — [opcode][result_type][result][fn][n_args][arg_type][arg_val]...
    // ==========================================================================
    uint32_t emitCall(ZithTypeId ret_ty, uint32_t fn,
                      const std::vector<std::pair<uint8_t, uint32_t>>& args) {
        uint32_t res = newValueId();
        emit_u8(OP_CALL);
        emit_u8(static_cast<uint8_t>(ret_ty));
        emit_uleb(res);
        emit_uleb(fn);
        emit_uleb(args.size());
        for (auto& [ty, val] : args) {
            emit_u8(ty);
            emit_uleb(val);
        }
        return res;
    }

    // ==========================================================================
    // Atomics
    // ==========================================================================
    uint32_t emitAtomicRMW(ZithTypeId ty, ZithAtomicRMWOp op, uint32_t addr, uint32_t value,
                           uint8_t ordering) {
        uint32_t res = newValueId();
        emit_u8(OP_ATOMICRMW);
        emit_u8(static_cast<uint8_t>(ty));
        emit_uleb(res);
        emit_u8(static_cast<uint8_t>(op));
        emit_uleb(addr);
        emit_uleb(value);
        emit_u8(ordering);
        return res;
    }

    uint32_t emitCmpXchg(ZithTypeId ty, uint32_t addr, uint32_t cmp, uint32_t new_val,
                         uint8_t success_ordering, uint8_t failure_ordering) {
        uint32_t res = newValueId();
        emit_u8(OP_CMPXCHG);
        emit_u8(static_cast<uint8_t>(ty));
        emit_uleb(res);
        emit_uleb(addr);
        emit_uleb(cmp);
        emit_uleb(new_val);
        emit_u8(success_ordering);
        emit_u8(failure_ordering);
        return res;
    }

    void emitFence(uint8_t ordering) {
        emit_u8(OP_FENCE);
        emit_u8(ordering);
    }

    // ==========================================================================
    // Constants
    // ==========================================================================
    uint32_t emitConstInt(ZithTypeId ty, int64_t value) {
        uint32_t res = newValueId();
        emit_u8(OP_CONST_INT);
        emit_u8(static_cast<uint8_t>(ty));
        emit_uleb(res);
        emit_sleb(value);
        return res;
    }

    uint32_t emitConstFloat(ZithTypeId ty, double value) {
        uint32_t res = newValueId();
        emit_u8(OP_CONST_FLOAT);
        emit_u8(static_cast<uint8_t>(ty));
        emit_uleb(res);
        emit_f64_bits(value);
        return res;
    }

    uint32_t emitConstNull(ZithTypeId ty) {
        uint32_t res = newValueId();
        emit_u8(OP_CONST_NULL);
        emit_u8(static_cast<uint8_t>(ty));
        emit_uleb(res);
        return res;
    }

    // ==========================================================================
    // Terminators
    // ==========================================================================
    void emitBr(uint32_t target_block) {
        emit_u8(OP_BR);
        emit_uleb(target_block);
    }

    void emitCondBr(uint32_t cond, uint32_t true_block, uint32_t false_block) {
        emit_u8(OP_COND_BR);
        emit_uleb(cond);
        emit_uleb(true_block);
        emit_uleb(false_block);
    }

    void emitSwitch(uint32_t value, uint32_t default_block,
                    const std::vector<std::pair<int64_t, uint32_t>>& cases) {
        emit_u8(OP_SWITCH);
        emit_uleb(value);
        emit_uleb(default_block);
        emit_uleb(cases.size());
        for (auto& [cv, target] : cases) {
            emit_sleb(cv);
            emit_uleb(target);
        }
    }

    void emitRet() {
        emit_u8(OP_RET);
        emit_uleb(0);
    }

    void emitRet(ZithTypeId ty, uint32_t value) {
        emit_u8(OP_RET);
        emit_uleb(1);
        emit_u8(static_cast<uint8_t>(ty));
        emit_uleb(value);
    }

    void emitUnreachable() {
        emit_u8(OP_UNREACHABLE);
    }

    // ==========================================================================
    // Raw byte emission (for custom instructions or extensions)
    // ==========================================================================
    void emitRaw(const std::vector<uint8_t>& bytes) {
        for (auto b : bytes) buffer.push_back(b);
    }

    // ==========================================================================
    // Finalize — returns the encoded module
    // ==========================================================================
    std::vector<uint8_t> finalize() {
        std::vector<uint8_t> out;

        // Header
        for (int i = 0; i < 4; i++) out.push_back(ZBC_MAGIC[i]);
        out.push_back(ZBC_VERSION);
        out.push_back(0); // flags

        // Type table
        uint8_t tmp[10];
        int n = zith_uleb128_encode_uint64(tmp, types.size());
        for (int i = 0; i < n; i++) out.push_back(tmp[i]);
        for (auto& t : types) {
            out.push_back(static_cast<uint8_t>(t.base));
        }

        // String table
        n = zith_uleb128_encode_uint64(tmp, string_storage.size());
        for (int i = 0; i < n; i++) out.push_back(tmp[i]);
        uint32_t offset = 0;
        for (auto& s : string_storage) {
            out.push_back(offset & 0xFF);
            out.push_back((offset >> 8) & 0xFF);
            out.push_back((offset >> 16) & 0xFF);
            out.push_back((offset >> 24) & 0xFF);
            uint32_t len = static_cast<uint32_t>(s.size());
            out.push_back(len & 0xFF);
            out.push_back((len >> 8) & 0xFF);
            out.push_back((len >> 16) & 0xFF);
            out.push_back((len >> 24) & 0xFF);
            for (char c : s) out.push_back(static_cast<uint8_t>(c));
            offset += len;
        }

        // Globals
        n = zith_uleb128_encode_uint64(tmp, globals.size());
        for (int i = 0; i < n; i++) out.push_back(tmp[i]);
        for (auto& g : globals) {
            uint8_t gtmp[20];
            uint8_t* p = gtmp;
            int sz = zith_uleb128_encode_uint64(p, g.name_string_id); p += sz;
            sz = zith_uleb128_encode_uint64(p, g.type_id); p += sz;
            sz = zith_uleb128_encode_uint64(p, static_cast<uint64_t>(g.linkage)); p += sz;
            *p++ = g.align_log2;
            *p++ = g.flags;
            sz = zith_uleb128_encode_uint64(p, g.init_value_id); p += sz;
            for (uint8_t* q = gtmp; q < p; q++) out.push_back(*q);
        }

        // Functions
        n = zith_uleb128_encode_uint64(tmp, functions.size());
        for (int i = 0; i < n; i++) out.push_back(tmp[i]);
        for (auto& f : functions) {
            uint8_t ftmp[40];
            uint8_t* p = ftmp;
            int sz = zith_uleb128_encode_uint64(p, f.name_string_id); p += sz;
            sz = zith_uleb128_encode_uint64(p, f.type_id); p += sz;
            sz = zith_uleb128_encode_uint64(p, static_cast<uint64_t>(f.linkage)); p += sz;
            sz = zith_uleb128_encode_uint64(p, f.attr_flags); p += sz;
            sz = zith_uleb128_encode_uint64(p, static_cast<uint64_t>(f.calling_conv)); p += sz;
            sz = zith_uleb128_encode_uint64(p, f.n_params); p += sz;
            sz = zith_uleb128_encode_uint64(p, f.n_blocks); p += sz;
            for (uint8_t* q = ftmp; q < p; q++) out.push_back(*q);
        }

        // Function bytecode
        for (auto b : buffer) out.push_back(b);

        return out;
    }

    const std::vector<uint8_t>& getBuffer() const { return buffer; }
};

} // namespace zith

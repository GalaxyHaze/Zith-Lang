#include "zirl-code-section.hpp"

namespace zith::zirl {

namespace {

void writeCompactExpr(const cache::CompactExpr &e, ByteWriter &w) {
    w.writeU8(static_cast<uint8_t>(e.kind));
    w.writeU8(e.op);
    w.writeU8(e.flags);
    w.writeU8(static_cast<uint8_t>(e.safety));
    w.writeU32(e.type_id);
    w.writeU32(e.ref_a);
    w.writeU32(e.ref_b);
    w.writeU32(e.ref_c);
    w.writeU32(e.ref_d);
    w.writeU32(e.ref_e);
    w.writeU32(e.ref_f);
    w.writeU32(e.name_id);
    w.writeU32(e.resolved_function_name_id);
    w.writeU32(e.resolved_function_signature_type_id);
    w.writeU32(e.variadic_slice_param);
    w.writeU8(e.is_variadic_slice ? 1U : 0U);
    w.writeU8(e.auto_collect_tail ? 1U : 0U);
    w.writeU32(e.alignmentBytes);
    w.writeI64(e.int_val);
    w.writeF64(e.flt_val);
    w.writeU32(static_cast<uint32_t>(e.args.size()));
    for (auto a : e.args)
        w.writeU32(a);
    w.writeU32(static_cast<uint32_t>(e.arg_types.size()));
    for (auto t : e.arg_types)
        w.writeU32(t);
    w.writeU32(static_cast<uint32_t>(e.ints.size()));
    for (auto value : e.ints)
        w.writeU64(value);
}

bool readCompactExpr(ByteReader &r, cache::CompactExpr &out) {
    uint8_t kind = 0, op = 0, flags = 0, safety = 0;
    uint8_t is_variadic_slice = 0, auto_collect_tail = 0;
    if (!r.readU8(kind) || !r.readU8(op) || !r.readU8(flags) || !r.readU8(safety) ||
        safety > 3U)
        return false;
    out.kind  = static_cast<cache::CompactExprKind>(kind);
    out.op    = op;
    out.flags = flags;
    out.safety = static_cast<cache::CompactSafetyMode>(safety);
    if (!r.readU32(out.type_id) || !r.readU32(out.ref_a) || !r.readU32(out.ref_b) ||
        !r.readU32(out.ref_c) || !r.readU32(out.ref_d) || !r.readU32(out.ref_e) ||
        !r.readU32(out.ref_f) || !r.readU32(out.name_id) ||
        !r.readU32(out.resolved_function_name_id) ||
        !r.readU32(out.resolved_function_signature_type_id) ||
        !r.readU32(out.variadic_slice_param) || !r.readU8(is_variadic_slice) ||
        !r.readU8(auto_collect_tail) || is_variadic_slice > 1U || auto_collect_tail > 1U ||
        !r.readU32(out.alignmentBytes))
        return false;
    out.is_variadic_slice = is_variadic_slice != 0U;
    out.auto_collect_tail = auto_collect_tail != 0U;
    if (!r.readI64(out.int_val) || !r.readF64(out.flt_val))
        return false;
    uint32_t n = 0;
    if (!r.readU32(n))
        return false;
    if (!r.canReadU32Count(n))
        return false;
    out.args.resize(n);
    for (auto &a : out.args)
        if (!r.readU32(a))
            return false;
    if (!r.readU32(n))
        return false;
    if (!r.canReadU32Count(n))
        return false;
    out.arg_types.resize(n);
    for (auto &t : out.arg_types)
        if (!r.readU32(t))
            return false;
    if (!r.readU32(n))
        return false;
    if (!r.canReadU32Count(n))
        return false;
    out.ints.resize(n);
    for (auto &value : out.ints)
        if (!r.readU64(value))
            return false;
    return true;
}

} // namespace

bool encodeCode(const cache::Artifact &artifact, ByteWriter &w) {
    for (const auto &vtable : artifact.vtables) {
        if (vtable.slot_function_name_ids.size() !=
            vtable.slot_function_signature_type_ids.size())
            return false;
    }

    w.writeU32(static_cast<uint32_t>(artifact.exprs.size()));
    for (const auto &e : artifact.exprs)
        writeCompactExpr(e, w);
    w.writeU32(static_cast<uint32_t>(artifact.functions.size()));
    for (const auto &fn : artifact.functions) {
        w.writeU32(fn.name_id);
        w.writeU32(fn.signature_type_id);
        w.writeU8(fn.is_extern ? 1 : 0);
        w.writeU8(fn.is_foreign_c ? 1 : 0);
        w.writeU8(fn.is_variadic ? 1 : 0);
        w.writeU8(fn.is_state ? 1 : 0);
        w.writeU8(fn.uses_tailcc ? 1 : 0);
        w.writeU8(fn.discardable ? 1 : 0);
        w.writeU32(fn.return_type_id);
        w.writeU32(fn.machine_return_type_id);
        w.writeU32(fn.machine_id);
        w.writeU32(fn.instance_index);
        w.writeU32(fn.variadic_slice_param);
        w.writeU32(fn.foreign_adapter_name_id);
        w.writeU32(static_cast<uint32_t>(fn.param_type_ids.size()));
        for (auto id : fn.param_type_ids)
            w.writeU32(id);
        w.writeU32(static_cast<uint32_t>(fn.param_name_ids.size()));
        for (auto id : fn.param_name_ids)
            w.writeU32(id);
        w.writeU32(static_cast<uint32_t>(fn.param_slot_ids.size()));
        for (auto id : fn.param_slot_ids)
            w.writeU32(id);
        w.writeU32(static_cast<uint32_t>(fn.blocks.size()));
        for (const auto &blk : fn.blocks) {
            w.writeU32(static_cast<uint32_t>(blk.insts.size()));
            for (auto id : blk.insts)
                w.writeU32(id);
            w.writeU32(blk.terminator);
        }
    }
    w.writeU32(static_cast<uint32_t>(artifact.globals.size()));
    for (const auto &global : artifact.globals) {
        w.writeU32(global.name_id);
        w.writeU32(global.type_id);
        w.writeU32(global.init_expr);
        w.writeU32(global.alignmentBytes);
    }
    w.writeU32(static_cast<uint32_t>(artifact.vtables.size()));
    for (const auto &vtable : artifact.vtables) {
        w.writeU32(vtable.name_id);
        w.writeU32(static_cast<uint32_t>(vtable.slot_function_name_ids.size()));
        for (size_t slot = 0; slot < vtable.slot_function_name_ids.size(); ++slot) {
            w.writeU32(vtable.slot_function_name_ids[slot]);
            w.writeU32(vtable.slot_function_signature_type_ids[slot]);
        }
    }
    w.writeU32(static_cast<uint32_t>(artifact.canonical_mappings.size()));
    for (const auto &mapping : artifact.canonical_mappings) {
        w.writeU64(mapping.hi);
        w.writeU64(mapping.lo);
        w.writeU32(mapping.runtime_id);
    }
    return true;
}

bool decodeCode(ByteReader &r, cache::Artifact &out) {
    uint32_t n = 0;
    if (!r.readU32(n))
        return false;
    if (!r.canReadU32Count(n))
        return false;
    out.exprs.resize(n);
    for (auto &e : out.exprs)
        if (!readCompactExpr(r, e))
            return false;
    if (!r.readU32(n))
        return false;
    if (!r.canReadU32Count(n))
        return false;
    out.functions.resize(n);
    for (auto &fn : out.functions) {
        uint8_t ext = 0, foreign = 0, a = 0, b = 0, c = 0, d = 0;
        if (!r.readU32(fn.name_id) || !r.readU32(fn.signature_type_id) ||
            !r.readU8(ext) || !r.readU8(foreign) || !r.readU8(a) ||
            !r.readU8(b) || !r.readU8(c) || !r.readU8(d) || !r.readU32(fn.return_type_id) ||
            !r.readU32(fn.machine_return_type_id) || !r.readU32(fn.machine_id) ||
            !r.readU32(fn.instance_index) || !r.readU32(fn.variadic_slice_param) ||
            !r.readU32(fn.foreign_adapter_name_id))
            return false;
        if (fn.name_id < out.strings.size())
            fn.name = out.strings[fn.name_id];
        fn.is_extern    = ext != 0;
        fn.is_foreign_c = foreign != 0;
        fn.is_variadic  = a != 0;
        fn.is_state     = b != 0;
        fn.uses_tailcc  = c != 0;
        fn.discardable  = d != 0;
        uint32_t k      = 0;
        if (!r.readU32(k))
            return false;
        if (!r.canReadU32Count(k))
            return false;
        fn.param_type_ids.resize(k);
        for (auto &id : fn.param_type_ids)
            if (!r.readU32(id))
                return false;
        if (!r.readU32(k))
            return false;
        if (!r.canReadU32Count(k))
            return false;
        fn.param_name_ids.resize(k);
        for (auto &id : fn.param_name_ids)
            if (!r.readU32(id))
                return false;
        if (!r.readU32(k))
            return false;
        if (!r.canReadU32Count(k))
            return false;
        fn.param_slot_ids.resize(k);
        for (auto &id : fn.param_slot_ids)
            if (!r.readU32(id))
                return false;
        if (!r.readU32(k))
            return false;
        if (!r.canReadU32Count(k))
            return false;
        fn.blocks.resize(k);
        for (auto &blk : fn.blocks) {
            uint32_t m = 0;
            if (!r.readU32(m))
                return false;
            if (!r.canReadU32Count(m))
                return false;
            blk.insts.resize(m);
            for (auto &id : blk.insts)
                if (!r.readU32(id))
                    return false;
            if (!r.readU32(blk.terminator))
                return false;
        }
    }
    if (!r.readU32(n))
        return false;
    if (!r.canReadU32Count(n))
        return false;
    out.globals.resize(n);
    for (auto &global : out.globals)
        if (!r.readU32(global.name_id) || !r.readU32(global.type_id) ||
            !r.readU32(global.init_expr) || !r.readU32(global.alignmentBytes))
            return false;
    if (!r.readU32(n))
        return false;
    if (!r.canReadU32Count(n))
        return false;
    out.vtables.resize(n);
    for (auto &vtable : out.vtables) {
        if (!r.readU32(vtable.name_id))
            return false;
        uint32_t k = 0;
        if (!r.readU32(k))
            return false;
        if (!r.canReadU32Count(k, 8))
            return false;
        vtable.slot_function_name_ids.resize(k);
        vtable.slot_function_signature_type_ids.resize(k);
        for (size_t slot = 0; slot < k; ++slot)
            if (!r.readU32(vtable.slot_function_name_ids[slot]) ||
                !r.readU32(vtable.slot_function_signature_type_ids[slot]))
                return false;
    }
    if (!r.readU32(n))
        return false;
    if (!r.canReadU32Count(n, 20))
        return false;
    out.canonical_mappings.resize(n);
    for (auto &mapping : out.canonical_mappings) {
        if (!r.readU64(mapping.hi) || !r.readU64(mapping.lo) || !r.readU32(mapping.runtime_id))
            return false;
    }
    return true;
}

} // namespace zith::zirl

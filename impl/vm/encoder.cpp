// impl/vm/encoder.cpp — ZBC binary format encoder
#include "zith/vm.hpp"
#include <cstring>

namespace Zith {

static uint8_t* encode_uleb(uint8_t* p, uint64_t v) {
    int n = zith_uleb128_encode_uint64(p, v);
    return p + n;
}

static uint8_t* encode_sleb(uint8_t* p, int64_t v) {
    int n = zith_sleb128_encode_int64(p, v);
    return p + n;
}

static uint8_t* encode_u32(uint8_t* p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
    return p + 4;
}

static uint8_t* encode_u8(uint8_t* p, uint8_t v) {
    *p = v;
    return p + 1;
}

static uint8_t* encode_f64_bits(uint8_t* p, double v) {
    uint64_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    for (int i = 0; i < 8; i++) {
        p[i] = (bits >> (i * 8)) & 0xFF;
    }
    return p + 8;
}

static size_t compute_type_entry_size(const TypeEntry& entry) {
    size_t sz = 1; // base type byte
    switch (entry.base) {
        case ZITH_TYPE_FUNC:
        case ZITH_TYPE_STRUCT:
            // Worst case: ~10 bytes per field/param
            sz += 20;
            break;
        case ZITH_TYPE_ARRAY:
        case ZITH_TYPE_VECTOR:
        case ZITH_TYPE_PTR:
            sz += 5; // type_id uleb
            break;
        default:
            break;
    }
    return sz;
}

static uint8_t* encode_type_entry(uint8_t* p, const TypeEntry& entry) {
    p = encode_uleb(p, static_cast<uint64_t>(entry.base));
    // Compound type data would be encoded here based on entry.base
    // For now, placeholder — full implementation needs type detail storage
    return p;
}

static uint8_t* encode_global_entry(uint8_t* p, const GlobalEntry& entry) {
    p = encode_uleb(p, entry.name_string_id);
    p = encode_uleb(p, entry.type_id);
    p = encode_uleb(p, static_cast<uint64_t>(entry.linkage));
    p = encode_u8(p, entry.align_log2);
    p = encode_u8(p, entry.flags);
    p = encode_uleb(p, entry.init_value_id);
    return p;
}

static uint8_t* encode_function_entry(uint8_t* p, const FunctionEntry& entry) {
    p = encode_uleb(p, entry.name_string_id);
    p = encode_uleb(p, entry.type_id);
    p = encode_uleb(p, static_cast<uint64_t>(entry.linkage));
    p = encode_uleb(p, entry.attr_flags);
    p = encode_uleb(p, static_cast<uint64_t>(entry.calling_conv));
    p = encode_uleb(p, entry.n_params);
    p = encode_uleb(p, entry.n_blocks);
    return p;
}

size_t ZbcEncodeModule(const DecodedModule* module, uint8_t* out_buf, size_t buf_size) {
    // Compute required size
    size_t needed = sizeof(ModuleHeader);

    // Type table
    needed += 5; // uleb count
    for (auto& t : module->types) {
        needed += compute_type_entry_size(t);
    }

    // String table: count + blob size
    needed += 5; // uleb count
    for (auto& s : module->strings) {
        needed += 8 + s.size(); // offset (4) + length (4) + data
    }

    // Globals
    needed += 5; // uleb count
    for (auto& g : module->globals) {
        needed += 20; // worst case per global
    }

    // Functions
    needed += 5; // uleb count
    for (auto& f : module->functions) {
        needed += 40; // header per function
    }

    if (!out_buf || buf_size < needed) {
        return needed;
    }

    uint8_t* p = out_buf;

    // Header
    std::memcpy(p, ZBC_MAGIC, 4);
    p += 4;
    p = encode_u8(p, ZBC_VERSION);
    p = encode_u8(p, ZBC_FLAG_NONE);

    // Type table
    p = encode_uleb(p, module->types.size());
    for (auto& t : module->types) {
        p = encode_type_entry(p, t);
    }

    // String table
    p = encode_uleb(p, module->strings.size());
    uint32_t blob_offset = 0;
    for (auto& s : module->strings) {
        p = encode_u32(p, blob_offset);
        p = encode_u32(p, static_cast<uint32_t>(s.size()));
        std::memcpy(p, s.data(), s.size());
        p += s.size();
        blob_offset += static_cast<uint32_t>(s.size());
    }

    // Globals
    p = encode_uleb(p, module->globals.size());
    for (auto& g : module->globals) {
        p = encode_global_entry(p, g);
    }

    // Functions
    p = encode_uleb(p, module->functions.size());
    for (auto& f : module->functions) {
        p = encode_function_entry(p, f);
    }

    // Function bytecode blob (not yet implemented — placeholder)
    // In full implementation, each function's block data follows

    return static_cast<size_t>(p - out_buf);
}

size_t ZbcEncodeFunction(const FunctionEntry* fn, const uint8_t* block_data,
                         size_t block_data_size, uint8_t* out_buf, size_t buf_size) {
    size_t needed = 40 + block_data_size; // header + block data
    if (!out_buf || buf_size < needed) {
        return needed;
    }

    uint8_t* p = out_buf;
    p = encode_function_entry(p, *fn);

    // Encode basic blocks
    p = encode_uleb(p, fn->n_blocks);
    if (block_data && block_data_size > 0) {
        std::memcpy(p, block_data, block_data_size);
        p += block_data_size;
    }

    return static_cast<size_t>(p - out_buf);
}

} // namespace Zith

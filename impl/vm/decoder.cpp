// impl/vm/decoder.cpp — ZBC binary format decoder
#include "zith/vm.hpp"
#include <cstring>

namespace Zith {

static const uint8_t* decode_uleb(const uint8_t* p, uint64_t* out) {
    int n = zith_uleb128_decode_uint64(p, out);
    return p + n;
}

static const uint8_t* decode_u32(const uint8_t* p, uint32_t* out) {
    *out = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    return p + 4;
}

static const uint8_t* decode_u8(const uint8_t* p, uint8_t* out) {
    *out = *p;
    return p + 1;
}

static const uint8_t* decode_f64_bits(const uint8_t* p, double* out) {
    uint64_t bits = 0;
    for (int i = 0; i < 8; i++) {
        bits |= (uint64_t)p[i] << (i * 8);
    }
    std::memcpy(out, &bits, sizeof(bits));
    return p + 8;
}

int ZbcDecodeModule(const uint8_t* data, size_t size, DecodedModule* out) {
    if (size < sizeof(ModuleHeader)) return -1;

    const uint8_t* p = data;

    // Validate header
    if (std::memcmp(p, ZBC_MAGIC, 4) != 0) return -1;
    p += 4;

    uint8_t version;
    p = decode_u8(p, &version);
    if (version != ZBC_VERSION) return -1;

    uint8_t flags;
    p = decode_u8(p, &flags);

    // Type table
    uint64_t n_types;
    p = decode_uleb(p, &n_types);
    out->types.resize(n_types);
    for (uint64_t i = 0; i < n_types; i++) {
        uint64_t base;
        p = decode_uleb(p, &base);
        out->types[i].base = static_cast<ZithTypeId>(base);
        // Compound type data would be decoded here
    }

    // String table
    uint64_t n_strings;
    p = decode_uleb(p, &n_strings);
    out->strings.resize(n_strings);
    for (uint64_t i = 0; i < n_strings; i++) {
        uint32_t offset, length;
        p = decode_u32(p, &offset);
        p = decode_u32(p, &length);
        out->strings[i] = std::string_view(
            reinterpret_cast<const char*>(data + sizeof(ModuleHeader) + offset),
            length
        );
    }

    // Globals
    uint64_t n_globals;
    p = decode_uleb(p, &n_globals);
    out->globals.resize(n_globals);
    for (uint64_t i = 0; i < n_globals; i++) {
        uint64_t name_id, type_id, linkage;
        p = decode_uleb(p, &name_id);
        p = decode_uleb(p, &type_id);
        p = decode_uleb(p, &linkage);
        out->globals[i].name_string_id = static_cast<uint32_t>(name_id);
        out->globals[i].type_id = static_cast<uint32_t>(type_id);
        out->globals[i].linkage = static_cast<ZithLinkage>(linkage);
        p = decode_u8(p, &out->globals[i].align_log2);
        p = decode_u8(p, &out->globals[i].flags);
        uint64_t init_id;
        p = decode_uleb(p, &init_id);
        out->globals[i].init_value_id = static_cast<uint32_t>(init_id);
    }

    // Functions
    uint64_t n_functions;
    p = decode_uleb(p, &n_functions);
    out->functions.resize(n_functions);
    for (uint64_t i = 0; i < n_functions; i++) {
        uint64_t name_id, type_id, linkage, attrs, cc, n_params, n_blocks;
        p = decode_uleb(p, &name_id);
        p = decode_uleb(p, &type_id);
        p = decode_uleb(p, &linkage);
        p = decode_uleb(p, &attrs);
        p = decode_uleb(p, &cc);
        p = decode_uleb(p, &n_params);
        p = decode_uleb(p, &n_blocks);
        out->functions[i].name_string_id = static_cast<uint32_t>(name_id);
        out->functions[i].type_id = static_cast<uint32_t>(type_id);
        out->functions[i].linkage = static_cast<ZithLinkage>(linkage);
        out->functions[i].attr_flags = static_cast<uint32_t>(attrs);
        out->functions[i].calling_conv = static_cast<ZithCallingConv>(cc);
        out->functions[i].n_params = static_cast<uint32_t>(n_params);
        out->functions[i].n_blocks = static_cast<uint32_t>(n_blocks);
    }

    // Function bytecode starts here
    out->function_data = p;
    out->function_data_size = static_cast<size_t>((data + size) - p);

    return 0;
}

int ZbcDecodeFunction(const uint8_t* data, size_t size, FunctionEntry* out_fn,
                      const uint8_t** out_block_data, size_t* out_block_data_size) {
    const uint8_t* p = data;
    const uint8_t* end = data + size;

    uint64_t name_id, type_id, linkage, attrs, cc, n_params, n_blocks;
    p = decode_uleb(p, &name_id);
    p = decode_uleb(p, &type_id);
    p = decode_uleb(p, &linkage);
    p = decode_uleb(p, &attrs);
    p = decode_uleb(p, &cc);
    p = decode_uleb(p, &n_params);
    p = decode_uleb(p, &n_blocks);

    out_fn->name_string_id = static_cast<uint32_t>(name_id);
    out_fn->type_id = static_cast<uint32_t>(type_id);
    out_fn->linkage = static_cast<ZithLinkage>(linkage);
    out_fn->attr_flags = static_cast<uint32_t>(attrs);
    out_fn->calling_conv = static_cast<ZithCallingConv>(cc);
    out_fn->n_params = static_cast<uint32_t>(n_params);
    out_fn->n_blocks = static_cast<uint32_t>(n_blocks);

    *out_block_data = p;
    *out_block_data_size = static_cast<size_t>(end - p);

    return 0;
}

} // namespace Zith

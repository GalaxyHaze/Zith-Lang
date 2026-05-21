// include/zith/leb128.h — Unsigned and signed LEB128 encode/decode
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Encode unsigned LEB128. Returns number of bytes written (1–5 for uint32, 1–9 for uint64).
static inline int zith_uleb128_encode_uint32(uint8_t* buf, uint32_t value) {
    int n = 0;
    do {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if (value != 0) byte |= 0x80;
        buf[n++] = byte;
    } while (value != 0);
    return n;
}

static inline int zith_uleb128_encode_uint64(uint8_t* buf, uint64_t value) {
    int n = 0;
    do {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if (value != 0) byte |= 0x80;
        buf[n++] = byte;
    } while (value != 0);
    return n;
}

// Encode signed LEB128. Returns number of bytes written.
static inline int zith_sleb128_encode_int32(uint8_t* buf, int32_t value) {
    int n = 0;
    int more = 1;
    while (more) {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        // Sign bit of byte is second high order bit (0x40)
        if ((value == 0 && !(byte & 0x40)) ||
            (value == -1 && (byte & 0x40))) {
            more = 0;
        } else {
            byte |= 0x80;
        }
        buf[n++] = byte;
    }
    return n;
}

static inline int zith_sleb128_encode_int64(uint8_t* buf, int64_t value) {
    int n = 0;
    int more = 1;
    while (more) {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if ((value == 0 && !(byte & 0x40)) ||
            (value == -1 && (byte & 0x40))) {
            more = 0;
        } else {
            byte |= 0x80;
        }
        buf[n++] = byte;
    }
    return n;
}

// Decode unsigned LEB128. Returns number of bytes read.
static inline int zith_uleb128_decode_uint32(const uint8_t* buf, uint32_t* out) {
    uint32_t result = 0;
    int shift = 0;
    int n = 0;
    while (1) {
        uint8_t byte = buf[n++];
        result |= (uint32_t)(byte & 0x7F) << shift;
        shift += 7;
        if (!(byte & 0x80)) break;
    }
    *out = result;
    return n;
}

static inline int zith_uleb128_decode_uint64(const uint8_t* buf, uint64_t* out) {
    uint64_t result = 0;
    int shift = 0;
    int n = 0;
    while (1) {
        uint8_t byte = buf[n++];
        result |= (uint64_t)(byte & 0x7F) << shift;
        shift += 7;
        if (!(byte & 0x80)) break;
    }
    *out = result;
    return n;
}

// Decode signed LEB128. Returns number of bytes read.
static inline int zith_sleb128_decode_int32(const uint8_t* buf, int32_t* out) {
    int32_t result = 0;
    int shift = 0;
    int n = 0;
    uint8_t byte;
    do {
        byte = buf[n++];
        result |= (int32_t)(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);
    // Sign extend if needed
    if (shift < 32 && (byte & 0x40)) {
        result |= ~((int32_t)0) << shift;
    }
    *out = result;
    return n;
}

static inline int zith_sleb128_decode_int64(const uint8_t* buf, int64_t* out) {
    int64_t result = 0;
    int shift = 0;
    int n = 0;
    uint8_t byte;
    do {
        byte = buf[n++];
        result |= (int64_t)(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);
    if (shift < 64 && (byte & 0x40)) {
        result |= ~((int64_t)0) << shift;
    }
    *out = result;
    return n;
}

// Compute encoded size without writing.
static inline int zith_uleb128_size_uint32(uint32_t value) {
    if (value == 0) return 1;
    int n = 0;
    while (value > 0) { value >>= 7; n++; }
    return n;
}

static inline int zith_uleb128_size_uint64(uint64_t value) {
    if (value == 0) return 1;
    int n = 0;
    while (value > 0) { value >>= 7; n++; }
    return n;
}

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace zith::leb128 {

inline int encode(uint8_t* buf, uint32_t v) { return zith_uleb128_encode_uint32(buf, v); }
inline int encode(uint8_t* buf, uint64_t v) { return zith_uleb128_encode_uint64(buf, v); }
inline int encode(uint8_t* buf, int32_t v)  { return zith_sleb128_encode_int32(buf, v); }
inline int encode(uint8_t* buf, int64_t v)  { return zith_sleb128_encode_int64(buf, v); }

inline int decode(const uint8_t* buf, uint32_t* out) { return zith_uleb128_decode_uint32(buf, out); }
inline int decode(const uint8_t* buf, uint64_t* out) { return zith_uleb128_decode_uint64(buf, out); }
inline int decode(const uint8_t* buf, int32_t* out)  { return zith_sleb128_decode_int32(buf, out); }
inline int decode(const uint8_t* buf, int64_t* out)  { return zith_sleb128_decode_int64(buf, out); }

inline int size(uint32_t v) { return zith_uleb128_size_uint32(v); }
inline int size(uint64_t v) { return zith_uleb128_size_uint64(v); }

} // namespace zith::leb128
#endif

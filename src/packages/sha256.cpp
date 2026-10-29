#include "packages/sha256.hpp"

#include <array>
#include <bit>
#include <cstdint>

namespace zith::packages {
namespace {

constexpr std::array<uint32_t, 64> kRoundConstants = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
    0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
    0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
    0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
    0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
    0xc67178f2U,
};

using State = std::array<uint32_t, 8>;
using Block = std::array<uint8_t, 64>;

void compress(State &state, const Block &block) {
    std::array<uint32_t, 64> words{};
    for (size_t index = 0; index < 16U; ++index) {
        const size_t offset = index * 4U;
        words[index]        = (static_cast<uint32_t>(block[offset]) << 24U) |
                              (static_cast<uint32_t>(block[offset + 1U]) << 16U) |
                              (static_cast<uint32_t>(block[offset + 2U]) << 8U) |
                              static_cast<uint32_t>(block[offset + 3U]);
    }

    for (size_t index = 16U; index < words.size(); ++index) {
        const uint32_t x      = words[index - 15U];
        const uint32_t y      = words[index - 2U];
        const uint32_t sigma0 = std::rotr(x, 7) ^ std::rotr(x, 18) ^ (x >> 3U);
        const uint32_t sigma1 = std::rotr(y, 17) ^ std::rotr(y, 19) ^ (y >> 10U);
        words[index]          = words[index - 16U] + sigma0 + words[index - 7U] + sigma1;
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];

    for (size_t index = 0; index < words.size(); ++index) {
        const uint32_t sum1     = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
        const uint32_t choice   = (e & f) ^ (~e & g);
        const uint32_t temp1    = h + sum1 + choice + kRoundConstants[index] + words[index];
        const uint32_t sum0     = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
        const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t temp2    = sum0 + majority;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void copyBlock(const std::string_view input, const size_t offset, Block &block) {
    for (size_t index = 0; index < block.size(); ++index)
        block[index] = static_cast<uint8_t>(static_cast<unsigned char>(input[offset + index]));
}

} // namespace

std::string sha256Hex(const std::string_view input) {
    State state = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                   0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};

    Block block{};
    size_t offset = 0;
    while (input.size() - offset >= block.size()) {
        copyBlock(input, offset, block);
        compress(state, block);
        offset += block.size();
    }

    std::array<uint8_t, 128> tail{};
    const size_t tailSize = input.size() - offset;
    for (size_t index = 0; index < tailSize; ++index)
        tail[index] = static_cast<uint8_t>(static_cast<unsigned char>(input[offset + index]));
    tail[tailSize] = 0x80U;

    const size_t paddedSize  = tailSize < 56U ? 64U : 128U;
    const uint64_t bitLength = static_cast<uint64_t>(input.size()) * 8U;
    for (size_t index = 0; index < 8U; ++index) {
        const auto shift              = static_cast<unsigned int>(index * 8U);
        tail[paddedSize - 1U - index] = static_cast<uint8_t>(bitLength >> shift);
    }

    for (size_t blockOffset = 0; blockOffset < paddedSize; blockOffset += block.size()) {
        for (size_t index = 0; index < block.size(); ++index)
            block[index] = tail[blockOffset + index];
        compress(state, block);
    }

    constexpr char kHexDigits[] = "0123456789abcdef";
    std::string result;
    result.reserve(64U);
    for (const uint32_t word : state) {
        for (unsigned int nibble = 0; nibble < 8U; ++nibble) {
            const auto shift = (7U - nibble) * 4U;
            result.push_back(kHexDigits[(word >> shift) & 0x0fU]);
        }
    }
    return result;
}

} // namespace zith::packages

/* Copyright 2026 Hans-Einar. MIT; see LICENSE. */
#include "emu_sha_internal.h"
#include <string.h>
static const uint32_t gSHA256Constants[64] =
{
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

static uint32_t rotate_right(uint32_t aValue, unsigned aBits)
{
    return (aValue >> aBits) | (aValue << (32u - aBits));
}

static void sha256_transform(struct sha256_context *aContext,
                             const uint8_t aBlock[64])
{
    uint32_t words[64];
    uint32_t a, b, c, d, e, f, g, h;
    unsigned i;

    for (i = 0; i < 16u; i++)
    {
        words[i] = ((uint32_t)aBlock[i * 4u] << 24) |
                   ((uint32_t)aBlock[i * 4u + 1u] << 16) |
                   ((uint32_t)aBlock[i * 4u + 2u] << 8) |
                   (uint32_t)aBlock[i * 4u + 3u];
    }
    for (i = 16u; i < 64u; i++)
    {
        uint32_t s0 = rotate_right(words[i - 15u], 7) ^
                      rotate_right(words[i - 15u], 18) ^
                      (words[i - 15u] >> 3);
        uint32_t s1 = rotate_right(words[i - 2u], 17) ^
                      rotate_right(words[i - 2u], 19) ^
                      (words[i - 2u] >> 10);
        words[i] = words[i - 16u] + s0 + words[i - 7u] + s1;
    }

    a = aContext->state[0]; b = aContext->state[1];
    c = aContext->state[2]; d = aContext->state[3];
    e = aContext->state[4]; f = aContext->state[5];
    g = aContext->state[6]; h = aContext->state[7];
    for (i = 0; i < 64u; i++)
    {
        uint32_t s1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^
                      rotate_right(e, 25);
        uint32_t choice = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + choice + gSHA256Constants[i] + words[i];
        uint32_t s0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^
                      rotate_right(a, 22);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + majority;
        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }
    aContext->state[0] += a; aContext->state[1] += b;
    aContext->state[2] += c; aContext->state[3] += d;
    aContext->state[4] += e; aContext->state[5] += f;
    aContext->state[6] += g; aContext->state[7] += h;
}

void sha256_init(struct sha256_context *aContext)
{
    static const uint32_t initial[8] =
    {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
    };
    memcpy(aContext->state, initial, sizeof(initial));
    aContext->bit_count = 0;
    aContext->used = 0;
}

void sha256_update(struct sha256_context *aContext,
                          const uint8_t *aData, size_t aSize)
{
    while (aSize != 0u)
    {
        size_t available = sizeof(aContext->block) - aContext->used;
        size_t take = aSize < available ? aSize : available;
        memcpy(aContext->block + aContext->used, aData, take);
        aContext->used += take;
        aData += take;
        aSize -= take;
        if (aContext->used == sizeof(aContext->block))
        {
            sha256_transform(aContext, aContext->block);
            aContext->bit_count += 512u;
            aContext->used = 0;
        }
    }
}

void sha256_final(struct sha256_context *aContext, uint8_t aDigest[32])
{
    uint64_t total_bits = aContext->bit_count + (uint64_t)aContext->used * 8u;
    unsigned i;
    aContext->block[aContext->used++] = 0x80u;
    if (aContext->used > 56u)
    {
        while (aContext->used < 64u)
            aContext->block[aContext->used++] = 0;
        sha256_transform(aContext, aContext->block);
        aContext->used = 0;
    }
    while (aContext->used < 56u)
        aContext->block[aContext->used++] = 0;
    for (i = 0; i < 8u; i++)
        aContext->block[63u - i] = (uint8_t)(total_bits >> (i * 8u));
    sha256_transform(aContext, aContext->block);
    for (i = 0; i < 8u; i++)
    {
        aDigest[i * 4u] = (uint8_t)(aContext->state[i] >> 24);
        aDigest[i * 4u + 1u] = (uint8_t)(aContext->state[i] >> 16);
        aDigest[i * 4u + 2u] = (uint8_t)(aContext->state[i] >> 8);
        aDigest[i * 4u + 3u] = (uint8_t)aContext->state[i];
    }
}

void sha256_hex(const uint8_t *aData, size_t aSize,
                       char aHex[65])
{
    static const char digits[] = "0123456789abcdef";
    struct sha256_context context;
    uint8_t digest[32];
    unsigned i;
    sha256_init(&context);
    sha256_update(&context, aData, aSize);
    sha256_final(&context, digest);
    for (i = 0; i < sizeof(digest); i++)
    {
        aHex[i * 2u] = digits[digest[i] >> 4];
        aHex[i * 2u + 1u] = digits[digest[i] & 0x0fu];
    }
    aHex[64] = '\0';
}

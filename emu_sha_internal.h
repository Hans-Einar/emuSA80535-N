/* Private SHA256, extracted from debugger. Copyright 2026 Hans-Einar. MIT. */
#ifndef EMU_SHA_INTERNAL_H
#define EMU_SHA_INTERNAL_H
#include <stdint.h>
#include <stddef.h>
struct sha256_context
{
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t block[64];
    size_t used;
};

void sha256_init(struct sha256_context *);
void sha256_update(struct sha256_context *, const uint8_t *, size_t);
void sha256_final(struct sha256_context *, uint8_t [32]);
void sha256_hex(const uint8_t *, size_t, char [65]);
#endif

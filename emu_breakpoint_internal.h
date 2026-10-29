/* Shared authoritative CODE bitmap primitive. Copyright 2026 Hans-Einar. MIT. */
#ifndef EMU_BREAKPOINT_INTERNAL_H
#define EMU_BREAKPOINT_INTERNAL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#define EMU_BREAKPOINT_BYTES 8192u
static inline bool emu_breakpoint_at(const uint8_t *bitmap, uint16_t address)
{
    return (bitmap[address >> 3] & (uint8_t)(1u << (address & 7u))) != 0;
}
static inline bool emu_breakpoint_replace(uint8_t *bitmap, const uint16_t *addresses, size_t count)
{
    uint8_t replacement[EMU_BREAKPOINT_BYTES] = {0};
    size_t i;
    for (i = 0; i < count; ++i)
    {
        if (emu_breakpoint_at(replacement, addresses[i]))
            return false;
        replacement[addresses[i] >> 3] |= (uint8_t)(1u << (addresses[i] & 7u));
    }
    memcpy(bitmap, replacement, sizeof replacement);
    return true;
}
#endif

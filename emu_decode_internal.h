/* Shared authoritative decoder history. Internal, same-build interface.
 * Copyright 2006 Jari Komppa; Copyright 2026 Hans-Einar. MIT; see LICENSE. */
#ifndef EMU_DECODE_INTERNAL_H
#define EMU_DECODE_INTERNAL_H
#include <emusa80535/emu_debug.h>
struct emu_decode_cache {
    uint16_t predecessor[65536];
    uint8_t known[65536];
};
void emu_decode_clear(struct emu_decode_cache *cache);
uint8_t emu_decode_at(struct em8051 *cpu, uint16_t address, char *text);
void emu_decode_completed(struct emu_decode_cache *cache, uint16_t pc, uint8_t size,
                          uint16_t resulting_pc);
enum em8051_debug_status emu_decode_window(struct em8051 *cpu, struct emu_decode_cache *cache,
                                           uint16_t reference, int32_t byte_offset,
                                           int32_t instruction_offset, size_t count,
                                           struct em8051_debug_decoded *records);
#endif

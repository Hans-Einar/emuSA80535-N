/* Shared authoritative decoder history extracted from emu_debug.c.
 * Copyright 2006 Jari Komppa; Copyright 2026 Hans-Einar. MIT; see LICENSE. */
#include "emu_decode_internal.h"
#include <stdlib.h>
#include <string.h>
#define CODE_SIZE 65536u
void emu_decode_clear(struct emu_decode_cache *cache) {
    memset(cache->known, 0, sizeof(cache->known));
}
uint8_t emu_decode_at(struct em8051 *cpu, uint16_t address, char *text) {
    return cpu->dec[cpu->mCodeMem[address]](cpu, address, text);
}
void emu_decode_completed(struct emu_decode_cache *cache, uint16_t pc, uint8_t size,
                          uint16_t resulting_pc) {
    uint32_t sequential = (uint32_t)pc + size;
    if (size != 0 && sequential < CODE_SIZE && resulting_pc == sequential) {
        cache->predecessor[sequential] = pc;
        cache->known[sequential] = 1;
    }
}
static enum em8051_debug_status decode_record(struct em8051 *cpu, uint32_t aAddress,
                                              struct em8051_debug_decoded *aRecord) {
    uint8_t size;
    if (aAddress > 0xffffu)
        return EM8051_DEBUG_RANGE;
    memset(aRecord, 0, sizeof(*aRecord));
    aRecord->address = (uint16_t)aAddress;
    size = emu_decode_at(cpu, (uint16_t)aAddress, aRecord->text);
    if (size == 0u || aAddress + size > CODE_SIZE)
        return EM8051_DEBUG_RANGE;
    aRecord->size = size;
    aRecord->valid = true;
    return EM8051_DEBUG_OK;
}

static void invalid_record(uint16_t aAddress, struct em8051_debug_decoded *aRecord) {
    memset(aRecord, 0, sizeof(*aRecord));
    aRecord->address = aAddress;
    aRecord->size = 1;
    aRecord->valid = false;
    memcpy(aRecord->text, "<invalid>", sizeof("<invalid>"));
}

enum em8051_debug_status emu_decode_window(struct em8051 *cpu, struct emu_decode_cache *cache,
                                           uint16_t aReference, int32_t aByteOffset,
                                           int32_t aInstructionOffset, size_t aCount,
                                           struct em8051_debug_decoded *aRecords) {
    struct em8051_debug_decoded staged[EM8051_DEBUG_MAX_DECODE_INSTRUCTIONS];
    struct em8051_debug_decoded *destination = aRecords;
    int64_t base;
    uint32_t cursor;
    size_t output = 0;
    uint16_t staged_addresses[EM8051_DEBUG_MAX_DECODE_INSTRUCTIONS];
    uint16_t staged_predecessors[EM8051_DEBUG_MAX_DECODE_INSTRUCTIONS];
    size_t staged_count = 0;
    int32_t step;
    enum em8051_debug_status status;

    if (!cpu || !cache || !aRecords || aCount == 0u ||
        aCount > EM8051_DEBUG_MAX_DECODE_INSTRUCTIONS || aByteOffset < -65536 ||
        aByteOffset > 65536 || aInstructionOffset < -65536 || aInstructionOffset > 65536)
        return EM8051_DEBUG_INVALID_ARGUMENT;
    aRecords = staged;
    base = (int64_t)aReference + aByteOffset;
    if (base < 0 || base > 0xffff)
        return EM8051_DEBUG_RANGE;

    if (aInstructionOffset < 0) {
        size_t predecessor_count = (size_t)(-(int64_t)aInstructionOffset);
        size_t temporary_count = predecessor_count < aCount ? predecessor_count : aCount;
        struct em8051_debug_decoded *temporary = NULL;
        bool known = true;
        cursor = (uint32_t)base;
        if (temporary_count != 0u) {
            temporary = (struct em8051_debug_decoded *)calloc(temporary_count, sizeof(*temporary));
            if (!temporary)
                return EM8051_DEBUG_INTERNAL;
        }
        for (step = 0; step < -aInstructionOffset; step++) {
            struct em8051_debug_decoded record;
            if (known && cache->known[cursor]) {
                cursor = cache->predecessor[cursor];
                status = decode_record(cpu, cursor, &record);
                if (status != EM8051_DEBUG_OK) {
                    free(temporary);
                    return status;
                }
            } else {
                known = false;
                if (cursor == 0u) {
                    free(temporary);
                    return EM8051_DEBUG_RANGE;
                }
                cursor--;
                invalid_record((uint16_t)cursor, &record);
            }
            if ((size_t)step >= predecessor_count - temporary_count)
                temporary[predecessor_count - (size_t)step - 1u] = record;
        }
        for (output = 0; output < temporary_count; output++)
            aRecords[output] = temporary[output];
        free(temporary);
        cursor = (uint32_t)base;
    } else {
        cursor = (uint32_t)base;
        for (step = 0; step < aInstructionOffset; step++) {
            struct em8051_debug_decoded skipped;
            status = decode_record(cpu, cursor, &skipped);
            if (status != EM8051_DEBUG_OK)
                return status;
            cursor += skipped.size;
            if (cursor > 0xffffu)
                return EM8051_DEBUG_RANGE;
        }
    }

    while (output < aCount) {
        uint32_t next;
        status = decode_record(cpu, cursor, &aRecords[output]);
        if (status != EM8051_DEBUG_OK)
            return status;
        next = cursor + aRecords[output].size;
        if (next < CODE_SIZE) {
            staged_addresses[staged_count] = (uint16_t)next;
            staged_predecessors[staged_count] = (uint16_t)cursor;
            staged_count++;
        }
        cursor = next;
        output++;
        if (output < aCount && cursor > 0xffffu)
            return EM8051_DEBUG_RANGE;
    }
    for (output = 0; output < staged_count; output++) {
        cache->predecessor[staged_addresses[output]] = staged_predecessors[output];
        cache->known[staged_addresses[output]] = 1;
    }
    memcpy(destination, staged, aCount * sizeof(*destination));
    return EM8051_DEBUG_OK;
}

/* Generic deterministic debugger facade for the 8051 emulator.
 * Copyright 2006 Jari Komppa
 * Copyright 2026 Hans-Einar
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the conditions in the repository LICENSE file.
 */

#ifndef EMU_DEBUG_H
#define EMU_DEBUG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "emu8051.h"

#define EM8051_DEBUG_MAX_BREAKPOINTS 1024u
#define EM8051_DEBUG_MAX_RUN_INSTRUCTIONS 1000000u
#define EM8051_DEBUG_MAX_DECODE_INSTRUCTIONS 256u
#define EM8051_DEBUG_SHA256_HEX_SIZE 65u
#define EM8051_DEBUG_DECODE_TEXT_SIZE 96u

enum em8051_debug_result_kind
{
    EM8051_DEBUG_ARCHITECTURAL_STOP = 0,
    EM8051_DEBUG_YIELD
};

enum em8051_debug_reason
{
    EM8051_DEBUG_REASON_ENTRY = 0,
    EM8051_DEBUG_REASON_BREAKPOINT,
    EM8051_DEBUG_REASON_STEP,
    EM8051_DEBUG_REASON_EXCEPTION,
    EM8051_DEBUG_REASON_HALT,
    EM8051_DEBUG_REASON_YIELD
};

struct em8051_debug_snapshot
{
    uint16_t pc;
    uint8_t a;
    uint8_t b;
    uint8_t psw;
    uint8_t sp;
    uint16_t dptr;
    uint8_t r[8];
    enum em8051_variant variant;
    uint64_t instruction_count;
    uint64_t machine_cycle_count;
    enum em8051_debug_result_kind result_kind;
    enum em8051_debug_reason reason;
    int exception_code;
};

struct em8051_debug_decoded
{
    uint16_t address;
    uint8_t size;
    bool valid;
    char text[EM8051_DEBUG_DECODE_TEXT_SIZE];
};

enum em8051_debug_status
{
    EM8051_DEBUG_OK = 0,
    EM8051_DEBUG_INVALID_ARGUMENT,
    EM8051_DEBUG_INVALID_STATE,
    EM8051_DEBUG_IMAGE_READ,
    EM8051_DEBUG_IMAGE_SIZE,
    EM8051_DEBUG_IMAGE_HASH,
    EM8051_DEBUG_RANGE,
    EM8051_DEBUG_BREAKPOINT_LIMIT,
    EM8051_DEBUG_INTERNAL
};

struct em8051_debugger;

/* Capture one value-only architectural snapshot. No CPU pointer, callback,
 * padding or private object representation appears in the result. */
bool em8051_debug_capture_snapshot(
    const struct em8051 *aCPU,
    enum em8051_debug_result_kind aResultKind,
    enum em8051_debug_reason aReason,
    int aExceptionCode,
    struct em8051_debug_snapshot *aSnapshot);

struct em8051_debugger *em8051_debugger_create(void);
void em8051_debugger_destroy(struct em8051_debugger *aDebugger);

enum em8051_debug_status em8051_debugger_load(
    struct em8051_debugger *aDebugger, const char *aAbsolutePath,
    const char *aExpectedSha256,
    char aActualSha256[EM8051_DEBUG_SHA256_HEX_SIZE]);
enum em8051_debug_status em8051_debugger_reset(
    struct em8051_debugger *aDebugger, uint32_t aSeed, uint16_t aEntry,
    struct em8051_debug_snapshot *aSnapshot);
enum em8051_debug_status em8051_debugger_get_state(
    const struct em8051_debugger *aDebugger,
    struct em8051_debug_snapshot *aSnapshot);
enum em8051_debug_status em8051_debugger_decode_code(
    struct em8051_debugger *aDebugger, uint16_t aReference,
    int32_t aByteOffset, int32_t aInstructionOffset, size_t aCount,
    struct em8051_debug_decoded *aRecords);
enum em8051_debug_status em8051_debugger_replace_breakpoints(
    struct em8051_debugger *aDebugger, const uint16_t *aAddresses,
    size_t aCount);
enum em8051_debug_status em8051_debugger_run(
    struct em8051_debugger *aDebugger, uint32_t aMaxInstructions,
    struct em8051_debug_snapshot *aSnapshot);
enum em8051_debug_status em8051_debugger_step(
    struct em8051_debugger *aDebugger,
    struct em8051_debug_snapshot *aSnapshot);

const char *em8051_debug_variant_name(enum em8051_variant aVariant);
const char *em8051_debug_reason_name(enum em8051_debug_reason aReason);
const char *em8051_debug_exception_code(int aCode);
const char *em8051_debug_exception_message(int aCode);

#endif

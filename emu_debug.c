/* Generic deterministic debugger facade for the 8051 emulator.
 * Copyright 2006 Jari Komppa
 * Copyright 2026 Hans-Einar
 * Licensed under the repository MIT license.
 */

#include <emusa80535/emu_debug.h>
#include "emu_decode_internal.h"
#include "emu_sha_internal.h"
#include "emu_breakpoint_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#ifdef exception_code
#undef exception_code
#endif
#endif

#define CODE_SIZE 65536u
#define BREAKPOINT_BYTES (CODE_SIZE / 8u)

struct em8051_debugger
{
    struct em8051 cpu;
    uint8_t code[CODE_SIZE];
    uint8_t image[CODE_SIZE];
    struct emu_decode_cache decode_cache;
    uint8_t breakpoints[BREAKPOINT_BYTES];
    struct em8051_debug_snapshot boundary;
    uint16_t observed_pc;
    uint8_t observed_size;
    bool loaded;
    bool boundary_valid;
    bool observed_instruction;
};

static bool absolute_path(const char *aPath)
{
    if (!aPath || !aPath[0])
        return false;
#ifdef _WIN32
    if (((aPath[0] >= 'A' && aPath[0] <= 'Z') ||
         (aPath[0] >= 'a' && aPath[0] <= 'z')) &&
        aPath[1] == ':' && (aPath[2] == '\\' || aPath[2] == '/'))
        return true;
    return aPath[0] == '\\' && aPath[1] == '\\';
#else
    return aPath[0] == '/';
#endif
}

static FILE *open_binary_utf8(const char *aPath)
{
#ifdef _WIN32
    int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, aPath,
                                     -1, NULL, 0);
    wchar_t *wide_path;
    FILE *result;
    if (needed <= 0)
        return NULL;
    wide_path = (wchar_t *)malloc((size_t)needed * sizeof(wchar_t));
    if (!wide_path)
        return NULL;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, aPath, -1,
                            wide_path, needed) <= 0)
    {
        free(wide_path);
        return NULL;
    }
    result = _wfopen(wide_path, L"rb");
    free(wide_path);
    return result;
#else
    return fopen(aPath, "rb");
#endif
}

static void clear_predecessors(struct em8051_debugger *aDebugger)
{
    emu_decode_clear(&aDebugger->decode_cache);
}

static uint8_t decode_at(struct em8051_debugger *aDebugger, uint16_t aAddress,
                         char *aText)
{
    return emu_decode_at(&aDebugger->cpu, aAddress, aText);
}

static void observe_instruction(const struct em8051_trace_record *aRecord,
                                void *aUser)
{
    struct em8051_debugger *debugger = (struct em8051_debugger *)aUser;
    char ignored[EM8051_DEBUG_DECODE_TEXT_SIZE];
    if (aRecord->type != EM8051_TRACE_INSTRUCTION)
        return;
    debugger->observed_pc = aRecord->pc;
    debugger->observed_size = decode_at(debugger, aRecord->pc, ignored);
    debugger->observed_instruction = debugger->observed_size != 0u;
}

bool em8051_debug_capture_snapshot(
    const struct em8051 *aCPU,
    enum em8051_debug_result_kind aResultKind,
    enum em8051_debug_reason aReason,
    int aExceptionCode,
    struct em8051_debug_snapshot *aSnapshot)
{
    uint8_t bank;
    unsigned i;
    if (!aCPU || !aSnapshot)
        return false;
    memset(aSnapshot, 0, sizeof(*aSnapshot));
    aSnapshot->pc = aCPU->mPC;
    aSnapshot->a = aCPU->mSFR[REG_ACC];
    aSnapshot->b = aCPU->mSFR[REG_B];
    aSnapshot->psw = aCPU->mSFR[REG_PSW];
    aSnapshot->sp = aCPU->mSFR[REG_SP];
    aSnapshot->dptr = (uint16_t)(((uint16_t)aCPU->mSFR[REG_DPH] << 8) |
                                aCPU->mSFR[REG_DPL]);
    bank = (uint8_t)((aSnapshot->psw >> PSW_RS0) & 3u);
    for (i = 0; i < 8u; i++)
        aSnapshot->r[i] = aCPU->mLowerData[(unsigned)bank * 8u + i];
    aSnapshot->variant = aCPU->mVariant;
    aSnapshot->instruction_count = aCPU->mInstructionCount;
    aSnapshot->machine_cycle_count = aCPU->mMachineCycleCount;
    aSnapshot->result_kind = aResultKind;
    aSnapshot->reason = aReason;
    aSnapshot->exception_code = aExceptionCode;
    return true;
}

struct em8051_debugger *em8051_debugger_create(void)
{
    struct em8051_debugger *debugger =
        (struct em8051_debugger *)calloc(1, sizeof(*debugger));
    if (!debugger)
        return NULL;
    debugger->cpu.mCodeMem = debugger->code;
    debugger->cpu.mCodeMemMaxIdx = 0xffffu;
    if (em8051_init_variant(&debugger->cpu, EM8051_VARIANT_SAB80535) != 0)
    {
        free(debugger);
        return NULL;
    }
    em8051_set_trace(&debugger->cpu, observe_instruction, debugger);
    return debugger;
}

void em8051_debugger_destroy(struct em8051_debugger *aDebugger)
{
    if (!aDebugger)
        return;
    memset(aDebugger, 0, sizeof(*aDebugger));
    free(aDebugger);
}

enum em8051_debug_status em8051_debugger_load(
    struct em8051_debugger *aDebugger, const char *aAbsolutePath,
    const char *aExpectedSha256,
    char aActualSha256[EM8051_DEBUG_SHA256_HEX_SIZE])
{
    uint8_t *candidate;
    FILE *input;
    size_t got;
    int trailing;
    char actual[EM8051_DEBUG_SHA256_HEX_SIZE];

    if (!aDebugger || !absolute_path(aAbsolutePath) || !aExpectedSha256 ||
        strlen(aExpectedSha256) != 64u || !aActualSha256)
        return EM8051_DEBUG_INVALID_ARGUMENT;
    candidate = (uint8_t *)malloc(CODE_SIZE);
    if (!candidate)
        return EM8051_DEBUG_INTERNAL;
    input = open_binary_utf8(aAbsolutePath);
    if (!input)
    {
        free(candidate);
        return EM8051_DEBUG_IMAGE_READ;
    }
    got = fread(candidate, 1, CODE_SIZE, input);
    trailing = fgetc(input);
    if (ferror(input))
    {
        (void)fclose(input);
        free(candidate);
        return EM8051_DEBUG_IMAGE_READ;
    }
    if (fclose(input) != 0)
    {
        free(candidate);
        return EM8051_DEBUG_IMAGE_READ;
    }
    if (got != CODE_SIZE || trailing != EOF)
    {
        free(candidate);
        return EM8051_DEBUG_IMAGE_SIZE;
    }
    sha256_hex(candidate, CODE_SIZE, actual);
    memcpy(aActualSha256, actual, sizeof(actual));
    if (strcmp(actual, aExpectedSha256) != 0)
    {
        free(candidate);
        return EM8051_DEBUG_IMAGE_HASH;
    }
    memcpy(aDebugger->image, candidate, CODE_SIZE);
    memcpy(aDebugger->code, candidate, CODE_SIZE);
    free(candidate);
    aDebugger->loaded = true;
    aDebugger->boundary_valid = false;
    clear_predecessors(aDebugger);
    return EM8051_DEBUG_OK;
}

enum em8051_debug_status em8051_debugger_reset(
    struct em8051_debugger *aDebugger, uint32_t aSeed, uint16_t aEntry,
    struct em8051_debug_snapshot *aSnapshot)
{
    if (!aDebugger || !aSnapshot)
        return EM8051_DEBUG_INVALID_ARGUMENT;
    if (!aDebugger->loaded)
        return EM8051_DEBUG_INVALID_STATE;
    em8051_set_reset_seed(&aDebugger->cpu, aSeed);
    reset(&aDebugger->cpu, true);
    memcpy(aDebugger->code, aDebugger->image, CODE_SIZE);
    aDebugger->cpu.mPC = aEntry;
    memset(aDebugger->breakpoints, 0, sizeof(aDebugger->breakpoints));
    clear_predecessors(aDebugger);
    if (!em8051_debug_capture_snapshot(&aDebugger->cpu,
            EM8051_DEBUG_ARCHITECTURAL_STOP, EM8051_DEBUG_REASON_ENTRY,
            -1, &aDebugger->boundary))
        return EM8051_DEBUG_INTERNAL;
    aDebugger->boundary_valid = true;
    *aSnapshot = aDebugger->boundary;
    return EM8051_DEBUG_OK;
}

enum em8051_debug_status em8051_debugger_get_state(
    const struct em8051_debugger *aDebugger,
    struct em8051_debug_snapshot *aSnapshot)
{
    if (!aDebugger || !aSnapshot)
        return EM8051_DEBUG_INVALID_ARGUMENT;
    if (!aDebugger->boundary_valid)
        return EM8051_DEBUG_INVALID_STATE;
    *aSnapshot = aDebugger->boundary;
    return EM8051_DEBUG_OK;
}

enum em8051_debug_status em8051_debugger_decode_code(
    struct em8051_debugger *aDebugger, uint16_t aReference,
    int32_t aByteOffset, int32_t aInstructionOffset, size_t aCount,
    struct em8051_debug_decoded *aRecords)
{
    if (!aDebugger || !aRecords || aCount == 0u ||
        aCount > EM8051_DEBUG_MAX_DECODE_INSTRUCTIONS ||
        aByteOffset < -65536 || aByteOffset > 65536 ||
        aInstructionOffset < -65536 || aInstructionOffset > 65536)
        return EM8051_DEBUG_INVALID_ARGUMENT;
    if (!aDebugger->loaded)
        return EM8051_DEBUG_INVALID_STATE;
    return emu_decode_window(&aDebugger->cpu, &aDebugger->decode_cache,
                             aReference, aByteOffset, aInstructionOffset,
                             aCount, aRecords);
}

enum em8051_debug_status em8051_debugger_replace_breakpoints(
    struct em8051_debugger *aDebugger, const uint16_t *aAddresses,
    size_t aCount)
{
    uint8_t replacement[BREAKPOINT_BYTES];
    if (!aDebugger || (aCount != 0u && !aAddresses))
        return EM8051_DEBUG_INVALID_ARGUMENT;
    if (aCount > EM8051_DEBUG_MAX_BREAKPOINTS)
        return EM8051_DEBUG_BREAKPOINT_LIMIT;
    if (!emu_breakpoint_replace(replacement, aAddresses, aCount))
        return EM8051_DEBUG_INVALID_ARGUMENT;
    memcpy(aDebugger->breakpoints, replacement, sizeof(replacement));
    return EM8051_DEBUG_OK;
}

static bool breakpoint_at(const struct em8051_debugger *aDebugger,
                          uint16_t aAddress)
{
    return emu_breakpoint_at(aDebugger->breakpoints, aAddress);
}

static enum em8051_debug_status execute_one(
    struct em8051_debugger *aDebugger,
    enum em8051_debug_reason *aReason, int *aException)
{
    struct em8051_run_result result;
    enum em8051_stop_reason core_reason;
    uint64_t before = aDebugger->cpu.mInstructionCount;
    aDebugger->observed_instruction = false;
    core_reason = em8051_step_instruction(&aDebugger->cpu, &result);
    if (aDebugger->cpu.mInstructionCount == before + 1u &&
        aDebugger->observed_instruction)
    {
        emu_decode_completed(&aDebugger->decode_cache,
                             aDebugger->observed_pc, aDebugger->observed_size,
                             aDebugger->cpu.mPC);
    }
    if (core_reason == EM8051_STOP_EXCEPTION)
    {
        *aReason = EM8051_DEBUG_REASON_EXCEPTION;
        *aException = result.exception_code;
    }
    else if (core_reason == EM8051_STOP_HALT)
    {
        *aReason = EM8051_DEBUG_REASON_HALT;
        *aException = -1;
    }
    else
    {
        *aReason = EM8051_DEBUG_REASON_STEP;
        *aException = -1;
    }
    return EM8051_DEBUG_OK;
}

enum em8051_debug_status em8051_debugger_run(
    struct em8051_debugger *aDebugger, uint32_t aMaxInstructions,
    struct em8051_debug_snapshot *aSnapshot)
{
    uint32_t count;
    enum em8051_debug_reason reason = EM8051_DEBUG_REASON_YIELD;
    int exception_code = -1;
    if (!aDebugger || !aSnapshot || aMaxInstructions == 0u ||
        aMaxInstructions > EM8051_DEBUG_MAX_RUN_INSTRUCTIONS)
        return EM8051_DEBUG_INVALID_ARGUMENT;
    if (!aDebugger->boundary_valid)
        return EM8051_DEBUG_INVALID_STATE;
    for (count = 0; count < aMaxInstructions; count++)
    {
        if (breakpoint_at(aDebugger, aDebugger->cpu.mPC))
        {
            reason = EM8051_DEBUG_REASON_BREAKPOINT;
            break;
        }
        (void)execute_one(aDebugger, &reason, &exception_code);
        if (reason == EM8051_DEBUG_REASON_EXCEPTION ||
            reason == EM8051_DEBUG_REASON_HALT)
            break;
        reason = EM8051_DEBUG_REASON_YIELD;
    }
    if (!em8051_debug_capture_snapshot(&aDebugger->cpu,
            reason == EM8051_DEBUG_REASON_YIELD ? EM8051_DEBUG_YIELD :
                EM8051_DEBUG_ARCHITECTURAL_STOP,
            reason, exception_code, &aDebugger->boundary))
        return EM8051_DEBUG_INTERNAL;
    aDebugger->boundary_valid = true;
    *aSnapshot = aDebugger->boundary;
    return EM8051_DEBUG_OK;
}

enum em8051_debug_status em8051_debugger_step(
    struct em8051_debugger *aDebugger,
    struct em8051_debug_snapshot *aSnapshot)
{
    enum em8051_debug_reason reason;
    int exception_code;
    if (!aDebugger || !aSnapshot)
        return EM8051_DEBUG_INVALID_ARGUMENT;
    if (!aDebugger->boundary_valid)
        return EM8051_DEBUG_INVALID_STATE;
    (void)execute_one(aDebugger, &reason, &exception_code);
    if (!em8051_debug_capture_snapshot(&aDebugger->cpu,
            EM8051_DEBUG_ARCHITECTURAL_STOP, reason, exception_code,
            &aDebugger->boundary))
        return EM8051_DEBUG_INTERNAL;
    aDebugger->boundary_valid = true;
    *aSnapshot = aDebugger->boundary;
    return EM8051_DEBUG_OK;
}

const char *em8051_debug_variant_name(enum em8051_variant aVariant)
{
    switch (aVariant)
    {
    case EM8051_VARIANT_8051: return "8051";
    case EM8051_VARIANT_8052: return "8052";
    case EM8051_VARIANT_SAB80535: return "sab80535";
    default: return "unknown";
    }
}

const char *em8051_debug_reason_name(enum em8051_debug_reason aReason)
{
    switch (aReason)
    {
    case EM8051_DEBUG_REASON_ENTRY: return "entry";
    case EM8051_DEBUG_REASON_BREAKPOINT: return "breakpoint";
    case EM8051_DEBUG_REASON_STEP: return "step";
    case EM8051_DEBUG_REASON_EXCEPTION: return "exception";
    case EM8051_DEBUG_REASON_HALT: return "halt";
    case EM8051_DEBUG_REASON_YIELD: return "yield";
    default: return "exception";
    }
}

const char *em8051_debug_exception_code(int aCode)
{
    switch (aCode)
    {
    case EXCEPTION_STACK: return "EMU_EXCEPTION_STACK";
    case EXCEPTION_ACC_TO_A: return "EMU_EXCEPTION_ACC_TO_A";
    case EXCEPTION_IRET_PSW_MISMATCH: return "EMU_EXCEPTION_RETI_PSW";
    case EXCEPTION_IRET_SP_MISMATCH: return "EMU_EXCEPTION_RETI_SP";
    case EXCEPTION_IRET_ACC_MISMATCH: return "EMU_EXCEPTION_RETI_ACC";
    case EXCEPTION_ILLEGAL_OPCODE: return "EMU_EXCEPTION_ILLEGAL_OPCODE";
    default: return "EMU_EXCEPTION_UNKNOWN";
    }
}

const char *em8051_debug_exception_message(int aCode)
{
    switch (aCode)
    {
    case EXCEPTION_STACK: return "invalid stack access";
    case EXCEPTION_ACC_TO_A: return "illegal accumulator move";
    case EXCEPTION_IRET_PSW_MISMATCH: return "RETI restored mismatched PSW";
    case EXCEPTION_IRET_SP_MISMATCH: return "RETI restored mismatched SP";
    case EXCEPTION_IRET_ACC_MISMATCH: return "RETI restored mismatched A";
    case EXCEPTION_ILLEGAL_OPCODE: return "illegal opcode";
    default: return "emulator execution exception";
    }
}

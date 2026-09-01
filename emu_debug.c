/* Generic deterministic debugger facade for the 8051 emulator.
 * Copyright 2006 Jari Komppa
 * Copyright 2026 Hans-Einar
 * Licensed under the repository MIT license.
 */

#include "emu_debug.h"

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

struct sha256_context
{
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t block[64];
    size_t used;
};

struct em8051_debugger
{
    struct em8051 cpu;
    uint8_t code[CODE_SIZE];
    uint8_t image[CODE_SIZE];
    uint16_t predecessor[CODE_SIZE];
    uint8_t predecessor_known[CODE_SIZE];
    uint8_t breakpoints[BREAKPOINT_BYTES];
    struct em8051_debug_snapshot boundary;
    uint16_t observed_pc;
    uint8_t observed_size;
    bool loaded;
    bool boundary_valid;
    bool observed_instruction;
};

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

static void sha256_init(struct sha256_context *aContext)
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

static void sha256_update(struct sha256_context *aContext,
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

static void sha256_final(struct sha256_context *aContext, uint8_t aDigest[32])
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

static void sha256_hex(const uint8_t *aData, size_t aSize,
                       char aHex[EM8051_DEBUG_SHA256_HEX_SIZE])
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
    memset(aDebugger->predecessor_known, 0,
           sizeof(aDebugger->predecessor_known));
}

static uint8_t decode_at(struct em8051_debugger *aDebugger, uint16_t aAddress,
                         char *aText)
{
    uint8_t opcode = aDebugger->code[aAddress];
    return aDebugger->cpu.dec[opcode](&aDebugger->cpu, aAddress, aText);
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

static enum em8051_debug_status decode_record(
    struct em8051_debugger *aDebugger, uint32_t aAddress,
    struct em8051_debug_decoded *aRecord)
{
    uint8_t size;
    if (aAddress > 0xffffu)
        return EM8051_DEBUG_RANGE;
    memset(aRecord, 0, sizeof(*aRecord));
    aRecord->address = (uint16_t)aAddress;
    size = decode_at(aDebugger, (uint16_t)aAddress, aRecord->text);
    if (size == 0u || aAddress + size > CODE_SIZE)
        return EM8051_DEBUG_RANGE;
    aRecord->size = size;
    aRecord->valid = true;
    return EM8051_DEBUG_OK;
}

static void invalid_record(uint16_t aAddress,
                           struct em8051_debug_decoded *aRecord)
{
    memset(aRecord, 0, sizeof(*aRecord));
    aRecord->address = aAddress;
    aRecord->size = 1;
    aRecord->valid = false;
    memcpy(aRecord->text, "<invalid>", sizeof("<invalid>"));
}

enum em8051_debug_status em8051_debugger_decode_code(
    struct em8051_debugger *aDebugger, uint16_t aReference,
    int32_t aByteOffset, int32_t aInstructionOffset, size_t aCount,
    struct em8051_debug_decoded *aRecords)
{
    int64_t base;
    uint32_t cursor;
    size_t output = 0;
    uint16_t staged_addresses[EM8051_DEBUG_MAX_DECODE_INSTRUCTIONS];
    uint16_t staged_predecessors[EM8051_DEBUG_MAX_DECODE_INSTRUCTIONS];
    size_t staged_count = 0;
    int32_t step;
    enum em8051_debug_status status;

    if (!aDebugger || !aRecords || aCount == 0u ||
        aCount > EM8051_DEBUG_MAX_DECODE_INSTRUCTIONS ||
        aByteOffset < -65536 || aByteOffset > 65536 ||
        aInstructionOffset < -65536 || aInstructionOffset > 65536)
        return EM8051_DEBUG_INVALID_ARGUMENT;
    if (!aDebugger->loaded)
        return EM8051_DEBUG_INVALID_STATE;
    base = (int64_t)aReference + aByteOffset;
    if (base < 0 || base > 0xffff)
        return EM8051_DEBUG_RANGE;

    if (aInstructionOffset < 0)
    {
        size_t predecessor_count = (size_t)(-(int64_t)aInstructionOffset);
        size_t temporary_count = predecessor_count < aCount ?
                                 predecessor_count : aCount;
        struct em8051_debug_decoded *temporary = NULL;
        bool known = true;
        cursor = (uint32_t)base;
        if (temporary_count != 0u)
        {
            temporary = (struct em8051_debug_decoded *)calloc(
                temporary_count, sizeof(*temporary));
            if (!temporary)
                return EM8051_DEBUG_INTERNAL;
        }
        for (step = 0; step < -aInstructionOffset; step++)
        {
            struct em8051_debug_decoded record;
            if (known && aDebugger->predecessor_known[cursor])
            {
                cursor = aDebugger->predecessor[cursor];
                status = decode_record(aDebugger, cursor, &record);
                if (status != EM8051_DEBUG_OK)
                {
                    free(temporary);
                    return status;
                }
            }
            else
            {
                known = false;
                if (cursor == 0u)
                {
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
    }
    else
    {
        cursor = (uint32_t)base;
        for (step = 0; step < aInstructionOffset; step++)
        {
            struct em8051_debug_decoded skipped;
            status = decode_record(aDebugger, cursor, &skipped);
            if (status != EM8051_DEBUG_OK)
                return status;
            cursor += skipped.size;
            if (cursor > 0xffffu)
                return EM8051_DEBUG_RANGE;
        }
    }

    while (output < aCount)
    {
        uint32_t next;
        status = decode_record(aDebugger, cursor, &aRecords[output]);
        if (status != EM8051_DEBUG_OK)
            return status;
        next = cursor + aRecords[output].size;
        if (next < CODE_SIZE)
        {
            staged_addresses[staged_count] = (uint16_t)next;
            staged_predecessors[staged_count] = (uint16_t)cursor;
            staged_count++;
        }
        cursor = next;
        output++;
        if (output < aCount && cursor > 0xffffu)
            return EM8051_DEBUG_RANGE;
    }
    for (output = 0; output < staged_count; output++)
    {
        aDebugger->predecessor[staged_addresses[output]] =
            staged_predecessors[output];
        aDebugger->predecessor_known[staged_addresses[output]] = 1;
    }
    return EM8051_DEBUG_OK;
}

enum em8051_debug_status em8051_debugger_replace_breakpoints(
    struct em8051_debugger *aDebugger, const uint16_t *aAddresses,
    size_t aCount)
{
    uint8_t replacement[BREAKPOINT_BYTES];
    size_t i;
    if (!aDebugger || (aCount != 0u && !aAddresses))
        return EM8051_DEBUG_INVALID_ARGUMENT;
    if (aCount > EM8051_DEBUG_MAX_BREAKPOINTS)
        return EM8051_DEBUG_BREAKPOINT_LIMIT;
    memset(replacement, 0, sizeof(replacement));
    for (i = 0; i < aCount; i++)
    {
        uint16_t address = aAddresses[i];
        uint8_t mask = (uint8_t)(1u << (address & 7u));
        uint8_t *slot = &replacement[address >> 3];
        if ((*slot & mask) != 0u)
            return EM8051_DEBUG_INVALID_ARGUMENT;
        *slot |= mask;
    }
    memcpy(aDebugger->breakpoints, replacement, sizeof(replacement));
    return EM8051_DEBUG_OK;
}

static bool breakpoint_at(const struct em8051_debugger *aDebugger,
                          uint16_t aAddress)
{
    return (aDebugger->breakpoints[aAddress >> 3] &
            (uint8_t)(1u << (aAddress & 7u))) != 0u;
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
        uint32_t sequential = (uint32_t)aDebugger->observed_pc +
                              aDebugger->observed_size;
        if (sequential < CODE_SIZE && aDebugger->cpu.mPC == sequential)
        {
            aDebugger->predecessor[sequential] = aDebugger->observed_pc;
            aDebugger->predecessor_known[sequential] = 1;
        }
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

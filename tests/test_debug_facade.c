#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

#include "../emu_debug.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static int write_image(const char *aPath, size_t aSize)
{
    FILE *file = fopen(aPath, "wb");
    size_t i;
    static const uint8_t program[] = { 0x74, 0x01, 0x04, 0x80, 0xfd };
    if (!file)
        return 0;
    for (i = 0; i < aSize; i++)
    {
        uint8_t value = i < sizeof(program) ? program[i] : 0;
        if (fwrite(&value, 1, 1, file) != 1)
        {
            fclose(file);
            return 0;
        }
    }
    return fclose(file) == 0;
}

static int snapshot_surface_test(void)
{
    struct em8051 cpu;
    struct em8051_debug_snapshot snapshot;
    uint8_t code[65536];
    unsigned i;
    memset(&cpu, 0, sizeof(cpu));
    memset(code, 0, sizeof(code));
    cpu.mCodeMem = code;
    cpu.mCodeMemMaxIdx = 0xffffu;
    CHECK(em8051_init_variant(&cpu, EM8051_VARIANT_SAB80535) == 0);
    cpu.mPC = 0xabcd;
    cpu.mSFR[REG_ACC] = 0x12;
    cpu.mSFR[REG_B] = 0x34;
    cpu.mSFR[REG_PSW] = 0x18;
    cpu.mSFR[REG_SP] = 0x7f;
    cpu.mSFR[REG_DPH] = 0x56;
    cpu.mSFR[REG_DPL] = 0x78;
    for (i = 0; i < 8u; i++)
        cpu.mLowerData[24u + i] = (uint8_t)(0x80u + i);
    cpu.mInstructionCount = UINT64_C(0x123456789);
    cpu.mMachineCycleCount = UINT64_C(0x23456789a);
    CHECK(em8051_debug_capture_snapshot(&cpu,
        EM8051_DEBUG_ARCHITECTURAL_STOP, EM8051_DEBUG_REASON_STEP, -1,
        &snapshot));
    CHECK(snapshot.pc == 0xabcd && snapshot.a == 0x12 && snapshot.b == 0x34);
    CHECK(snapshot.psw == 0x18 && snapshot.sp == 0x7f &&
          snapshot.dptr == 0x5678);
    for (i = 0; i < 8u; i++)
        CHECK(snapshot.r[i] == (uint8_t)(0x80u + i));
    CHECK(snapshot.variant == EM8051_VARIANT_SAB80535);
    CHECK(snapshot.instruction_count == UINT64_C(0x123456789));
    CHECK(snapshot.machine_cycle_count == UINT64_C(0x23456789a));
    return 0;
}

static int facade_test(void)
{
    struct em8051_debugger *debugger;
    struct em8051_debug_snapshot first, repeated, snapshot;
    struct em8051_debug_decoded decoded[4];
    uint16_t breakpoint;
    uint16_t duplicates[2] = { 3, 3 };
    char cwd[2048];
    char path[2300];
    char short_path[2300];
    char digest[EM8051_DEBUG_SHA256_HEX_SIZE];
    const char *expected =
        "1550101bc337eba836f6fc6a3012b80677b9dfe6a0c658fcf615194be54e5b88";

    CHECK(getcwd(cwd, sizeof(cwd)) != NULL);
#ifdef _WIN32
    snprintf(path, sizeof(path), "%s\\debug-image.bin", cwd);
    snprintf(short_path, sizeof(short_path), "%s\\debug-short.bin", cwd);
#else
    snprintf(path, sizeof(path), "%s/debug-image.bin", cwd);
    snprintf(short_path, sizeof(short_path), "%s/debug-short.bin", cwd);
#endif
    CHECK(write_image(path, 65536u));
    CHECK(write_image(short_path, 65535u));
    debugger = em8051_debugger_create();
    CHECK(debugger != NULL);
    CHECK(em8051_debugger_get_state(debugger, &snapshot) ==
          EM8051_DEBUG_INVALID_STATE);
    CHECK(em8051_debugger_load(debugger, short_path, expected, digest) ==
          EM8051_DEBUG_IMAGE_SIZE);
    CHECK(em8051_debugger_load(debugger, path,
          "0550101bc337eba836f6fc6a3012b80677b9dfe6a0c658fcf615194be54e5b88",
          digest) == EM8051_DEBUG_IMAGE_HASH);
    CHECK(strcmp(digest, expected) == 0);
    CHECK(em8051_debugger_load(debugger, path, expected, digest) ==
          EM8051_DEBUG_OK);
    CHECK(strcmp(digest, expected) == 0);
    CHECK(em8051_debugger_reset(debugger, 525109u, 0, &first) ==
          EM8051_DEBUG_OK);
    CHECK(first.pc == 0 && first.reason == EM8051_DEBUG_REASON_ENTRY &&
          first.instruction_count == 0 && first.machine_cycle_count == 0);
    CHECK(em8051_debugger_reset(debugger, 525109u, 0, &repeated) ==
          EM8051_DEBUG_OK);
    CHECK(memcmp(&first, &repeated, sizeof(first)) == 0);

    CHECK(em8051_debugger_decode_code(debugger, 2, 0, -1, 3, decoded) ==
          EM8051_DEBUG_OK);
    CHECK(!decoded[0].valid && decoded[0].address == 1 &&
          strcmp(decoded[0].text, "<invalid>") == 0);
    CHECK(decoded[1].valid && decoded[1].address == 2 &&
          decoded[1].size == 1);
    CHECK(decoded[2].valid && decoded[2].address == 3 &&
          decoded[2].size == 2);
    CHECK(em8051_debugger_decode_code(debugger, 0, 0, 0, 2, decoded) ==
          EM8051_DEBUG_OK);
    CHECK(decoded[0].address == 0 && decoded[0].size == 2 &&
          decoded[1].address == 2);
    CHECK(em8051_debugger_decode_code(debugger, 2, 0, -1, 2, decoded) ==
          EM8051_DEBUG_OK);
    CHECK(decoded[0].valid && decoded[0].address == 0 &&
          decoded[1].address == 2);
    CHECK(em8051_debugger_decode_code(debugger, 0xffffu, 0, -1, 1,
          decoded) == EM8051_DEBUG_OK);
    CHECK(!decoded[0].valid && decoded[0].address == 0xfffeu);
    CHECK(em8051_debugger_decode_code(debugger, 0xfffeu, 0, 0, 3,
          decoded) == EM8051_DEBUG_RANGE);
    CHECK(em8051_debugger_decode_code(debugger, 0xffffu, 0, -1, 1,
          decoded) == EM8051_DEBUG_OK);
    CHECK(!decoded[0].valid && decoded[0].address == 0xfffeu &&
          strcmp(decoded[0].text, "<invalid>") == 0);
    CHECK(em8051_debugger_decode_code(debugger, 0xffffu, 1, 0, 1, decoded) ==
          EM8051_DEBUG_RANGE);

    breakpoint = 2;
    CHECK(em8051_debugger_replace_breakpoints(debugger, &breakpoint, 1) ==
          EM8051_DEBUG_OK);
    CHECK(em8051_debugger_run(debugger, 10, &snapshot) == EM8051_DEBUG_OK);
    CHECK(snapshot.reason == EM8051_DEBUG_REASON_BREAKPOINT &&
          snapshot.pc == 2 && snapshot.instruction_count == 1);
    CHECK(em8051_debugger_step(debugger, &snapshot) == EM8051_DEBUG_OK);
    CHECK(snapshot.reason == EM8051_DEBUG_REASON_STEP && snapshot.pc == 3 &&
          snapshot.a == 2 && snapshot.instruction_count == 2);
    CHECK(em8051_debugger_replace_breakpoints(debugger, duplicates, 2) ==
          EM8051_DEBUG_INVALID_ARGUMENT);
    CHECK(em8051_debugger_run(debugger, 1, &snapshot) == EM8051_DEBUG_OK);
    CHECK(snapshot.reason == EM8051_DEBUG_REASON_YIELD && snapshot.pc == 2);
    CHECK(em8051_debugger_run(debugger, 1, &snapshot) == EM8051_DEBUG_OK);
    CHECK(snapshot.reason == EM8051_DEBUG_REASON_BREAKPOINT && snapshot.pc == 2);
    CHECK(em8051_debugger_replace_breakpoints(debugger, NULL, 0) ==
          EM8051_DEBUG_OK);
    CHECK(em8051_debugger_run(debugger, 2, &snapshot) == EM8051_DEBUG_OK);
    CHECK(snapshot.reason == EM8051_DEBUG_REASON_YIELD && snapshot.pc == 2 &&
          snapshot.a == 3);
    CHECK(em8051_debugger_get_state(debugger, &repeated) == EM8051_DEBUG_OK);
    CHECK(memcmp(&snapshot, &repeated, sizeof(snapshot)) == 0);

    em8051_debugger_destroy(debugger);
    CHECK(remove(path) == 0);
    CHECK(remove(short_path) == 0);
    return 0;
}

int main(void)
{
    CHECK(snapshot_surface_test() == 0);
    CHECK(facade_test() == 0);
    printf("debug facade tests passed\n");
    return 0;
}

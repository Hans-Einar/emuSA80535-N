/* Installed SDK example/check. Copyright 2026 Hans-Einar.
 * Distributed under the MIT license; see LICENSE.
 * Compile using pkg-config --cflags --libs emusa80535.
 */
#include <emu8051.h>
#include <emu_debug.h>
#include <emu_debug_runtime.h>
#include <stdio.h>
#include <string.h>

static unsigned reads;
static unsigned writes;
static uint8_t external_value;

static uint8_t read_external(struct em8051 *cpu, uint16_t address)
{
    (void)cpu;
    if (address == 0x1234u)
        ++reads;
    return external_value;
}

static void write_external(struct em8051 *cpu, uint16_t address, uint8_t value)
{
    (void)cpu;
    if (address == 0x1234u)
        ++writes;
    external_value = value;
}

int main(void)
{
    struct em8051 cpu;
    unsigned char code[65536] = {0};
    /* MOV DPTR,#1234; MOV A,#5A; MOVX @DPTR,A; CLR A; MOVX A,@DPTR. */
    const unsigned char program[] = {0x90, 0x12, 0x34, 0x74, 0x5a,
                                     0xf0, 0xe4, 0xe0};
    struct em8051_run_result result;
    struct em8051_debug_snapshot snapshot;
    struct em8051_debug_runtime *runtime = NULL;
    struct em8051_debug_runtime_status status;
    struct em8051_debugger *debugger;
    char decoded[128];

    memset(&cpu, 0, sizeof(cpu));
    cpu.mCodeMem = code;
    cpu.mCodeMemMaxIdx = 0xffffu;
    if (em8051_init_variant(&cpu, EM8051_VARIANT_SAB80535) != 0)
        return 1;
    /* Variant initialization performs a cold reset, which clears CODE. */
    memcpy(code, program, sizeof(program));
    cpu.xread = read_external;
    cpu.xwrite = write_external;
    if (decode(&cpu, 0, decoded) != 3 || decoded[0] == '\0')
        return 2;
    if (em8051_run(&cpu, 5, &result) != EM8051_STOP_INSTRUCTION_LIMIT ||
        result.instructions != 5 || result.pc != sizeof(program) ||
        reads != 1 || writes != 1 || external_value != 0x5au)
        return 3;
    if (!em8051_debug_capture_snapshot(&cpu, EM8051_DEBUG_YIELD,
            EM8051_DEBUG_REASON_YIELD, 0, &snapshot) || snapshot.a != 0x5au)
        return 4;
    debugger = em8051_debugger_create();
    if (!debugger)
        return 5;
    em8051_debugger_destroy(debugger);
    if (em8051_debug_runtime_create(&runtime) != EM8051_DEBUG_EVENT_OK)
        return 6;
    if (em8051_debug_runtime_get_status(runtime, &status) != EM8051_DEBUG_EVENT_OK ||
        status.accepted_source_events != 0)
    {
        em8051_debug_runtime_destroy(runtime);
        return 7;
    }
    em8051_debug_runtime_destroy(runtime);
    puts("installed SDK: CPU, MOVX callbacks, decoder, facade and runtime passed");
    return 0;
}

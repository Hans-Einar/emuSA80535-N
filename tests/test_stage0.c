/* Focused deterministic Stage-0 tests for the emu8051 core. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../emu8051.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static int gFailures;

#define CHECK(condition)                                                       \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
        {                                                                      \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n",                     \
                    __FILE__, __LINE__, #condition);                           \
            gFailures++;                                                       \
        }                                                                      \
    } while (0)

struct fixture
{
    struct em8051 cpu;
    unsigned char code[65536];
    unsigned char xdata[65536];
};

static void setup_fixture(struct fixture *aFixture, enum em8051_variant aVariant)
{
    memset(aFixture, 0, sizeof(*aFixture));
    aFixture->cpu.mCodeMem = aFixture->code;
    aFixture->cpu.mCodeMemMaxIdx = 0xffffu;
    aFixture->cpu.mExtData = aFixture->xdata;
    aFixture->cpu.mExtDataMaxIdx = 0xffffu;
    CHECK(em8051_init_variant(&aFixture->cpu, aVariant) == 0);
}

static void test_variant_and_sab_reset_map(void)
{
    static const uint8_t zero_reset_sfrs[] =
    {
        EM8051_SAB_SFR_IEN0, EM8051_SAB_SFR_IP0,
        EM8051_SAB_SFR_IEN1, EM8051_SAB_SFR_IP1,
        EM8051_SAB_SFR_IRCON, EM8051_SAB_SFR_CCEN,
        EM8051_SAB_SFR_T2CON, EM8051_SAB_SFR_CRCL,
        EM8051_SAB_SFR_CRCH, EM8051_SAB_SFR_TL2,
        EM8051_SAB_SFR_TH2, EM8051_SAB_SFR_ADCON,
        EM8051_SAB_SFR_ADDAT, EM8051_SAB_SFR_DAPR
    };
    const struct em8051_variant_descriptor *classic;
    const struct em8051_variant_descriptor *classic52;
    const struct em8051_variant_descriptor *sab;
    struct fixture fixture;
    struct em8051_run_result result;
    char disassembly[64];
    size_t i;

    classic = em8051_get_variant_descriptor(EM8051_VARIANT_8051);
    classic52 = em8051_get_variant_descriptor(EM8051_VARIANT_8052);
    sab = em8051_get_variant_descriptor(EM8051_VARIANT_SAB80535);
    CHECK(classic != NULL);
    CHECK(classic52 != NULL);
    CHECK(sab != NULL);
    CHECK(classic->interrupt_enable0_sfr == 0xA8u);
    CHECK(classic->interrupt_priority0_sfr == 0xB8u);
    CHECK(classic->interrupt_enable1_sfr == EM8051_SFR_UNAVAILABLE);
    CHECK(!classic->has_upper_iram);
    CHECK(classic52->has_upper_iram);
    CHECK(sab->interrupt_enable0_sfr == 0xA8u);
    CHECK(sab->interrupt_priority0_sfr == 0xA9u);
    CHECK(sab->interrupt_enable1_sfr == 0xB8u);
    CHECK(sab->interrupt_priority1_sfr == 0xB9u);
    CHECK(sab->default_oscillator_hz == 11059200u);

    setup_fixture(&fixture, EM8051_VARIANT_8052);
    CHECK(fixture.cpu.mUpperData == fixture.cpu.mOwnedUpperData);
    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    CHECK(fixture.cpu.mUpperData == fixture.cpu.mOwnedUpperData);
    CHECK(fixture.cpu.mOscillatorHz == 11059200u);
    for (i = 0; i < ARRAY_SIZE(zero_reset_sfrs); i++)
        CHECK(fixture.cpu.mSFR[zero_reset_sfrs[i] - 0x80] == 0);
    CHECK(fixture.cpu.mSFR[EM8051_SAB_SFR_P4 - 0x80] == 0xff);
    CHECK(fixture.cpu.mSFR[EM8051_SAB_SFR_P5 - 0x80] == 0xff);
    CHECK(fixture.cpu.mSFR[EM8051_SAB_SFR_P6 - 0x80] == 0xff);

    fixture.code[0] = 0x75; /* MOV IEN1,#01 */
    fixture.code[1] = 0xB8;
    fixture.code[2] = 0x01;
    CHECK(decode(&fixture.cpu, 0, disassembly) == 3);
    CHECK(strstr(disassembly, "IEN1") != NULL);

    /* Stage 0 must not interpret SAB IEN1 at B8 as classic IP. */
    fixture.code[0] = 0x00; /* NOP */
    fixture.cpu.mSFR[EM8051_SAB_SFR_IEN0 - 0x80] = IEMASK_EA | IEMASK_EX0;
    fixture.cpu.mSFR[EM8051_SAB_SFR_IEN1 - 0x80] = IPMASK_PX0;
    fixture.cpu.mSFR[REG_TCON] = TCONMASK_IE0;
    CHECK(em8051_run(&fixture.cpu, 1, &result) == EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(fixture.cpu.mPC == 1);
}

static void test_classic_instruction_regression(void)
{
    struct fixture fixture;
    struct em8051_run_result result;
    char disassembly[64];
    static const unsigned char program[] =
    {
        0x74, 0x12,       /* MOV A,#12 */
        0x24, 0x34,       /* ADD A,#34 */
        0xF5, 0x20        /* MOV 20,A */
    };

    setup_fixture(&fixture, EM8051_VARIANT_8051);
    fixture.code[0] = 0x75; /* MOV IP,#01 */
    fixture.code[1] = 0xB8;
    fixture.code[2] = 0x01;
    CHECK(decode(&fixture.cpu, 0, disassembly) == 3);
    CHECK(strstr(disassembly, "IP") != NULL);
    memcpy(fixture.code, program, sizeof(program));
    CHECK(em8051_run(&fixture.cpu, 3, &result) == EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(fixture.cpu.mSFR[REG_ACC] == 0x46);
    CHECK(fixture.cpu.mLowerData[0x20] == 0x46);
    CHECK(fixture.cpu.mPC == sizeof(program));
    CHECK(result.instructions == 3);
    CHECK(result.machine_cycles == 3);
    CHECK(fixture.cpu.mInstructionCount == 3);
    CHECK(fixture.cpu.mMachineCycleCount == 3);

    /* Classic interrupt entry remains functional and contributes virtual
     * machine cycles independently of the SAB controller boundary. */
    reset(&fixture.cpu, false);
    fixture.code[3] = 0x00; /* NOP at external interrupt 0 vector */
    fixture.cpu.mSFR[REG_IE] = IEMASK_EA | IEMASK_EX0;
    fixture.cpu.mSFR[REG_TCON] = TCONMASK_IE0;
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.instructions == 1);
    CHECK(result.machine_cycles == 3);
    CHECK(result.pc == 4);
}

static void test_upper_iram_and_stack(void)
{
    struct fixture fixture;
    struct em8051_run_result result;
    static const unsigned char memory_program[] =
    {
        0x78, 0xA2,       /* MOV R0,#A2 */
        0x76, 0x5A,       /* MOV @R0,#5A */
        0xE6,             /* MOV A,@R0 */
        0x74, 0xC3,       /* MOV A,#C3 */
        0xF5, 0xA2        /* MOV A2,A (direct SFR) */
    };

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    memcpy(fixture.code, memory_program, sizeof(memory_program));
    fixture.cpu.mSFR[0xA2 - 0x80] = 0x11;
    CHECK(em8051_run(&fixture.cpu, 3, &result) == EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(fixture.cpu.mUpperData[0xA2 - 0x80] == 0x5A);
    CHECK(fixture.cpu.mSFR[0xA2 - 0x80] == 0x11);
    CHECK(fixture.cpu.mSFR[REG_ACC] == 0x5A);
    CHECK(em8051_run(&fixture.cpu, 2, &result) == EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(fixture.cpu.mUpperData[0xA2 - 0x80] == 0x5A);
    CHECK(fixture.cpu.mSFR[0xA2 - 0x80] == 0xC3);

    reset(&fixture.cpu, false);
    memset(fixture.code, 0, sizeof(fixture.code));
    fixture.code[0] = 0x12; /* LCALL 0006 */
    fixture.code[1] = 0x00;
    fixture.code[2] = 0x06;
    fixture.code[6] = 0x74; /* MOV A,#5A */
    fixture.code[7] = 0x5A;
    fixture.code[8] = 0x22; /* RET */
    fixture.cpu.mSFR[REG_SP] = 0xA2;
    fixture.cpu.mSFR[0xA2 - 0x80] = 0xCC;
    fixture.cpu.mSFR[0xA3 - 0x80] = 0xDD;
    fixture.cpu.mSFR[0xA4 - 0x80] = 0xEE;
    CHECK(em8051_run(&fixture.cpu, 1, &result) == EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(fixture.cpu.mPC == 6);
    CHECK(fixture.cpu.mSFR[REG_SP] == 0xA4);
    CHECK(fixture.cpu.mUpperData[0xA3 - 0x80] == 0x03);
    CHECK(fixture.cpu.mUpperData[0xA4 - 0x80] == 0x00);
    CHECK(fixture.cpu.mSFR[0xA2 - 0x80] == 0xCC);
    CHECK(fixture.cpu.mSFR[0xA3 - 0x80] == 0xDD);
    CHECK(fixture.cpu.mSFR[0xA4 - 0x80] == 0xEE);
    CHECK(em8051_run(&fixture.cpu, 2, &result) == EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(fixture.cpu.mSFR[REG_ACC] == 0x5A);
    CHECK(fixture.cpu.mPC == 3);
    CHECK(fixture.cpu.mSFR[REG_SP] == 0xA2);
}

static void write_test_image(const char *aName, size_t aSize)
{
    FILE *output = fopen(aName, "wb");
    size_t i;
    CHECK(output != NULL);
    if (!output)
        return;
    for (i = 0; i < aSize; i++)
        CHECK(fputc((int)(i & 0xffu), output) != EOF);
    CHECK(fclose(output) == 0);
}

static void test_exact_raw_loader(void)
{
    const char *short_name = "stage0-short.bin";
    const char *exact_name = "stage0-exact.bin";
    const char *long_name = "stage0-long.bin";
    struct fixture fixture;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    fixture.code[0] = 0xE5;
    fixture.xdata[0] = 0xA5;
    write_test_image(short_name, 65535u);
    write_test_image(exact_name, 65536u);
    write_test_image(long_name, 65537u);

    CHECK(em8051_load_binary(&fixture.cpu, short_name) == EM8051_LOAD_SIZE_ERROR);
    CHECK(fixture.code[0] == 0xE5);
    CHECK(em8051_load_binary(&fixture.cpu, long_name) == EM8051_LOAD_SIZE_ERROR);
    CHECK(fixture.code[0] == 0xE5);
    CHECK(em8051_load_binary(&fixture.cpu, "stage0-missing.bin") == EM8051_LOAD_IO_ERROR);
    CHECK(em8051_load_binary(&fixture.cpu, exact_name) == EM8051_LOAD_OK);
    CHECK(fixture.code[0] == 0x00);
    CHECK(fixture.code[0x1234] == 0x34);
    CHECK(fixture.code[0xffff] == 0xff);
    CHECK(fixture.xdata[0] == 0xA5);

    fixture.cpu.mCodeMemMaxIdx = 0x7fffu;
    CHECK(em8051_load_binary(&fixture.cpu, exact_name) ==
          EM8051_LOAD_CONFIGURATION_ERROR);
    CHECK(remove(short_name) == 0);
    CHECK(remove(exact_name) == 0);
    CHECK(remove(long_name) == 0);
}

static void test_run_control_and_counters(void)
{
    struct fixture fixture;
    struct em8051_run_result result;

    setup_fixture(&fixture, EM8051_VARIANT_8051);
    fixture.code[0] = 0x00; /* NOP */
    fixture.code[1] = 0x80; /* SJMP -2 */
    fixture.code[2] = 0xFE;
    CHECK(em8051_step_instruction(&fixture.cpu, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.instructions == 1);
    CHECK(result.machine_cycles == 1);
    CHECK(em8051_run(&fixture.cpu, 2, &result) == EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.instructions == 2);
    CHECK(result.machine_cycles == 4);
    CHECK(result.pc == 1);
    CHECK(fixture.cpu.mInstructionCount == 3);
    CHECK(fixture.cpu.mMachineCycleCount == 5);

    reset(&fixture.cpu, false);
    em8051_set_breakpoint(&fixture.cpu, 1, true);
    CHECK(em8051_run(&fixture.cpu, 10, &result) == EM8051_STOP_BREAKPOINT);
    CHECK(result.instructions == 1);
    CHECK(result.pc == 1);

    reset(&fixture.cpu, false);
    CHECK(em8051_run_until_pc(&fixture.cpu, 0, 0, &result) ==
          EM8051_STOP_TARGET_PC);
    CHECK(result.instructions == 0);
    CHECK(em8051_run_until_pc(&fixture.cpu, 1, 10, &result) ==
          EM8051_STOP_TARGET_PC);
    CHECK(result.instructions == 1);
    CHECK(result.pc == 1);

    reset(&fixture.cpu, false);
    fixture.code[0] = 0x75; /* MOV PCON,#01 (IDLE) */
    fixture.code[1] = 0x87;
    fixture.code[2] = 0x01;
    CHECK(em8051_run(&fixture.cpu, 10, &result) == EM8051_STOP_HALT);
    CHECK(result.instructions == 1);
    CHECK(result.machine_cycles == 2);

    reset(&fixture.cpu, false);
    fixture.code[0] = 0xA5; /* reserved opcode */
    CHECK(em8051_run(&fixture.cpu, 10, &result) == EM8051_STOP_EXCEPTION);
    CHECK(result.instructions == 1);
    CHECK(result.exception_code == EXCEPTION_ILLEGAL_OPCODE);
}

struct trace_capture
{
    struct em8051_trace_record records[128];
    size_t count;
};

static void capture_trace(struct em8051 *aCPU,
                          const struct em8051_trace_record *aRecord,
                          void *aUser)
{
    struct trace_capture *capture = (struct trace_capture *)aUser;
    (void)aCPU;
    if (capture->count < ARRAY_SIZE(capture->records))
        capture->records[capture->count++] = *aRecord;
}

static void test_trace_contract(void)
{
    static const unsigned char program[] =
    {
        0x75, 0x82, 0x34, /* MOV DPL,#34 */
        0x75, 0x83, 0x12, /* MOV DPH,#12 */
        0x74, 0x5A,       /* MOV A,#5A */
        0xF0,             /* MOVX @DPTR,A */
        0xE4,             /* CLR A */
        0xE0,             /* MOVX A,@DPTR */
        0xD2, 0x90,       /* SETB P1.0 */
        0x53, 0x90, 0xFE, /* ANL P1,#FE */
        0x75, 0x90, 0xAA  /* MOV P1,#AA */
    };
    struct fixture traced;
    struct fixture untraced;
    struct trace_capture capture;
    struct em8051_run_result result;
    size_t instruction_count = 0;
    size_t sfr_count = 0;
    size_t movx_read_count = 0;
    size_t movx_write_count = 0;
    size_t i;

    memset(&capture, 0, sizeof(capture));
    setup_fixture(&traced, EM8051_VARIANT_SAB80535);
    memcpy(traced.code, program, sizeof(program));
    em8051_set_trace(&traced.cpu, capture_trace, &capture);
    CHECK(em8051_run(&traced.cpu, 9, &result) == EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(traced.xdata[0x1234] == 0x5A);
    CHECK(traced.cpu.mSFR[REG_ACC] == 0x5A);
    CHECK(traced.cpu.mSFR[REG_P1] == 0xAA);

    for (i = 0; i < capture.count; i++)
    {
        if (i > 0)
            CHECK(capture.records[i].machine_cycle >=
                  capture.records[i - 1].machine_cycle);
        switch (capture.records[i].type)
        {
        case EM8051_TRACE_INSTRUCTION:
            instruction_count++;
            break;
        case EM8051_TRACE_SFR_WRITE:
            sfr_count++;
            break;
        case EM8051_TRACE_MOVX_READ:
            movx_read_count++;
            CHECK(capture.records[i].pc == 10);
            CHECK(capture.records[i].address == 0x1234);
            CHECK(capture.records[i].value == 0x5A);
            break;
        case EM8051_TRACE_MOVX_WRITE:
            movx_write_count++;
            CHECK(capture.records[i].pc == 8);
            CHECK(capture.records[i].address == 0x1234);
            CHECK(capture.records[i].value == 0x5A);
            break;
        default:
            CHECK(false);
            break;
        }
    }
    CHECK(instruction_count == 9);
    CHECK(sfr_count == 5);
    CHECK(movx_read_count == 1);
    CHECK(movx_write_count == 1);

    setup_fixture(&untraced, EM8051_VARIANT_SAB80535);
    memcpy(untraced.code, program, sizeof(program));
    CHECK(em8051_run(&untraced.cpu, 9, &result) == EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(untraced.cpu.mPC == traced.cpu.mPC);
    CHECK(untraced.cpu.mInstructionCount == traced.cpu.mInstructionCount);
    CHECK(untraced.cpu.mMachineCycleCount == traced.cpu.mMachineCycleCount);
    CHECK(memcmp(untraced.cpu.mSFR, traced.cpu.mSFR,
                 sizeof(traced.cpu.mSFR)) == 0);
    CHECK(memcmp(untraced.xdata, traced.xdata, sizeof(traced.xdata)) == 0);

    /* A reached but unbacked generic XDATA access remains diagnosable. */
    memset(&capture, 0, sizeof(capture));
    reset(&untraced.cpu, false);
    untraced.code[0] = 0xE0; /* MOVX A,@DPTR */
    untraced.cpu.mExtData = NULL;
    untraced.cpu.mSFR[REG_ACC] = 0x77;
    em8051_set_trace(&untraced.cpu, capture_trace, &capture);
    CHECK(em8051_run(&untraced.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(capture.count == 2);
    CHECK(capture.records[1].type == EM8051_TRACE_UNSUPPORTED_MOVX_READ);
    CHECK(capture.records[1].pc == 0);
    CHECK(capture.records[1].address == 0);
    CHECK(capture.records[1].value == 0x77);
}

static void test_seeded_reset_is_repeatable(void)
{
    struct fixture first;
    struct fixture second;
    struct trace_capture first_trace;
    struct trace_capture second_trace;
    struct em8051_run_result first_result;
    struct em8051_run_result second_result;
    unsigned char first_lower[128];
    size_t i;

    setup_fixture(&first, EM8051_VARIANT_SAB80535);
    setup_fixture(&second, EM8051_VARIANT_SAB80535);
    em8051_set_reset_seed(&first.cpu, 0x12345678u);
    em8051_set_reset_seed(&second.cpu, 0x12345678u);
    reset(&first.cpu, true);
    reset(&second.cpu, true);
    CHECK(memcmp(first.cpu.mLowerData, second.cpu.mLowerData, 128) == 0);
    CHECK(memcmp(first.cpu.mUpperData, second.cpu.mUpperData, 128) == 0);
    CHECK(memcmp(first.cpu.mSFR, second.cpu.mSFR, 128) == 0);
    CHECK(first.cpu.mInstructionCount == 0);
    CHECK(first.cpu.mMachineCycleCount == 0);

    memcpy(first_lower, first.cpu.mLowerData, sizeof(first_lower));
    memset(&first_trace, 0, sizeof(first_trace));
    memset(&second_trace, 0, sizeof(second_trace));
    first.code[0] = second.code[0] = 0x74; /* MOV A,#42 */
    first.code[1] = second.code[1] = 0x42;
    first.code[2] = second.code[2] = 0xF5; /* MOV 20,A */
    first.code[3] = second.code[3] = 0x20;
    em8051_set_trace(&first.cpu, capture_trace, &first_trace);
    em8051_set_trace(&second.cpu, capture_trace, &second_trace);
    CHECK(em8051_run(&first.cpu, 2, &first_result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(em8051_run(&second.cpu, 2, &second_result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(first_result.instructions == second_result.instructions);
    CHECK(first_result.machine_cycles == second_result.machine_cycles);
    CHECK(first_trace.count == second_trace.count);
    for (i = 0; i < first_trace.count && i < second_trace.count; i++)
    {
        CHECK(first_trace.records[i].type == second_trace.records[i].type);
        CHECK(first_trace.records[i].machine_cycle ==
              second_trace.records[i].machine_cycle);
        CHECK(first_trace.records[i].pc == second_trace.records[i].pc);
        CHECK(first_trace.records[i].address == second_trace.records[i].address);
        CHECK(first_trace.records[i].value == second_trace.records[i].value);
    }
    CHECK(memcmp(first.cpu.mLowerData, second.cpu.mLowerData, 128) == 0);
    CHECK(memcmp(first.cpu.mSFR, second.cpu.mSFR, 128) == 0);

    em8051_set_reset_seed(&first.cpu, 0x87654321u);
    reset(&first.cpu, true);
    CHECK(memcmp(first_lower, first.cpu.mLowerData, sizeof(first_lower)) != 0);
}

int main(void)
{
    test_variant_and_sab_reset_map();
    test_classic_instruction_regression();
    test_upper_iram_and_stack();
    test_exact_raw_loader();
    test_run_control_and_counters();
    test_trace_contract();
    test_seeded_reset_is_repeatable();

    if (gFailures != 0)
    {
        fprintf(stderr, "Stage-0 tests failed: %d failure(s)\n", gFailures);
        return EXIT_FAILURE;
    }
    printf("Stage-0 tests passed\n");
    return EXIT_SUCCESS;
}

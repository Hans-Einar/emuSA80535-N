/* Focused deterministic Stage-0 tests for the emu8051 core. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../emu8051.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static int gFailures;
static int gStackExceptionCount;

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

static void count_stack_exception(struct em8051 *aCPU, int aCode)
{
    (void)aCPU;
    if (aCode == EXCEPTION_STACK)
        gStackExceptionCount++;
}

static void test_variant_and_sab_reset_map(void)
{
    static const uint8_t documented_zero_reset_sfrs[] =
    {
        EM8051_SAB_SFR_IEN0, EM8051_SAB_SFR_IEN1,
        EM8051_SAB_SFR_IRCON, EM8051_SAB_SFR_CCEN,
        EM8051_SAB_SFR_T2CON, EM8051_SAB_SFR_CRCL,
        EM8051_SAB_SFR_CRCH, EM8051_SAB_SFR_TL2,
        EM8051_SAB_SFR_TH2, EM8051_SAB_SFR_ADDAT,
        EM8051_SAB_SFR_DAPR
    };
    static const uint8_t deterministic_zero_model_sfrs[] =
    {
        /* These hardware fields are indeterminate; zero is a repeatable
         * Stage-0 model choice, not a physical reset assertion. */
        EM8051_SAB_SFR_IP0, EM8051_SAB_SFR_IP1,
        EM8051_SAB_SFR_ADCON
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
    for (i = 0; i < ARRAY_SIZE(documented_zero_reset_sfrs); i++)
        CHECK(fixture.cpu.mSFR[documented_zero_reset_sfrs[i] - 0x80] == 0);
    for (i = 0; i < ARRAY_SIZE(deterministic_zero_model_sfrs); i++)
        CHECK(fixture.cpu.mSFR[deterministic_zero_model_sfrs[i] - 0x80] == 0);
    CHECK(fixture.cpu.mSFR[EM8051_SAB_SFR_P4 - 0x80] == 0xff);
    CHECK(fixture.cpu.mSFR[EM8051_SAB_SFR_P5 - 0x80] == 0xff);
    /* P6 is input-only and has no proven reset latch value. High is the
     * deterministic Stage-0 sample chosen for otherwise unspecified state. */
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
    uint8_t invalid_sp;
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

    /* A classic 8051 has no upper IRAM. Entering address 80 through the
     * indirect stack path must fail immediately instead of losing the byte. */
    setup_fixture(&fixture, EM8051_VARIANT_8051);
    fixture.code[0] = 0xC0; /* PUSH 20 */
    fixture.code[1] = 0x20;
    fixture.cpu.mLowerData[0x20] = 0xA5;
    fixture.cpu.mSFR[REG_SP] = 0x7F;
    CHECK(em8051_run(&fixture.cpu, 1, &result) == EM8051_STOP_EXCEPTION);
    CHECK(result.instructions == 1);
    CHECK(result.exception_code == EXCEPTION_STACK);
    CHECK(fixture.cpu.mSFR[REG_SP] == 0x80);
    CHECK(fixture.cpu.mUpperData == NULL);

    /* A missing classic upper stack must fail before POP can overwrite its
     * destination or RET can replace the PC with the old sentinel value. */
    for (invalid_sp = 0x80; invalid_sp <= 0x81; invalid_sp++)
    {
        setup_fixture(&fixture, EM8051_VARIANT_8051);
        fixture.code[0] = 0xD0; /* POP 20 */
        fixture.code[1] = 0x20;
        fixture.cpu.mLowerData[0x20] = 0xA5;
        fixture.cpu.mSFR[REG_SP] = invalid_sp;
        CHECK(em8051_run(&fixture.cpu, 1, &result) == EM8051_STOP_EXCEPTION);
        CHECK(result.exception_code == EXCEPTION_STACK);
        CHECK(fixture.cpu.mLowerData[0x20] == 0xA5);
        CHECK(fixture.cpu.mPC == 0);
        CHECK(fixture.cpu.mSFR[REG_SP] == invalid_sp);

        setup_fixture(&fixture, EM8051_VARIANT_8051);
        fixture.cpu.mPC = 0x0100;
        fixture.code[0x0100] = 0x22; /* RET */
        fixture.cpu.mSFR[REG_SP] = invalid_sp;
        CHECK(em8051_run(&fixture.cpu, 1, &result) == EM8051_STOP_EXCEPTION);
        CHECK(result.exception_code == EXCEPTION_STACK);
        CHECK(fixture.cpu.mPC == 0x0100);
        CHECK(fixture.cpu.mSFR[REG_SP] == invalid_sp);
    }
}

static void test_sfr_gateway_validation(void)
{
    struct fixture fixture;
    unsigned char sfr_before[128];

    setup_fixture(&fixture, EM8051_VARIANT_8051);
    fixture.cpu.mPC = 0x1234;
    fixture.cpu.mSFR[REG_ACC] = 0x5A;
    memcpy(sfr_before, fixture.cpu.mSFR, sizeof(sfr_before));

    CHECK(em8051_sfr_read(&fixture.cpu, 0x00) == 0xff);
    em8051_sfr_write(&fixture.cpu, 0x00, 0xAB);
    CHECK(fixture.cpu.mPC == 0x1234);
    CHECK(memcmp(sfr_before, fixture.cpu.mSFR, sizeof(sfr_before)) == 0);
    CHECK(em8051_sfr_read(NULL, 0x80) == 0xff);
    em8051_sfr_write(NULL, 0x80, 0xAB);

    em8051_sfr_write(&fixture.cpu, 0xE0, 0xC3);
    CHECK(em8051_sfr_read(&fixture.cpu, 0xE0) == 0xC3);
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

    CHECK(em8051_load_binary(&fixture.cpu, NULL) == EM8051_LOAD_IO_ERROR);
    CHECK(fixture.code[0] == 0xE5);
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
    CHECK(fixture.cpu.mTickDelay == 0);

    /* A multi-cycle instruction returns only after every credited cycle has
     * advanced Timer 0 and the pending delay is fully drained. */
    reset(&fixture.cpu, false);
    fixture.code[0] = 0x80; /* SJMP -2 */
    fixture.code[1] = 0xFE;
    fixture.cpu.mSFR[REG_TMOD] = TMODMASK_M0_0; /* Timer 0 mode 1 */
    fixture.cpu.mSFR[REG_TCON] = TCONMASK_TR0;
    CHECK(em8051_step_instruction(&fixture.cpu, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.instructions == 1);
    CHECK(result.machine_cycles == 2);
    CHECK(fixture.cpu.mSFR[REG_TL0] == 2);
    CHECK(fixture.cpu.mTickDelay == 0);

    fixture.code[0] = 0x00; /* restore NOP; SJMP -2 remains at address 1 */
    fixture.code[1] = 0x80;
    fixture.code[2] = 0xFE;
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
    CHECK(fixture.cpu.mTickDelay == 0);

    /* IDLE starts only after the two-cycle MOV has completed. Thereafter a
     * raw idle tick advances virtual time and Timer 0 without an opcode. */
    reset(&fixture.cpu, false);
    fixture.code[0] = 0x75; /* MOV PCON,#01 (IDLE) */
    fixture.code[1] = 0x87;
    fixture.code[2] = 0x01;
    fixture.cpu.mSFR[REG_TMOD] = TMODMASK_M0_0;
    fixture.cpu.mSFR[REG_TCON] = TCONMASK_TR0;
    CHECK(em8051_run(&fixture.cpu, 10, &result) == EM8051_STOP_HALT);
    CHECK(result.machine_cycles == 2);
    CHECK(fixture.cpu.mSFR[REG_TL0] == 2);
    CHECK(!tick(&fixture.cpu));
    CHECK(fixture.cpu.mMachineCycleCount == 3);
    CHECK(fixture.cpu.mSFR[REG_TL0] == 3);
    CHECK(fixture.cpu.mInstructionCount == 1);

    reset(&fixture.cpu, false);
    fixture.code[0] = 0xA5; /* reserved opcode */
    CHECK(em8051_run(&fixture.cpu, 10, &result) == EM8051_STOP_EXCEPTION);
    CHECK(result.instructions == 1);
    CHECK(result.exception_code == EXCEPTION_ILLEGAL_OPCODE);
}

static void test_interrupt_vector_stops(void)
{
    struct fixture fixture;
    struct em8051_run_result result;

    setup_fixture(&fixture, EM8051_VARIANT_8051);
    fixture.code[ISR_INT0] = 0x00; /* vector NOP must remain unexecuted */
    fixture.cpu.mSFR[REG_IE] = IEMASK_EA | IEMASK_EX0;
    fixture.cpu.mSFR[REG_TCON] = TCONMASK_IE0;
    CHECK(em8051_run_until_pc(&fixture.cpu, ISR_INT0, 10, &result) ==
          EM8051_STOP_TARGET_PC);
    CHECK(result.instructions == 0);
    CHECK(result.machine_cycles == 2);
    CHECK(result.pc == ISR_INT0);
    CHECK(fixture.cpu.mInstructionCount == 0);
    CHECK(fixture.cpu.mTickDelay == 0);

    reset(&fixture.cpu, false);
    fixture.cpu.mSFR[REG_IE] = IEMASK_EA | IEMASK_EX0;
    fixture.cpu.mSFR[REG_TCON] = TCONMASK_IE0;
    em8051_set_breakpoint(&fixture.cpu, ISR_INT0, true);
    CHECK(em8051_run(&fixture.cpu, 10, &result) == EM8051_STOP_BREAKPOINT);
    CHECK(result.instructions == 0);
    CHECK(result.machine_cycles == 2);
    CHECK(result.pc == ISR_INT0);
    CHECK(fixture.cpu.mInstructionCount == 0);
    CHECK(fixture.cpu.mTickDelay == 0);

    /* At SP=7F the first interrupt push targets absent upper IRAM. Entry must
     * raise once and stop without executing an opcode or installing a vector. */
    setup_fixture(&fixture, EM8051_VARIANT_8051);
    fixture.cpu.mPC = 0x1234;
    fixture.code[0x1234] = 0x00;
    fixture.cpu.mSFR[REG_SP] = 0x7F;
    fixture.cpu.mSFR[REG_IE] = IEMASK_EA | IEMASK_EX0;
    fixture.cpu.mSFR[REG_TCON] = TCONMASK_IE0;
    fixture.cpu.except = count_stack_exception;
    gStackExceptionCount = 0;
    CHECK(em8051_run(&fixture.cpu, 1, &result) == EM8051_STOP_EXCEPTION);
    CHECK(result.exception_code == EXCEPTION_STACK);
    CHECK(result.instructions == 0);
    CHECK(result.machine_cycles == 1);
    CHECK(result.pc == 0x1234);
    CHECK(fixture.cpu.mPC == 0x1234);
    CHECK(fixture.cpu.mSFR[REG_SP] == 0x80);
    CHECK(fixture.cpu.mTickDelay == 0);
    CHECK(fixture.cpu.mInterruptActive == 0);
    CHECK((fixture.cpu.mSFR[REG_TCON] & TCONMASK_IE0) != 0);
    CHECK(gStackExceptionCount == 1);
}

struct trace_capture
{
    struct em8051_trace_record records[128];
    size_t count;
};

/* This exact record-only signature is checked under the strict
 * warnings-as-errors build and provides no CPU pointer to the observer. */
static void capture_trace(const struct em8051_trace_record *aRecord,
                          void *aUser)
{
    struct trace_capture *capture = (struct trace_capture *)aUser;
    if (capture->count < ARRAY_SIZE(capture->records))
        capture->records[capture->count++] = *aRecord;
}

struct trace_isolation_context
{
    struct em8051_trace_record writable_copy;
    size_t count;
};

static void exercise_record_only_trace(
    const struct em8051_trace_record *aRecord, void *aUser)
{
    struct trace_isolation_context *context =
        (struct trace_isolation_context *)aUser;

    context->writable_copy = *aRecord;
    context->writable_copy.value = 0x42;
    context->count++;
}

/* Assignment to the public callback typedef is the compile-time signature
 * regression: an observer taking the former CPU parameter is incompatible. */
static em8051trace const record_only_trace_compile_check =
    exercise_record_only_trace;

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
    static const struct em8051_trace_record expected[] =
    {
        /* Exact instruction records plus direct, bit and read-modify-write
         * SFR writes prove PC/cycle/address/value field semantics. */
        { EM8051_TRACE_INSTRUCTION, 0,  0,  0,      0x75 },
        { EM8051_TRACE_SFR_WRITE,   0,  0,  0x82,   0x34 },
        { EM8051_TRACE_INSTRUCTION, 2,  3,  3,      0x75 },
        { EM8051_TRACE_SFR_WRITE,   2,  3,  0x83,   0x12 },
        { EM8051_TRACE_INSTRUCTION, 4,  6,  6,      0x74 },
        { EM8051_TRACE_INSTRUCTION, 5,  8,  8,      0xF0 },
        { EM8051_TRACE_MOVX_WRITE,  5,  8,  0x1234, 0x5A },
        { EM8051_TRACE_INSTRUCTION, 7,  9,  9,      0xE4 },
        { EM8051_TRACE_INSTRUCTION, 8,  10, 10,     0xE0 },
        { EM8051_TRACE_MOVX_READ,   8,  10, 0x1234, 0x5A },
        { EM8051_TRACE_INSTRUCTION, 10, 11, 11,     0xD2 },
        { EM8051_TRACE_SFR_WRITE,   10, 11, 0x90,   0xFF },
        { EM8051_TRACE_INSTRUCTION, 11, 13, 13,     0x53 },
        { EM8051_TRACE_SFR_WRITE,   11, 13, 0x90,   0xFE },
        { EM8051_TRACE_INSTRUCTION, 13, 16, 16,     0x75 },
        { EM8051_TRACE_SFR_WRITE,   13, 16, 0x90,   0xAA }
    };
    struct fixture traced;
    struct fixture untraced;
    struct fixture isolated;
    struct fixture isolation_control;
    struct trace_capture capture;
    struct trace_isolation_context isolation;
    struct em8051_run_result result;
    size_t i;

    memset(&capture, 0, sizeof(capture));
    setup_fixture(&traced, EM8051_VARIANT_SAB80535);
    memcpy(traced.code, program, sizeof(program));
    em8051_set_trace(&traced.cpu, capture_trace, &capture);
    CHECK(em8051_run(&traced.cpu, 9, &result) == EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(traced.xdata[0x1234] == 0x5A);
    CHECK(traced.cpu.mSFR[REG_ACC] == 0x5A);
    CHECK(traced.cpu.mSFR[REG_P1] == 0xAA);

    CHECK(capture.count == ARRAY_SIZE(expected));
    for (i = 0; i < capture.count && i < ARRAY_SIZE(expected); i++)
    {
        CHECK(capture.records[i].type == expected[i].type);
        CHECK(capture.records[i].machine_cycle == expected[i].machine_cycle);
        CHECK(capture.records[i].pc == expected[i].pc);
        CHECK(capture.records[i].address == expected[i].address);
        CHECK(capture.records[i].value == expected[i].value);
    }

    setup_fixture(&untraced, EM8051_VARIANT_SAB80535);
    memcpy(untraced.code, program, sizeof(program));
    CHECK(em8051_run(&untraced.cpu, 9, &result) == EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(untraced.cpu.mPC == traced.cpu.mPC);
    CHECK(untraced.cpu.mInstructionCount == traced.cpu.mInstructionCount);
    CHECK(untraced.cpu.mMachineCycleCount == traced.cpu.mMachineCycleCount);
    CHECK(memcmp(untraced.cpu.mSFR, traced.cpu.mSFR,
                 sizeof(traced.cpu.mSFR)) == 0);
    CHECK(memcmp(untraced.xdata, traced.xdata, sizeof(traced.xdata)) == 0);

    /* Instruction trace fires before operand reads. Mutating the observer's
     * writable record copy must not alter the current immediate operand or
     * any CPU-owned memory because none is reachable through the signature. */
    memset(&isolation, 0, sizeof(isolation));
    setup_fixture(&isolated, EM8051_VARIANT_8051);
    setup_fixture(&isolation_control, EM8051_VARIANT_8051);
    isolated.code[0] = 0x74; /* MOV A,#11 */
    isolated.code[1] = 0x11;
    isolated.code[2] = 0xF5; /* MOV 20,A */
    isolated.code[3] = 0x20;
    memcpy(isolation_control.code, isolated.code, 4);
    em8051_set_trace(&isolated.cpu, record_only_trace_compile_check,
                     &isolation);
    CHECK(em8051_run(&isolated.cpu, 2, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(em8051_run(&isolation_control.cpu, 2, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(isolation.count == 2);
    CHECK(isolation.writable_copy.value == 0x42);
    CHECK(isolated.code[1] == 0x11);
    CHECK(isolated.cpu.mSFR[REG_ACC] == 0x11);
    CHECK(isolated.cpu.mLowerData[0x20] == 0x11);
    CHECK(memcmp(isolated.code, isolation_control.code,
                 sizeof(isolated.code)) == 0);
    CHECK(memcmp(isolated.cpu.mLowerData, isolation_control.cpu.mLowerData,
                 sizeof(isolated.cpu.mLowerData)) == 0);
    CHECK(memcmp(isolated.cpu.mSFR, isolation_control.cpu.mSFR,
                 sizeof(isolated.cpu.mSFR)) == 0);

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
    CHECK(capture.records[0].type == EM8051_TRACE_INSTRUCTION);
    CHECK(capture.records[0].machine_cycle == 0);
    CHECK(capture.records[0].pc == 0);
    CHECK(capture.records[0].address == 0);
    CHECK(capture.records[0].value == 0xE0);
    CHECK(capture.records[1].type == EM8051_TRACE_UNSUPPORTED_MOVX_READ);
    CHECK(capture.records[1].machine_cycle == 0);
    CHECK(capture.records[1].pc == 0);
    CHECK(capture.records[1].address == 0);
    CHECK(capture.records[1].value == 0x77);

    /* Unsupported writes carry the same exact diagnostic fields. */
    memset(&capture, 0, sizeof(capture));
    reset(&untraced.cpu, false);
    untraced.code[0] = 0xF0; /* MOVX @DPTR,A */
    untraced.cpu.mExtData = NULL;
    untraced.cpu.mSFR[REG_DPL] = 0x56;
    untraced.cpu.mSFR[REG_DPH] = 0x34;
    untraced.cpu.mSFR[REG_ACC] = 0x66;
    em8051_set_trace(&untraced.cpu, capture_trace, &capture);
    CHECK(em8051_run(&untraced.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(capture.count == 2);
    CHECK(capture.records[0].type == EM8051_TRACE_INSTRUCTION);
    CHECK(capture.records[0].machine_cycle == 0);
    CHECK(capture.records[0].pc == 0);
    CHECK(capture.records[0].address == 0);
    CHECK(capture.records[0].value == 0xF0);
    CHECK(capture.records[1].type == EM8051_TRACE_UNSUPPORTED_MOVX_WRITE);
    CHECK(capture.records[1].machine_cycle == 0);
    CHECK(capture.records[1].pc == 0);
    CHECK(capture.records[1].address == 0x3456);
    CHECK(capture.records[1].value == 0x66);
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
    test_sfr_gateway_validation();
    test_exact_raw_loader();
    test_run_control_and_counters();
    test_interrupt_vector_stops();
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

/* Focused deterministic SLC-006 Timer0/Timer1 tests. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../emu8051.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define SFR_INDEX(a) ((uint8_t)((a) - 0x80u))

static int gFailures;

#define CHECK(condition)                                                       \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
        {                                                                      \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n",                   \
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

struct timer_capture
{
    struct em8051_timer_overflow_record records[32];
    size_t count;
};

static void setup_fixture(struct fixture *aFixture,
                          enum em8051_variant aVariant)
{
    memset(aFixture, 0, sizeof(*aFixture));
    aFixture->cpu.mCodeMem = aFixture->code;
    aFixture->cpu.mCodeMemMaxIdx = 0xffffu;
    aFixture->cpu.mExtData = aFixture->xdata;
    aFixture->cpu.mExtDataMaxIdx = 0xffffu;
    CHECK(em8051_init_variant(&aFixture->cpu, aVariant) == 0);
}

static void capture_timer_overflow(
    const struct em8051_timer_overflow_record *aRecord, void *aUser)
{
    struct timer_capture *capture = (struct timer_capture *)aUser;
    if (capture->count < ARRAY_SIZE(capture->records))
        capture->records[capture->count++] = *aRecord;
}

static em8051timeroverflow const record_only_timer_compile_check =
    capture_timer_overflow;

static uint16_t timer0_value(const struct em8051 *aCPU)
{
    return (uint16_t)(((uint16_t)aCPU->mSFR[REG_TH0] << 8) |
                      aCPU->mSFR[REG_TL0]);
}

static void run_cycles(struct em8051 *aCPU, uint64_t aCycles)
{
    uint64_t cycle;
    for (cycle = 0; cycle < aCycles; cycle++)
        (void)tick(aCPU);
}

static void configure_timer0_mode1(struct em8051 *aCPU, uint16_t aValue)
{
    aCPU->mSFR[REG_TMOD] =
        (uint8_t)((aCPU->mSFR[REG_TMOD] & 0xf0u) | TMODMASK_M0_0);
    aCPU->mSFR[REG_TH0] = (uint8_t)(aValue >> 8);
    aCPU->mSFR[REG_TL0] = (uint8_t)aValue;
    aCPU->mSFR[REG_TCON] |= TCONMASK_TR0;
}

static void configure_timer1_mode2(struct em8051 *aCPU, uint8_t aReload,
                                   uint8_t aValue)
{
    aCPU->mSFR[REG_TMOD] =
        (uint8_t)((aCPU->mSFR[REG_TMOD] & 0x0fu) | TMODMASK_M1_1);
    aCPU->mSFR[REG_TH1] = aReload;
    aCPU->mSFR[REG_TL1] = aValue;
    aCPU->mSFR[REG_TCON] |= TCONMASK_TR1;
}

static void test_timer0_dcef_exact_boundary(void)
{
    struct fixture fixture;
    struct timer_capture capture;

    memset(&capture, 0, sizeof(capture));
    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_timer0_mode1(&fixture.cpu, 0xdcefu);
    em8051_set_timer_overflow_callback(&fixture.cpu,
                                       record_only_timer_compile_check,
                                       &capture);

    run_cycles(&fixture.cpu, 8976u);
    CHECK(timer0_value(&fixture.cpu) == 0xffffu);
    CHECK((fixture.cpu.mSFR[REG_TCON] & TCONMASK_TF0) == 0);
    CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER0] == 0);
    CHECK(capture.count == 0);

    run_cycles(&fixture.cpu, 1u);
    CHECK(timer0_value(&fixture.cpu) == 0);
    CHECK((fixture.cpu.mSFR[REG_TCON] & TCONMASK_TF0) != 0);
    CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER0] == 1);
    CHECK(capture.count == 1);
    CHECK(capture.records[0].timer == EM8051_TIMER0);
    CHECK(capture.records[0].machine_cycle == 8977u);
    CHECK(capture.records[0].tl == 0);
    CHECK(capture.records[0].th == 0);
}

static void test_timer0_ffff_wrap(void)
{
    struct fixture fixture;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_timer0_mode1(&fixture.cpu, 0xffffu);
    CHECK(tick(&fixture.cpu));
    CHECK(timer0_value(&fixture.cpu) == 0);
    CHECK((fixture.cpu.mSFR[REG_TCON] & TCONMASK_TF0) != 0);
    CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER0] == 1);
}

static void test_timer0_live_byte_writes(void)
{
    struct fixture fixture;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_timer0_mode1(&fixture.cpu, 0x12feu);
    run_cycles(&fixture.cpu, 1u);
    CHECK(timer0_value(&fixture.cpu) == 0x12ffu);

    em8051_sfr_write(&fixture.cpu, 0x8au, 0xfeu); /* TL0 */
    em8051_sfr_write(&fixture.cpu, 0x8cu, 0xabu); /* TH0 */
    CHECK(timer0_value(&fixture.cpu) == 0xabfeu);
    run_cycles(&fixture.cpu, 1u);
    CHECK(timer0_value(&fixture.cpu) == 0xabffu);
    run_cycles(&fixture.cpu, 1u);
    CHECK(timer0_value(&fixture.cpu) == 0xac00u);
}

static void test_timer0_controller_vector_and_entry_cycles(void)
{
    struct fixture fixture;
    struct em8051_run_result result;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_timer0_mode1(&fixture.cpu, 0xffffu);
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)] =
        IEMASK_EA | IEMASK_ET0;

    run_cycles(&fixture.cpu, 1u);
    CHECK(timer0_value(&fixture.cpu) == 0);
    CHECK(em8051_run_until_pc(&fixture.cpu, EM8051_SAB_VECTOR_TIMER0,
                              1u, &result) == EM8051_STOP_TARGET_PC);
    CHECK(result.instructions == 0);
    CHECK(result.machine_cycles == 2);
    CHECK(timer0_value(&fixture.cpu) == 2u);
    CHECK((fixture.cpu.mSFR[REG_TCON] & TCONMASK_TF0) == 0);
    CHECK(fixture.cpu.mSABIrqDepth == 1);
}

static void test_timer0_isr_reload_latency(void)
{
    struct fixture fixture;
    struct em8051_run_result result;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_timer0_mode1(&fixture.cpu, 0xffffu);
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)] =
        IEMASK_EA | IEMASK_ET0;
    fixture.code[EM8051_SAB_VECTOR_TIMER0 + 0] = 0x75u; /* MOV TL0,#EF */
    fixture.code[EM8051_SAB_VECTOR_TIMER0 + 1] = 0x8au;
    fixture.code[EM8051_SAB_VECTOR_TIMER0 + 2] = 0xefu;
    fixture.code[EM8051_SAB_VECTOR_TIMER0 + 3] = 0x75u; /* MOV TH0,#DC */
    fixture.code[EM8051_SAB_VECTOR_TIMER0 + 4] = 0x8cu;
    fixture.code[EM8051_SAB_VECTOR_TIMER0 + 5] = 0xdcu;
    fixture.code[EM8051_SAB_VECTOR_TIMER0 + 6] = 0x32u; /* RETI */

    run_cycles(&fixture.cpu, 1u);
    CHECK(em8051_run_until_pc(&fixture.cpu, EM8051_SAB_VECTOR_TIMER0,
                              1u, &result) == EM8051_STOP_TARGET_PC);
    CHECK(timer0_value(&fixture.cpu) == 2u);
    CHECK(em8051_step_instruction(&fixture.cpu, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.machine_cycles == 2);
    CHECK(timer0_value(&fixture.cpu) == 0x00f1u);
    CHECK(em8051_step_instruction(&fixture.cpu, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.machine_cycles == 2);
    CHECK(timer0_value(&fixture.cpu) == 0xdcf3u);
    CHECK(em8051_step_instruction(&fixture.cpu, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.machine_cycles == 2);
    CHECK(timer0_value(&fixture.cpu) == 0xdcf5u);
    CHECK(fixture.cpu.mSABIrqDepth == 0);
}

static void test_timer0_four_interrupt_rom_cadence(void)
{
    struct fixture fixture;
    struct timer_capture capture;
    uint64_t limit = 50000u;
    bool completed = false;

    memset(&capture, 0, sizeof(capture));
    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    fixture.cpu.mLowerData[0x20] = 0;
    fixture.cpu.mLowerData[0x21] = 0;
    fixture.code[0x0000] = 0x80u; /* SJMP $ */
    fixture.code[0x0001] = 0xfeu;
    fixture.code[0x000b] = 0x05u; /* INC 20 */
    fixture.code[0x000c] = 0x20u;
    fixture.code[0x000d] = 0xe5u; /* MOV A,20 */
    fixture.code[0x000e] = 0x20u;
    fixture.code[0x000f] = 0x54u; /* ANL A,#03 */
    fixture.code[0x0010] = 0x03u;
    fixture.code[0x0011] = 0x70u; /* JNZ reload */
    fixture.code[0x0012] = 0x02u;
    fixture.code[0x0013] = 0x05u; /* INC 21 every fourth interrupt */
    fixture.code[0x0014] = 0x21u;
    fixture.code[0x0015] = 0x75u; /* MOV TL0,#EF */
    fixture.code[0x0016] = 0x8au;
    fixture.code[0x0017] = 0xefu;
    fixture.code[0x0018] = 0x75u; /* MOV TH0,#DC */
    fixture.code[0x0019] = 0x8cu;
    fixture.code[0x001a] = 0xdcu;
    fixture.code[0x001b] = 0x32u; /* RETI */
    configure_timer0_mode1(&fixture.cpu, 0xdcefu);
    em8051_set_timer_overflow_callback(&fixture.cpu,
                                       capture_timer_overflow, &capture);
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)] =
        IEMASK_EA | IEMASK_ET0;

    while (limit != 0)
    {
        limit--;
        (void)tick(&fixture.cpu);
        if (fixture.cpu.mLowerData[0x20] == 4u &&
            fixture.cpu.mLowerData[0x21] == 1u &&
            fixture.cpu.mSABIrqDepth == 0)
        {
            completed = true;
            break;
        }
    }
    CHECK(completed);
    CHECK(fixture.cpu.mLowerData[0x20] == 4u);
    CHECK(fixture.cpu.mLowerData[0x21] == 1u);
    CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER0] == 4u);
    CHECK(capture.count == 4u);
    CHECK(capture.records[0].machine_cycle == 8977u);
    CHECK(capture.records[1].machine_cycle -
          capture.records[0].machine_cycle > 8977u);
    CHECK(capture.records[2].machine_cycle -
          capture.records[1].machine_cycle ==
          capture.records[1].machine_cycle -
          capture.records[0].machine_cycle);
    CHECK(capture.records[3].machine_cycle -
          capture.records[2].machine_cycle ==
          capture.records[2].machine_cycle -
          capture.records[1].machine_cycle);
    CHECK(fixture.cpu.mSABIrqDepth == 0);
}

static void test_timer1_reload_from_th1(void)
{
    struct fixture fixture;
    struct timer_capture capture;

    memset(&capture, 0, sizeof(capture));
    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_timer1_mode2(&fixture.cpu, 0xfdu, 0xffu);
    em8051_set_timer_overflow_callback(&fixture.cpu,
                                       capture_timer_overflow, &capture);
    run_cycles(&fixture.cpu, 1u);
    CHECK(fixture.cpu.mSFR[REG_TL1] == 0xfdu);
    CHECK(fixture.cpu.mSFR[REG_TH1] == 0xfdu);
    CHECK((fixture.cpu.mSFR[REG_TCON] & TCONMASK_TF1) != 0);
    CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER1] == 1u);
    CHECK(capture.count == 1u);
    CHECK(capture.records[0].timer == EM8051_TIMER1);
    CHECK(capture.records[0].tl == 0xfdu);
    CHECK(capture.records[0].th == 0xfdu);
}

static void test_timer1_three_cycle_cadence(void)
{
    struct fixture fixture;
    struct timer_capture capture;

    memset(&capture, 0, sizeof(capture));
    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_timer1_mode2(&fixture.cpu, 0xfdu, 0xfdu);
    em8051_set_timer_overflow_callback(&fixture.cpu,
                                       capture_timer_overflow, &capture);
    run_cycles(&fixture.cpu, 9u);
    CHECK(capture.count == 3u);
    CHECK(capture.records[0].machine_cycle == 3u);
    CHECK(capture.records[1].machine_cycle == 6u);
    CHECK(capture.records[2].machine_cycle == 9u);
    CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER1] == 3u);
}

static void test_timer1_repeats_while_tf1_sticky(void)
{
    struct fixture fixture;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_timer1_mode2(&fixture.cpu, 0xfdu, 0xfdu);
    run_cycles(&fixture.cpu, 12u);
    CHECK((fixture.cpu.mSFR[REG_TCON] & TCONMASK_TF1) != 0);
    CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER1] == 4u);
    CHECK(fixture.cpu.mSFR[REG_TL1] == 0xfdu);
}

static void test_timer1_controller_vector(void)
{
    struct fixture fixture;
    struct em8051_run_result result;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_timer1_mode2(&fixture.cpu, 0xfdu, 0xffu);
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)] =
        IEMASK_EA | IEMASK_ET1;
    run_cycles(&fixture.cpu, 1u);
    CHECK(em8051_run_until_pc(&fixture.cpu, EM8051_SAB_VECTOR_TIMER1,
                              1u, &result) == EM8051_STOP_TARGET_PC);
    CHECK(result.instructions == 0);
    CHECK(result.machine_cycles == 2);
    CHECK((fixture.cpu.mSFR[REG_TCON] & TCONMASK_TF1) == 0);
    CHECK(fixture.cpu.mSABIrqDepth == 1);
    CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER1] == 1u);
    CHECK(fixture.cpu.mSFR[REG_TL1] == 0xffu);
}

static void test_timer1_live_th1_next_reload(void)
{
    struct fixture fixture;
    struct timer_capture capture;

    memset(&capture, 0, sizeof(capture));
    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_timer1_mode2(&fixture.cpu, 0xfdu, 0xfeu);
    run_cycles(&fixture.cpu, 1u);
    CHECK(fixture.cpu.mSFR[REG_TL1] == 0xffu);
    em8051_sfr_write(&fixture.cpu, 0x8du, 0xf8u); /* TH1 */
    CHECK(fixture.cpu.mSFR[REG_TL1] == 0xffu);
    em8051_set_timer_overflow_callback(&fixture.cpu,
                                       capture_timer_overflow, &capture);
    run_cycles(&fixture.cpu, 1u);
    CHECK(fixture.cpu.mSFR[REG_TL1] == 0xf8u);
    CHECK(fixture.cpu.mSFR[REG_TH1] == 0xf8u);
    CHECK(capture.count == 1u);
    CHECK(capture.records[0].tl == 0xf8u);
    run_cycles(&fixture.cpu, 7u);
    CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER1] == 1u);
    run_cycles(&fixture.cpu, 1u);
    CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER1] == 2u);
}

static void test_timer1_long_integer_run(void)
{
    struct fixture fixture;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_timer1_mode2(&fixture.cpu, 0xfdu, 0xfdu);
    run_cycles(&fixture.cpu, 300000u);
    CHECK(fixture.cpu.mMachineCycleCount == 300000u);
    CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER1] == 100000u);
    CHECK(fixture.cpu.mSFR[REG_TL1] == 0xfdu);
    CHECK((fixture.cpu.mSFR[REG_TCON] & TCONMASK_TF1) != 0);
}

static void test_classic_timer_regression(void)
{
    enum em8051_variant variant;

    for (variant = EM8051_VARIANT_8051;
         variant <= EM8051_VARIANT_8052; variant++)
    {
        struct fixture fixture;
        setup_fixture(&fixture, variant);
        configure_timer0_mode1(&fixture.cpu, 0xffffu);
        configure_timer1_mode2(&fixture.cpu, 0xfdu, 0xfdu);
        run_cycles(&fixture.cpu, 3u);
        CHECK(timer0_value(&fixture.cpu) == 2u);
        CHECK(fixture.cpu.mSFR[REG_TL1] == 0xfdu);
        CHECK((fixture.cpu.mSFR[REG_TCON] & TCONMASK_TF0) != 0);
        CHECK((fixture.cpu.mSFR[REG_TCON] & TCONMASK_TF1) != 0);
        CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER0] == 1u);
        CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER1] == 1u);
    }
}

static void test_timer_observer_determinism_and_neutrality(void)
{
    struct fixture traced;
    struct fixture repeated;
    struct fixture untraced;
    struct timer_capture capture;
    struct timer_capture repeated_capture;
    size_t i;

    memset(&capture, 0, sizeof(capture));
    memset(&repeated_capture, 0, sizeof(repeated_capture));
    setup_fixture(&traced, EM8051_VARIANT_SAB80535);
    setup_fixture(&repeated, EM8051_VARIANT_SAB80535);
    setup_fixture(&untraced, EM8051_VARIANT_SAB80535);
    configure_timer1_mode2(&traced.cpu, 0xfdu, 0xfdu);
    configure_timer1_mode2(&repeated.cpu, 0xfdu, 0xfdu);
    configure_timer1_mode2(&untraced.cpu, 0xfdu, 0xfdu);
    em8051_set_timer_overflow_callback(&traced.cpu,
                                       capture_timer_overflow, &capture);
    em8051_set_timer_overflow_callback(&repeated.cpu,
                                       capture_timer_overflow,
                                       &repeated_capture);
    em8051_set_timer_overflow_callback(NULL, capture_timer_overflow, &capture);
    run_cycles(&traced.cpu, 12u);
    run_cycles(&repeated.cpu, 12u);
    run_cycles(&untraced.cpu, 12u);

    CHECK(capture.count == 4u);
    CHECK(repeated_capture.count == capture.count);
    for (i = 0; i < capture.count; i++)
    {
        CHECK(repeated_capture.records[i].timer == capture.records[i].timer);
        CHECK(repeated_capture.records[i].machine_cycle ==
              capture.records[i].machine_cycle);
        CHECK(repeated_capture.records[i].tl == capture.records[i].tl);
        CHECK(repeated_capture.records[i].th == capture.records[i].th);
    }
    CHECK(traced.cpu.mPC == untraced.cpu.mPC);
    CHECK(traced.cpu.mInstructionCount == untraced.cpu.mInstructionCount);
    CHECK(traced.cpu.mMachineCycleCount == untraced.cpu.mMachineCycleCount);
    CHECK(traced.cpu.mTimerOverflowCount[EM8051_TIMER1] ==
          untraced.cpu.mTimerOverflowCount[EM8051_TIMER1]);
    CHECK(memcmp(traced.cpu.mSFR, untraced.cpu.mSFR,
                 sizeof(traced.cpu.mSFR)) == 0);

    reset(&traced.cpu, false);
    CHECK(traced.cpu.mTimerOverflowCount[EM8051_TIMER0] == 0);
    CHECK(traced.cpu.mTimerOverflowCount[EM8051_TIMER1] == 0);
}

static void test_sab_timer_seam_has_no_uart_side_effects(void)
{
    struct fixture fixture;
    uint8_t sbuf = 0xa5u;
    uint8_t scon = SCONMASK_SM0 | SCONMASK_SM1 | SCONMASK_TB8 |
                   SCONMASK_RB8 | SCONMASK_TI | SCONMASK_RI;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_timer1_mode2(&fixture.cpu, 0xfdu, 0xfdu);
    fixture.cpu.mSFR[REG_SBUF] = sbuf;
    fixture.cpu.mSFR[REG_SCON] = scon;
    fixture.cpu.serial_out_remaining_bits = 9u;
    fixture.cpu.serial_out_idx = 4u;
    run_cycles(&fixture.cpu, 6u);
    CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER1] == 2u);
    CHECK(fixture.cpu.mSFR[REG_SBUF] == sbuf);
    CHECK(fixture.cpu.mSFR[REG_SCON] == scon);
    CHECK(fixture.cpu.serial_out_remaining_bits == 9u);
    CHECK(fixture.cpu.serial_out_idx == 4u);
}

int main(void)
{
    test_timer0_dcef_exact_boundary();
    test_timer0_ffff_wrap();
    test_timer0_live_byte_writes();
    test_timer0_controller_vector_and_entry_cycles();
    test_timer0_isr_reload_latency();
    test_timer0_four_interrupt_rom_cadence();
    test_timer1_reload_from_th1();
    test_timer1_three_cycle_cadence();
    test_timer1_repeats_while_tf1_sticky();
    test_timer1_controller_vector();
    test_timer1_live_th1_next_reload();
    test_timer1_long_integer_run();
    test_classic_timer_regression();
    test_timer_observer_determinism_and_neutrality();
    test_sab_timer_seam_has_no_uart_side_effects();

    if (gFailures != 0)
    {
        fprintf(stderr, "SLC-006 timer tests failed: %d failure(s)\n",
                gFailures);
        return EXIT_FAILURE;
    }
    printf("SLC-006 timer tests passed\n");
    return EXIT_SUCCESS;
}

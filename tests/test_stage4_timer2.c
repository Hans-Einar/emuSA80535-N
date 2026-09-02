/* Focused deterministic SLC-015 tests for Siemens SAB80535 Timer2. */

#include <stdio.h>
#include <string.h>
#include "../emu8051.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define SFR_INDEX(a) ((uint8_t)((a) - 0x80u))
#define IRQ_BIT(a) ((uint16_t)(1u << (unsigned)(a)))

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

struct timer_capture
{
    struct em8051_timer_overflow_record records[16];
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

static void advance_cycles(struct em8051 *aCPU, uint64_t aCount)
{
    uint64_t before = aCPU->mMachineCycleCount;
    uint64_t i;
    for (i = 0; i < aCount; i++)
        (void)tick(aCPU);
    CHECK(aCPU->mMachineCycleCount == before + aCount);
}

static uint16_t timer2_value(const struct em8051 *aCPU)
{
    return (uint16_t)(
        ((uint16_t)aCPU->mSFR[SFR_INDEX(EM8051_SAB_SFR_TH2)] << 8) |
        aCPU->mSFR[SFR_INDEX(EM8051_SAB_SFR_TL2)]);
}

static void write_timer2(struct em8051 *aCPU, uint16_t aValue)
{
    em8051_sfr_write(aCPU, EM8051_SAB_SFR_TL2,
                     (uint8_t)(aValue & 0xffu));
    em8051_sfr_write(aCPU, EM8051_SAB_SFR_TH2,
                     (uint8_t)(aValue >> 8));
}

static void capture_timer(const struct em8051_timer_overflow_record *aRecord,
                          void *aUser)
{
    struct timer_capture *capture = (struct timer_capture *)aUser;
    if (aRecord->timer != EM8051_TIMER2)
        return;
    CHECK(capture->count < ARRAY_SIZE(capture->records));
    if (capture->count < ARRAY_SIZE(capture->records))
        capture->records[capture->count++] = *aRecord;
}

static void test_masks_reset_stop_and_classic_isolation(void)
{
    struct fixture fixture;
    enum em8051_variant variant;

    CHECK(EM8051_TIMER2 == 2);
    CHECK(EM8051_TIMER_COUNT == 3);
    CHECK(SAB_T2CONMASK_T2I0 == 0x01u);
    CHECK(SAB_T2CONMASK_T2I1 == 0x02u);
    CHECK(SAB_T2CONMASK_T2CM == 0x04u);
    CHECK(SAB_T2CONMASK_T2R0 == 0x08u);
    CHECK(SAB_T2CONMASK_T2R1 == 0x10u);
    CHECK(SAB_T2CONMASK_I2FR == 0x20u);
    CHECK(SAB_T2CONMASK_I3FR == 0x40u);
    CHECK(SAB_T2CONMASK_T2PS == 0x80u);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    CHECK(timer2_value(&fixture.cpu) == 0u);
    CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_T2CON)] == 0u);
    CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER2] == 0u);
    write_timer2(&fixture.cpu, 0x1234u);
    advance_cycles(&fixture.cpu, 7u);
    CHECK(timer2_value(&fixture.cpu) == 0x1234u);

    for (variant = EM8051_VARIANT_8051;
         variant <= EM8051_VARIANT_8052; variant++)
    {
        setup_fixture(&fixture, variant);
        write_timer2(&fixture.cpu, 0xffffu);
        em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_T2CON,
                         SAB_T2CONMASK_T2I0);
        advance_cycles(&fixture.cpu, 4u);
        CHECK(timer2_value(&fixture.cpu) == 0xffffu);
        CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER2] == 0u);
    }
}

static void test_fosc12_fosc24_and_free_running_phase(void)
{
    struct fixture fixture;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_T2CON,
                     SAB_T2CONMASK_T2I0);
    advance_cycles(&fixture.cpu, 3u);
    CHECK(timer2_value(&fixture.cpu) == 3u);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_T2CON,
                     SAB_T2CONMASK_T2PS | SAB_T2CONMASK_T2I0);
    advance_cycles(&fixture.cpu, 1u);
    CHECK(timer2_value(&fixture.cpu) == 0u);
    advance_cycles(&fixture.cpu, 1u);
    CHECK(timer2_value(&fixture.cpu) == 1u);
    advance_cycles(&fixture.cpu, 4u);
    CHECK(timer2_value(&fixture.cpu) == 3u);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    advance_cycles(&fixture.cpu, 1u);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_T2CON,
                     SAB_T2CONMASK_T2PS | SAB_T2CONMASK_T2I0);
    advance_cycles(&fixture.cpu, 1u);
    CHECK(timer2_value(&fixture.cpu) == 1u);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_T2CON, 0u);
    advance_cycles(&fixture.cpu, 1u);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_T2CON,
                     SAB_T2CONMASK_T2PS | SAB_T2CONMASK_T2I0);
    advance_cycles(&fixture.cpu, 1u);
    CHECK(timer2_value(&fixture.cpu) == 2u);
}

static void test_exact_5555_overflow_and_observer(void)
{
    struct fixture fixture;
    struct timer_capture capture;
    uint8_t p5_before;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    memset(&capture, 0, sizeof(capture));
    em8051_set_timer_overflow_callback(&fixture.cpu, capture_timer, &capture);
    write_timer2(&fixture.cpu, 0x5555u);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_P5, 0xefu);
    p5_before = fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_P5)];
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_T2CON,
                     SAB_T2CONMASK_T2I0);

    advance_cycles(&fixture.cpu, 43690u);
    CHECK(timer2_value(&fixture.cpu) == 0xffffu);
    CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] &
           SAB_IRCONMASK_TF2) == 0u);
    CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER2] == 0u);
    CHECK(capture.count == 0u);

    advance_cycles(&fixture.cpu, 1u);
    CHECK(timer2_value(&fixture.cpu) == 0u);
    CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] &
           SAB_IRCONMASK_TF2) != 0u);
    CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER2] == 1u);
    CHECK(capture.count == 1u);
    CHECK(capture.records[0].timer == EM8051_TIMER2);
    CHECK(capture.records[0].machine_cycle == 43691u);
    CHECK(capture.records[0].tl == 0u);
    CHECK(capture.records[0].th == 0u);
    CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_P5)] == p5_before);
}

static void test_sticky_repeated_overflow_and_masked_service(void)
{
    struct fixture fixture;
    struct timer_capture capture;
    unsigned i;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    memset(&capture, 0, sizeof(capture));
    em8051_set_timer_overflow_callback(&fixture.cpu, capture_timer, &capture);
    write_timer2(&fixture.cpu, 0xffffu);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_T2CON,
                     SAB_T2CONMASK_T2I0);
    advance_cycles(&fixture.cpu, 1u);
    CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER2] == 1u);
    CHECK((fixture.cpu.mSABIrqPending & IRQ_BIT(EM8051_SAB_IRQ_TIMER2)) != 0u);
    CHECK((fixture.cpu.mSABIrqEnabled & IRQ_BIT(EM8051_SAB_IRQ_TIMER2)) == 0u);

    write_timer2(&fixture.cpu, 0xffffu);
    advance_cycles(&fixture.cpu, 1u);
    CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER2] == 2u);
    CHECK(capture.count == 2u);
    CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] &
           SAB_IRCONMASK_TF2) != 0u);

    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_T2CON, 0u);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_IEN0, IEMASK_ET2);
    advance_cycles(&fixture.cpu, 2u);
    CHECK(fixture.cpu.mPC != EM8051_SAB_VECTOR_TIMER2);
    CHECK((fixture.cpu.mSABIrqPending & IRQ_BIT(EM8051_SAB_IRQ_TIMER2)) != 0u);

    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_IEN0,
                     IEMASK_EA | IEMASK_ET2);
    for (i = 0; i < 8u && fixture.cpu.mPC != EM8051_SAB_VECTOR_TIMER2; i++)
        (void)tick(&fixture.cpu);
    CHECK(fixture.cpu.mPC == EM8051_SAB_VECTOR_TIMER2);
    CHECK((fixture.cpu.mSABIrqInService &
           IRQ_BIT(EM8051_SAB_IRQ_TIMER2)) != 0u);
    CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] &
           SAB_IRCONMASK_TF2) != 0u);
}

static void test_live_writes_stop_restart_and_unsupported_modes(void)
{
    struct fixture fixture;
    uint8_t preserved_edge_bits;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_T2CON,
                     SAB_T2CONMASK_I2FR | SAB_T2CONMASK_I3FR |
                     SAB_T2CONMASK_T2I0);
    advance_cycles(&fixture.cpu, 1u);
    CHECK(timer2_value(&fixture.cpu) == 1u);
    preserved_edge_bits = (uint8_t)(fixture.cpu.mSFR[
        SFR_INDEX(EM8051_SAB_SFR_T2CON)] &
        (SAB_T2CONMASK_I2FR | SAB_T2CONMASK_I3FR));
    CHECK(preserved_edge_bits ==
          (SAB_T2CONMASK_I2FR | SAB_T2CONMASK_I3FR));

    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_T2CON, preserved_edge_bits);
    advance_cycles(&fixture.cpu, 4u);
    CHECK(timer2_value(&fixture.cpu) == 1u);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_TL2, 0xfeu);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_TH2, 0x12u);
    CHECK(timer2_value(&fixture.cpu) == 0x12feu);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_T2CON,
                     preserved_edge_bits | SAB_T2CONMASK_T2I0);
    advance_cycles(&fixture.cpu, 1u);
    CHECK(timer2_value(&fixture.cpu) == 0x12ffu);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_TL2, 0xffu);
    advance_cycles(&fixture.cpu, 1u);
    CHECK(timer2_value(&fixture.cpu) == 0x1300u);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    write_timer2(&fixture.cpu, 0x2345u);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_T2CON,
                     SAB_T2CONMASK_T2I1);
    advance_cycles(&fixture.cpu, 8u);
    CHECK(timer2_value(&fixture.cpu) == 0x2345u);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_T2CON,
                     SAB_T2CONMASK_T2I1 | SAB_T2CONMASK_T2I0);
    advance_cycles(&fixture.cpu, 8u);
    CHECK(timer2_value(&fixture.cpu) == 0x2345u);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_T2CON,
                     SAB_T2CONMASK_T2R1 | SAB_T2CONMASK_T2I0);
    advance_cycles(&fixture.cpu, 8u);
    CHECK(timer2_value(&fixture.cpu) == 0x2345u);
}

static void test_reload_disabled_crcs_and_isr_style_sequence(void)
{
    struct fixture fixture;
    bool entered = false;
    unsigned i;
    uint8_t initial_p5;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_CRCL, 0xaau);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_CRCH, 0x55u);
    write_timer2(&fixture.cpu, 0xffffu);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_T2CON,
                     SAB_T2CONMASK_T2I0);
    advance_cycles(&fixture.cpu, 1u);
    CHECK(timer2_value(&fixture.cpu) == 0u);
    CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_CRCL)] == 0xaau);
    CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_CRCH)] == 0x55u);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    initial_p5 = fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_P5)];
    fixture.code[EM8051_SAB_VECTOR_TIMER2 + 0u] = 0xc2u;
    fixture.code[EM8051_SAB_VECTOR_TIMER2 + 1u] = 0xc8u;
    fixture.code[EM8051_SAB_VECTOR_TIMER2 + 2u] = 0xb2u;
    fixture.code[EM8051_SAB_VECTOR_TIMER2 + 3u] = 0xfcu;
    fixture.code[EM8051_SAB_VECTOR_TIMER2 + 4u] = 0x75u;
    fixture.code[EM8051_SAB_VECTOR_TIMER2 + 5u] = 0xccu;
    fixture.code[EM8051_SAB_VECTOR_TIMER2 + 6u] = 0xf0u;
    fixture.code[EM8051_SAB_VECTOR_TIMER2 + 7u] = 0x75u;
    fixture.code[EM8051_SAB_VECTOR_TIMER2 + 8u] = 0xcdu;
    fixture.code[EM8051_SAB_VECTOR_TIMER2 + 9u] = 0xffu;
    fixture.code[EM8051_SAB_VECTOR_TIMER2 + 10u] = 0xc2u;
    fixture.code[EM8051_SAB_VECTOR_TIMER2 + 11u] = 0xc6u;
    fixture.code[EM8051_SAB_VECTOR_TIMER2 + 12u] = 0xd2u;
    fixture.code[EM8051_SAB_VECTOR_TIMER2 + 13u] = 0xc8u;
    fixture.code[EM8051_SAB_VECTOR_TIMER2 + 14u] = 0x32u;

    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_IEN0,
                     IEMASK_EA | IEMASK_ET2);
    write_timer2(&fixture.cpu, 0xffffu);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_T2CON,
                     SAB_T2CONMASK_T2I0);

    for (i = 0; i < 80u; i++)
    {
        (void)tick(&fixture.cpu);
        if (fixture.cpu.mSABIrqDepth != 0u)
            entered = true;
        if (entered && fixture.cpu.mSABIrqDepth == 0u &&
            fixture.cpu.mTickDelay == 0u)
            break;
    }
    CHECK(entered);
    CHECK(fixture.cpu.mSABIrqDepth == 0u);
    CHECK(((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_P5)] ^ initial_p5) &
           0x10u) != 0u);
    CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] &
           SAB_IRCONMASK_TF2) == 0u);
    CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_T2CON)] &
           SAB_T2CONMASK_T2I0) != 0u);
    CHECK(timer2_value(&fixture.cpu) >= 0xfff2u);
}

static void test_idle_progress_and_reset_counter(void)
{
    struct fixture fixture;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    write_timer2(&fixture.cpu, 0xfffeu);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_T2CON,
                     SAB_T2CONMASK_T2I0);
    fixture.cpu.mSFR[REG_PCON] |= 0x01u;
    advance_cycles(&fixture.cpu, 2u);
    CHECK(timer2_value(&fixture.cpu) == 0u);
    CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER2] == 1u);
    reset(&fixture.cpu, false);
    CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER2] == 0u);
    CHECK(timer2_value(&fixture.cpu) == 0u);
}

int main(void)
{
    test_masks_reset_stop_and_classic_isolation();
    test_fosc12_fosc24_and_free_running_phase();
    test_exact_5555_overflow_and_observer();
    test_sticky_repeated_overflow_and_masked_service();
    test_live_writes_stop_restart_and_unsupported_modes();
    test_reload_disabled_crcs_and_isr_style_sequence();
    test_idle_progress_and_reset_counter();

    if (gFailures != 0)
    {
        fprintf(stderr, "SLC-015 Timer2 tests failed: %d\n", gFailures);
        return 1;
    }
    puts("SLC-015 Timer2 tests passed");
    return 0;
}

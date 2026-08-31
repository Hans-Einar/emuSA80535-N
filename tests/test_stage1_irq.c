/* Focused deterministic SLC-004 tests for the SAB80535 IRQ controller. */

#include <stdio.h>
#include <stdlib.h>
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

static const uint16_t gVectors[EM8051_SAB_IRQ_SOURCE_COUNT] =
{
    EM8051_SAB_VECTOR_INT0,
    EM8051_SAB_VECTOR_TIMER0,
    EM8051_SAB_VECTOR_INT1,
    EM8051_SAB_VECTOR_TIMER1,
    EM8051_SAB_VECTOR_UART,
    EM8051_SAB_VECTOR_TIMER2,
    EM8051_SAB_VECTOR_ADC,
    EM8051_SAB_VECTOR_INT2,
    EM8051_SAB_VECTOR_INT3,
    EM8051_SAB_VECTOR_INT4,
    EM8051_SAB_VECTOR_INT5,
    EM8051_SAB_VECTOR_INT6
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

static void enable_source(struct em8051 *aCPU,
                          enum em8051_sab_irq_source aSource)
{
    uint8_t *ien0 = &aCPU->mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)];
    uint8_t *ien1 = &aCPU->mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN1)];
    *ien0 |= IEMASK_EA;
    if (aSource <= EM8051_SAB_IRQ_TIMER2)
        *ien0 |= (uint8_t)(1u << (unsigned)aSource);
    else
        *ien1 |= (uint8_t)(1u <<
            ((unsigned)aSource - (unsigned)EM8051_SAB_IRQ_ADC));
}

static void set_priority(struct em8051 *aCPU,
                         enum em8051_sab_irq_source aSource,
                         uint8_t aPriority)
{
    unsigned pair = (unsigned)aSource;
    uint8_t bit;
    uint8_t *ip0 = &aCPU->mSFR[SFR_INDEX(EM8051_SAB_SFR_IP0)];
    uint8_t *ip1 = &aCPU->mSFR[SFR_INDEX(EM8051_SAB_SFR_IP1)];
    if (pair >= (unsigned)EM8051_SAB_IRQ_ADC)
        pair -= (unsigned)EM8051_SAB_IRQ_ADC;
    bit = (uint8_t)(1u << pair);
    *ip0 = (uint8_t)((*ip0 & (uint8_t)~bit) |
                     ((aPriority & 1u) ? bit : 0u));
    *ip1 = (uint8_t)((*ip1 & (uint8_t)~bit) |
                     ((aPriority & 2u) ? bit : 0u));
}

static void accept_source(struct fixture *aFixture,
                          enum em8051_sab_irq_source aSource)
{
    struct em8051_run_result result;
    CHECK(em8051_run_until_pc(&aFixture->cpu, gVectors[aSource], 1, &result) ==
          EM8051_STOP_TARGET_PC);
    CHECK(result.instructions == 0);
    CHECK(result.machine_cycles == 2);
    CHECK(result.pc == gVectors[aSource]);
    CHECK(aFixture->cpu.mSABIrqDepth == 1);
    CHECK(aFixture->cpu.mSABIrqSourceStack[0] == (uint8_t)aSource);
    CHECK((aFixture->cpu.mSABIrqInService & IRQ_BIT(aSource)) != 0);
}

static void test_all_sources_pending_and_vectors(void)
{
    struct fixture fixture;
    unsigned source;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    for (source = 0; source < (unsigned)EM8051_SAB_IRQ_SOURCE_COUNT;
         source++)
    {
        CHECK(em8051_sab_irq_set_pending(
            &fixture.cpu, (enum em8051_sab_irq_source)source, true));
        CHECK((fixture.cpu.mSABIrqPending & IRQ_BIT(source)) != 0);
    }
    CHECK(fixture.cpu.mSABIrqPending == 0x0fffu);
    for (source = 0; source < (unsigned)EM8051_SAB_IRQ_SOURCE_COUNT;
         source++)
    {
        CHECK(em8051_sab_irq_set_pending(
            &fixture.cpu, (enum em8051_sab_irq_source)source, false));
    }
    CHECK(fixture.cpu.mSABIrqPending == 0);
    CHECK(!em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_SOURCE_COUNT, true));

    for (source = 0; source < (unsigned)EM8051_SAB_IRQ_SOURCE_COUNT;
         source++)
    {
        enum em8051_sab_irq_source irq =
            (enum em8051_sab_irq_source)source;
        setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
        enable_source(&fixture.cpu, irq);
        fixture.cpu.mSFR[REG_TCON] |= TCONMASK_IT0 | TCONMASK_IT1;
        CHECK(em8051_sab_irq_set_pending(&fixture.cpu, irq, true));
        accept_source(&fixture, irq);
    }
}

static void test_masking_and_enable_write_inhibit(void)
{
    struct fixture fixture;
    struct em8051_run_result result;
    uint16_t bit = IRQ_BIT(EM8051_SAB_IRQ_INT3);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)] = IEMASK_EA;
    CHECK(em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_INT3, true));
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.pc == 1);
    CHECK((fixture.cpu.mSABIrqPending & bit) != 0);

    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_IEN1,
                     SAB_IEN1MASK_EX3);
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.pc == 2);
    CHECK(fixture.cpu.mSABIrqDepth == 0);
    accept_source(&fixture, EM8051_SAB_IRQ_INT3);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)] = IEMASK_ET0;
    CHECK(em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_TIMER0, true));
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK((fixture.cpu.mSABIrqPending &
           IRQ_BIT(EM8051_SAB_IRQ_TIMER0)) != 0);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_IEN0,
                     IEMASK_EA | IEMASK_ET0);
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.pc == 2);
    accept_source(&fixture, EM8051_SAB_IRQ_TIMER0);
}

static void test_equal_poll_order(void)
{
    struct fixture fixture;
    unsigned first;

    for (first = 0; first + 1u < (unsigned)EM8051_SAB_IRQ_SOURCE_COUNT;
         first++)
    {
        enum em8051_sab_irq_source left =
            (enum em8051_sab_irq_source)first;
        enum em8051_sab_irq_source right =
            (enum em8051_sab_irq_source)(first + 1u);
        setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
        fixture.cpu.mSFR[REG_TCON] |= TCONMASK_IT0 | TCONMASK_IT1;
        enable_source(&fixture.cpu, left);
        enable_source(&fixture.cpu, right);
        CHECK(em8051_sab_irq_set_pending(&fixture.cpu, right, true));
        CHECK(em8051_sab_irq_set_pending(&fixture.cpu, left, true));
        accept_source(&fixture, left);
    }
}

static void test_priority_pair_mapping_and_b8_isolation(void)
{
    struct fixture fixture;
    unsigned pair;
    unsigned member;

    for (pair = 0; pair < 6u; pair++)
    {
        for (member = 0; member < 2u; member++)
        {
            enum em8051_sab_irq_source source =
                (enum em8051_sab_irq_source)(pair + (member ? 6u : 0u));
            setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
            enable_source(&fixture.cpu, source);
            set_priority(&fixture.cpu, source, 3);
            CHECK(em8051_sab_irq_set_pending(&fixture.cpu, source, true));
            accept_source(&fixture, source);
            CHECK(fixture.cpu.mSABIrqPriorityStack[0] == 3);
        }
    }

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    enable_source(&fixture.cpu, EM8051_SAB_IRQ_TIMER0);
    enable_source(&fixture.cpu, EM8051_SAB_IRQ_UART);
    enable_source(&fixture.cpu, EM8051_SAB_IRQ_INT5);
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IP0)] = 0;
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IP1)] = 0x10;
    CHECK(em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_TIMER0, true));
    CHECK(em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_INT5, true));
    CHECK(em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_UART, true));
    accept_source(&fixture, EM8051_SAB_IRQ_UART);
    CHECK(fixture.cpu.mSABIrqPriorityStack[0] == 2);

    /* B8 is IEN1.EADC in the SAB variant, never classic IP.PX0. */
    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    enable_source(&fixture.cpu, EM8051_SAB_IRQ_INT0);
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN1)] =
        SAB_IEN1MASK_EADC;
    fixture.cpu.mSFR[REG_TCON] |= TCONMASK_IT0;
    CHECK(em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_INT0, true));
    accept_source(&fixture, EM8051_SAB_IRQ_INT0);
    CHECK(fixture.cpu.mSABIrqPriorityStack[0] == 0);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)] = IEMASK_EA;
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN1)] =
        SAB_IEN1MASK_EADC;
    CHECK(em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_ADC, true));
    accept_source(&fixture, EM8051_SAB_IRQ_ADC);
}

static void test_nesting_equal_wait_and_reti_stack(void)
{
    struct fixture fixture;
    struct em8051_run_result result;
    static const enum em8051_sab_irq_source nested_sources[] =
    {
        EM8051_SAB_IRQ_INT0,
        EM8051_SAB_IRQ_TIMER0,
        EM8051_SAB_IRQ_INT1,
        EM8051_SAB_IRQ_TIMER1
    };
    size_t i;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    enable_source(&fixture.cpu, EM8051_SAB_IRQ_TIMER0);
    CHECK(em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_TIMER0, true));
    accept_source(&fixture, EM8051_SAB_IRQ_TIMER0);

    enable_source(&fixture.cpu, EM8051_SAB_IRQ_INT1);
    CHECK(em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_INT1, true));
    fixture.code[EM8051_SAB_VECTOR_TIMER0] = 0x00;
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.pc == EM8051_SAB_VECTOR_TIMER0 + 1u);
    CHECK(fixture.cpu.mSABIrqDepth == 1);
    CHECK((fixture.cpu.mSABIrqInService &
           IRQ_BIT(EM8051_SAB_IRQ_TIMER0)) != 0);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    enable_source(&fixture.cpu, EM8051_SAB_IRQ_TIMER0);
    CHECK(em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_TIMER0, true));
    accept_source(&fixture, EM8051_SAB_IRQ_TIMER0);
    enable_source(&fixture.cpu, EM8051_SAB_IRQ_UART);
    set_priority(&fixture.cpu, EM8051_SAB_IRQ_UART, 2);
    CHECK(em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_UART, true));
    CHECK(em8051_run_until_pc(&fixture.cpu, EM8051_SAB_VECTOR_UART,
                              1, &result) == EM8051_STOP_TARGET_PC);
    CHECK(fixture.cpu.mSABIrqDepth == 2);
    CHECK(fixture.cpu.mSABIrqSourceStack[0] == EM8051_SAB_IRQ_TIMER0);
    CHECK(fixture.cpu.mSABIrqSourceStack[1] == EM8051_SAB_IRQ_UART);
    CHECK(fixture.cpu.mSABIrqPriorityStack[1] == 2);

    CHECK(em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_UART, false));
    fixture.code[EM8051_SAB_VECTOR_UART] = 0x32; /* RETI */
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.pc == EM8051_SAB_VECTOR_TIMER0);
    CHECK(fixture.cpu.mSABIrqDepth == 1);
    CHECK((fixture.cpu.mSABIrqInService &
           IRQ_BIT(EM8051_SAB_IRQ_UART)) == 0);
    CHECK((fixture.cpu.mSABIrqInService &
           IRQ_BIT(EM8051_SAB_IRQ_TIMER0)) != 0);

    fixture.code[EM8051_SAB_VECTOR_TIMER0] = 0x00; /* inhibit NOP */
    fixture.code[EM8051_SAB_VECTOR_TIMER0 + 1u] = 0x32; /* RETI */
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.pc == EM8051_SAB_VECTOR_TIMER0 + 1u);
    CHECK(fixture.cpu.mSABIrqDepth == 1);
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.pc == 0);
    CHECK(fixture.cpu.mSABIrqDepth == 0);
    CHECK(fixture.cpu.mSABIrqInService == 0);

    /* Four strictly increasing levels fill the complete legal nesting
     * stack without merging source or priority state. */
    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    fixture.cpu.mSFR[REG_TCON] |= TCONMASK_IT0 | TCONMASK_IT1;
    for (i = 0; i < ARRAY_SIZE(nested_sources); i++)
    {
        enum em8051_sab_irq_source source = nested_sources[i];
        enable_source(&fixture.cpu, source);
        set_priority(&fixture.cpu, source, (uint8_t)i);
        CHECK(em8051_sab_irq_set_pending(&fixture.cpu, source, true));
        CHECK(em8051_run_until_pc(&fixture.cpu, gVectors[source], 1,
                                  &result) == EM8051_STOP_TARGET_PC);
        CHECK(result.machine_cycles == 2);
        CHECK(fixture.cpu.mSABIrqDepth == i + 1u);
        CHECK(fixture.cpu.mSABIrqSourceStack[i] == (uint8_t)source);
        CHECK(fixture.cpu.mSABIrqPriorityStack[i] == (uint8_t)i);
    }
    CHECK(fixture.cpu.mSABIrqDepth == 4);
}

static void test_ret_does_not_release(void)
{
    struct fixture fixture;
    struct em8051_run_result result;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    enable_source(&fixture.cpu, EM8051_SAB_IRQ_ADC);
    CHECK(em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_ADC, true));
    accept_source(&fixture, EM8051_SAB_IRQ_ADC);
    CHECK(em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_ADC, false));
    fixture.code[EM8051_SAB_VECTOR_ADC] = 0x22; /* RET */
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.pc == 0);
    CHECK(fixture.cpu.mSABIrqDepth == 1);
    CHECK((fixture.cpu.mSABIrqInService & IRQ_BIT(EM8051_SAB_IRQ_ADC)) != 0);

    enable_source(&fixture.cpu, EM8051_SAB_IRQ_INT2);
    CHECK(em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_INT2, true));
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.pc == 1);
    CHECK(fixture.cpu.mSABIrqDepth == 1);
}

static bool source_flag_is_set(const struct em8051 *aCPU,
                               enum em8051_sab_irq_source aSource)
{
    uint8_t ircon = aCPU->mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)];
    switch (aSource)
    {
    case EM8051_SAB_IRQ_INT0:
        return (aCPU->mSFR[REG_TCON] & TCONMASK_IE0) != 0;
    case EM8051_SAB_IRQ_TIMER0:
        return (aCPU->mSFR[REG_TCON] & TCONMASK_TF0) != 0;
    case EM8051_SAB_IRQ_INT1:
        return (aCPU->mSFR[REG_TCON] & TCONMASK_IE1) != 0;
    case EM8051_SAB_IRQ_TIMER1:
        return (aCPU->mSFR[REG_TCON] & TCONMASK_TF1) != 0;
    case EM8051_SAB_IRQ_UART:
        return (aCPU->mSFR[REG_SCON] & (SCONMASK_RI | SCONMASK_TI)) != 0;
    case EM8051_SAB_IRQ_TIMER2:
        return (ircon & (SAB_IRCONMASK_TF2 | SAB_IRCONMASK_EXF2)) != 0;
    case EM8051_SAB_IRQ_ADC:
        return (ircon & SAB_IRCONMASK_IADC) != 0;
    case EM8051_SAB_IRQ_INT2:
    case EM8051_SAB_IRQ_INT3:
    case EM8051_SAB_IRQ_INT4:
    case EM8051_SAB_IRQ_INT5:
    case EM8051_SAB_IRQ_INT6:
        return (ircon & (uint8_t)(1u <<
            ((unsigned)aSource - (unsigned)EM8051_SAB_IRQ_ADC))) != 0;
    default:
        return false;
    }
}

static void test_request_clear_matrix(void)
{
    struct fixture fixture;
    unsigned source;

    for (source = 0; source < (unsigned)EM8051_SAB_IRQ_SOURCE_COUNT;
         source++)
    {
        enum em8051_sab_irq_source irq =
            (enum em8051_sab_irq_source)source;
        bool software_clear = irq == EM8051_SAB_IRQ_UART ||
                              irq == EM8051_SAB_IRQ_TIMER2 ||
                              irq == EM8051_SAB_IRQ_ADC;
        setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
        enable_source(&fixture.cpu, irq);
        fixture.cpu.mSFR[REG_TCON] |= TCONMASK_IT0 | TCONMASK_IT1;
        if (irq == EM8051_SAB_IRQ_UART)
            fixture.cpu.mSFR[REG_SCON] |= SCONMASK_RI | SCONMASK_TI;
        else if (irq == EM8051_SAB_IRQ_TIMER2)
            fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] |=
                SAB_IRCONMASK_TF2 | SAB_IRCONMASK_EXF2;
        else
            CHECK(em8051_sab_irq_set_pending(&fixture.cpu, irq, true));
        accept_source(&fixture, irq);
        CHECK(source_flag_is_set(&fixture.cpu, irq) == software_clear);
        CHECK(((fixture.cpu.mSABIrqPending & IRQ_BIT(irq)) != 0) ==
              software_clear);
    }

    /* Level-triggered IE0/IE1 remain asserted on entry. */
    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    enable_source(&fixture.cpu, EM8051_SAB_IRQ_INT0);
    CHECK(em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_INT0, true));
    accept_source(&fixture, EM8051_SAB_IRQ_INT0);
    CHECK(source_flag_is_set(&fixture.cpu, EM8051_SAB_IRQ_INT0));

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    enable_source(&fixture.cpu, EM8051_SAB_IRQ_INT1);
    CHECK(em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_INT1, true));
    accept_source(&fixture, EM8051_SAB_IRQ_INT1);
    CHECK(source_flag_is_set(&fixture.cpu, EM8051_SAB_IRQ_INT1));
}

static void test_software_clear_and_timer2_split_gates(void)
{
    struct fixture fixture;
    struct em8051_run_result result;
    uint8_t *ircon;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    enable_source(&fixture.cpu, EM8051_SAB_IRQ_UART);
    fixture.cpu.mSFR[REG_SCON] = SCONMASK_RI | SCONMASK_TI;
    accept_source(&fixture, EM8051_SAB_IRQ_UART);
    em8051_sfr_write(&fixture.cpu, 0x98u, 0);
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK((fixture.cpu.mSABIrqPending & IRQ_BIT(EM8051_SAB_IRQ_UART)) == 0);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)] = IEMASK_EA;
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN1)] =
        SAB_IEN1MASK_EXEN2;
    ircon = &fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)];
    *ircon = SAB_IRCONMASK_TF2;
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.pc == 1); /* EXEN2 does not gate TF2. */
    *ircon = SAB_IRCONMASK_EXF2;
    accept_source(&fixture, EM8051_SAB_IRQ_TIMER2);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)] =
        IEMASK_EA | IEMASK_ET2;
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN1)] = 0;
    ircon = &fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)];
    *ircon = SAB_IRCONMASK_EXF2;
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.pc == 1); /* ET2 does not gate EXF2. */
    *ircon = SAB_IRCONMASK_TF2;
    accept_source(&fixture, EM8051_SAB_IRQ_TIMER2);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_IRCON, 0);
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK((fixture.cpu.mSABIrqPending & IRQ_BIT(EM8051_SAB_IRQ_TIMER2)) == 0);
}

static void test_ip_write_and_post_reti_inhibit(void)
{
    struct fixture fixture;
    struct em8051_run_result result;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    enable_source(&fixture.cpu, EM8051_SAB_IRQ_TIMER0);
    CHECK(em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_TIMER0, true));
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_IP0, 0x02);
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.pc == 1);
    accept_source(&fixture, EM8051_SAB_IRQ_TIMER0);
    CHECK(fixture.cpu.mSABIrqPriorityStack[0] == 1);

    /* An instruction-side enable write is itself followed by one complete
     * instruction before the newly eligible request can arbitrate. */
    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    CHECK(em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_TIMER0, true));
    fixture.code[0] = 0x75; /* MOV IEN0,#EA|ET0 */
    fixture.code[1] = EM8051_SAB_SFR_IEN0;
    fixture.code[2] = IEMASK_EA | IEMASK_ET0;
    fixture.code[3] = 0x00; /* mandatory following instruction */
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.pc == 3);
    CHECK(fixture.cpu.mSABIrqInhibitInstructions == 1);
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.pc == 4);
    CHECK(fixture.cpu.mSABIrqDepth == 0);
    accept_source(&fixture, EM8051_SAB_IRQ_TIMER0);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    enable_source(&fixture.cpu, EM8051_SAB_IRQ_INT3);
    CHECK(em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_INT3, true));
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_IP1, 0x04);
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.pc == 1);
    accept_source(&fixture, EM8051_SAB_IRQ_INT3);
    CHECK(fixture.cpu.mSABIrqPriorityStack[0] == 2);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    enable_source(&fixture.cpu, EM8051_SAB_IRQ_TIMER0);
    enable_source(&fixture.cpu, EM8051_SAB_IRQ_INT1);
    CHECK(em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_TIMER0, true));
    accept_source(&fixture, EM8051_SAB_IRQ_TIMER0);
    fixture.code[EM8051_SAB_VECTOR_TIMER0] = 0x32; /* RETI */
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.pc == 0);
    CHECK(em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_INT1, true));
    fixture.code[0] = 0x00;
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.pc == 1);
    CHECK(fixture.cpu.mSABIrqDepth == 0);
    accept_source(&fixture, EM8051_SAB_IRQ_INT1);
}

struct irq_trace_capture
{
    struct em8051_sab_irq_trace_record records[8];
    size_t count;
};

static void capture_irq_trace(
    const struct em8051_sab_irq_trace_record *aRecord, void *aUser)
{
    struct irq_trace_capture *capture = (struct irq_trace_capture *)aUser;
    if (capture->count < ARRAY_SIZE(capture->records))
        capture->records[capture->count++] = *aRecord;
}

static void test_irq_trace_and_observer_neutrality(void)
{
    struct fixture traced;
    struct fixture repeated;
    struct fixture untraced;
    struct irq_trace_capture capture;
    struct irq_trace_capture repeated_capture;
    struct em8051_run_result result;
    size_t i;

    memset(&capture, 0, sizeof(capture));
    memset(&repeated_capture, 0, sizeof(repeated_capture));
    setup_fixture(&traced, EM8051_VARIANT_SAB80535);
    setup_fixture(&repeated, EM8051_VARIANT_SAB80535);
    setup_fixture(&untraced, EM8051_VARIANT_SAB80535);
    enable_source(&traced.cpu, EM8051_SAB_IRQ_TIMER0);
    enable_source(&repeated.cpu, EM8051_SAB_IRQ_TIMER0);
    enable_source(&untraced.cpu, EM8051_SAB_IRQ_TIMER0);
    traced.code[EM8051_SAB_VECTOR_TIMER0] = 0x32;
    repeated.code[EM8051_SAB_VECTOR_TIMER0] = 0x32;
    untraced.code[EM8051_SAB_VECTOR_TIMER0] = 0x32;
    em8051_set_sab_irq_trace(&traced.cpu, capture_irq_trace, &capture);
    em8051_set_sab_irq_trace(&repeated.cpu, capture_irq_trace,
                             &repeated_capture);
    CHECK(em8051_sab_irq_set_pending(
        &traced.cpu, EM8051_SAB_IRQ_TIMER0, true));
    CHECK(em8051_sab_irq_set_pending(
        &repeated.cpu, EM8051_SAB_IRQ_TIMER0, true));
    CHECK(em8051_sab_irq_set_pending(
        &untraced.cpu, EM8051_SAB_IRQ_TIMER0, true));
    accept_source(&traced, EM8051_SAB_IRQ_TIMER0);
    accept_source(&repeated, EM8051_SAB_IRQ_TIMER0);
    accept_source(&untraced, EM8051_SAB_IRQ_TIMER0);
    CHECK(em8051_run(&traced.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(em8051_run(&repeated.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(em8051_run(&untraced.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);

    CHECK(capture.count == 3);
    CHECK(repeated_capture.count == capture.count);
    for (i = 0; i < capture.count && i < repeated_capture.count; i++)
    {
        CHECK(repeated_capture.records[i].event == capture.records[i].event);
        CHECK(repeated_capture.records[i].machine_cycle ==
              capture.records[i].machine_cycle);
        CHECK(repeated_capture.records[i].pc == capture.records[i].pc);
        CHECK(repeated_capture.records[i].source == capture.records[i].source);
        CHECK(repeated_capture.records[i].priority ==
              capture.records[i].priority);
        CHECK(repeated_capture.records[i].asserted ==
              capture.records[i].asserted);
        CHECK(repeated_capture.records[i].global_enabled ==
              capture.records[i].global_enabled);
        CHECK(repeated_capture.records[i].pending_mask ==
              capture.records[i].pending_mask);
        CHECK(repeated_capture.records[i].enabled_mask ==
              capture.records[i].enabled_mask);
        CHECK(repeated_capture.records[i].in_service_mask ==
              capture.records[i].in_service_mask);
        CHECK(repeated_capture.records[i].in_service_depth ==
              capture.records[i].in_service_depth);
    }
    CHECK(capture.records[0].event == EM8051_SAB_IRQ_TRACE_REQUEST);
    CHECK(capture.records[0].source == EM8051_SAB_IRQ_TIMER0);
    CHECK(capture.records[0].machine_cycle == 0);
    CHECK((capture.records[0].pending_mask &
           IRQ_BIT(EM8051_SAB_IRQ_TIMER0)) != 0);
    CHECK((capture.records[0].enabled_mask &
           IRQ_BIT(EM8051_SAB_IRQ_TIMER0)) != 0);
    CHECK(capture.records[0].in_service_depth == 0);
    CHECK(capture.records[1].event == EM8051_SAB_IRQ_TRACE_ACCEPT);
    CHECK(capture.records[1].priority == 0);
    CHECK(capture.records[1].in_service_depth == 1);
    CHECK((capture.records[1].in_service_mask &
           IRQ_BIT(EM8051_SAB_IRQ_TIMER0)) != 0);
    CHECK(capture.records[2].event == EM8051_SAB_IRQ_TRACE_RELEASE);
    CHECK(capture.records[2].machine_cycle == 2);
    CHECK(capture.records[2].in_service_depth == 0);
    CHECK(capture.records[2].in_service_mask == 0);
    CHECK(traced.cpu.mPC == untraced.cpu.mPC);
    CHECK(traced.cpu.mInstructionCount == untraced.cpu.mInstructionCount);
    CHECK(traced.cpu.mMachineCycleCount == untraced.cpu.mMachineCycleCount);
    CHECK(traced.cpu.mSABIrqPending == untraced.cpu.mSABIrqPending);
    CHECK(traced.cpu.mSABIrqEnabled == untraced.cpu.mSABIrqEnabled);
    CHECK(traced.cpu.mSABIrqInService == untraced.cpu.mSABIrqInService);
    CHECK(memcmp(traced.cpu.mSFR, untraced.cpu.mSFR,
                 sizeof(traced.cpu.mSFR)) == 0);
}

static void test_failed_entry_and_classic_regression(void)
{
    struct fixture fixture;
    struct em8051_run_result result;
    enum em8051_variant variant;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    fixture.cpu.mSFR[REG_SP] = 0xff;
    enable_source(&fixture.cpu, EM8051_SAB_IRQ_TIMER0);
    CHECK(em8051_sab_irq_set_pending(
        &fixture.cpu, EM8051_SAB_IRQ_TIMER0, true));
    CHECK(em8051_run(&fixture.cpu, 1, &result) == EM8051_STOP_EXCEPTION);
    CHECK(result.exception_code == EXCEPTION_STACK);
    CHECK(result.instructions == 0);
    CHECK(result.machine_cycles == 1);
    CHECK(result.pc == 0);
    CHECK(fixture.cpu.mSABIrqDepth == 0);
    CHECK(fixture.cpu.mSABIrqInService == 0);
    CHECK((fixture.cpu.mSABIrqPending &
           IRQ_BIT(EM8051_SAB_IRQ_TIMER0)) != 0);

    for (variant = EM8051_VARIANT_8051;
         variant <= EM8051_VARIANT_8052; variant++)
    {
        setup_fixture(&fixture, variant);
        fixture.cpu.mSFR[REG_IE] = IEMASK_EA | IEMASK_EX0;
        fixture.cpu.mSFR[REG_IP] = IPMASK_PX0;
        fixture.cpu.mSFR[REG_TCON] = TCONMASK_IE0;
        fixture.code[ISR_INT0] = 0x32; /* RETI */
        CHECK(em8051_run_until_pc(&fixture.cpu, ISR_INT0, 1, &result) ==
              EM8051_STOP_TARGET_PC);
        CHECK(result.machine_cycles == 2);
        CHECK(fixture.cpu.mInterruptActive == 2);
        fixture.cpu.mSFR[REG_TCON] &= (uint8_t)~TCONMASK_IE0;
        CHECK(em8051_run(&fixture.cpu, 1, &result) ==
              EM8051_STOP_INSTRUCTION_LIMIT);
        CHECK(result.pc == 0);
        CHECK(fixture.cpu.mInterruptActive == 0);
        CHECK(fixture.cpu.mSABIrqDepth == 0);
    }
}

int main(void)
{
    test_all_sources_pending_and_vectors();
    test_masking_and_enable_write_inhibit();
    test_equal_poll_order();
    test_priority_pair_mapping_and_b8_isolation();
    test_nesting_equal_wait_and_reti_stack();
    test_ret_does_not_release();
    test_request_clear_matrix();
    test_software_clear_and_timer2_split_gates();
    test_ip_write_and_post_reti_inhibit();
    test_irq_trace_and_observer_neutrality();
    test_failed_entry_and_classic_regression();

    if (gFailures != 0)
    {
        fprintf(stderr, "Stage-1 IRQ tests failed: %d failure(s)\n", gFailures);
        return EXIT_FAILURE;
    }
    printf("Stage-1 IRQ tests passed\n");
    return EXIT_SUCCESS;
}

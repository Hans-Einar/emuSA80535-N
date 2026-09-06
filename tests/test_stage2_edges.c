/* Focused deterministic SLC-013 tests for SAB80535 external interrupts. */

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

struct trace_capture
{
    struct em8051_sab_external_trace_record records[128];
    size_t count;
};

static const enum em8051_sab_irq_source gIrqSources[] =
{
    EM8051_SAB_IRQ_INT0,
    EM8051_SAB_IRQ_INT1,
    EM8051_SAB_IRQ_INT2,
    EM8051_SAB_IRQ_INT3,
    EM8051_SAB_IRQ_INT4,
    EM8051_SAB_IRQ_INT5,
    EM8051_SAB_IRQ_INT6
};

static const uint16_t gVectors[] =
{
    EM8051_SAB_VECTOR_INT0,
    EM8051_SAB_VECTOR_INT1,
    EM8051_SAB_VECTOR_INT2,
    EM8051_SAB_VECTOR_INT3,
    EM8051_SAB_VECTOR_INT4,
    EM8051_SAB_VECTOR_INT5,
    EM8051_SAB_VECTOR_INT6
};

static const uint8_t gRequestMasks[] =
{
    0x02u, 0x08u, 0x02u, 0x04u, 0x08u, 0x10u, 0x20u
};

static const uint8_t gPorts[] =
{
    0xb0u, 0xb0u, 0x90u, 0x90u, 0x90u, 0x90u, 0x90u
};

static const uint8_t gPinMasks[] =
{
    0x04u, 0x08u, 0x10u, 0x01u, 0x02u, 0x04u, 0x08u
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

static void capture_trace(
    const struct em8051_sab_external_trace_record *aRecord, void *aUser)
{
    struct trace_capture *capture = (struct trace_capture *)aUser;
    if (capture->count < ARRAY_SIZE(capture->records))
        capture->records[capture->count++] = *aRecord;
}

static uint8_t *request_sfr(struct fixture *aFixture, unsigned aExternal)
{
    if (aExternal < 2u)
        return &aFixture->cpu.mSFR[REG_TCON];
    return &aFixture->cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)];
}

static bool request_is_set(struct fixture *aFixture, unsigned aExternal)
{
    return (*request_sfr(aFixture, aExternal) &
            gRequestMasks[aExternal]) != 0;
}

static void clear_request(struct fixture *aFixture, unsigned aExternal)
{
    *request_sfr(aFixture, aExternal) &=
        (uint8_t)~gRequestMasks[aExternal];
}

static void select_edge_mode(struct fixture *aFixture, unsigned aExternal,
                             bool aRising)
{
    if (aExternal == 0u)
        aFixture->cpu.mSFR[REG_TCON] |= TCONMASK_IT0;
    else if (aExternal == 1u)
        aFixture->cpu.mSFR[REG_TCON] |= TCONMASK_IT1;
    else if (aExternal == 2u)
    {
        if (aRising)
            aFixture->cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_T2CON)] |= 0x20u;
        else
            aFixture->cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_T2CON)] &=
                (uint8_t)~0x20u;
    }
    else if (aExternal == 3u)
    {
        if (aRising)
            aFixture->cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_T2CON)] |= 0x40u;
        else
            aFixture->cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_T2CON)] &=
                (uint8_t)~0x40u;
    }
}

static void enable_source(struct fixture *aFixture, unsigned aExternal)
{
    enum em8051_sab_irq_source source = gIrqSources[aExternal];
    aFixture->cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)] |= IEMASK_EA;
    if (source == EM8051_SAB_IRQ_INT0)
        aFixture->cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)] |= IEMASK_EX0;
    else if (source == EM8051_SAB_IRQ_INT1)
        aFixture->cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)] |= IEMASK_EX1;
    else
        aFixture->cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN1)] |=
            (uint8_t)(1u <<
                ((unsigned)source - (unsigned)EM8051_SAB_IRQ_ADC));
}

static void set_priority(struct fixture *aFixture, unsigned aExternal,
                         uint8_t aPriority)
{
    unsigned pair = (unsigned)gIrqSources[aExternal];
    uint8_t bit;
    if (pair >= (unsigned)EM8051_SAB_IRQ_ADC)
        pair -= (unsigned)EM8051_SAB_IRQ_ADC;
    bit = (uint8_t)(1u << pair);
    if (aPriority & 1u)
        aFixture->cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IP0)] |= bit;
    if (aPriority & 2u)
        aFixture->cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IP1)] |= bit;
}

static void trigger_qualifying(struct fixture *aFixture, unsigned aExternal)
{
    enum em8051_sab_external_source source =
        (enum em8051_sab_external_source)aExternal;
    if (aExternal <= 3u)
    {
        CHECK(em8051_sab_external_drive(&aFixture->cpu, source, false));
    }
    else
    {
        CHECK(em8051_sab_external_drive(&aFixture->cpu, source, false));
        CHECK(!request_is_set(aFixture, aExternal));
        CHECK(em8051_sab_external_release(&aFixture->cpu, source));
    }
}

static void accept_source(struct fixture *aFixture, unsigned aExternal)
{
    struct em8051_run_result result;
    CHECK(em8051_run_until_pc(&aFixture->cpu, gVectors[aExternal], 1,
                              &result) == EM8051_STOP_TARGET_PC);
    CHECK(result.instructions == 0);
    CHECK(result.machine_cycles == 2u);
    CHECK(aFixture->cpu.mSABIrqDepth == 1u);
    CHECK(aFixture->cpu.mSABIrqSourceStack[0] ==
          (uint8_t)gIrqSources[aExternal]);
}

static void test_literal_pin_flag_map_and_all_vectors(void)
{
    unsigned external;

    CHECK(SAB_T2CONMASK_I2FR == 0x20u);
    CHECK(SAB_T2CONMASK_I3FR == 0x40u);
    CHECK(SAB_IRCONMASK_IEX2 == 0x02u);
    CHECK(SAB_IRCONMASK_IEX3 == 0x04u);
    CHECK(SAB_IRCONMASK_IEX4 == 0x08u);
    CHECK(SAB_IRCONMASK_IEX5 == 0x10u);
    CHECK(SAB_IRCONMASK_IEX6 == 0x20u);

    for (external = 0; external < 7u; external++)
    {
        struct fixture fixture;
        uint8_t latch_before;
        uint8_t latch_after;
        uint8_t pins;

        setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
        CHECK(em8051_sab_port_get_latch(&fixture.cpu, gPorts[external],
                                        &latch_before));
        CHECK(em8051_sab_external_drive(&fixture.cpu,
            (enum em8051_sab_external_source)external, false));
        CHECK(em8051_sab_port_get_pins(&fixture.cpu, gPorts[external],
                                       &pins));
        CHECK((pins & gPinMasks[external]) == 0u);
        CHECK(em8051_sab_external_release(&fixture.cpu,
            (enum em8051_sab_external_source)external));
        CHECK(em8051_sab_port_get_pins(&fixture.cpu, gPorts[external],
                                       &pins));
        CHECK((pins & gPinMasks[external]) == gPinMasks[external]);
        CHECK(em8051_sab_port_get_latch(&fixture.cpu, gPorts[external],
                                        &latch_after));
        CHECK(latch_after == latch_before);

        setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
        select_edge_mode(&fixture, external, false);
        enable_source(&fixture, external);
        CHECK(em8051_sab_port_get_latch(&fixture.cpu, gPorts[external],
                                        &latch_before));
        trigger_qualifying(&fixture, external);
        CHECK(request_is_set(&fixture, external));
        CHECK(em8051_sab_port_get_latch(&fixture.cpu, gPorts[external],
                                        &latch_after));
        CHECK(latch_after == latch_before);
        accept_source(&fixture, external);
        CHECK(!request_is_set(&fixture, external));
    }
}

static void test_nonqualifying_and_int0_rearm(void)
{
    struct fixture fixture;
    struct trace_capture capture;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    memset(&capture, 0, sizeof(capture));
    em8051_set_sab_external_trace(&fixture.cpu, capture_trace, &capture);
    select_edge_mode(&fixture, 0u, false);
    CHECK(em8051_sab_external_drive(&fixture.cpu,
                                    EM8051_SAB_EXTERNAL_INT0, false));
    CHECK(request_is_set(&fixture, 0u));
    clear_request(&fixture, 0u);
    CHECK(em8051_sab_external_drive(&fixture.cpu,
                                    EM8051_SAB_EXTERNAL_INT0, false));
    CHECK(!request_is_set(&fixture, 0u));
    CHECK(em8051_sab_external_release(&fixture.cpu,
                                      EM8051_SAB_EXTERNAL_INT0));
    CHECK(!request_is_set(&fixture, 0u));
    CHECK(capture.records[capture.count - 1u].trigger ==
          EM8051_SAB_EXTERNAL_TRACE_NON_QUALIFYING);
    CHECK(em8051_sab_external_drive(&fixture.cpu,
                                    EM8051_SAB_EXTERNAL_INT0, false));
    CHECK(request_is_set(&fixture, 0u));
}

static void test_int0_int1_level_and_edge_modes(void)
{
    unsigned external;
    for (external = 0; external < 2u; external++)
    {
        struct fixture fixture;
        struct em8051_run_result result;
        enum em8051_sab_external_source source =
            (enum em8051_sab_external_source)external;

        setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
        CHECK(em8051_sab_external_drive(&fixture.cpu, source, false));
        CHECK(request_is_set(&fixture, external));
        CHECK(em8051_run(&fixture.cpu, 1, &result) ==
              EM8051_STOP_INSTRUCTION_LIMIT);
        CHECK(request_is_set(&fixture, external));
        CHECK(em8051_sab_external_release(&fixture.cpu, source));
        CHECK(!request_is_set(&fixture, external));

        enable_source(&fixture, external);
        CHECK(em8051_sab_external_drive(&fixture.cpu, source, false));
        accept_source(&fixture, external);
        CHECK(request_is_set(&fixture, external));
        CHECK(em8051_sab_external_release(&fixture.cpu, source));
        CHECK(!request_is_set(&fixture, external));

        setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
        select_edge_mode(&fixture, external, false);
        trigger_qualifying(&fixture, external);
        CHECK(request_is_set(&fixture, external));
    }
}

static void test_int2_int3_both_selections_and_live_change(void)
{
    unsigned external;
    for (external = 2u; external <= 3u; external++)
    {
        struct fixture fixture;
        enum em8051_sab_external_source source =
            (enum em8051_sab_external_source)external;

        setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
        select_edge_mode(&fixture, external, false);
        CHECK(em8051_sab_external_drive(&fixture.cpu, source, false));
        CHECK(request_is_set(&fixture, external));
        clear_request(&fixture, external);
        CHECK(em8051_sab_external_release(&fixture.cpu, source));
        CHECK(!request_is_set(&fixture, external));

        CHECK(em8051_sab_external_drive(&fixture.cpu, source, false));
        clear_request(&fixture, external);
        select_edge_mode(&fixture, external, true);
        CHECK(!request_is_set(&fixture, external));
        CHECK(em8051_sab_external_release(&fixture.cpu, source));
        CHECK(request_is_set(&fixture, external));
        clear_request(&fixture, external);
        CHECK(em8051_sab_external_drive(&fixture.cpu, source, false));
        CHECK(!request_is_set(&fixture, external));
    }
}

static void test_fixed_rising_int4_int5_int6_independent(void)
{
    struct fixture fixture;
    unsigned external;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    for (external = 4u; external <= 6u; external++)
    {
        CHECK(em8051_sab_external_drive(&fixture.cpu,
            (enum em8051_sab_external_source)external, false));
        CHECK(!request_is_set(&fixture, external));
    }
    for (external = 4u; external <= 6u; external++)
    {
        uint8_t expected = gRequestMasks[external];
        fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] = 0;
        CHECK(em8051_sab_external_release(&fixture.cpu,
            (enum em8051_sab_external_source)external));
        CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] ==
              expected);
    }
}

static void test_masking_enable_boundary_and_polling(void)
{
    struct fixture fixture;
    struct em8051_run_result result;
    struct em8051_sab_external_schedule_event event;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    select_edge_mode(&fixture, 0u, false);
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)] = IEMASK_EX0;
    trigger_qualifying(&fixture, 0u);
    CHECK(request_is_set(&fixture, 0u));
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(fixture.cpu.mPC == 1u);
    CHECK(fixture.cpu.mSABIrqDepth == 0u);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    select_edge_mode(&fixture, 0u, false);
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)] = IEMASK_EA;
    trigger_qualifying(&fixture, 0u);
    CHECK(request_is_set(&fixture, 0u));
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(fixture.cpu.mPC == 1u);
    CHECK(fixture.cpu.mSABIrqDepth == 0u);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_IEN0,
                     IEMASK_EA | IEMASK_EX0);
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(fixture.cpu.mPC == 2u);
    accept_source(&fixture, 0u);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    select_edge_mode(&fixture, 0u, false);
    select_edge_mode(&fixture, 1u, false);
    enable_source(&fixture, 0u);
    enable_source(&fixture, 1u);
    event.machine_cycle = 1u;
    event.action = EM8051_SAB_EXTERNAL_SCHEDULE_DRIVE;
    event.level = false;
    event.source = EM8051_SAB_EXTERNAL_INT1;
    CHECK(em8051_sab_external_schedule(&fixture.cpu, &event));
    event.source = EM8051_SAB_EXTERNAL_INT0;
    CHECK(em8051_sab_external_schedule(&fixture.cpu, &event));
    CHECK(em8051_run_until_pc(&fixture.cpu, EM8051_SAB_VECTOR_INT0, 2,
                              &result) == EM8051_STOP_TARGET_PC);
    CHECK((fixture.cpu.mSABIrqPending &
           IRQ_BIT(EM8051_SAB_IRQ_INT1)) != 0);
}

static void test_preemption_and_wait_until_reti(void)
{
    struct fixture fixture;
    struct em8051_run_result result;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    select_edge_mode(&fixture, 0u, false);
    select_edge_mode(&fixture, 3u, false);
    enable_source(&fixture, 0u);
    enable_source(&fixture, 3u);
    trigger_qualifying(&fixture, 0u);
    accept_source(&fixture, 0u);
    set_priority(&fixture, 3u, 2u);
    trigger_qualifying(&fixture, 3u);
    CHECK(em8051_run_until_pc(&fixture.cpu, EM8051_SAB_VECTOR_INT3, 1,
                              &result) == EM8051_STOP_TARGET_PC);
    CHECK(fixture.cpu.mSABIrqDepth == 2u);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    select_edge_mode(&fixture, 0u, false);
    select_edge_mode(&fixture, 1u, false);
    enable_source(&fixture, 0u);
    enable_source(&fixture, 1u);
    trigger_qualifying(&fixture, 0u);
    accept_source(&fixture, 0u);
    trigger_qualifying(&fixture, 1u);
    fixture.code[EM8051_SAB_VECTOR_INT0] = 0x00u;
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(fixture.cpu.mSABIrqDepth == 1u);
    fixture.code[EM8051_SAB_VECTOR_INT0 + 1u] = 0x32u;
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(fixture.cpu.mSABIrqDepth == 0u);
    fixture.code[0] = 0x00u;
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(em8051_run_until_pc(&fixture.cpu, EM8051_SAB_VECTOR_INT1, 1,
                              &result) == EM8051_STOP_TARGET_PC);
}

static void schedule_replay_sequence(struct fixture *aFixture)
{
    static const struct em8051_sab_external_schedule_event events[] =
    {
        { 1u, EM8051_SAB_EXTERNAL_INT2,
          EM8051_SAB_EXTERNAL_SCHEDULE_DRIVE, false },
        { 2u, EM8051_SAB_EXTERNAL_INT2,
          EM8051_SAB_EXTERNAL_SCHEDULE_RELEASE, false },
        { 3u, EM8051_SAB_EXTERNAL_INT2,
          EM8051_SAB_EXTERNAL_SCHEDULE_DRIVE, false },
        { 3u, EM8051_SAB_EXTERNAL_INT3,
          EM8051_SAB_EXTERNAL_SCHEDULE_DRIVE, false },
        { 4u, EM8051_SAB_EXTERNAL_INT4,
          EM8051_SAB_EXTERNAL_SCHEDULE_DRIVE, false },
        { 5u, EM8051_SAB_EXTERNAL_INT4,
          EM8051_SAB_EXTERNAL_SCHEDULE_RELEASE, false }
    };
    size_t i;
    for (i = 0; i < ARRAY_SIZE(events); i++)
        CHECK(em8051_sab_external_schedule(&aFixture->cpu, &events[i]));
}

static void test_schedule_contract_replay_reset_and_observer_neutrality(void)
{
    struct fixture first;
    struct fixture second;
    struct trace_capture first_trace;
    struct trace_capture second_trace;
    struct em8051_run_result first_result;
    struct em8051_run_result second_result;
    struct em8051_sab_external_schedule_event event;
    unsigned i;

    setup_fixture(&first, EM8051_VARIANT_SAB80535);
    setup_fixture(&second, EM8051_VARIANT_SAB80535);
    memset(&first_trace, 0, sizeof(first_trace));
    memset(&second_trace, 0, sizeof(second_trace));
    em8051_set_sab_external_trace(&first.cpu, capture_trace, &first_trace);
    em8051_set_sab_external_trace(&second.cpu, capture_trace, &second_trace);
    schedule_replay_sequence(&first);
    schedule_replay_sequence(&second);
    CHECK(em8051_sab_external_scheduled_count(&first.cpu) == 6u);
    CHECK(em8051_run(&first.cpu, 6, &first_result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(em8051_run(&second.cpu, 6, &second_result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(first_trace.count == second_trace.count);
    CHECK(first_trace.count == 6u);
    CHECK(memcmp(first_trace.records, second_trace.records,
                 first_trace.count * sizeof(first_trace.records[0])) == 0);
    CHECK(first_trace.records[0].machine_cycle == 1u);
    CHECK(first_trace.records[1].machine_cycle == 2u);
    CHECK(first_trace.records[2].machine_cycle == 3u);
    CHECK(first_trace.records[3].machine_cycle == 3u);
    CHECK(first_trace.records[first_trace.count - 1u].machine_cycle == 5u);
    CHECK(first.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] ==
          second.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)]);
    CHECK(first.cpu.mMachineCycleCount == second.cpu.mMachineCycleCount);

    event.machine_cycle = 4u;
    event.source = EM8051_SAB_EXTERNAL_INT0;
    event.action = EM8051_SAB_EXTERNAL_SCHEDULE_DRIVE;
    event.level = false;
    CHECK(!em8051_sab_external_schedule(&first.cpu, &event));
    event.machine_cycle = 8u;
    CHECK(em8051_sab_external_schedule(&first.cpu, &event));
    event.machine_cycle = 7u;
    CHECK(!em8051_sab_external_schedule(&first.cpu, &event));
    em8051_sab_external_clear_schedule(&first.cpu);
    CHECK(em8051_sab_external_scheduled_count(&first.cpu) == 0u);

    event.machine_cycle = first.cpu.mMachineCycleCount + 10u;
    for (i = 0; i < EM8051_SAB_EXTERNAL_SCHEDULE_CAPACITY; i++)
        CHECK(em8051_sab_external_schedule(&first.cpu, &event));
    CHECK(!em8051_sab_external_schedule(&first.cpu, &event));
    reset(&first.cpu, false);
    CHECK(em8051_sab_external_scheduled_count(&first.cpu) == 0u);
    CHECK(first.cpu.mSABExternalSampledLevels == 0x7fu);

    setup_fixture(&first, EM8051_VARIANT_SAB80535);
    select_edge_mode(&first, 0u, false);
    event.machine_cycle = 0u;
    event.source = EM8051_SAB_EXTERNAL_INT0;
    event.action = EM8051_SAB_EXTERNAL_SCHEDULE_DRIVE;
    event.level = false;
    CHECK(em8051_sab_external_schedule(&first.cpu, &event));
    CHECK(em8051_sab_external_scheduled_count(&first.cpu) == 0u);
    CHECK(request_is_set(&first, 0u));

    setup_fixture(&first, EM8051_VARIANT_SAB80535);
    setup_fixture(&second, EM8051_VARIANT_SAB80535);
    memset(&first_trace, 0, sizeof(first_trace));
    em8051_set_sab_external_trace(&first.cpu, capture_trace, &first_trace);
    schedule_replay_sequence(&first);
    schedule_replay_sequence(&second);
    CHECK(em8051_run(&first.cpu, 6, &first_result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(em8051_run(&second.cpu, 6, &second_result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(first_trace.count != 0u);
    CHECK(first.cpu.mPC == second.cpu.mPC);
    CHECK(first.cpu.mMachineCycleCount == second.cpu.mMachineCycleCount);
    CHECK(first.cpu.mSABIrqPending == second.cpu.mSABIrqPending);
    CHECK(memcmp(first.cpu.mSFR, second.cpu.mSFR,
                 sizeof(first.cpu.mSFR)) == 0);
}

static void install_pin_sample_isr(struct fixture *aFixture, uint16_t aVector,
                                   uint8_t aPort, uint8_t aMask)
{
    aFixture->code[aVector] = 0xc0u;       /* PUSH ACC */
    aFixture->code[aVector + 1u] = 0xe0u;
    aFixture->code[aVector + 2u] = 0xe5u;  /* MOV A,direct */
    aFixture->code[aVector + 3u] = aPort;
    aFixture->code[aVector + 4u] = 0x54u;  /* ANL A,#mask */
    aFixture->code[aVector + 5u] = aMask;
    aFixture->code[aVector + 6u] = 0xf5u;  /* MOV 20,A */
    aFixture->code[aVector + 7u] = 0x20u;
    aFixture->code[aVector + 8u] = 0xd0u;  /* POP ACC */
    aFixture->code[aVector + 9u] = 0xe0u;
    aFixture->code[aVector + 10u] = 0x32u; /* RETI */
}

static void run_isr_body(struct fixture *aFixture)
{
    struct em8051_run_result result;
    CHECK(em8051_run(&aFixture->cpu, 6, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
}

static void test_generic_length_fixture(void)
{
    unsigned companion_high;
    for (companion_high = 0; companion_high < 2u; companion_high++)
    {
        struct fixture fixture;
        struct em8051_run_result result;
        struct em8051_sab_external_schedule_event event =
        {
            1u, EM8051_SAB_EXTERNAL_INT0,
            EM8051_SAB_EXTERNAL_SCHEDULE_DRIVE, false
        };
        setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
        select_edge_mode(&fixture, 0u, false);
        enable_source(&fixture, 0u);
        install_pin_sample_isr(&fixture, EM8051_SAB_VECTOR_INT0,
                               0xb0u, 0x20u);
        CHECK(em8051_sab_port_drive(&fixture.cpu, 0xb0u, 0x20u,
                                    companion_high ? 0x20u : 0u));
        CHECK(em8051_sab_external_schedule(&fixture.cpu, &event));
        CHECK(em8051_run_until_pc(&fixture.cpu, EM8051_SAB_VECTOR_INT0, 2,
                                  &result) == EM8051_STOP_TARGET_PC);
        run_isr_body(&fixture);
        CHECK(fixture.cpu.mLowerData[0x20] ==
              (companion_high ? 0x20u : 0u));
    }
}

static void test_generic_saw_fixture_live_i3fr(void)
{
    struct fixture fixture;
    struct em8051_run_result result;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    select_edge_mode(&fixture, 3u, false);
    enable_source(&fixture, 3u);
    install_pin_sample_isr(&fixture, EM8051_SAB_VECTOR_INT3,
                           0x90u, 0x02u);
    CHECK(em8051_sab_port_drive(&fixture.cpu, 0x90u, 0x02u, 0u));
    trigger_qualifying(&fixture, 3u);
    CHECK(em8051_run_until_pc(&fixture.cpu, EM8051_SAB_VECTOR_INT3, 1,
                              &result) == EM8051_STOP_TARGET_PC);
    run_isr_body(&fixture);
    CHECK(fixture.cpu.mLowerData[0x20] == 0u);

    select_edge_mode(&fixture, 3u, true);
    CHECK(em8051_sab_port_release(&fixture.cpu, 0x90u, 0x02u));
    CHECK(em8051_sab_external_release(&fixture.cpu,
                                      EM8051_SAB_EXTERNAL_INT3));
    fixture.code[fixture.cpu.mPC] = 0x00u; /* post-RETI inhibit instruction */
    CHECK(em8051_run(&fixture.cpu, 1, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(em8051_run_until_pc(&fixture.cpu, EM8051_SAB_VECTOR_INT3, 1,
                              &result) == EM8051_STOP_TARGET_PC);
    run_isr_body(&fixture);
    CHECK(fixture.cpu.mLowerData[0x20] == 0x02u);
}

static void test_reset_no_spurious_edge_and_classic_isolation(void)
{
    struct fixture fixture;
    struct trace_capture capture;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    memset(&capture, 0, sizeof(capture));
    em8051_set_sab_external_trace(&fixture.cpu, capture_trace, &capture);
    CHECK(em8051_sab_external_drive(&fixture.cpu,
                                    EM8051_SAB_EXTERNAL_INT2, false));
    CHECK(capture.count == 1u);
    reset(&fixture.cpu, false);
    CHECK(capture.count == 1u);
    CHECK(fixture.cpu.mSABIrqPending == 0u);
    CHECK(em8051_sab_external_drive(&fixture.cpu,
                                    EM8051_SAB_EXTERNAL_INT6, true));
    CHECK(capture.count == 1u);

    setup_fixture(&fixture, EM8051_VARIANT_8051);
    CHECK(!em8051_sab_external_drive(&fixture.cpu,
                                     EM8051_SAB_EXTERNAL_INT0, false));
    CHECK(!em8051_sab_external_release(&fixture.cpu,
                                       EM8051_SAB_EXTERNAL_INT0));
    CHECK(fixture.cpu.mSABIrqPending == 0u);
}

int main(void)
{
    test_literal_pin_flag_map_and_all_vectors();
    test_nonqualifying_and_int0_rearm();
    test_int0_int1_level_and_edge_modes();
    test_int2_int3_both_selections_and_live_change();
    test_fixed_rising_int4_int5_int6_independent();
    test_masking_enable_boundary_and_polling();
    test_preemption_and_wait_until_reti();
    test_schedule_contract_replay_reset_and_observer_neutrality();
    test_generic_length_fixture();
    test_generic_saw_fixture_live_i3fr();
    test_reset_no_spurious_edge_and_classic_isolation();

    if (gFailures != 0)
    {
        fprintf(stderr, "SLC-013 external-edge tests failed: %d failure(s)\n",
                gFailures);
        return EXIT_FAILURE;
    }
    puts("SLC-013 external-edge tests passed");
    return EXIT_SUCCESS;
}

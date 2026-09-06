/* Focused deterministic SLC-014 tests for the SAB80535 ADC. */

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

struct adc_capture
{
    struct em8051_sab_adc_trace_record records[640];
    size_t count;
};

struct reentrant_probe
{
    unsigned calls;
    bool saw_initial_value;
    bool saw_busy;
    uint64_t callback_cycle;
};

static struct reentrant_probe *gReentrantProbe;

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

static void capture_adc(const struct em8051_sab_adc_trace_record *aRecord,
                        void *aUser)
{
    struct adc_capture *capture = (struct adc_capture *)aUser;
    CHECK(capture->count < ARRAY_SIZE(capture->records));
    if (capture->count < ARRAY_SIZE(capture->records))
        capture->records[capture->count++] = *aRecord;
}

static void advance_cycles(struct em8051 *aCPU, unsigned aCount)
{
    uint64_t before = aCPU->mMachineCycleCount;
    unsigned i;
    for (i = 0; i < aCount; i++)
        (void)tick(aCPU);
    CHECK(aCPU->mMachineCycleCount == before + aCount);
}

static bool reference_pair_valid(uint8_t aDAPR)
{
    unsigned lower = aDAPR & 0x0fu;
    unsigned programmed_upper = aDAPR >> 4;
    unsigned upper = programmed_upper == 0u ? 16u : programmed_upper;
    return lower <= 12u &&
           (programmed_upper == 0u || programmed_upper >= 4u) &&
           upper >= lower + 4u;
}

/* Deliberately use 64-bit arithmetic in the test oracle, independent of the
 * product's bounded 32-bit implementation. */
static uint8_t reference_code(uint16_t aInput, uint8_t aDAPR)
{
    uint64_t lower = aDAPR & 0x0fu;
    uint64_t programmed_upper = aDAPR >> 4;
    uint64_t upper = programmed_upper == 0u ? 16u : programmed_upper;
    uint64_t position = 16u * (uint64_t)aInput;
    uint64_t bottom = 65535u * lower;
    uint64_t top = 65535u * upper;
    uint64_t code;

    if (position <= bottom)
        return 0u;
    if (position >= top)
        return 0xffu;
    code = ((position - bottom) << 8) / (top - bottom);
    return (uint8_t)(code > 255u ? 255u : code);
}

static void start_conversion(struct em8051 *aCPU, uint8_t aDAPR)
{
    em8051_sfr_write(aCPU, EM8051_SAB_SFR_DAPR, aDAPR);
}

static void complete_conversion(struct em8051 *aCPU)
{
    advance_cycles(aCPU, 15u);
}

/* Verification classes 1, 27: deterministic reset/API and classic isolation. */
static void test_reset_api_and_classic_isolation(void)
{
    struct fixture fixture;
    struct adc_capture capture;
    enum em8051_variant variant;
    unsigned channel;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    memset(&capture, 0, sizeof(capture));
    em8051_set_sab_adc_trace(&fixture.cpu, capture_adc, &capture);
    CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADCON)] == 0u);
    CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADDAT)] == 0u);
    CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_DAPR)] == 0u);
    CHECK(!fixture.cpu.mSABADCArmed);
    CHECK(!fixture.cpu.mSABADCActive);
    CHECK(!fixture.cpu.mSABADCBusy);
    for (channel = 0; channel < EM8051_SAB_ADC_CHANNEL_COUNT; channel++)
        CHECK(fixture.cpu.mSABADCInputs[channel] == 0u);

    CHECK(em8051_sab_adc_set_input(&fixture.cpu, 7u, 0xbeefu));
    CHECK(!em8051_sab_adc_set_input(NULL, 0u, 1u));
    CHECK(!em8051_sab_adc_set_input(&fixture.cpu, 8u, 1u));
    CHECK(fixture.cpu.mSABADCInputs[7] == 0xbeefu);
    reset(&fixture.cpu, false);
    CHECK(fixture.cpu.mSABADCInputs[7] == 0u);
    CHECK(capture.count == 0u);

    for (variant = EM8051_VARIANT_8051;
         variant <= EM8051_VARIANT_8052; variant++)
    {
        setup_fixture(&fixture, variant);
        CHECK(!em8051_sab_adc_set_input(&fixture.cpu, 0u, 0xffffu));
        em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_ADCON,
                         SAB_ADCONMASK_BSY | SAB_ADCONMASK_BD);
        em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_ADDAT, 0x5au);
        em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_DAPR, 0x00u);
        advance_cycles(&fixture.cpu, 20u);
        CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADCON)] ==
              (SAB_ADCONMASK_BSY | SAB_ADCONMASK_BD));
        CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADDAT)] == 0x5au);
        CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] &
               SAB_IRCONMASK_IADC) == 0u);
        CHECK(!fixture.cpu.mSABADCArmed);
        CHECK(!fixture.cpu.mSABADCActive);
    }
}

/* Verification classes 2..8: literal map, all channels, start and cycle 15. */
static void test_channels_start_timing_bsy_addat_iadc(void)
{
    static const uint16_t samples[EM8051_SAB_ADC_CHANNEL_COUNT] =
    {
        0u, 8192u, 16384u, 24576u,
        32768u, 40960u, 49152u, 65535u
    };
    static const uint8_t expected[EM8051_SAB_ADC_CHANNEL_COUNT] =
    {
        0u, 32u, 64u, 96u, 128u, 160u, 192u, 255u
    };
    unsigned selected;

    CHECK(EM8051_SAB_SFR_ADCON == 0xd8u);
    CHECK(EM8051_SAB_SFR_ADDAT == 0xd9u);
    CHECK(EM8051_SAB_SFR_DAPR == 0xdau);
    CHECK(SAB_ADCONMASK_MX == 0x07u);
    CHECK(SAB_ADCONMASK_ADM == 0x08u);
    CHECK(SAB_ADCONMASK_BSY == 0x10u);
    CHECK(SAB_ADCONMASK_CLK == 0x40u);
    CHECK(SAB_ADCONMASK_BD == 0x80u);

    for (selected = 0; selected < EM8051_SAB_ADC_CHANNEL_COUNT; selected++)
    {
        struct fixture fixture;
        struct adc_capture capture;
        unsigned channel;

        setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
        memset(&capture, 0, sizeof(capture));
        em8051_set_sab_adc_trace(&fixture.cpu, capture_adc, &capture);
        for (channel = 0; channel < EM8051_SAB_ADC_CHANNEL_COUNT; channel++)
            CHECK(em8051_sab_adc_set_input(&fixture.cpu, (uint8_t)channel,
                                           samples[channel]));
        em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_ADDAT, 0xa5u);
        em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_ADCON,
                         (uint8_t)(SAB_ADCONMASK_BD | SAB_ADCONMASK_CLK |
                                   SAB_ADCONMASK_RESERVED | selected));
        start_conversion(&fixture.cpu, 0x00u);
        CHECK(fixture.cpu.mSABADCArmed);
        CHECK(!fixture.cpu.mSABADCActive);
        CHECK(!fixture.cpu.mSABADCBusy);
        CHECK(capture.count == 0u);

        advance_cycles(&fixture.cpu, 1u);
        CHECK(!fixture.cpu.mSABADCArmed);
        CHECK(fixture.cpu.mSABADCActive);
        CHECK(fixture.cpu.mSABADCCycles == 1u);
        CHECK(fixture.cpu.mSABADCBusy);
        CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADCON)] &
               SAB_ADCONMASK_BSY) != 0u);
        CHECK(capture.count == 1u);
        CHECK(capture.records[0].event == EM8051_SAB_ADC_TRACE_START);
        CHECK(capture.records[0].machine_cycle == 1u);
        CHECK(capture.records[0].channel == selected);
        CHECK(capture.records[0].normalized_input == samples[selected]);
        CHECK(capture.records[0].busy);
        CHECK(!capture.records[0].iadc);

        advance_cycles(&fixture.cpu, 13u);
        CHECK(fixture.cpu.mSABADCCycles == 14u);
        CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADDAT)] == 0xa5u);
        CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] &
               SAB_IRCONMASK_IADC) == 0u);
        CHECK(capture.count == 1u);

        advance_cycles(&fixture.cpu, 1u);
        CHECK(!fixture.cpu.mSABADCActive);
        CHECK(!fixture.cpu.mSABADCBusy);
        CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADDAT)] ==
              expected[selected]);
        CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] &
               SAB_IRCONMASK_IADC) != 0u);
        CHECK(capture.count == 2u);
        CHECK(capture.records[1].event == EM8051_SAB_ADC_TRACE_COMPLETE);
        CHECK(capture.records[1].machine_cycle == 15u);
        CHECK(capture.records[1].addat == expected[selected]);
        CHECK(!capture.records[1].busy);
        CHECK(capture.records[1].iadc);
    }
}

/* Verification classes 3, 17, 18: every DAPR value starts, valid-range and
 * zero-endpoint rules, clipping, floor arithmetic and invalid diagnostics. */
static void test_reference_programming_and_arithmetic(void)
{
    struct fixture fixture;
    struct adc_capture capture;
    unsigned dapr;
    static const struct
    {
        uint8_t dapr;
        uint16_t input;
        uint8_t expected;
    } literals[] =
    {
        { 0x00u, 0u, 0u },
        { 0x00u, 0x1234u, 18u },
        { 0x00u, 32768u, 128u },
        { 0x00u, 65534u, 255u },
        { 0x00u, 65535u, 255u },
        { 0xc4u, 16383u, 0u },
        { 0xc4u, 20000u, 28u },
        { 0xc4u, 32768u, 128u },
        { 0xc4u, 49152u, 255u },
        { 0x40u, 8192u, 128u },
        { 0x0cu, 49151u, 0u },
        { 0x0cu, 57343u, 127u },
        { 0x0cu, 57344u, 128u },
        { 0x0cu, 65535u, 255u }
    };
    size_t i;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    memset(&capture, 0, sizeof(capture));
    em8051_set_sab_adc_trace(&fixture.cpu, capture_adc, &capture);
    CHECK(em8051_sab_adc_set_input(&fixture.cpu, 0u, 32768u));
    for (dapr = 0; dapr <= 0xffu; dapr++)
    {
        bool valid = reference_pair_valid((uint8_t)dapr);
        size_t start_record = capture.count;
        em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_IRCON, 0u);
        em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_ADDAT, 0x5au);
        start_conversion(&fixture.cpu, (uint8_t)dapr);
        advance_cycles(&fixture.cpu, 1u);
        CHECK(capture.count == start_record + 1u);
        CHECK(capture.records[start_record].dapr == (uint8_t)dapr);
        CHECK(capture.records[start_record].references_valid == valid);
        advance_cycles(&fixture.cpu, 14u);
        CHECK(capture.count == start_record + 2u);
        CHECK(capture.records[start_record + 1u].references_valid == valid);
        CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] &
               SAB_IRCONMASK_IADC) != 0u);
        if (valid)
        {
            CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADDAT)] ==
                  reference_code(32768u, (uint8_t)dapr));
        }
        else
        {
            CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADDAT)] ==
                  0x5au);
        }
    }

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    for (i = 0; i < ARRAY_SIZE(literals); i++)
    {
        CHECK(reference_pair_valid(literals[i].dapr));
        CHECK(reference_code(literals[i].input, literals[i].dapr) ==
              literals[i].expected);
        CHECK(em8051_sab_adc_set_input(&fixture.cpu, 0u,
                                       literals[i].input));
        start_conversion(&fixture.cpu, literals[i].dapr);
        complete_conversion(&fixture.cpu);
        CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADDAT)] ==
              literals[i].expected);
        em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_IRCON, 0u);
    }
}

/* Verification classes 6, 7, 15, 21: ADCON hardware ownership and latches. */
static void test_adcon_ownership_latches_and_adm_boundary(void)
{
    struct fixture fixture;
    struct adc_capture capture;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    memset(&capture, 0, sizeof(capture));
    em8051_set_sab_adc_trace(&fixture.cpu, capture_adc, &capture);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_ADCON,
                     SAB_ADCONMASK_BSY | SAB_ADCONMASK_BD |
                     SAB_ADCONMASK_CLK | SAB_ADCONMASK_RESERVED);
    CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADCON)] == 0xe0u);
    fixture.code[0] = 0xd2u; /* SETB ADCON.BSY: software write is ignored */
    fixture.code[1] = 0xdcu;
    advance_cycles(&fixture.cpu, 1u);
    CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADCON)] == 0xe0u);
    CHECK(em8051_sab_adc_set_input(&fixture.cpu, 0u, 0x1234u));
    CHECK(em8051_sab_adc_set_input(&fixture.cpu, 7u, 32768u));
    start_conversion(&fixture.cpu, 0x00u);
    advance_cycles(&fixture.cpu, 1u);
    CHECK(fixture.cpu.mSABADCLatchedChannel == 0u);
    CHECK(fixture.cpu.mSABADCLatchedInput == 0x1234u);
    fixture.code[2] = 0xc2u; /* CLR ADCON.BSY: active hardware wins */
    fixture.code[3] = 0xdcu;
    advance_cycles(&fixture.cpu, 1u);
    CHECK(fixture.cpu.mSABADCCycles == 2u);
    CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADCON)] &
           SAB_ADCONMASK_BSY) != 0u);

    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_ADCON, 0xffu);
    CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADCON)] == 0xffu);
    CHECK(em8051_sab_adc_set_input(&fixture.cpu, 0u, 0xffffu));
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_ADDAT, 0x6bu);
    advance_cycles(&fixture.cpu, 13u);
    CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADDAT)] == 18u);
    CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADCON)] == 0xefu);

    /* A pre-existing software-owned request survives the next explicit
     * ADM=1 conversion; no unsupported continuous auto-chain is created. */
    CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] &
           SAB_IRCONMASK_IADC) != 0u);
    start_conversion(&fixture.cpu, 0x00u);
    CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] &
           SAB_IRCONMASK_IADC) != 0u);
    complete_conversion(&fixture.cpu);
    CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADDAT)] == 128u);
    CHECK(capture.records[capture.count - 1u].continuous_requested);
    CHECK(!fixture.cpu.mSABADCActive);
    CHECK(!fixture.cpu.mSABADCArmed);
    advance_cycles(&fixture.cpu, 30u);
    CHECK(!fixture.cpu.mSABADCActive);
    CHECK(!fixture.cpu.mSABADCArmed);
    CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADDAT)] == 128u);

    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_ADCON,
                     SAB_ADCONMASK_BSY);
    CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADCON)] == 0u);
}

static void reentrant_dapr_callback(struct em8051 *aCPU, uint8_t aRegister)
{
    struct reentrant_probe *probe = gReentrantProbe;
    if (!probe || aRegister != EM8051_SAB_SFR_DAPR)
        return;
    probe->calls++;
    if (probe->calls != 1u)
        return;

    probe->saw_initial_value =
        aCPU->mSFR[SFR_INDEX(EM8051_SAB_SFR_DAPR)] == 0x40u;
    probe->saw_busy =
        (aCPU->mSFR[SFR_INDEX(EM8051_SAB_SFR_ADCON)] &
         SAB_ADCONMASK_BSY) != 0u;
    (void)tick(aCPU);
    probe->callback_cycle = aCPU->mMachineCycleCount;
    em8051_sfr_write(aCPU, EM8051_SAB_SFR_ADCON,
                     SAB_ADCONMASK_BD | SAB_ADCONMASK_CLK |
                     SAB_ADCONMASK_RESERVED | 3u);
    CHECK(em8051_sab_adc_set_input(aCPU, 3u, 32768u));
    em8051_sfr_write(aCPU, EM8051_SAB_SFR_DAPR, 0xc4u);
}

/* Verification classes 14..16, 22: superseding restart and reentrant writes. */
static void test_restart_iadc_and_reentrant_gateway(void)
{
    struct fixture fixture;
    struct adc_capture capture;
    struct reentrant_probe probe;
    size_t records_before;
    uint64_t restart_cycle;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    memset(&capture, 0, sizeof(capture));
    memset(&probe, 0, sizeof(probe));
    em8051_set_sab_adc_trace(&fixture.cpu, capture_adc, &capture);
    CHECK(em8051_sab_adc_set_input(&fixture.cpu, 0u, 1000u));
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_ADCON, 0u);
    start_conversion(&fixture.cpu, 0x00u);
    advance_cycles(&fixture.cpu, 5u);
    CHECK(fixture.cpu.mSABADCCycles == 5u);
    CHECK(capture.count == 1u);

    /* Model a software-owned request from an already completed conversion.
     * The restart must not clear it. */
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_IRCON,
                     SAB_IRCONMASK_IADC);
    gReentrantProbe = &probe;
    fixture.cpu.sfrwrite[SFR_INDEX(EM8051_SAB_SFR_DAPR)] =
        reentrant_dapr_callback;
    records_before = capture.count;
    start_conversion(&fixture.cpu, 0x40u);
    gReentrantProbe = NULL;

    CHECK(probe.calls == 2u);
    CHECK(probe.saw_initial_value);
    CHECK(probe.saw_busy);
    CHECK(probe.callback_cycle == 6u);
    CHECK(fixture.cpu.mSABADCArmed);
    CHECK(!fixture.cpu.mSABADCActive);
    CHECK(fixture.cpu.mSABADCBusy);
    CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_DAPR)] == 0xc4u);
    CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADCON)] & 0xefu) ==
          0xe3u);
    CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] &
           SAB_IRCONMASK_IADC) != 0u);
    CHECK(capture.count == records_before);

    advance_cycles(&fixture.cpu, 1u);
    restart_cycle = fixture.cpu.mMachineCycleCount;
    CHECK(capture.count == records_before + 1u);
    CHECK(capture.records[records_before].event ==
          EM8051_SAB_ADC_TRACE_RESTART);
    CHECK(capture.records[records_before].machine_cycle == restart_cycle);
    CHECK(capture.records[records_before].channel == 3u);
    CHECK(capture.records[records_before].dapr == 0xc4u);
    CHECK(capture.records[records_before].normalized_input == 32768u);
    CHECK(capture.records[records_before].iadc);
    advance_cycles(&fixture.cpu, 13u);
    CHECK(capture.count == records_before + 1u);
    advance_cycles(&fixture.cpu, 1u);
    CHECK(capture.count == records_before + 2u);
    CHECK(capture.records[records_before + 1u].machine_cycle ==
          restart_cycle + 14u);
    CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADDAT)] == 128u);
    CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] &
           SAB_IRCONMASK_IADC) != 0u);
    advance_cycles(&fixture.cpu, 20u);
    CHECK(capture.count == records_before + 2u);
}

static void enable_adc(struct em8051 *aCPU, bool aGlobal)
{
    aCPU->mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)] =
        aGlobal ? IEMASK_EA : 0u;
    aCPU->mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN1)] = SAB_IEN1MASK_EADC;
}

/* Verification classes 9..13, 20: canonical pending/gates/vector/preemption. */
static void test_interrupt_controller_integration(void)
{
    struct fixture fixture;
    uint16_t adc_bit = IRQ_BIT(EM8051_SAB_IRQ_ADC);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)] = IEMASK_EA;
    CHECK(em8051_sab_adc_set_input(&fixture.cpu, 0u, 32768u));
    start_conversion(&fixture.cpu, 0u);
    complete_conversion(&fixture.cpu);
    CHECK((fixture.cpu.mSABIrqPending & adc_bit) != 0u);
    CHECK((fixture.cpu.mSABIrqEnabled & adc_bit) == 0u);
    CHECK(fixture.cpu.mSABIrqDepth == 0u);
    advance_cycles(&fixture.cpu, 1u);
    CHECK(fixture.cpu.mSABIrqDepth == 0u);

    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_IEN1,
                     SAB_IEN1MASK_EADC);
    CHECK(tick(&fixture.cpu)); /* required post-enable inhibit instruction */
    CHECK(fixture.cpu.mSABIrqDepth == 0u);
    CHECK(!tick(&fixture.cpu));
    CHECK(fixture.cpu.mPC == EM8051_SAB_VECTOR_ADC);
    CHECK(fixture.cpu.mSABIrqDepth == 1u);
    CHECK((fixture.cpu.mSABIrqInService & adc_bit) != 0u);
    CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] &
           SAB_IRCONMASK_IADC) != 0u);
    fixture.code[EM8051_SAB_VECTOR_ADC] = 0x32u; /* RETI */
    CHECK(!tick(&fixture.cpu));                 /* entry cycle 2 */
    CHECK(tick(&fixture.cpu));                  /* execute RETI */
    CHECK(fixture.cpu.mSABIrqDepth == 0u);
    CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] &
           SAB_IRCONMASK_IADC) != 0u);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_IRCON, 0u);
    CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] &
           SAB_IRCONMASK_IADC) == 0u);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    enable_adc(&fixture.cpu, false);
    start_conversion(&fixture.cpu, 0u);
    complete_conversion(&fixture.cpu);
    CHECK((fixture.cpu.mSABIrqPending & adc_bit) != 0u);
    CHECK(fixture.cpu.mSABIrqDepth == 0u);
    advance_cycles(&fixture.cpu, 1u);
    CHECK(fixture.cpu.mSABIrqDepth == 0u);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_SFR_IEN0, IEMASK_EA);
    CHECK(tick(&fixture.cpu));
    CHECK(!tick(&fixture.cpu));
    CHECK(fixture.cpu.mPC == EM8051_SAB_VECTOR_ADC);

    /* ADC at level 3 preempts a level-0 Timer0 service. Conversion cycles 14
     * and 15 occur during the two Timer0 entry cycles. */
    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)] =
        IEMASK_EA | IEMASK_ET0;
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN1)] = SAB_IEN1MASK_EADC;
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IP0)] = 0x01u;
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IP1)] = 0x01u;
    start_conversion(&fixture.cpu, 0u);
    advance_cycles(&fixture.cpu, 13u);
    CHECK(em8051_sab_irq_set_pending(&fixture.cpu,
                                     EM8051_SAB_IRQ_TIMER0, true));
    CHECK(!tick(&fixture.cpu));
    CHECK(fixture.cpu.mPC == EM8051_SAB_VECTOR_TIMER0);
    CHECK(fixture.cpu.mSABIrqDepth == 1u);
    CHECK(!tick(&fixture.cpu));
    CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] &
           SAB_IRCONMASK_IADC) != 0u);
    CHECK(fixture.cpu.mSABIrqDepth == 1u);
    CHECK(!tick(&fixture.cpu));
    CHECK(fixture.cpu.mPC == EM8051_SAB_VECTOR_ADC);
    CHECK(fixture.cpu.mSABIrqDepth == 2u);
    CHECK(fixture.cpu.mSABIrqSourceStack[1] == EM8051_SAB_IRQ_ADC);

    /* An active conversion also advances through instructions executed in an
     * ISR, not merely through its two entry cycles. */
    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)] =
        IEMASK_EA | IEMASK_ET0;
    fixture.code[EM8051_SAB_VECTOR_TIMER0] = 0xa4u; /* four-cycle MUL AB */
    start_conversion(&fixture.cpu, 0u);
    advance_cycles(&fixture.cpu, 5u);
    CHECK(em8051_sab_irq_set_pending(&fixture.cpu,
                                     EM8051_SAB_IRQ_TIMER0, true));
    CHECK(!tick(&fixture.cpu)); /* entry cycle 1, ADC cycle 6 */
    CHECK(!tick(&fixture.cpu)); /* entry cycle 2, ADC cycle 7 */
    CHECK(em8051_step_instruction(&fixture.cpu, NULL) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(fixture.cpu.mSABADCCycles == 11u);
    CHECK(fixture.cpu.mSABIrqDepth == 1u);
    advance_cycles(&fixture.cpu, 4u);
    CHECK(!fixture.cpu.mSABADCActive);
    CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] &
           SAB_IRCONMASK_IADC) != 0u);
}

/* Verification classes 4, 5, 19, 20: ordinary/multi-cycle/IDLE/run chunks. */
static void test_virtual_cycle_progression_boundaries(void)
{
    struct fixture fixture;
    struct em8051_run_result result;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    fixture.code[0] = 0xa4u; /* MUL AB: four machine cycles */
    fixture.code[1] = 0xa4u;
    fixture.code[2] = 0xa4u;
    CHECK(em8051_sab_adc_set_input(&fixture.cpu, 0u, 32768u));
    start_conversion(&fixture.cpu, 0u);
    CHECK(em8051_step_instruction(&fixture.cpu, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.machine_cycles == 4u);
    CHECK(fixture.cpu.mSABADCCycles == 4u);
    em8051_set_breakpoint(&fixture.cpu, fixture.cpu.mPC, true);
    CHECK(em8051_run(&fixture.cpu, 10u, &result) == EM8051_STOP_BREAKPOINT);
    CHECK(result.machine_cycles == 0u);
    CHECK(fixture.cpu.mSABADCCycles == 4u);
    em8051_set_breakpoint(&fixture.cpu, 0u, false);
    CHECK(em8051_step_instruction(&fixture.cpu, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.machine_cycles == 4u);
    CHECK(em8051_step_instruction(&fixture.cpu, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.machine_cycles == 4u);
    CHECK(fixture.cpu.mSABADCCycles == 12u);
    CHECK(em8051_run(&fixture.cpu, 3u, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.machine_cycles == 3u);
    CHECK(fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADDAT)] == 128u);
    CHECK(!fixture.cpu.mSABADCActive);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    start_conversion(&fixture.cpu, 0u);
    fixture.cpu.mSFR[REG_PCON] |= 0x01u; /* accepted IDLE peripheral cycles */
    advance_cycles(&fixture.cpu, 15u);
    CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] &
           SAB_IRCONMASK_IADC) != 0u);
    CHECK(!fixture.cpu.mSABADCBusy);
}

static void run_replay_sequence(struct fixture *aFixture,
                                struct adc_capture *aCapture)
{
    unsigned conversion;
    if (aCapture)
        em8051_set_sab_adc_trace(&aFixture->cpu, capture_adc, aCapture);
    for (conversion = 0; conversion < 256u; conversion++)
    {
        uint8_t channel = (uint8_t)(conversion & 7u);
        uint8_t dapr = (conversion & 1u) ? 0xc4u : 0x00u;
        uint16_t input = (uint16_t)(conversion * 257u);
        CHECK(em8051_sab_adc_set_input(&aFixture->cpu, channel, input));
        em8051_sfr_write(&aFixture->cpu, EM8051_SAB_SFR_ADCON,
                         (uint8_t)(SAB_ADCONMASK_BD | channel));
        em8051_sfr_write(&aFixture->cpu, EM8051_SAB_SFR_IRCON, 0u);
        start_conversion(&aFixture->cpu, dapr);
        complete_conversion(&aFixture->cpu);
    }
}

/* Verification classes 19, 23: long replay and immutable observer neutrality. */
static void test_long_replay_and_observer_neutrality(void)
{
    struct fixture first;
    struct fixture second;
    struct fixture unobserved;
    struct adc_capture first_capture;
    struct adc_capture second_capture;

    setup_fixture(&first, EM8051_VARIANT_SAB80535);
    setup_fixture(&second, EM8051_VARIANT_SAB80535);
    setup_fixture(&unobserved, EM8051_VARIANT_SAB80535);
    memset(&first_capture, 0, sizeof(first_capture));
    memset(&second_capture, 0, sizeof(second_capture));
    run_replay_sequence(&first, &first_capture);
    run_replay_sequence(&second, &second_capture);
    run_replay_sequence(&unobserved, NULL);

    CHECK(first_capture.count == 512u);
    CHECK(second_capture.count == first_capture.count);
    CHECK(memcmp(first_capture.records, second_capture.records,
                 first_capture.count * sizeof(first_capture.records[0])) == 0);
    CHECK(first.cpu.mMachineCycleCount == 256u * 15u);
    CHECK(first.cpu.mMachineCycleCount == second.cpu.mMachineCycleCount);
    CHECK(first.cpu.mMachineCycleCount == unobserved.cpu.mMachineCycleCount);
    CHECK(first.cpu.mInstructionCount == second.cpu.mInstructionCount);
    CHECK(first.cpu.mInstructionCount == unobserved.cpu.mInstructionCount);
    CHECK(first.cpu.mPC == second.cpu.mPC);
    CHECK(first.cpu.mPC == unobserved.cpu.mPC);
    CHECK(memcmp(first.cpu.mSFR, second.cpu.mSFR,
                 sizeof(first.cpu.mSFR)) == 0);
    CHECK(memcmp(first.cpu.mSFR, unobserved.cpu.mSFR,
                 sizeof(first.cpu.mSFR)) == 0);
    CHECK(memcmp(first.cpu.mSABADCInputs, second.cpu.mSABADCInputs,
                 sizeof(first.cpu.mSABADCInputs)) == 0);
    CHECK(memcmp(first.cpu.mSABADCInputs, unobserved.cpu.mSABADCInputs,
                 sizeof(first.cpu.mSABADCInputs)) == 0);
}

/* Verification classes 11..13, 20: generic ROM-style real-controller path. */
static void test_generic_rom_style_fixture(void)
{
    struct fixture fixture;
    struct adc_capture capture;
    struct em8051_run_result result;
    static const uint8_t main_program[] =
    {
        0x75u, 0xd8u, 0x00u, /* MOV ADCON,#00 */
        0x75u, 0xdau, 0x00u, /* MOV DAPR,#00: architectural start */
        0x80u, 0xfeu         /* SJMP $ */
    };
    static const uint8_t adc_isr[] =
    {
        0xc0u, 0xe0u,       /* PUSH ACC */
        0x53u, 0xc0u, 0xfeu,/* ANL IRCON,#~IADC */
        0xe5u, 0xd9u,       /* MOV A,ADDAT */
        0xf5u, 0x20u,       /* MOV 20,A */
        0xd0u, 0xe0u,       /* POP ACC */
        0x32u               /* RETI */
    };

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    memset(&capture, 0, sizeof(capture));
    memcpy(fixture.code, main_program, sizeof(main_program));
    memcpy(&fixture.code[EM8051_SAB_VECTOR_ADC], adc_isr, sizeof(adc_isr));
    em8051_set_sab_adc_trace(&fixture.cpu, capture_adc, &capture);
    CHECK(em8051_sab_adc_set_input(&fixture.cpu, 0u, 0x4000u));
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)] = IEMASK_EA;
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN1)] = SAB_IEN1MASK_EADC;
    CHECK(em8051_run_until_pc(&fixture.cpu, EM8051_SAB_VECTOR_ADC, 64u,
                              &result) == EM8051_STOP_TARGET_PC);
    CHECK(capture.count == 2u);
    CHECK(capture.records[0].event == EM8051_SAB_ADC_TRACE_START);
    CHECK(capture.records[1].event == EM8051_SAB_ADC_TRACE_COMPLETE);
    CHECK(capture.records[1].machine_cycle ==
          capture.records[0].machine_cycle + 14u);
    CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] &
           SAB_IRCONMASK_IADC) != 0u);
    CHECK(fixture.cpu.mPC == EM8051_SAB_VECTOR_ADC);
    CHECK(em8051_run(&fixture.cpu, 6u, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(fixture.cpu.mLowerData[0x20] == 64u);
    CHECK((fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IRCON)] &
           SAB_IRCONMASK_IADC) == 0u);
    CHECK(fixture.cpu.mSABIrqDepth == 0u);
    CHECK(!fixture.cpu.mExceptionRaised);
}

int main(void)
{
    test_reset_api_and_classic_isolation();
    test_channels_start_timing_bsy_addat_iadc();
    test_reference_programming_and_arithmetic();
    test_adcon_ownership_latches_and_adm_boundary();
    test_restart_iadc_and_reentrant_gateway();
    test_interrupt_controller_integration();
    test_virtual_cycle_progression_boundaries();
    test_long_replay_and_observer_neutrality();
    test_generic_rom_style_fixture();

    if (gFailures != 0)
    {
        fprintf(stderr, "SLC-014 ADC tests failed: %d failure(s)\n",
                gFailures);
        return EXIT_FAILURE;
    }
    puts("SLC-014 ADC tests passed");
    return EXIT_SUCCESS;
}

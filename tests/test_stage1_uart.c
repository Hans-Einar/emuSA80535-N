/* Focused deterministic SLC-007 SAB80535 mode-3 UART tests. */

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

struct uart_capture
{
    struct em8051_sab_uart_trace_record records[128];
    size_t count;
};

struct timer_capture
{
    uint64_t count;
};

struct trace_capture
{
    struct em8051_trace_record records[8];
    size_t count;
};

static uint8_t gSbufReadOverride;
static uint8_t gSbufWriteObserved;
static unsigned gSbufReadCount;
static unsigned gSbufWriteCount;
static bool gSbufReceiveProgressActive;

static uint8_t sbuf_read_override(struct em8051 *aCPU, uint8_t aRegister)
{
    CHECK(aRegister == 0x99u);
    CHECK(aCPU->mSFR[REG_SBUF] == aCPU->mSABUartRxData);
    gSbufReadCount++;
    return gSbufReadOverride;
}

static void sbuf_write_observer(struct em8051 *aCPU, uint8_t aRegister)
{
    CHECK(aRegister == 0x99u);
    gSbufWriteObserved = aCPU->mSFR[REG_SBUF];
    gSbufWriteCount++;
}

static void sbuf_write_mutate_rx(struct em8051 *aCPU, uint8_t aRegister)
{
    sbuf_write_observer(aCPU, aRegister);
    aCPU->mSABUartRxData = 0x22u;
    aCPU->mSFR[REG_SBUF] = 0x22u;
}

static void capture_trace(const struct em8051_trace_record *aRecord,
                          void *aUser)
{
    struct trace_capture *capture = (struct trace_capture *)aUser;
    if (capture->count < ARRAY_SIZE(capture->records))
        capture->records[capture->count++] = *aRecord;
}

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

static void capture_uart(
    const struct em8051_sab_uart_trace_record *aRecord, void *aUser)
{
    struct uart_capture *capture = (struct uart_capture *)aUser;
    if (capture->count < ARRAY_SIZE(capture->records))
        capture->records[capture->count++] = *aRecord;
}

static em8051sabuarttrace const record_only_uart_compile_check = capture_uart;

static void capture_timer(
    const struct em8051_timer_overflow_record *aRecord, void *aUser)
{
    struct timer_capture *capture = (struct timer_capture *)aUser;
    if (aRecord->timer == EM8051_TIMER1)
        capture->count++;
}

static void run_cycles(struct em8051 *aCPU, uint64_t aCycles)
{
    uint64_t cycle;
    for (cycle = 0; cycle < aCycles; cycle++)
        (void)tick(aCPU);
}

static void sbuf_write_progress_rx(struct em8051 *aCPU, uint8_t aRegister)
{
    sbuf_write_observer(aCPU, aRegister);
    if (gSbufReceiveProgressActive)
        return;

    gSbufReceiveProgressActive = true;
    run_cycles(aCPU, 480u);
    gSbufReceiveProgressActive = false;
}

static void configure_uart(struct em8051 *aCPU, bool aSmod, bool aRen)
{
    aCPU->mSFR[REG_TMOD] = TMODMASK_M1_1;
    aCPU->mSFR[REG_TH1] = 0xfdu;
    aCPU->mSFR[REG_TL1] = 0xfdu;
    aCPU->mSFR[REG_TCON] = TCONMASK_TR1;
    aCPU->mSFR[REG_SCON] = SCONMASK_SM0 | SCONMASK_SM1 |
        (aRen ? SCONMASK_REN : 0u);
    aCPU->mSFR[REG_PCON] = aSmod ? PCONMASK_SMOD : 0u;
}

static const struct em8051_sab_uart_trace_record *find_event(
    const struct uart_capture *aCapture,
    enum em8051_sab_uart_trace_event aEvent, size_t aOccurrence)
{
    size_t i;
    for (i = 0; i < aCapture->count; i++)
    {
        if (aCapture->records[i].event == aEvent)
        {
            if (aOccurrence == 0)
                return &aCapture->records[i];
            aOccurrence--;
        }
    }
    return NULL;
}

static void test_sbuf_callback_and_trace_contract(void)
{
    struct fixture fixture;
    struct trace_capture trace;

    memset(&trace, 0, sizeof(trace));
    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_uart(&fixture.cpu, true, true);
    fixture.cpu.mSABUartRxData = 0x11u;
    fixture.cpu.mSFR[REG_SBUF] = 0x11u;
    fixture.cpu.sfrread[REG_SBUF] = sbuf_read_override;
    fixture.cpu.sfrwrite[REG_SBUF] = sbuf_write_observer;
    gSbufReadOverride = 0xe7u;
    gSbufWriteObserved = 0;
    gSbufReadCount = 0;
    gSbufWriteCount = 0;
    em8051_set_trace(&fixture.cpu, capture_trace, &trace);

    em8051_sfr_write(&fixture.cpu, 0x99u, 0xa5u);
    CHECK(gSbufWriteCount == 1u);
    CHECK(gSbufWriteObserved == 0xa5u);
    CHECK(fixture.cpu.mSABUartRxData == 0x11u);
    CHECK(fixture.cpu.mSFR[REG_SBUF] == 0x11u);
    CHECK(fixture.cpu.mSABUartTxPendingData == 0xa5u);
    CHECK(fixture.cpu.mSABUartTxPending);
    CHECK(trace.count == 1u);
    if (trace.count == 1u)
    {
        CHECK(trace.records[0].type == EM8051_TRACE_SFR_WRITE);
        CHECK(trace.records[0].address == 0x99u);
        CHECK(trace.records[0].value == 0xa5u);
    }

    CHECK(em8051_sfr_read(&fixture.cpu, 0x99u) == 0xe7u);
    CHECK(gSbufReadCount == 1u);
    CHECK(fixture.cpu.mSABUartRxData == 0x11u);
    CHECK(fixture.cpu.mSFR[REG_SBUF] == 0x11u);
    fixture.cpu.sfrread[REG_SBUF] = NULL;
    CHECK(em8051_sfr_read(&fixture.cpu, 0x99u) == 0x11u);
}

static void test_sbuf_callback_rx_coherence(void)
{
    struct fixture fixture;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_uart(&fixture.cpu, true, true);
    fixture.cpu.mSABUartRxData = 0x11u;
    fixture.cpu.mSFR[REG_SBUF] = 0x11u;
    fixture.cpu.sfrwrite[REG_SBUF] = sbuf_write_mutate_rx;
    gSbufWriteObserved = 0;
    gSbufWriteCount = 0;

    em8051_sfr_write(&fixture.cpu, 0x99u, 0xa5u);
    CHECK(gSbufWriteCount == 1u);
    CHECK(gSbufWriteObserved == 0xa5u);
    CHECK(fixture.cpu.mSABUartRxData == 0x22u);
    CHECK(fixture.cpu.mSFR[REG_SBUF] == 0x22u);
    CHECK(em8051_sfr_read(&fixture.cpu, 0x99u) == 0x22u);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_uart(&fixture.cpu, true, true);
    fixture.code[0] = 0x80u; /* Keep reentrant progress away from SBUF writes. */
    fixture.code[1] = 0xfeu;
    fixture.cpu.mSABUartRxData = 0x33u;
    fixture.cpu.mSFR[REG_SBUF] = 0x33u;
    CHECK(em8051_sab_uart_inject_rx_frame(&fixture.cpu, 0x6du, true));
    fixture.cpu.sfrwrite[REG_SBUF] = sbuf_write_progress_rx;
    gSbufWriteObserved = 0;
    gSbufWriteCount = 0;
    gSbufReceiveProgressActive = false;

    em8051_sfr_write(&fixture.cpu, 0x99u, 0xb6u);
    CHECK(!gSbufReceiveProgressActive);
    CHECK(gSbufWriteCount == 1u);
    CHECK(gSbufWriteObserved == 0xb6u);
    CHECK(fixture.cpu.mSABUartRxBitIndex == 10u);
    CHECK(fixture.cpu.mSABUartRxData == 0x6du);
    CHECK(fixture.cpu.mSFR[REG_SBUF] == 0x6du);
    CHECK(em8051_sfr_read(&fixture.cpu, 0x99u) == 0x6du);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_RI) != 0);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_RB8) != 0);
}

static void test_unsupported_writes_preserve_mode3_state(void)
{
    struct fixture fixture;
    struct uart_capture uart;
    struct trace_capture trace;
    const struct em8051_sab_uart_trace_record *start;

    memset(&uart, 0, sizeof(uart));
    memset(&trace, 0, sizeof(trace));
    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_uart(&fixture.cpu, true, true);
    fixture.cpu.mSFR[REG_SCON] |= SCONMASK_TB8;
    em8051_sfr_write(&fixture.cpu, 0x99u, 0x51u);
    CHECK(fixture.cpu.mSABUartTxPending);
    CHECK(fixture.cpu.mSABUartTxPendingData == 0x51u);
    CHECK(fixture.cpu.mSABUartTxPendingNinth);

    fixture.cpu.sfrwrite[REG_SBUF] = sbuf_write_observer;
    em8051_set_trace(&fixture.cpu, capture_trace, &trace);
    gSbufWriteCount = 0;
    fixture.cpu.mSFR[REG_SCON] = SCONMASK_SM1 | SCONMASK_REN;
    em8051_sfr_write(&fixture.cpu, 0x99u, 0xa2u);
    CHECK(gSbufWriteCount == 1u);
    CHECK(gSbufWriteObserved == 0xa2u);
    CHECK(fixture.cpu.mSABUartTxPendingData == 0x51u);
    CHECK(fixture.cpu.mSABUartTxPendingNinth);

    fixture.cpu.mSFR[REG_SCON] = SCONMASK_SM0 | SCONMASK_SM1 |
        SCONMASK_REN;
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADCON)] |= SAB_ADCONMASK_BD;
    em8051_sfr_write(&fixture.cpu, 0x99u, 0xb3u);
    CHECK(gSbufWriteCount == 2u);
    CHECK(gSbufWriteObserved == 0xb3u);
    CHECK(fixture.cpu.mSABUartTxPendingData == 0x51u);
    CHECK(fixture.cpu.mSABUartTxPendingNinth);
    CHECK(trace.count == 2u);
    if (trace.count == 2u)
    {
        CHECK(trace.records[0].value == 0xa2u);
        CHECK(trace.records[1].value == 0xb3u);
    }

    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADCON)] &=
        (uint8_t)~SAB_ADCONMASK_BD;
    em8051_set_sab_uart_trace(&fixture.cpu, capture_uart, &uart);
    run_cycles(&fixture.cpu, 48u);
    start = find_event(&uart, EM8051_SAB_UART_TRACE_TX_START, 0);
    CHECK(start != NULL);
    if (start)
    {
        CHECK(start->data == 0x51u);
        CHECK(start->ninth_bit);
    }
    CHECK(fixture.cpu.mSABUartTxActive);
    CHECK(fixture.cpu.mSABUartTxData == 0x51u);
    CHECK(fixture.cpu.mSABUartTxNinth);
    CHECK(!fixture.cpu.mSABUartTxPending);

    fixture.cpu.mSFR[REG_SCON] = SCONMASK_SM1 | SCONMASK_REN;
    em8051_sfr_write(&fixture.cpu, 0x99u, 0xc4u);
    CHECK(fixture.cpu.mSABUartTxActive);
    CHECK(fixture.cpu.mSABUartTxData == 0x51u);
    CHECK(fixture.cpu.mSABUartTxNinth);
    CHECK(!fixture.cpu.mSABUartTxPending);
    CHECK(fixture.cpu.mSABUartTxPendingData == 0x51u);
    CHECK(fixture.cpu.mSABUartTxPendingNinth);
}

static void test_exact_startup_baud_and_long_phase(void)
{
    struct fixture fixture;
    struct uart_capture capture;
    const struct em8051_sab_uart_trace_record *start;
    uint64_t write_cycle;

    memset(&capture, 0, sizeof(capture));
    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_uart(&fixture.cpu, true, true);
    em8051_set_sab_uart_trace(&fixture.cpu,
                              record_only_uart_compile_check, &capture);
    em8051_sfr_write(&fixture.cpu, 0x99u, 0x5au);
    run_cycles(&fixture.cpu, 47u);
    CHECK(capture.count == 0);
    CHECK(fixture.cpu.mSABUartDividerPhase == 15u);
    run_cycles(&fixture.cpu, 1u);
    start = find_event(&capture, EM8051_SAB_UART_TRACE_TX_START, 0);
    CHECK(start != NULL);
    if (start)
        CHECK(start->machine_cycle == 48u);
    CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER1] == 16u);

    /* 3,000,003 cycles are 1,000,001 exact Timer1 events. The five-bit
     * divider must retain phase one without a host-time/floating accumulator. */
    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_uart(&fixture.cpu, true, true);
    run_cycles(&fixture.cpu, 3000003u);
    CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER1] == 1000001u);
    CHECK(fixture.cpu.mSABUartDividerPhase == 1u);
    memset(&capture, 0, sizeof(capture));
    em8051_set_sab_uart_trace(&fixture.cpu, capture_uart, &capture);
    write_cycle = fixture.cpu.mMachineCycleCount;
    em8051_sfr_write(&fixture.cpu, 0x99u, 0xa5u);
    run_cycles(&fixture.cpu, 44u);
    CHECK(capture.count == 0);
    run_cycles(&fixture.cpu, 1u);
    start = find_event(&capture, EM8051_SAB_UART_TRACE_TX_START, 0);
    CHECK(start != NULL);
    if (start)
        CHECK(start->machine_cycle == write_cycle + 45u);
}

static void test_smod_divide_16_and_32(void)
{
    struct fixture fixture;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_uart(&fixture.cpu, true, true);
    em8051_sfr_write(&fixture.cpu, 0x99u, 1u);
    run_cycles(&fixture.cpu, 47u);
    CHECK(!fixture.cpu.mSABUartTxActive);
    run_cycles(&fixture.cpu, 1u);
    CHECK(fixture.cpu.mSABUartTxActive);
    CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER1] == 16u);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_uart(&fixture.cpu, false, true);
    em8051_sfr_write(&fixture.cpu, 0x99u, 1u);
    run_cycles(&fixture.cpu, 95u);
    CHECK(!fixture.cpu.mSABUartTxActive);
    run_cycles(&fixture.cpu, 1u);
    CHECK(fixture.cpu.mSABUartTxActive);
    CHECK(fixture.cpu.mTimerOverflowCount[EM8051_TIMER1] == 32u);

    /* Selecting the unsupported dedicated generator must not silently keep
     * consuming Timer1 as a source. */
    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_uart(&fixture.cpu, true, true);
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_ADCON)] = SAB_ADCONMASK_BD;
    em8051_sfr_write(&fixture.cpu, 0x99u, 1u);
    run_cycles(&fixture.cpu, 96u);
    CHECK(!fixture.cpu.mSABUartTxActive);
    CHECK(!fixture.cpu.mSABUartTxPending);
    CHECK(fixture.cpu.mSABUartDividerPhase == 0);
    CHECK(!em8051_sab_uart_inject_rx_frame(&fixture.cpu, 1u, true));
}

static void test_sbuf_capture_and_separate_storage(void)
{
    struct fixture fixture;
    uint8_t original_rx;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_uart(&fixture.cpu, true, true);
    original_rx = fixture.cpu.mSABUartRxData;
    fixture.cpu.mSFR[REG_SCON] |= SCONMASK_TB8;
    em8051_sfr_write(&fixture.cpu, 0x99u, 0x96u);
    CHECK(em8051_sfr_read(&fixture.cpu, 0x99u) == original_rx);
    CHECK(fixture.cpu.mSFR[REG_SBUF] == original_rx);
    fixture.cpu.mSFR[REG_SCON] &= (uint8_t)~SCONMASK_TB8;
    run_cycles(&fixture.cpu, 48u);
    CHECK(fixture.cpu.mSABUartTxActive);
    CHECK(fixture.cpu.mSABUartTxData == 0x96u);
    CHECK(fixture.cpu.mSABUartTxNinth);
}

static void test_tx_frame_order_and_ti_timing(void)
{
    struct fixture fixture;
    struct uart_capture capture;
    bool expected[11] = {false, true, false, true, false, false,
                         true, false, true, true, true};
    size_t i;

    memset(&capture, 0, sizeof(capture));
    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_uart(&fixture.cpu, true, true);
    fixture.cpu.mSFR[REG_SCON] |= SCONMASK_TB8;
    em8051_set_sab_uart_trace(&fixture.cpu, capture_uart, &capture);
    em8051_sfr_write(&fixture.cpu, 0x99u, 0xa5u);

    run_cycles(&fixture.cpu, 527u);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_TI) == 0);
    run_cycles(&fixture.cpu, 1u);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_TI) != 0);
    CHECK(fixture.cpu.mSABUartTxActive);
    CHECK(fixture.cpu.mSABUartTxBitIndex == 10u);
    CHECK(capture.count == 11u);
    for (i = 0; i < 11u && i < capture.count; i++)
    {
        CHECK(capture.records[i].bit_index == i);
        CHECK(capture.records[i].bit_value == expected[i]);
        CHECK(capture.records[i].machine_cycle == 48u * (i + 1u));
    }
    CHECK(capture.records[0].event == EM8051_SAB_UART_TRACE_TX_START);
    CHECK(capture.records[9].event == EM8051_SAB_UART_TRACE_TX_BIT);
    CHECK(capture.records[10].event == EM8051_SAB_UART_TRACE_TX_STOP);

    run_cycles(&fixture.cpu, 48u);
    CHECK(!fixture.cpu.mSABUartTxActive);
    CHECK(capture.count == 12u);
    CHECK(capture.records[11].event == EM8051_SAB_UART_TRACE_TX_END);
    CHECK(capture.records[11].machine_cycle == 576u);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_TI) != 0);
    run_cycles(&fixture.cpu, 480u);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_TI) != 0);
    em8051_sfr_write(&fixture.cpu, 0x98u,
                     fixture.cpu.mSFR[REG_SCON] & (uint8_t)~SCONMASK_TI);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_TI) == 0);
}

static void test_back_to_back_tx_during_stop(void)
{
    struct fixture fixture;
    struct uart_capture capture;
    const struct em8051_sab_uart_trace_record *start1;
    const struct em8051_sab_uart_trace_record *start2;
    const struct em8051_sab_uart_trace_record *end1;
    const struct em8051_sab_uart_trace_record *stop2;

    memset(&capture, 0, sizeof(capture));
    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_uart(&fixture.cpu, true, true);
    em8051_set_sab_uart_trace(&fixture.cpu, capture_uart, &capture);
    em8051_sfr_write(&fixture.cpu, 0x99u, 0x12u);
    run_cycles(&fixture.cpu, 528u);
    CHECK(fixture.cpu.mSABUartTxActive);
    CHECK(fixture.cpu.mSABUartTxBitIndex == 10u);

    em8051_sfr_write(&fixture.cpu, 0x99u, 0x33u);
    fixture.cpu.mSFR[REG_SCON] |= SCONMASK_TB8;
    em8051_sfr_write(&fixture.cpu, 0x99u, 0x34u);
    fixture.cpu.mSFR[REG_SCON] &= (uint8_t)~SCONMASK_TB8;
    run_cycles(&fixture.cpu, 47u);
    CHECK(fixture.cpu.mSABUartTxData == 0x12u);
    CHECK(fixture.cpu.mSABUartTxPending);
    run_cycles(&fixture.cpu, 1u);
    CHECK(fixture.cpu.mSABUartTxData == 0x34u);
    CHECK(fixture.cpu.mSABUartTxNinth);
    CHECK(fixture.cpu.mSABUartTxBitIndex == 0u);

    start1 = find_event(&capture, EM8051_SAB_UART_TRACE_TX_START, 0);
    start2 = find_event(&capture, EM8051_SAB_UART_TRACE_TX_START, 1);
    end1 = find_event(&capture, EM8051_SAB_UART_TRACE_TX_END, 0);
    CHECK(start1 && start2 && end1);
    if (start1)
        CHECK(start1->machine_cycle == 48u && start1->data == 0x12u);
    if (end1)
        CHECK(end1->machine_cycle == 576u);
    if (start2)
        CHECK(start2->machine_cycle == 576u && start2->data == 0x34u &&
              start2->ninth_bit);

    run_cycles(&fixture.cpu, 480u);
    stop2 = find_event(&capture, EM8051_SAB_UART_TRACE_TX_STOP, 1);
    CHECK(stop2 != NULL);
    if (stop2)
        CHECK(stop2->machine_cycle == 1056u && stop2->data == 0x34u);
}

static void test_ti_driven_isr_back_to_back(void)
{
    struct fixture fixture;
    struct uart_capture capture;
    const struct em8051_sab_uart_trace_record *start2;

    memset(&capture, 0, sizeof(capture));
    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_uart(&fixture.cpu, true, true);
    fixture.code[0] = 0x80u; /* SJMP $ keeps foreground off the vector area. */
    fixture.code[1] = 0xfeu;
    fixture.code[EM8051_SAB_VECTOR_UART + 0u] = 0xa2u; /* MOV C,TI */
    fixture.code[EM8051_SAB_VECTOR_UART + 1u] = 0x99u;
    fixture.code[EM8051_SAB_VECTOR_UART + 2u] = 0xd2u; /* SETB TB8 */
    fixture.code[EM8051_SAB_VECTOR_UART + 3u] = 0x9bu;
    fixture.code[EM8051_SAB_VECTOR_UART + 4u] = 0x75u; /* MOV SBUF,#34 */
    fixture.code[EM8051_SAB_VECTOR_UART + 5u] = 0x99u;
    fixture.code[EM8051_SAB_VECTOR_UART + 6u] = 0x34u;
    fixture.code[EM8051_SAB_VECTOR_UART + 7u] = 0xc2u; /* CLR TI */
    fixture.code[EM8051_SAB_VECTOR_UART + 8u] = 0x99u;
    fixture.code[EM8051_SAB_VECTOR_UART + 9u] = 0x32u; /* RETI */
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)] =
        IEMASK_EA | IEMASK_ES;
    em8051_set_sab_uart_trace(&fixture.cpu, capture_uart, &capture);
    em8051_sfr_write(&fixture.cpu, 0x99u, 0x12u);

    run_cycles(&fixture.cpu, 575u);
    CHECK(find_event(&capture, EM8051_SAB_UART_TRACE_TX_START, 1) == NULL);
    CHECK(fixture.cpu.mSABUartTxPending);
    CHECK(fixture.cpu.mSABUartTxPendingData == 0x34u);
    CHECK(fixture.cpu.mSABUartTxPendingNinth);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_TI) == 0);
    CHECK(fixture.cpu.mSABIrqDepth == 0u);
    run_cycles(&fixture.cpu, 1u);
    start2 = find_event(&capture, EM8051_SAB_UART_TRACE_TX_START, 1);
    CHECK(start2 != NULL);
    if (start2)
    {
        CHECK(start2->machine_cycle == 576u);
        CHECK(start2->data == 0x34u);
        CHECK(start2->ninth_bit);
    }
}

static void test_repeated_tx_ordering(void)
{
    struct fixture fixture;
    struct uart_capture capture;
    uint8_t data[3] = {0x11u, 0x22u, 0x44u};
    bool ninth[3] = {false, true, false};
    size_t i;

    memset(&capture, 0, sizeof(capture));
    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_uart(&fixture.cpu, true, true);
    em8051_set_sab_uart_trace(&fixture.cpu, capture_uart, &capture);
    em8051_sfr_write(&fixture.cpu, 0x99u, data[0]);
    run_cycles(&fixture.cpu, 528u);
    fixture.cpu.mSFR[REG_SCON] |= SCONMASK_TB8;
    em8051_sfr_write(&fixture.cpu, 0x99u, data[1]);
    run_cycles(&fixture.cpu, 528u);
    fixture.cpu.mSFR[REG_SCON] &= (uint8_t)~SCONMASK_TB8;
    em8051_sfr_write(&fixture.cpu, 0x99u, data[2]);
    run_cycles(&fixture.cpu, 528u);

    for (i = 0; i < 3u; i++)
    {
        const struct em8051_sab_uart_trace_record *start =
            find_event(&capture, EM8051_SAB_UART_TRACE_TX_START, i);
        CHECK(start != NULL);
        if (start)
        {
            CHECK(start->machine_cycle == 48u + i * 528u);
            CHECK(start->data == data[i]);
            CHECK(start->ninth_bit == ninth[i]);
        }
    }
}

static void test_rx_accept_timing_and_ren_gate(void)
{
    struct fixture fixture;
    struct uart_capture capture;
    const struct em8051_sab_uart_trace_record *accept;
    uint8_t old_data;

    memset(&capture, 0, sizeof(capture));
    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_uart(&fixture.cpu, true, false);
    old_data = em8051_sfr_read(&fixture.cpu, 0x99u);
    CHECK(!em8051_sab_uart_inject_rx_frame(&fixture.cpu, 0x6cu, true));
    run_cycles(&fixture.cpu, 600u);
    CHECK(em8051_sfr_read(&fixture.cpu, 0x99u) == old_data);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_RI) == 0);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_uart(&fixture.cpu, true, true);
    em8051_set_sab_uart_trace(&fixture.cpu, capture_uart, &capture);
    run_cycles(&fixture.cpu, 15u); /* global phase is five overflows */
    CHECK(fixture.cpu.mSABUartDividerPhase == 5u);
    CHECK(em8051_sab_uart_inject_rx_frame(&fixture.cpu, 0x6cu, true));
    CHECK(capture.count == 1u);
    CHECK(capture.records[0].event == EM8051_SAB_UART_TRACE_RX_START);
    CHECK(capture.records[0].machine_cycle == 15u);
    CHECK(!em8051_sab_uart_inject_rx_frame(&fixture.cpu, 0x55u, false));
    run_cycles(&fixture.cpu, 479u);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_RI) == 0);
    run_cycles(&fixture.cpu, 1u);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_RI) != 0);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_RB8) != 0);
    CHECK(em8051_sfr_read(&fixture.cpu, 0x99u) == 0x6cu);
    CHECK(fixture.cpu.mSABUartRxActive);
    accept = find_event(&capture, EM8051_SAB_UART_TRACE_RX_ACCEPT, 0);
    CHECK(accept != NULL);
    if (accept)
        CHECK(accept->machine_cycle == 495u);
    run_cycles(&fixture.cpu, 48u);
    CHECK(!fixture.cpu.mSABUartRxActive);
    CHECK(find_event(&capture, EM8051_SAB_UART_TRACE_RX_END, 0) != NULL);
}

static void test_rx_ri_and_sm2_loss_rules(void)
{
    struct fixture fixture;
    uint8_t preserved;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_uart(&fixture.cpu, true, true);
    fixture.cpu.mSABUartRxData = 0x33u;
    fixture.cpu.mSFR[REG_SBUF] = 0x33u;
    fixture.cpu.mSFR[REG_SCON] |= SCONMASK_RI | SCONMASK_RB8;
    CHECK(em8051_sab_uart_inject_rx_frame(&fixture.cpu, 0x77u, false));
    run_cycles(&fixture.cpu, 480u);
    CHECK(em8051_sfr_read(&fixture.cpu, 0x99u) == 0x33u);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_RB8) != 0);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_RI) != 0);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_uart(&fixture.cpu, true, true);
    preserved = em8051_sfr_read(&fixture.cpu, 0x99u);
    fixture.cpu.mSFR[REG_SCON] |= SCONMASK_SM2;
    CHECK(em8051_sab_uart_inject_rx_frame(&fixture.cpu, 0x88u, false));
    run_cycles(&fixture.cpu, 480u);
    CHECK(em8051_sfr_read(&fixture.cpu, 0x99u) == preserved);
    CHECK((fixture.cpu.mSFR[REG_SCON] & (SCONMASK_RI | SCONMASK_RB8)) == 0);
    run_cycles(&fixture.cpu, 48u);
    CHECK(em8051_sab_uart_inject_rx_frame(&fixture.cpu, 0x99u, true));
    run_cycles(&fixture.cpu, 480u);
    CHECK(em8051_sfr_read(&fixture.cpu, 0x99u) == 0x99u);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_RI) != 0);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_RB8) != 0);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_uart(&fixture.cpu, true, true);
    CHECK(em8051_sab_uart_inject_rx_frame(&fixture.cpu, 0xaau, false));
    run_cycles(&fixture.cpu, 480u);
    CHECK(em8051_sfr_read(&fixture.cpu, 0x99u) == 0xaau);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_RI) != 0);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_RB8) == 0);
}

static void test_full_duplex_independence(void)
{
    struct fixture fixture;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_uart(&fixture.cpu, true, true);
    fixture.cpu.mSFR[REG_SCON] |= SCONMASK_TB8;
    em8051_sfr_write(&fixture.cpu, 0x99u, 0x3cu);
    CHECK(em8051_sab_uart_inject_rx_frame(&fixture.cpu, 0xc3u, false));
    fixture.cpu.mSFR[REG_SCON] &= (uint8_t)~SCONMASK_TB8;
    run_cycles(&fixture.cpu, 480u);
    CHECK(fixture.cpu.mSABUartTxActive);
    CHECK(fixture.cpu.mSABUartTxData == 0x3cu);
    CHECK(fixture.cpu.mSABUartTxNinth);
    CHECK(fixture.cpu.mSABUartTxBitIndex == 9u);
    CHECK(em8051_sfr_read(&fixture.cpu, 0x99u) == 0xc3u);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_RB8) == 0);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_RI) != 0);
    run_cycles(&fixture.cpu, 48u);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_TI) != 0);
    CHECK(em8051_sfr_read(&fixture.cpu, 0x99u) == 0xc3u);
}

static void accept_uart(struct fixture *aFixture)
{
    aFixture->cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)] =
        IEMASK_EA | IEMASK_ES;
    (void)tick(&aFixture->cpu);
    CHECK(aFixture->cpu.mPC == EM8051_SAB_VECTOR_UART);
    CHECK(aFixture->cpu.mSABIrqDepth == 1u);
    CHECK((aFixture->cpu.mSABIrqInService &
           IRQ_BIT(EM8051_SAB_IRQ_UART)) != 0);
}

static void test_shared_uart_interrupt_flags(void)
{
    struct fixture fixture;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_uart(&fixture.cpu, true, true);
    em8051_sfr_write(&fixture.cpu, 0x99u, 0x55u);
    run_cycles(&fixture.cpu, 528u);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_TI) != 0);
    accept_uart(&fixture);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_TI) != 0);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_uart(&fixture.cpu, true, true);
    CHECK(em8051_sab_uart_inject_rx_frame(&fixture.cpu, 0x66u, true));
    run_cycles(&fixture.cpu, 480u);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_RI) != 0);
    accept_uart(&fixture);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_RI) != 0);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    fixture.cpu.mSFR[REG_SCON] = SCONMASK_RI | SCONMASK_TI;
    accept_uart(&fixture);
    em8051_sfr_write(&fixture.cpu, 0x98u, SCONMASK_RI);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_RI) != 0);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_TI) == 0);
    (void)tick(&fixture.cpu);
    CHECK((fixture.cpu.mSABIrqPending & IRQ_BIT(EM8051_SAB_IRQ_UART)) != 0);
    em8051_sfr_write(&fixture.cpu, 0x98u, SCONMASK_TI);
    (void)tick(&fixture.cpu);
    CHECK((fixture.cpu.mSABIrqPending & IRQ_BIT(EM8051_SAB_IRQ_UART)) != 0);
}

static void test_software_ti_and_reti_semantics(void)
{
    struct fixture fixture;
    struct em8051_run_result result;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)] =
        IEMASK_EA | IEMASK_ES;
    fixture.code[0] = 0xd2u; /* SETB TI */
    fixture.code[1] = 0x99u;
    CHECK(em8051_run(&fixture.cpu, 1u, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_TI) != 0);
    CHECK(em8051_run_until_pc(&fixture.cpu, EM8051_SAB_VECTOR_UART,
                              1u, &result) == EM8051_STOP_TARGET_PC);
    CHECK(fixture.cpu.mSABIrqDepth == 1u);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_TI) != 0);

    fixture.cpu.mSFR[REG_SCON] |= SCONMASK_RI;
    fixture.code[EM8051_SAB_VECTOR_UART] = 0x32u; /* RETI */
    CHECK(em8051_run(&fixture.cpu, 1u, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(fixture.cpu.mSABIrqDepth == 0u);
    CHECK((fixture.cpu.mSFR[REG_SCON] &
           (SCONMASK_RI | SCONMASK_TI)) ==
          (SCONMASK_RI | SCONMASK_TI));

    fixture.cpu.mSFR[SFR_INDEX(EM8051_SAB_SFR_IEN0)] = 0;
    fixture.code[0] = 0xc2u; /* CLR TI */
    fixture.code[1] = 0x99u;
    fixture.cpu.mPC = 0;
    CHECK(em8051_run(&fixture.cpu, 1u, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_TI) == 0);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_RI) != 0);
}

static void test_instruction_isr_access_pattern(void)
{
    struct fixture fixture;
    struct em8051_run_result result;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    configure_uart(&fixture.cpu, true, true);
    CHECK(em8051_sab_uart_inject_rx_frame(&fixture.cpu, 0x6du, true));
    run_cycles(&fixture.cpu, 480u);
    fixture.cpu.mSFR[REG_SCON] |= SCONMASK_TI;
    fixture.cpu.mSFR[REG_TCON] &= (uint8_t)~TCONMASK_TR1;
    fixture.code[0] = 0xa2u; /* MOV C,TI */
    fixture.code[1] = 0x99u;
    fixture.code[2] = 0x92u; /* MOV TB8,C */
    fixture.code[3] = 0x9bu;
    fixture.code[4] = 0x75u; /* MOV SBUF,#A6 */
    fixture.code[5] = 0x99u;
    fixture.code[6] = 0xa6u;
    fixture.code[7] = 0xe5u; /* MOV A,SBUF */
    fixture.code[8] = 0x99u;
    fixture.code[9] = 0xa2u; /* MOV C,RB8 */
    fixture.code[10] = 0x9au;
    fixture.code[11] = 0xc2u; /* CLR TI */
    fixture.code[12] = 0x99u;
    fixture.cpu.mPC = 0;

    CHECK(em8051_run(&fixture.cpu, 6u, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.pc == 13u);
    CHECK(fixture.cpu.mSABUartTxPendingData == 0xa6u);
    CHECK(fixture.cpu.mSABUartTxPendingNinth);
    CHECK(fixture.cpu.mSFR[REG_ACC] == 0x6du);
    CHECK((fixture.cpu.mSFR[REG_PSW] & PSWMASK_C) != 0);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_TI) == 0);
    CHECK((fixture.cpu.mSFR[REG_SCON] & SCONMASK_RI) != 0);
}

static void test_observer_determinism_and_reset(void)
{
    struct fixture traced;
    struct fixture repeated;
    struct fixture untraced;
    struct uart_capture first;
    struct uart_capture second;
    struct timer_capture timer_capture;
    size_t i;

    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    memset(&timer_capture, 0, sizeof(timer_capture));
    setup_fixture(&traced, EM8051_VARIANT_SAB80535);
    setup_fixture(&repeated, EM8051_VARIANT_SAB80535);
    setup_fixture(&untraced, EM8051_VARIANT_SAB80535);
    configure_uart(&traced.cpu, true, true);
    configure_uart(&repeated.cpu, true, true);
    configure_uart(&untraced.cpu, true, true);
    em8051_set_sab_uart_trace(&traced.cpu, capture_uart, &first);
    em8051_set_sab_uart_trace(&repeated.cpu, capture_uart, &second);
    em8051_set_timer_overflow_callback(&traced.cpu, capture_timer,
                                       &timer_capture);
    em8051_sfr_write(&traced.cpu, 0x99u, 0x5au);
    em8051_sfr_write(&repeated.cpu, 0x99u, 0x5au);
    em8051_sfr_write(&untraced.cpu, 0x99u, 0x5au);
    CHECK(em8051_sab_uart_inject_rx_frame(&traced.cpu, 0xa5u, true));
    CHECK(em8051_sab_uart_inject_rx_frame(&repeated.cpu, 0xa5u, true));
    CHECK(em8051_sab_uart_inject_rx_frame(&untraced.cpu, 0xa5u, true));
    run_cycles(&traced.cpu, 600u);
    run_cycles(&repeated.cpu, 600u);
    run_cycles(&untraced.cpu, 600u);
    CHECK(first.count == second.count);
    CHECK(timer_capture.count == 200u);
    for (i = 0; i < first.count && i < second.count; i++)
    {
        CHECK(first.records[i].event == second.records[i].event);
        CHECK(first.records[i].machine_cycle == second.records[i].machine_cycle);
        CHECK(first.records[i].pc == second.records[i].pc);
        CHECK(first.records[i].data == second.records[i].data);
        CHECK(first.records[i].ninth_bit == second.records[i].ninth_bit);
        CHECK(first.records[i].bit_index == second.records[i].bit_index);
        CHECK(first.records[i].bit_value == second.records[i].bit_value);
    }
    CHECK(traced.cpu.mMachineCycleCount == untraced.cpu.mMachineCycleCount);
    CHECK(traced.cpu.mTimerOverflowCount[EM8051_TIMER1] ==
          untraced.cpu.mTimerOverflowCount[EM8051_TIMER1]);
    CHECK(traced.cpu.mSABUartDividerPhase ==
          untraced.cpu.mSABUartDividerPhase);
    CHECK(traced.cpu.mSABUartTxActive == untraced.cpu.mSABUartTxActive);
    CHECK(traced.cpu.mSABUartRxActive == untraced.cpu.mSABUartRxActive);
    CHECK(traced.cpu.mSFR[REG_SCON] == untraced.cpu.mSFR[REG_SCON]);
    CHECK(em8051_sfr_read(&traced.cpu, 0x99u) ==
          em8051_sfr_read(&untraced.cpu, 0x99u));

    reset(&traced.cpu, false);
    CHECK(traced.cpu.mSABUartDividerPhase == 0);
    CHECK(traced.cpu.mSABUartRxDividerPhase == 0);
    CHECK(!traced.cpu.mSABUartTxPending);
    CHECK(!traced.cpu.mSABUartTxActive);
    CHECK(!traced.cpu.mSABUartRxActive);
    CHECK(traced.cpu.mSABUartRxData == traced.cpu.mSFR[REG_SBUF]);
}

static void test_classic_sbuf_regression(void)
{
    struct fixture fixture;
    enum em8051_variant variant;

    for (variant = EM8051_VARIANT_8051;
         variant <= EM8051_VARIANT_8052; variant++)
    {
        setup_fixture(&fixture, variant);
        em8051_sfr_write(&fixture.cpu, 0x99u, 0xceu);
        CHECK(fixture.cpu.mSFR[REG_SBUF] == 0xceu);
        CHECK(em8051_sfr_read(&fixture.cpu, 0x99u) == 0xceu);
        CHECK(!em8051_sab_uart_inject_rx_frame(&fixture.cpu, 1u, true));
        CHECK(!fixture.cpu.mSABUartTxPending);
    }
}

int main(void)
{
    test_sbuf_callback_and_trace_contract();
    test_sbuf_callback_rx_coherence();
    test_unsupported_writes_preserve_mode3_state();
    test_exact_startup_baud_and_long_phase();
    test_smod_divide_16_and_32();
    test_sbuf_capture_and_separate_storage();
    test_tx_frame_order_and_ti_timing();
    test_back_to_back_tx_during_stop();
    test_ti_driven_isr_back_to_back();
    test_repeated_tx_ordering();
    test_rx_accept_timing_and_ren_gate();
    test_rx_ri_and_sm2_loss_rules();
    test_full_duplex_independence();
    test_shared_uart_interrupt_flags();
    test_software_ti_and_reti_semantics();
    test_instruction_isr_access_pattern();
    test_observer_determinism_and_reset();
    test_classic_sbuf_regression();

    if (gFailures != 0)
    {
        fprintf(stderr, "SLC-007 UART tests failed: %d\n", gFailures);
        return EXIT_FAILURE;
    }
    printf("SLC-007 UART tests passed\n");
    return EXIT_SUCCESS;
}

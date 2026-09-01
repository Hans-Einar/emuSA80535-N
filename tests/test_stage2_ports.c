/* Focused deterministic SLC-010 tests for SAB ports and MOVX context. */

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
            fprintf(stderr, "%s:%d: CHECK failed: %s\n",                    \
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

static void run_instructions(struct fixture *aFixture, uint64_t aCount)
{
    struct em8051_run_result result;
    CHECK(em8051_run(&aFixture->cpu, aCount, &result) ==
          EM8051_STOP_INSTRUCTION_LIMIT);
    CHECK(result.instructions == aCount);
}

static void test_reset_resolution_and_api_safety(void)
{
    static const uint8_t ports[] =
    {
        EM8051_SAB_PORT_P1, EM8051_SAB_PORT_P3,
        EM8051_SAB_PORT_P4, EM8051_SAB_PORT_P5
    };
    struct fixture fixture;
    struct fixture classic;
    uint8_t value;
    size_t i;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    for (i = 0; i < ARRAY_SIZE(ports); i++)
    {
        value = 0;
        CHECK(em8051_sab_port_get_latch(&fixture.cpu, ports[i], &value));
        CHECK(value == 0xffu);
        value = 0;
        CHECK(em8051_sab_port_get_pins(&fixture.cpu, ports[i], &value));
        CHECK(value == 0xffu);

        CHECK(em8051_sab_port_drive(&fixture.cpu, ports[i], 0x0fu, 0x05u));
        CHECK(em8051_sab_port_get_pins(&fixture.cpu, ports[i], &value));
        CHECK(value == 0xf5u);
        CHECK(em8051_sab_port_get_latch(&fixture.cpu, ports[i], &value));
        CHECK(value == 0xffu);
        reset(&fixture.cpu, false);
        CHECK(em8051_sab_port_get_pins(&fixture.cpu, ports[i], &value));
        CHECK(value == 0xffu);
    }

    value = 0x5au;
    CHECK(!em8051_sab_port_drive(NULL, EM8051_SAB_PORT_P1, 0xffu, 0));
    CHECK(!em8051_sab_port_release(NULL, EM8051_SAB_PORT_P1, 0xffu));
    CHECK(!em8051_sab_port_get_latch(NULL, EM8051_SAB_PORT_P1, &value));
    CHECK(value == 0x5au);
    CHECK(!em8051_sab_port_get_pins(&fixture.cpu, 0x80u, &value));
    CHECK(value == 0x5au);
    CHECK(!em8051_sab_port_get_latch(&fixture.cpu,
                                      EM8051_SAB_PORT_P1, NULL));

    setup_fixture(&classic, EM8051_VARIANT_8051);
    CHECK(!em8051_sab_port_drive(&classic.cpu, EM8051_SAB_PORT_P1,
                                  0xffu, 0));
    CHECK(!em8051_sab_port_release(&classic.cpu, EM8051_SAB_PORT_P1, 0xffu));
    CHECK(!em8051_sab_port_get_latch(&classic.cpu,
                                      EM8051_SAB_PORT_P1, &value));
    CHECK(classic.cpu.mSFR[REG_P1] == 0xffu);
}

static void test_quasi_bidirectional_resolution(void)
{
    struct fixture fixture;
    uint8_t value;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_PORT_P1, 0xa5u);
    CHECK(em8051_sab_port_drive(&fixture.cpu, EM8051_SAB_PORT_P1,
                                0x0fu, 0xffu));
    CHECK(em8051_sab_port_get_pins(&fixture.cpu,
                                   EM8051_SAB_PORT_P1, &value));
    CHECK(value == 0xa5u); /* External high cannot override latch zero. */
    CHECK(em8051_sab_port_drive(&fixture.cpu, EM8051_SAB_PORT_P1,
                                0xf0u, 0x00u));
    CHECK(em8051_sab_port_get_pins(&fixture.cpu,
                                   EM8051_SAB_PORT_P1, &value));
    CHECK(value == 0x05u);
    CHECK(em8051_sab_port_get_latch(&fixture.cpu,
                                    EM8051_SAB_PORT_P1, &value));
    CHECK(value == 0xa5u);
    CHECK(em8051_sab_port_release(&fixture.cpu, EM8051_SAB_PORT_P1, 0xf0u));
    CHECK(em8051_sab_port_get_pins(&fixture.cpu,
                                   EM8051_SAB_PORT_P1, &value));
    CHECK(value == 0xa5u);
}

static void test_ordinary_byte_and_bit_reads_use_pins(void)
{
    static const uint8_t ports[] =
    {
        EM8051_SAB_PORT_P1, EM8051_SAB_PORT_P3,
        EM8051_SAB_PORT_P4, EM8051_SAB_PORT_P5
    };
    struct fixture fixture;
    size_t i;

    for (i = 0; i < ARRAY_SIZE(ports); i++)
    {
        setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
        em8051_sfr_write(&fixture.cpu, ports[i], 0xffu);
        CHECK(em8051_sab_port_drive(&fixture.cpu, ports[i], 0x0fu, 0x05u));
        fixture.code[0] = 0xe5u; /* MOV A,direct: ordinary byte read. */
        fixture.code[1] = ports[i];
        run_instructions(&fixture, 1);
        CHECK(fixture.cpu.mSFR[REG_ACC] == 0xf5u);

        reset(&fixture.cpu, false);
        fixture.code[0] = 0xa2u; /* MOV C,port.0: ordinary bit read. */
        fixture.code[1] = ports[i];
        CHECK(em8051_sab_port_drive(&fixture.cpu, ports[i], 0x01u, 0));
        fixture.cpu.mSFR[REG_PSW] |= PSWMASK_C;
        run_instructions(&fixture, 1);
        CHECK((fixture.cpu.mSFR[REG_PSW] & PSWMASK_C) == 0);
        CHECK(fixture.cpu.mSFR[ports[i] - 0x80u] == 0xffu);
    }
}

static int gSfrReadCount;
static int gSfrWriteCount;
static uint8_t gSfrReadValue;
static uint8_t gFirstWriteValue;
static uint8_t gLastWriteValue;
static uint8_t gReentrantReadWriteValue;
static bool gReenterRead;
static bool gReenterWrite;
static bool gInsideWrite;

static uint8_t test_sfr_read(struct em8051 *aCPU, uint8_t aAddress)
{
    gSfrReadCount++;
    if (gReenterRead)
    {
        gReenterRead = false;
        em8051_sfr_write(aCPU, aAddress, gReentrantReadWriteValue);
    }
    return gSfrReadValue;
}

static void test_sfr_write(struct em8051 *aCPU, uint8_t aAddress)
{
    uint8_t value = aCPU->mSFR[aAddress - 0x80u];
    if (gSfrWriteCount == 0)
        gFirstWriteValue = value;
    gLastWriteValue = value;
    gSfrWriteCount++;
    if (gReenterWrite && !gInsideWrite)
    {
        gInsideWrite = true;
        em8051_sfr_write(aCPU, aAddress, 0x3cu);
        gInsideWrite = false;
    }
}

static void clear_callback_counters(void)
{
    gSfrReadCount = 0;
    gSfrWriteCount = 0;
    gSfrReadValue = 0;
    gFirstWriteValue = 0;
    gLastWriteValue = 0;
    gReentrantReadWriteValue = 0;
    gReenterRead = false;
    gReenterWrite = false;
    gInsideWrite = false;
}

struct byte_rmw_probe
{
    uint8_t opcode;
    uint8_t operand;
    uint8_t initial;
    uint8_t acc;
    uint8_t expected;
    uint8_t length;
};

static void test_all_documented_byte_rmw_handlers(void)
{
    static const struct byte_rmw_probe probes[] =
    {
        { 0x42u, 0,    0xa0u, 0x05u, 0xa5u, 2 }, /* ORL direct,A */
        { 0x43u, 0x05u,0xa0u, 0,     0xa5u, 3 }, /* ORL direct,# */
        { 0x52u, 0,    0xafu, 0xf0u, 0xa0u, 2 }, /* ANL direct,A */
        { 0x53u, 0xf0u,0xafu, 0,     0xa0u, 3 }, /* ANL direct,# */
        { 0x62u, 0,    0xa0u, 0x0fu, 0xafu, 2 }, /* XRL direct,A */
        { 0x63u, 0x0fu,0xa0u, 0,     0xafu, 3 }, /* XRL direct,# */
        { 0x05u, 0,    0xa0u, 0,     0xa1u, 2 }, /* INC direct */
        { 0x15u, 0,    0xa0u, 0,     0x9fu, 2 }, /* DEC direct */
        { 0xd5u, 0,    0x02u, 0,     0x01u, 3 }  /* DJNZ direct,rel */
    };
    struct fixture fixture;
    uint8_t latch;
    size_t i;

    for (i = 0; i < ARRAY_SIZE(probes); i++)
    {
        setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
        clear_callback_counters();
        fixture.cpu.sfrread[EM8051_SAB_PORT_P1 - 0x80u] = test_sfr_read;
        fixture.cpu.sfrwrite[EM8051_SAB_PORT_P1 - 0x80u] = test_sfr_write;
        em8051_sfr_write(&fixture.cpu, EM8051_SAB_PORT_P1,
                         probes[i].initial);
        clear_callback_counters();
        CHECK(em8051_sab_port_drive(&fixture.cpu, EM8051_SAB_PORT_P1,
                                    0xffu, 0));
        fixture.cpu.mSFR[REG_ACC] = probes[i].acc;
        fixture.code[0] = probes[i].opcode;
        fixture.code[1] = EM8051_SAB_PORT_P1;
        if (probes[i].length == 3)
            fixture.code[2] = probes[i].operand;
        run_instructions(&fixture, 1);
        CHECK(em8051_sab_port_get_latch(&fixture.cpu,
                                        EM8051_SAB_PORT_P1, &latch));
        CHECK(latch == probes[i].expected);
        CHECK(gSfrReadCount == 0);
        CHECK(gSfrWriteCount == 1);
        CHECK(gLastWriteValue == probes[i].expected);
    }
}

struct bit_rmw_probe
{
    uint8_t opcode;
    uint8_t initial;
    uint8_t expected;
    bool carry;
};

static void test_all_documented_bit_rmw_handlers(void)
{
    static const struct bit_rmw_probe probes[] =
    {
        { 0x10u, 0xa1u, 0xa0u, false }, /* JBC bit,rel */
        { 0xb2u, 0xa0u, 0xa1u, false }, /* CPL bit */
        { 0x92u, 0xa0u, 0xa1u, true  }, /* MOV bit,C */
        { 0xc2u, 0xa1u, 0xa0u, false }, /* CLR bit */
        { 0xd2u, 0xa0u, 0xa1u, false }  /* SETB bit */
    };
    struct fixture fixture;
    uint8_t latch;
    size_t i;

    for (i = 0; i < ARRAY_SIZE(probes); i++)
    {
        setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
        clear_callback_counters();
        fixture.cpu.sfrread[EM8051_SAB_PORT_P1 - 0x80u] = test_sfr_read;
        fixture.cpu.sfrwrite[EM8051_SAB_PORT_P1 - 0x80u] = test_sfr_write;
        em8051_sfr_write(&fixture.cpu, EM8051_SAB_PORT_P1,
                         probes[i].initial);
        clear_callback_counters();
        CHECK(em8051_sab_port_drive(&fixture.cpu, EM8051_SAB_PORT_P1,
                                    0xffu, 0));
        fixture.cpu.mSFR[REG_PSW] = probes[i].carry ? PSWMASK_C : 0;
        fixture.code[0] = probes[i].opcode;
        fixture.code[1] = EM8051_SAB_PORT_P1; /* P1.0 bit address */
        fixture.code[2] = 2; /* JBC branch makes latch-read visible in PC. */
        run_instructions(&fixture, 1);
        CHECK(em8051_sab_port_get_latch(&fixture.cpu,
                                        EM8051_SAB_PORT_P1, &latch));
        CHECK(latch == probes[i].expected);
        CHECK(gSfrReadCount == 0);
        CHECK(gSfrWriteCount == 1);
        CHECK(gLastWriteValue == probes[i].expected);
        if (probes[i].opcode == 0x10u)
            CHECK(fixture.cpu.mPC == 5u);
    }
}

static void test_p4_p5_rmw_and_callback_compatibility(void)
{
    static const uint8_t ports[] = { EM8051_SAB_PORT_P4,
                                     EM8051_SAB_PORT_P5 };
    struct fixture fixture;
    uint8_t value;
    size_t i;

    for (i = 0; i < ARRAY_SIZE(ports); i++)
    {
        setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
        em8051_sfr_write(&fixture.cpu, ports[i], 0xa0u);
        CHECK(em8051_sab_port_drive(&fixture.cpu, ports[i], 0xffu, 0));
        fixture.code[0] = 0x43u; /* ORL port,#01 */
        fixture.code[1] = ports[i];
        fixture.code[2] = 0x01u;
        fixture.code[3] = 0xb2u; /* CPL port.1 */
        fixture.code[4] = (uint8_t)(ports[i] | 0x01u);
        run_instructions(&fixture, 2);
        CHECK(em8051_sab_port_get_latch(&fixture.cpu, ports[i], &value));
        CHECK(value == 0xa3u);
        CHECK(em8051_sab_port_get_pins(&fixture.cpu, ports[i], &value));
        CHECK(value == 0x00u);
    }

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    clear_callback_counters();
    fixture.cpu.sfrread[EM8051_SAB_PORT_P3 - 0x80u] = test_sfr_read;
    fixture.cpu.sfrwrite[EM8051_SAB_PORT_P3 - 0x80u] = test_sfr_write;
    gSfrReadValue = 0x69u;
    CHECK(em8051_sfr_read(&fixture.cpu, EM8051_SAB_PORT_P3) == 0x69u);
    CHECK(gSfrReadCount == 1);
    CHECK(em8051_sab_port_get_pins(&fixture.cpu,
                                   EM8051_SAB_PORT_P3, &value));
    CHECK(value == 0xffu); /* Host query is canonical, not callback override. */

    gReentrantReadWriteValue = 0x5au;
    gReenterRead = true;
    CHECK(em8051_sfr_read(&fixture.cpu, EM8051_SAB_PORT_P3) == 0x69u);
    CHECK(em8051_sab_port_get_latch(&fixture.cpu,
                                    EM8051_SAB_PORT_P3, &value));
    CHECK(value == 0x5au);

    clear_callback_counters();
    gReenterWrite = true;
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_PORT_P3, 0xf0u);
    CHECK(gSfrWriteCount == 2);
    CHECK(gFirstWriteValue == 0xf0u);
    CHECK(gLastWriteValue == 0x3cu);
    CHECK(em8051_sab_port_get_latch(&fixture.cpu,
                                    EM8051_SAB_PORT_P3, &value));
    CHECK(value == 0x3cu);
    CHECK(em8051_sab_port_drive(&fixture.cpu,
                                EM8051_SAB_PORT_P3, 0x0cu, 0));
    CHECK(em8051_sab_port_get_pins(&fixture.cpu,
                                   EM8051_SAB_PORT_P3, &value));
    CHECK(value == 0x30u);
}

static void test_classic_callback_behavior_is_unchanged(void)
{
    struct fixture fixture;

    setup_fixture(&fixture, EM8051_VARIANT_8051);
    clear_callback_counters();
    gSfrReadValue = 0x10u;
    fixture.cpu.sfrread[REG_P1] = test_sfr_read;
    fixture.cpu.sfrwrite[REG_P1] = test_sfr_write;
    fixture.code[0] = 0x05u; /* Existing INC direct reads callback on classic. */
    fixture.code[1] = EM8051_SAB_PORT_P1;
    run_instructions(&fixture, 1);
    CHECK(gSfrReadCount == 1);
    CHECK(gSfrWriteCount == 1);
    CHECK(fixture.cpu.mSFR[REG_P1] == 0x11u);
}

struct movx_capture
{
    struct em8051_movx_context records[16];
    size_t count;
};

static void capture_movx(const struct em8051_movx_context *aContext,
                         void *aUser)
{
    struct movx_capture *capture = (struct movx_capture *)aUser;
    struct em8051_movx_context writable_copy = *aContext;
    if (capture->count < ARRAY_SIZE(capture->records))
        capture->records[capture->count++] = *aContext;
    writable_copy.p1_latch = 0; /* Proves only the caller-owned copy is mutable. */
    (void)writable_copy;
}

static void check_movx(const struct em8051_movx_context *aRecord,
                       uint64_t aCycle, uint16_t aPC, uint16_t aAddress,
                       enum em8051_movx_direction aDirection,
                       uint8_t aValue, uint8_t aP1)
{
    CHECK(aRecord->machine_cycle == aCycle);
    CHECK(aRecord->pc == aPC);
    CHECK(aRecord->address == aAddress);
    CHECK(aRecord->direction == aDirection);
    CHECK(aRecord->value == aValue);
    CHECK(aRecord->p1_latch == aP1);
}

static void test_movx_backing_dptr_and_ri_context(void)
{
    static const uint8_t program[] =
    {
        0x90u, 0x12u, 0x34u, /* MOV DPTR,#1234 */
        0x74u, 0x5au,       /* MOV A,#5A */
        0xf0u,              /* MOVX @DPTR,A */
        0x74u, 0x00u,       /* MOV A,#00 */
        0xe0u,              /* MOVX A,@DPTR */
        0x78u, 0x56u,       /* MOV R0,#56 */
        0x74u, 0x7cu,       /* MOV A,#7C */
        0xf2u,              /* MOVX @R0,A */
        0x74u, 0x00u,       /* MOV A,#00 */
        0xe2u               /* MOVX A,@R0 */
    };
    struct fixture fixture;
    struct movx_capture capture;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    memset(&capture, 0, sizeof(capture));
    memcpy(fixture.code, program, sizeof(program));
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_PORT_P1, 0xc3u);
    em8051_set_movx_observer(&fixture.cpu, capture_movx, &capture);
    run_instructions(&fixture, 10);
    CHECK(capture.count == 4u);
    check_movx(&capture.records[0], 3u, 5u, 0x1234u,
               EM8051_MOVX_WRITE, 0x5au, 0xc3u);
    check_movx(&capture.records[1], 6u, 8u, 0x1234u,
               EM8051_MOVX_READ, 0x5au, 0xc3u);
    check_movx(&capture.records[2], 10u, 13u, 0x0056u,
               EM8051_MOVX_WRITE, 0x7cu, 0xc3u);
    check_movx(&capture.records[3], 13u, 16u, 0x0056u,
               EM8051_MOVX_READ, 0x7cu, 0xc3u);
    CHECK(fixture.xdata[0x1234] == 0x5au);
    CHECK(fixture.xdata[0x0056] == 0x7cu);
    CHECK(fixture.cpu.mSFR[REG_ACC] == 0x7cu);
    CHECK(fixture.cpu.mSFR[REG_P1] == 0xc3u);
}

static struct em8051 *gLegacyCPU;
static uint16_t gLegacyAddress;
static uint8_t gLegacyValue;

static uint8_t mutating_xread(struct em8051 *aCPU, uint16_t aAddress)
{
    gLegacyCPU = aCPU;
    gLegacyAddress = aAddress;
    em8051_sfr_write(aCPU, EM8051_SAB_PORT_P1, 0x00u);
    return 0xa6u;
}

static void mutating_xwrite(struct em8051 *aCPU, uint16_t aAddress,
                            uint8_t aValue)
{
    gLegacyCPU = aCPU;
    gLegacyAddress = aAddress;
    gLegacyValue = aValue;
    em8051_sfr_write(aCPU, EM8051_SAB_PORT_P1, 0x00u);
}

static void test_legacy_callbacks_and_pre_callback_snapshot(void)
{
    struct fixture fixture;
    struct movx_capture capture;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    memset(&capture, 0, sizeof(capture));
    fixture.cpu.mSFR[REG_DPH] = 0x22u;
    fixture.cpu.mSFR[REG_DPL] = 0x11u;
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_PORT_P1, 0xc0u);
    fixture.cpu.xread = mutating_xread;
    fixture.code[0] = 0xe0u;
    em8051_set_movx_observer(&fixture.cpu, capture_movx, &capture);
    run_instructions(&fixture, 1);
    CHECK(gLegacyCPU == &fixture.cpu);
    CHECK(gLegacyAddress == 0x2211u);
    CHECK(fixture.cpu.mSFR[REG_ACC] == 0xa6u);
    CHECK(capture.count == 1u);
    check_movx(&capture.records[0], 0u, 0u, 0x2211u,
               EM8051_MOVX_READ, 0xa6u, 0xc0u);
    CHECK(fixture.cpu.mSFR[REG_P1] == 0x00u);

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    memset(&capture, 0, sizeof(capture));
    fixture.cpu.mSFR[REG_DPH] = 0x44u;
    fixture.cpu.mSFR[REG_DPL] = 0x33u;
    fixture.cpu.mSFR[REG_ACC] = 0x5du;
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_PORT_P1, 0x80u);
    fixture.cpu.xwrite = mutating_xwrite;
    fixture.code[0] = 0xf0u;
    em8051_set_movx_observer(&fixture.cpu, capture_movx, &capture);
    run_instructions(&fixture, 1);
    CHECK(gLegacyAddress == 0x4433u);
    CHECK(gLegacyValue == 0x5du);
    CHECK(capture.count == 1u);
    check_movx(&capture.records[0], 0u, 0u, 0x4433u,
               EM8051_MOVX_WRITE, 0x5du, 0x80u);
}

static void test_save_change_access_restore_and_external_force(void)
{
    static const uint8_t program[] =
    {
        0xf0u,                   /* access with original P1 */
        0x85u, 0x90u, 0x20u,    /* save P1 to internal 20 */
        0x75u, 0x90u, 0x40u,    /* change P1 */
        0xf0u,                   /* access with changed P1 */
        0x85u, 0x20u, 0x90u,    /* restore P1 */
        0xf0u                    /* access with restored P1 */
    };
    struct fixture fixture;
    struct movx_capture capture;
    uint8_t pins;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    memset(&capture, 0, sizeof(capture));
    memcpy(fixture.code, program, sizeof(program));
    fixture.cpu.mSFR[REG_ACC] = 0x55u;
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_PORT_P1, 0xc0u);
    em8051_set_movx_observer(&fixture.cpu, capture_movx, &capture);
    run_instructions(&fixture, 6);
    CHECK(capture.count == 3u);
    CHECK(capture.records[0].p1_latch == 0xc0u);
    CHECK(capture.records[1].p1_latch == 0x40u);
    CHECK(capture.records[2].p1_latch == 0xc0u);
    CHECK(capture.records[0].address == 0u);
    CHECK(capture.records[1].address == 0u);
    CHECK(capture.records[2].address == 0u);
    CHECK(fixture.cpu.mLowerData[0x20] == 0xc0u);
    CHECK(fixture.cpu.mSFR[REG_P1] == 0xc0u);

    /* An external low on the two consumer bank bits changes pins only. */
    reset(&fixture.cpu, false);
    memset(&capture, 0, sizeof(capture));
    fixture.code[0] = 0xf0u;
    fixture.cpu.mSFR[REG_ACC] = 0x55u;
    em8051_sfr_write(&fixture.cpu, EM8051_SAB_PORT_P1, 0xc0u);
    CHECK(em8051_sab_port_drive(&fixture.cpu, EM8051_SAB_PORT_P1,
                                0xc0u, 0));
    CHECK(em8051_sab_port_get_pins(&fixture.cpu,
                                   EM8051_SAB_PORT_P1, &pins));
    CHECK((pins & 0xc0u) == 0);
    run_instructions(&fixture, 1);
    CHECK(capture.count == 1u);
    CHECK(capture.records[0].p1_latch == 0xc0u);
}

static int gIrqTraceCount;

static void count_irq_trace(const struct em8051_sab_irq_trace_record *aRecord,
                            void *aUser)
{
    (void)aRecord;
    (void)aUser;
    gIrqTraceCount++;
}

static void test_external_drive_has_no_irq_or_edge_semantics(void)
{
    struct fixture fixture;
    uint16_t pending;
    uint8_t tcon;
    uint8_t ircon;

    setup_fixture(&fixture, EM8051_VARIANT_SAB80535);
    gIrqTraceCount = 0;
    em8051_set_sab_irq_trace(&fixture.cpu, count_irq_trace, NULL);
    pending = fixture.cpu.mSABIrqPending;
    tcon = fixture.cpu.mSFR[REG_TCON];
    ircon = fixture.cpu.mSFR[EM8051_SAB_SFR_IRCON - 0x80u];
    CHECK(em8051_sab_port_drive(&fixture.cpu, EM8051_SAB_PORT_P1,
                                0xffu, 0));
    CHECK(em8051_sab_port_drive(&fixture.cpu, EM8051_SAB_PORT_P3,
                                0xffu, 0xffu));
    CHECK(em8051_sab_port_release(&fixture.cpu,
                                  EM8051_SAB_PORT_P1, 0xffu));
    CHECK(fixture.cpu.mSABIrqPending == pending);
    CHECK(fixture.cpu.mSFR[REG_TCON] == tcon);
    CHECK(fixture.cpu.mSFR[EM8051_SAB_SFR_IRCON - 0x80u] == ircon);
    CHECK(gIrqTraceCount == 0);
}

int main(void)
{
    test_reset_resolution_and_api_safety();
    test_quasi_bidirectional_resolution();
    test_ordinary_byte_and_bit_reads_use_pins();
    test_all_documented_byte_rmw_handlers();
    test_all_documented_bit_rmw_handlers();
    test_p4_p5_rmw_and_callback_compatibility();
    test_classic_callback_behavior_is_unchanged();
    test_movx_backing_dptr_and_ri_context();
    test_legacy_callbacks_and_pre_callback_snapshot();
    test_save_change_access_restore_and_external_force();
    test_external_drive_has_no_irq_or_edge_semantics();

    if (gFailures != 0)
    {
        fprintf(stderr, "SLC-010 port/MOVX tests failed: %d failure(s)\n",
                gFailures);
        return EXIT_FAILURE;
    }
    printf("SLC-010 port/MOVX tests passed\n");
    return EXIT_SUCCESS;
}

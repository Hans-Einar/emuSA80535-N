/* Copyright 2026 Hans-Einar. MIT; see LICENSE. */
#include <assert.h>
#include <emusa80535/contracts/cpu.h>
#include <emusa80535/emu8051.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define INIT(x)                                                                                    \
    do {                                                                                           \
        memset(&(x), 0, sizeof(x));                                                                \
        (x).struct_size = sizeof(x);                                                               \
        (x).api_version = 1;                                                                       \
    } while (0)
static struct emuif_cpu_api api;
struct fixture {
    struct emuif_cpu *cpu;
    unsigned buses, ports, signals;
    emuif_status bus_error, port_error, signal_error;
    int bad_result, reenter;
    struct emuif_bus_access access;
    struct emuif_port_output port;
    struct emuif_signal_output signal;
    uint64_t last_sequence;
};
static void ordered(struct fixture *f, uint64_t sequence) {
    assert(sequence > f->last_sequence);
    f->last_sequence = sequence;
}
static emuif_status bus(void *context, const struct emuif_bus_access *a,
                        struct emuif_bus_result *r) {
    struct fixture *f = context;
    f->buses++;
    f->access = *a;
    ordered(f, a->sequence);
    if (f->reenter) {
        struct emuif_snapshot s;
        INIT(s);
        struct emuif_reset reset_request;
        struct emuif_cycle_result result;
        struct emuif_cpu_api queried;
        INIT(reset_request);
        INIT(result);
        INIT(queried);
        assert(api.snapshot(f->cpu, &s) == EMUIF_BUSY);
        assert(api.reset(f->cpu, &reset_request) == EMUIF_BUSY);
        assert(api.cycle(f->cpu, &result) == EMUIF_BUSY);
        assert(emusa80535_cpu_query_v1(&queried) == EMUIF_OK);
        if (f->reenter == 2)
            api.destroy(f->cpu);
    }
    r->value = a->direction == EMUIF_ACCESS_WRITE ? a->value : 0x5a;
    if (f->bad_result)
        r->reserved[0] = 1;
    return f->bus_error;
}
static emuif_status port(void *context, const struct emuif_port_output *p) {
    struct fixture *f = context;
    f->ports++;
    f->port = *p;
    ordered(f, p->sequence);
    return f->port_error;
}
static emuif_status signal_out(void *context, const struct emuif_signal_output *s) {
    struct fixture *f = context;
    f->signals++;
    f->signal = *s;
    ordered(f, s->sequence);
    return f->signal_error;
}
static void start(struct fixture *f, unsigned variant) {
    struct emuif_cpu_config c;
    struct emuif_cpu_hooks h;
    memset(f, 0, sizeof(*f));
    INIT(c);
    INIT(h);
    c.variant = variant;
    c.oscillator_hz = 12000000;
    h.context = f;
    h.bus_access = bus;
    h.port_output = port;
    h.signal_output = signal_out;
    assert(api.create(&c, &h, &f->cpu) == EMUIF_OK);
}
static void load(struct fixture *f, const uint8_t *program, size_t size) {
    uint8_t *rom = calloc(65536, 1);
    struct emuif_reset r;
    INIT(r);
    r.seed = 123;
    assert(rom);
    memcpy(rom, program, size);
    assert(api.reset(f->cpu, &r) == EMUIF_OK);
    assert(api.load_code(f->cpu, rom, 65536) == EMUIF_OK);
    free(rom);
}
static struct emuif_snapshot snapshot(struct fixture *f) {
    struct emuif_snapshot s;
    INIT(s);
    assert(api.snapshot(f->cpu, &s) == EMUIF_OK);
    return s;
}
static emuif_status cycle(struct fixture *f) {
    struct emuif_cycle_result r;
    INIT(r);
    return api.cycle(f->cpu, &r);
}
static void step(struct fixture *f) {
    do {
        assert(cycle(f) == EMUIF_OK);
    } while (!snapshot(f).safe_boundary);
}
static uint8_t peek(struct fixture *f, unsigned space, unsigned address) {
    struct emuif_memory_request r;
    uint8_t out = 0;
    INIT(r);
    r.space = space;
    r.address = address;
    r.count = 1;
    assert(api.peek(f->cpu, &r, &out, 1) == EMUIF_OK);
    return out;
}
extern void emu_cpu_test_limits(struct emuif_cpu *, uint64_t, uint64_t, uint64_t);
static void validation(void) {
    struct emuif_cpu_api before;
    struct emuif_cpu *cpu = (void *)1;
    struct emuif_cpu_config c;
    struct emuif_cpu_hooks h;
    uint32_t tiny = 4;
    struct fixture f;
    struct emuif_cycle_result out, copy;
    INIT(before);
    before.flags = 1;
    assert(emusa80535_cpu_query_v1(&before) == EMUIF_INVALID);
    assert(before.flags == 1);
    assert(emusa80535_cpu_query_v1((void *)&tiny) == EMUIF_INVALID);
    INIT(c);
    INIT(h);
    c.oscillator_hz = 1;
    assert(api.create((void *)&tiny, &h, &cpu) == EMUIF_INVALID && cpu == NULL);
    assert(api.create(&c, &h, &cpu) == EMUIF_INVALID);
    h.bus_access = bus;
    h.port_output = port;
    h.signal_output = signal_out;
    c.variant = 3;
    assert(api.create(&c, &h, &cpu) == EMUIF_UNSUPPORTED);
    c.variant = 0;
    c.oscillator_hz = 0;
    assert(api.create(&c, &h, &cpu) == EMUIF_RANGE);
    start(&f, 2);
    INIT(out);
    copy = out;
    assert(api.cycle(f.cpu, &out) == EMUIF_STATE && !memcmp(&copy, &out, sizeof(out)));
    assert(api.load_code(f.cpu, (const uint8_t *)&tiny, 4) == EMUIF_RANGE);
    api.destroy(f.cpu);
    api.destroy(NULL);
}
static void bus_and_ports(void) {
    const uint8_t code[] = {0x75, 0x90, 0x33, 0x75, 0x90, 0x33, 0x90, 0x12,
                            0x34, 0xe0, 0xf0, 0x78, 0x66, 0xe2, 0xf2};
    struct fixture f, g;
    struct emuif_snapshot s;
    unsigned i;
    start(&f, 2);
    start(&g, 2);
    load(&f, code, sizeof(code));
    load(&g, code, sizeof(code));
    f.reenter = 1;
    step(&f);
    assert(f.ports == 1 && f.port.previous_latch == 255 && f.port.latch == 0x33 &&
           f.port.changed_mask == 0xcc);
    step(&f);
    assert(f.ports == 2 && !f.port.changed_mask);
    step(&f);
    step(&f);
    assert(f.buses == 1 && f.access.address == 0x1234 && f.access.pc == 9 &&
           f.access.port_latches[1] == 0x33 && snapshot(&f).a == 0x5a);
    step(&f);
    assert(f.buses == 2 && f.access.direction == 1 && f.access.value == 0x5a);
    step(&f);
    step(&f);
    assert(f.access.address == 0x0066);
    step(&f);
    assert(f.buses == 4);
    assert(snapshot(&g).cycle == 0 && !g.buses && !g.ports);
    s = snapshot(&f);
    for (i = 0; i < 8; i++)
        step(&g);
    assert(snapshot(&g).cycle == s.cycle && snapshot(&g).pc == s.pc);
    load(&f, code, sizeof(code));
    step(&f);
    assert(f.port.sequence > g.last_sequence);
    api.destroy(f.cpu);
    api.destroy(g.cpu);
}
static void faults(void) {
    const uint8_t prefix[] = {0x74, 0x55};
    const uint8_t cases[][3] = {{0xe0, 0, 0},       {0xf0, 0, 0},       {0xc5, 0x90, 0},
                                {0xd5, 0x90, 0xfe}, {0x10, 0x90, 0xfe}, {0xa5, 0, 0},
                                {0xe5, 0xe0, 0}};
    unsigned i;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        struct fixture f;
        uint8_t rom[5];
        struct emuif_snapshot before, after;
        struct emuif_cycle_result result, copy;
        start(&f, 2);
        memcpy(rom, prefix, 2);
        memcpy(rom + 2, cases[i], 3);
        load(&f, rom, sizeof(rom));
        step(&f);
        before = snapshot(&f);
        f.bus_error = EMUIF_IO;
        f.port_error = EMUIF_IO;
        INIT(result);
        copy = result;
        assert(api.cycle(f.cpu, &result) == (i < 5 ? EMUIF_IO : EMUIF_FAULT));
        assert(!memcmp(&copy, &result, sizeof(copy)));
        after = snapshot(&f);
        assert(after.pc == before.pc && after.a == before.a && after.cycle == before.cycle &&
               after.instructions == before.instructions && !after.safe_boundary &&
               after.state == EMUIF_STATE_FAULTED);
        assert(cycle(&f) == EMUIF_STATE);
        f.bus_error = f.port_error = 0;
        load(&f, prefix, 2);
        step(&f);
        assert(snapshot(&f).state == EMUIF_STATE_PAUSED);
        api.destroy(f.cpu);
    }
    {
        struct fixture f;
        const uint8_t code[] = {0xe0};
        start(&f, 2);
        load(&f, code, 1);
        f.bad_result = 1;
        assert(cycle(&f) == EMUIF_FAULT && f.buses == 1);
        api.destroy(f.cpu);
    }
}
static void inspection(void) {
    struct fixture f;
    struct emuif_memory_request m;
    struct emuif_decode_request d;
    struct emuif_decoded out[3], copy[3];
    uint8_t bytes[3] = {0xaa, 0xbb, 0xcc};
    const uint8_t code[] = {0x74, 0x55, 0x75, 0x90, 0x33, 0};
    unsigned i;
    start(&f, 2);
    load(&f, code, sizeof(code));
    INIT(m);
    m.space = EMUIF_SPACE_XDATA;
    m.count = 1;
    assert(api.peek(f.cpu, &m, bytes, 3) == EMUIF_UNSUPPORTED && bytes[0] == 0xaa);
    m.space = EMUIF_SPACE_SFR;
    m.address = 0x83;
    m.count = 2;
    assert(api.peek(f.cpu, &m, bytes, 3) == EMUIF_UNSUPPORTED && bytes[0] == 0xaa);
    m.space = EMUIF_SPACE_CODE;
    m.address = 65535;
    m.count = 2;
    assert(api.peek(f.cpu, &m, bytes, 3) == EMUIF_RANGE);
    assert(!f.buses && !f.ports);
    INIT(d);
    d.address = 2;
    d.instruction_offset = -1;
    d.count = 1;
    INIT(out[0]);
    assert(api.decode(f.cpu, &d, out, sizeof(out[0]), 3) == EMUIF_OK && !out[0].valid);
    d.address = 0;
    d.instruction_offset = 0;
    d.count = 3;
    for (i = 0; i < 3; i++)
        INIT(out[i]);
    assert(api.decode(f.cpu, &d, out, sizeof(out[0]), 3) == EMUIF_OK && out[0].size == 2 &&
           out[1].address == 2 && out[2].address == 5);
    d.address = 2;
    d.instruction_offset = -1;
    d.count = 1;
    assert(api.decode(f.cpu, &d, out, sizeof(out[0]), 3) == EMUIF_OK && out[0].valid &&
           out[0].address == 0);
    memcpy(copy, out, sizeof(out));
    d.byte_offset = -65537;
    assert(api.decode(f.cpu, &d, out, sizeof(out[0]), 3) == EMUIF_INVALID &&
           !memcmp(copy, out, sizeof(out)));
    assert(peek(&f, EMUIF_SPACE_CODE, 1) == 0x55);
    step(&f);
    assert(snapshot(&f).a == 0x55);
    api.destroy(f.cpu);
    start(&f, 0);
    load(&f, code, sizeof(code));
    INIT(m);
    m.space = EMUIF_SPACE_IRAM;
    m.address = 128;
    m.count = 1;
    assert(api.peek(f.cpu, &m, bytes, 3) == EMUIF_RANGE);
    api.destroy(f.cpu);
}
static void inputs_sleep_uart(void) {
    struct fixture f;
    struct emuif_input input;
    struct emuif_cycle_result r;
    unsigned i;
    const uint8_t idle[] = {0x75, 0x87, 1};
    const uint8_t pd[] = {0x75, 0x87, 2};
    const uint8_t uart[] = {0x75, 0x89, 0x20, 0x75, 0x8d, 0xff, 0x75, 0x8b, 0xff, 0x75, 0x98, 0xd8,
                            0x75, 0x87, 0x80, 0x75, 0x88, 0x40, 0x75, 0x99, 0x5a, 0x75, 0x87, 0x81};
    start(&f, 2);
    load(&f, idle, sizeof(idle));
    INIT(input);
    input.kind = EMUIF_INPUT_PORT;
    input.index = 1;
    input.value = 0x12;
    input.mask = 255;
    input.order = 1;
    assert(api.apply_input(f.cpu, &input) == EMUIF_OK);
    assert(api.apply_input(f.cpu, &input) == EMUIF_STALE);
    input.order++;
    input.index = 0;
    assert(api.apply_input(f.cpu, &input) == EMUIF_UNSUPPORTED);
    input.kind = EMUIF_INPUT_INTERRUPT;
    input.index = 6;
    input.value = 1;
    input.mask = 1;
    assert(api.apply_input(f.cpu, &input) == EMUIF_OK);
    input.order++;
    input.kind = EMUIF_INPUT_RELEASE;
    input.index = 262;
    input.value = 0;
    assert(api.apply_input(f.cpu, &input) == EMUIF_OK);
    input.order++;
    input.kind = EMUIF_INPUT_ADC;
    input.index = 7;
    input.value = 65535;
    input.mask = 65535;
    assert(api.apply_input(f.cpu, &input) == EMUIF_OK);
    step(&f);
    for (i = 0; i < 20; i++)
        assert(cycle(&f) == EMUIF_OK);
    assert(snapshot(&f).idle && snapshot(&f).instructions == 1 && snapshot(&f).cycle == 22);
    load(&f, pd, sizeof(pd));
    input.cycle = 0;
    assert(api.apply_input(f.cpu, &input) == EMUIF_STALE);
    step(&f);
    INIT(r);
    assert(api.cycle(f.cpu, &r) == EMUIF_UNSUPPORTED && !r.progressed && r.power_down);
    load(&f, uart, sizeof(uart));
    for (i = 0; i < 8; i++)
        step(&f);
    input.order++;
    input.kind = EMUIF_INPUT_UART_RX;
    input.index = 0;
    input.value = 0x155;
    input.mask = 511;
    input.cycle = snapshot(&f).cycle;
    assert(api.apply_input(f.cpu, &input) == EMUIF_OK);
    input.order++;
    assert(api.apply_input(f.cpu, &input) == EMUIF_BUSY);
    for (i = 0; i < 220; i++)
        assert(cycle(&f) == EMUIF_OK);
    assert(f.signals == 1 && f.signal.kind == EMUIF_OUTPUT_UART_FRAME && f.signal.value == 0x15a &&
           f.signal.width == 9 && f.signal.cycle > f.signal.start_cycle);
    api.destroy(f.cpu);
    start(&f, 2);
    load(&f, uart, sizeof(uart));
    f.signal_error = EMUIF_IO;
    for (i = 0; i < 240; i++) {
        emuif_status status = cycle(&f);
        if (status != EMUIF_OK) {
            assert(status == EMUIF_IO);
            break;
        }
    }
    assert(i < 240 && f.signals == 1 && !snapshot(&f).safe_boundary);
    assert(cycle(&f) == EMUIF_STATE);
    api.destroy(f.cpu);
}
static bool aborted(const struct em8051 *cpu, void *context) {
    (void)cpu;
    return *(bool *)context;
}
static void limits(void) {
    struct fixture f;
    const uint8_t code[] = {0xe0};
    struct em8051 core;
    struct em8051_run_result r;
    bool stop = true;
    start(&f, 2);
    load(&f, code, 1);
    emu_cpu_test_limits(f.cpu, UINT64_MAX, 0, 0);
    assert(cycle(&f) == EMUIF_EXHAUSTED && !f.buses);
    load(&f, code, 1);
    emu_cpu_test_limits(f.cpu, 0, 0, UINT64_MAX);
    assert(cycle(&f) == EMUIF_EXHAUSTED && !f.buses);
    api.destroy(f.cpu);
    memset(&core, 0, sizeof(core));
    core.mTickDelay = 2;
    em8051_set_execution_abort(&core, aborted, &stop);
    assert(em8051_run(&core, 1, &r) == EM8051_STOP_ABORTED && r.machine_cycles == 0 &&
           core.mTickDelay == 2);
    assert(em8051_run_until_pc(&core, 42, 1, &r) == EM8051_STOP_ABORTED);
    assert(em8051_step_instruction(&core, &r) == EM8051_STOP_ABORTED);
}

static void oversized_records(void) {
    struct {
        struct emuif_cpu_api value;
        unsigned char tail[17];
    } query;
    struct {
        struct emuif_snapshot value;
        unsigned char tail[17];
    } snap;
    struct {
        struct emuif_cycle_result value;
        unsigned char tail[17];
    } result;
    struct fixture f;
    struct emuif_decode_request request;
    struct emuif_decoded record;
    unsigned char elements[2 * (sizeof(record) + 1)], original[sizeof(elements)];
    const uint8_t code[] = {0, 0};
    unsigned i;
    memset(&query, 0xa6, sizeof(query));
    INIT(query.value);
    query.value.struct_size = sizeof(query);
    assert(emusa80535_cpu_query_v1(&query.value) == EMUIF_OK);
    for (i = 0; i < sizeof(query.tail); i++)
        assert(query.tail[i] == 0xa6);
    start(&f, 2);
    load(&f, code, sizeof(code));
    memset(&snap, 0xa6, sizeof(snap));
    INIT(snap.value);
    snap.value.struct_size = sizeof(snap);
    assert(api.snapshot(f.cpu, &snap.value) == EMUIF_OK);
    for (i = 0; i < sizeof(snap.tail); i++)
        assert(snap.tail[i] == 0xa6);
    snap.value.flags = 1;
    {
        unsigned char before[sizeof(snap)];
        memcpy(before, &snap, sizeof(snap));
        assert(api.snapshot(f.cpu, &snap.value) == EMUIF_INVALID &&
               !memcmp(before, &snap, sizeof(snap)));
    }
    snap.value.flags = 0;
    snap.value.reserved = 1;
    assert(api.snapshot(f.cpu, &snap.value) == EMUIF_INVALID && snap.value.reserved == 1);
    memset(&result, 0xa6, sizeof(result));
    INIT(result.value);
    result.value.struct_size = sizeof(result);
    result.value.api_version = 2;
    assert(api.cycle(f.cpu, &result.value) == EMUIF_VERSION && snapshot(&f).cycle == 0);
    result.value.api_version = 1;
    assert(api.cycle(f.cpu, &result.value) == EMUIF_OK);
    for (i = 0; i < sizeof(result.tail); i++)
        assert(result.tail[i] == 0xa6);
    memset(elements, 0xa6, sizeof(elements));
    INIT(record);
    record.struct_size = sizeof(record) + 1;
    memcpy(elements, &record, sizeof(record));
    memcpy(elements + sizeof(record) + 1, &record, sizeof(record));
    INIT(request);
    request.count = 2;
    assert(api.decode(f.cpu, &request, (void *)elements, sizeof(record) + 1, 2) == EMUIF_OK);
    assert(elements[sizeof(record)] == 0xa6 && elements[sizeof(elements) - 1] == 0xa6);
    record.flags = 1;
    memcpy(elements + sizeof(record) + 1, &record, sizeof(record));
    memcpy(original, elements, sizeof(elements));
    assert(api.decode(f.cpu, &request, (void *)elements, sizeof(record) + 1, 2) == EMUIF_INVALID &&
           !memcmp(original, elements, sizeof(elements)));
    api.destroy(f.cpu);
    start(&f, 2);
    {
        const uint8_t movx[] = {0xe0};
        load(&f, movx, 1);
    }
    f.reenter = 2;
    assert(cycle(&f) == EMUIF_FAULT && !snapshot(&f).safe_boundary);
    api.destroy(f.cpu);
}
struct abort_uart_capture {
    bool stop;
    unsigned starts, ends;
};
static void abort_uart(const struct em8051_sab_uart_trace_record *record, void *context) {
    struct abort_uart_capture *capture = context;
    if (record->event == EM8051_SAB_UART_TRACE_TX_START)
        capture->starts++;
    if (record->event == EM8051_SAB_UART_TRACE_TX_END) {
        capture->ends++;
        capture->stop = true;
    }
}
static void uart_pending_fault(void) {
    struct em8051 cpu;
    struct abort_uart_capture capture = {false, 0, 0};
    unsigned i;
    memset(&cpu, 0, sizeof(cpu));
    assert(em8051_init_variant(&cpu, EM8051_VARIANT_SAB80535) == 0);
    reset(&cpu, true);
    cpu.mSFR[REG_PCON] = 0x81;
    cpu.mSFR[REG_SCON] = 0xd0;
    cpu.mSFR[REG_TMOD] = 0x20;
    cpu.mSFR[REG_TH1] = 255;
    cpu.mSFR[REG_TL1] = 255;
    cpu.mSFR[REG_TCON] = 0x40;
    em8051_set_execution_abort(&cpu, aborted, &capture.stop);
    em8051_set_sab_uart_trace(&cpu, abort_uart, &capture);
    em8051_sfr_write(&cpu, 0x99, 0x12);
    for (i = 0; i < 32 && !capture.starts; i++)
        tick(&cpu);
    assert(capture.starts == 1);
    em8051_sfr_write(&cpu, 0x99, 0x34);
    assert(cpu.mSABUartTxPending);
    for (i = 0; i < 200 && !capture.stop; i++)
        tick(&cpu);
    assert(capture.stop && capture.ends == 1 && capture.starts == 1 && cpu.mSABUartTxPending &&
           cpu.mSABUartTxActive);
    {
        uint64_t cycles = cpu.mMachineCycleCount;
        tick(&cpu);
        assert(cpu.mMachineCycleCount == cycles && capture.starts == 1);
    }
}
static void idle_interrupt_wake(void) {
    struct fixture f;
    struct emuif_input input;
    struct emuif_snapshot before, after;
    const uint8_t code[] = {0x75, 0xa8, 0x81, 0x75, 0x88, 0x01, 0x75, 0x87, 0x01};
    start(&f, 2);
    load(&f, code, sizeof(code));
    step(&f);
    step(&f);
    step(&f);
    before = snapshot(&f);
    assert(before.idle);
    INIT(input);
    input.order = 1;
    input.cycle = before.cycle;
    input.kind = EMUIF_INPUT_INTERRUPT;
    input.index = 0;
    input.mask = 1;
    input.value = 0;
    assert(api.apply_input(f.cpu, &input) == EMUIF_OK);
    assert(cycle(&f) == EMUIF_OK);
    after = snapshot(&f);
    assert(!after.idle && !after.safe_boundary && after.pc == 3 &&
           after.instructions == before.instructions);
    assert(cycle(&f) == EMUIF_OK);
    after = snapshot(&f);
    assert(after.safe_boundary && after.pc == 3 && after.instructions == before.instructions);
    api.destroy(f.cpu);
}

static void input_admission_order(void) {
    struct fixture f;
    struct emuif_input input;
    unsigned i;
    const uint8_t code[] = {0x75, 0x87, 1};
    start(&f, 2);
    load(&f, code, sizeof(code));
    INIT(input);
    input.kind = EMUIF_INPUT_PORT;
    input.index = 1;
    input.mask = 255;
    input.value = 0x55;
    for (i = 0; i < 50; i++)
        assert(cycle(&f) == EMUIF_OK);
    input.cycle = 50;
    input.order = 2;
    assert(api.apply_input(f.cpu, &input) == EMUIF_OK);
    input.order = 1;
    assert(api.apply_input(f.cpu, &input) == EMUIF_STALE);
    input.order = 3;
    input.index = 6;
    assert(api.apply_input(f.cpu, &input) == EMUIF_RANGE);
    for (i = 0; i < 50; i++)
        assert(cycle(&f) == EMUIF_OK);
    input.cycle = 100;
    input.order = 1;
    input.index = 1;
    assert(api.apply_input(f.cpu, &input) == EMUIF_OK);
    load(&f, code, sizeof(code));
    input.cycle = 0;
    input.order = 2;
    assert(api.apply_input(f.cpu, &input) == EMUIF_STALE);
    input.order = 3;
    assert(api.apply_input(f.cpu, &input) == EMUIF_OK);
    api.destroy(f.cpu);
}

struct checked_core {
    struct em8051 core;
    bool stop;
};
static void abort_exception(struct em8051 *cpu, int exception_code) {
    struct checked_core *checked = (struct checked_core *)cpu;
    (void)exception_code;
    checked->stop = true;
}
static void reti_exception_fault(void) {
    unsigned variant;
    for (variant = 0; variant < 3; variant += 2) {
        struct checked_core checked;
        uint8_t code[1] = {0x32};
        struct em8051 *cpu = &checked.core;
        memset(&checked, 0, sizeof(checked));
        assert(em8051_init_variant(cpu, (enum em8051_variant)variant) == 0);
        reset(cpu, true);
        cpu->mCodeMem = code;
        cpu->mCodeMemMaxIdx = 0;
        cpu->mSFR[REG_SP] = 9;
        cpu->mSFR[REG_ACC] = 1;
        cpu->mLowerData[8] = 0x34;
        cpu->mLowerData[9] = 0x12;
        cpu->except = abort_exception;
        cpu->mInterruptActive = 1;
        cpu->int_a[0] = 0;
        cpu->int_sp[0] = 9;
        cpu->mSABIrqDepth = 1;
        cpu->mSABIrqInService = 1;
        cpu->mSABIrqSavedACC[0] = 0;
        cpu->mSABIrqSavedSP[0] = 9;
        em8051_set_execution_abort(cpu, aborted, &checked.stop);
        tick(cpu);
        assert(checked.stop && cpu->mPC == 0 && cpu->mSFR[REG_SP] == 7 &&
               cpu->mMachineCycleCount == 0 && cpu->mInstructionCount == 0);
        assert(cpu->mInterruptActive == 1 && cpu->mSABIrqDepth == 1 && cpu->mSABIrqInService == 1);
    }
}

static void port_profiles(void) {
    const uint8_t addresses[] = {0x80, 0x90, 0xa0, 0xb0, 0xe8, 0xf8};
    unsigned variant, p;
    for (variant = 0; variant < 3; variant++) {
        struct fixture f;
        uint8_t code[18];
        unsigned ports = variant == 2 ? 6 : 4;
        for (p = 0; p < ports; p++) {
            code[3 * p] = 0x75;
            code[3 * p + 1] = addresses[p];
            code[3 * p + 2] = (uint8_t)p;
        }
        start(&f, variant);
        load(&f, code, ports * 3);
        for (p = 0; p < ports; p++) {
            step(&f);
            assert(f.ports == p + 1 && f.port.port == p && f.port.latch == p);
        }
        assert(!f.signals);
        api.destroy(f.cpu);
    }
}
int main(void) {
    INIT(api);
    assert(emusa80535_cpu_query_v1(&api) == EMUIF_OK);
    assert(api.capabilities == (EMUIF_CPU_CAP_PORT_LATCH | EMUIF_CPU_CAP_UART_FRAME));
    validation();
    port_profiles();
    reti_exception_fault();
    input_admission_order();
    oversized_records();
    uart_pending_fault();
    idle_interrupt_wake();
    bus_and_ports();
    faults();
    inspection();
    inputs_sleep_uart();
    limits();
    puts("CPU API1 facade: validation, isolation, MOVX, callbacks, faults, decode, inputs, sleep, "
         "UART, exhaustion PASS");
    return 0;
}

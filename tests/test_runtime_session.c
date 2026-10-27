/* Native session, scheduler and wire conformance. Copyright 2026 Hans-Einar. MIT. */
#include "../emu_json_internal.h"
#include "../emu_session_internal.h"
#include "../emu_sha_internal.h"
#include <assert.h>
#include <emusa80535/contracts/debug.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define INIT(x)                                                                                    \
    do                                                                                             \
    {                                                                                              \
        memset(&(x), 0, sizeof(x));                                                                \
        (x).struct_size = sizeof(x);                                                               \
        (x).api_version = 1;                                                                       \
    } while (0)
struct board
{
    struct emuif_board_host host;
    unsigned boundaries, accesses, ports, resets, stimuli;
    uint64_t cycle, last_seq;
    uint8_t memory[65536];
};
static struct board *board;
static unsigned creates, destroys;
static bool access_fault, stimulus_fault, schedule_test, reentrant;
static bool reset_fault, wake_test, inspect_destroy, signal_schedule, inspect_deep;
static unsigned signal_count;
/* Board ABI permits required to report its fixed minimum output capacity. */
static unsigned inspect_required_mode;
static emuif_status access_error = EMUIF_IO;
static struct emuif_session *session;
static struct emuif_cpu_api cpu;
static struct emuif_board_api api;
static uint8_t code[65536];
static emuif_status create(const struct emuif_board_host *h, const struct emuif_board_create *c,
                           void **p)
{
    (void)c;
    board = calloc(1, sizeof *board);
    assert(board);
    board->host = *h;
    *p = board;
    creates++;
    return 0;
}
static void destroy(void *p)
{
    destroys++;
    free(p);
    board = NULL;
}
static emuif_status reset(void *p, const struct emuif_board_reset *r)
{
    struct board *b = p;
    unsigned n = b->resets + 1;
    struct emuif_board_host h = b->host;
    (void)r;
    if (reset_fault)
        return EMUIF_IO;
    memset(b, 0, sizeof *b);
    b->host = h;
    b->resets = n;
    return 0;
}
static emuif_status boundary(void *p, const struct emuif_board_boundary *r)
{
    struct board *b = p;
    b->boundaries++;
    b->cycle = r->cycle;
    if (wake_test && r->cycle == 0)
    {
        struct emuif_input i;
        INIT(i);
        i.kind = EMUIF_INPUT_INTERRUPT;
        i.index = 0;
        i.value = 0;
        i.mask = 1;
        i.cycle = 20;
        assert(!b->host.schedule_input(b->host.context, &i));
    }
    if (schedule_test && r->cycle == 0)
    {
        struct emuif_input i;
        INIT(i);
        i.kind = EMUIF_INPUT_ADC;
        i.index = 0;
        i.mask = 65535;
        i.value = 123;
        i.cycle = 100;
        assert(!b->host.schedule_input(b->host.context, &i));
        i.cycle = 2;
        assert(!b->host.schedule_input(b->host.context, &i));
    }
    return 0;
}
static emuif_status access(void *p, const struct emuif_bus_access *a, struct emuif_bus_result *r)
{
    struct board *b = p;
    b->accesses++;
    assert(a->sequence > b->last_seq);
    b->last_seq = a->sequence;
    if (reentrant)
    {
        struct emuif_progress v;
        INIT(v);
        assert(emusa80535_session_pump_v1(session, 1, &v) == EMUIF_BUSY);
        assert(emusa80535_debug_close_v1(session, 1) == EMUIF_BUSY);
    }
    if (access_fault)
        return access_error;
    if (a->direction)
        b->memory[a->address] = a->value;
    r->value = b->memory[a->address];
    return 0;
}
static emuif_status port(void *p, const struct emuif_port_output *a)
{
    struct board *b = p;
    assert(a->sequence > b->last_seq);
    b->last_seq = a->sequence;
    b->ports++;
    return 0;
}
static emuif_status signal_out(void *p, const struct emuif_signal_output *a)
{
    struct board *b = p;
    signal_count++;
    if (signal_schedule)
    {
        struct emuif_input i;
        INIT(i);
        i.kind = EMUIF_INPUT_ADC;
        i.mask = 65535;
        i.cycle = a->cycle;
        assert(b->host.schedule_input(b->host.context, &i) == EMUIF_RANGE);
        i.cycle++;
        assert(!b->host.schedule_input(b->host.context, &i));
    }
    return 0;
}
static emuif_status peek(const void *p, const struct emuif_memory_request *r, uint8_t *out,
                         uint32_t cap)
{
    const struct board *b = p;
    if (r->count > cap)
        return EMUIF_CAPACITY;
    memcpy(out, b->memory + r->address, r->count);
    return 0;
}
static emuif_status validate(const void *p, const struct emuif_board_stimulus *r)
{
    (void)p;
    return strcmp(r->schema, "fixture.v1") ? EMUIF_UNSUPPORTED : EMUIF_OK;
}
static emuif_status apply(void *p, const struct emuif_board_stimulus *r)
{
    struct board *b = p;
    struct emuif_input i;
    b->stimuli++;
    if (stimulus_fault)
        return EMUIF_RANGE;
    INIT(i);
    i.kind = EMUIF_INPUT_PORT;
    i.index = 1;
    i.mask = 255;
    i.value = 0;
    i.cycle = r->cycle + 1;
    return b->host.schedule_input(b->host.context, &i);
}
static emuif_status inspect(const void *p, const struct emuif_board_inspect *r,
                            struct emuif_board_inspection *out)
{
    const struct board *b = p;
    int n;
    (void)r;
    if (inspect_destroy)
        emusa80535_session_destroy_v1(session);
    if (inspect_deep)
    {
        unsigned k;
        size_t pos = 0;
        for (k = 0; k < 14; k++)
        {
            memcpy(out->response_utf8 + pos, "{\"x\":", 5);
            pos += 5;
        }
        out->response_utf8[pos++] = '0';
        for (k = 0; k < 14; k++)
            out->response_utf8[pos++] = '}';
        out->written = (uint32_t)pos;
        out->required = (uint32_t)pos;
        return 0;
    }
    n = snprintf((char *)out->response_utf8, out->capacity, "{\n\"accesses\":%u,\"stimuli\":%u\n}",
                 b->accesses, b->stimuli);
    out->written = (uint32_t)n;
    out->required = inspect_required_mode == 1   ? 4096
                    : inspect_required_mode == 2 ? (uint32_t)n - 1
                    : inspect_required_mode == 3 ? 4097
                                                 : (uint32_t)n;
    return 0;
}
static struct emuif_session_config config(void)
{
    struct emuif_session_config c;
    INIT(c);
    c.variant = 2;
    c.oscillator_hz = 11059200;
    c.code = code;
    c.code_bytes = 65536;
    c.cpu_api = &cpu;
    c.board_api = &api;
    memset(c.module_sha256, 'a', 64);
    return c;
}
static void setup(void)
{
    INIT(cpu);
    assert(!emusa80535_cpu_query_v1(&cpu));
    INIT(api);
    api.abi_major = 1;
    api.platform = 1;
    api.capabilities = 7;
    strcpy(api.module_id, "fixture");
    strcpy(api.module_version, "1");
    memset(api.build_id, 'b', 40);
    api.create = create;
    api.destroy = destroy;
    api.reset = reset;
    api.boundary = boundary;
    api.access = access;
    api.port_output = port;
    api.signal_output = signal_out;
    api.peek = peek;
    api.validate_stimulus = validate;
    api.apply_stimulus = apply;
    api.inspect = inspect;
}
static uint32_t request_id;
static uint64_t generation;
static char sid[33];
static struct json_document document;
static char response_bytes[65537];
static emuif_status request(uint64_t connection, const char *text, uint32_t capacity)
{
    struct emuif_json_request q;
    struct emuif_json_response r;
    emuif_status e;
    INIT(q);
    INIT(r);
    q.bytes = (const uint8_t *)text;
    q.byte_count = (uint32_t)strlen(text);
    r.bytes = (uint8_t *)response_bytes;
    r.capacity = capacity;
    e = emusa80535_debug_request_v1(session, connection, &q, &r);
    if (e == EMUIF_CAPACITY)
    {
        assert(r.required == 65536 && r.written == 0);
        return e;
    }
    assert(!e);
    response_bytes[r.written] = 0;
    if (r.written)
    {
        const char *path = getenv("WIRE_OUTPUT");
        assert(json_document_parse(&document, response_bytes, r.written, true));
        if (path)
        {
            FILE *f = fopen(path, "ab");
            assert(f);
            fwrite(response_bytes, 1, r.written, f);
            fputc('\n', f);
            fclose(f);
        }
    }
    return 0;
}
static bool successful(void)
{
    int t = json_member(&document, 0, "success");
    return document.tokens[t].end - document.tokens[t].start == 4;
}
static void error_is(const char *code_name)
{
    char got[32];
    assert(!successful());
    assert(json_string(&document,
                       json_member(&document, json_member(&document, 0, "error"), "code"), got,
                       sizeof got));
    assert(!strcmp(got, code_name));
}
static int body(void)
{
    return json_member(&document, 0, "body");
}
static uint64_t field64(const char *key)
{
    uint64_t v;
    assert(json_u64(&document, json_member(&document, body(), key), &v));
    return v;
}
static void command(const char *cmd, const char *extra)
{
    char q[10000];
    snprintf(q, sizeof q,
             "{\"type\":\"request\",\"id\":%u,\"command\":\"%s\",\"arguments\":{\"sessionId\":\"%"
             "s\"%s%s}}",
             ++request_id, cmd, sid, extra ? "," : "", extra ? extra : "");
    request(1, q, 65536);
}
static void mutate(const char *cmd, const char *extra)
{
    char args[9000];
    snprintf(args, sizeof args, "\"generation\":\"%" PRIu64 "\"%s%s", generation, extra ? "," : "",
             extra ? extra : "");
    command(cmd, args);
}
static void open_client(uint64_t id, uint32_t role)
{
    struct emuif_client c;
    INIT(c);
    c.connection_id = id;
    c.role = role;
    c.may_terminate = 1;
    assert(!emusa80535_debug_open_v1(session, &c));
}
static void start(bool allow_fault)
{
    struct emuif_session_config c = config();
    assert(!emusa80535_session_create_v1(&c, &session));
    if (allow_fault)
        assert(!emu_session_allow_fault_inspection(session, 1));
    open_client(1, 1);
    request_id = 1;
    request(1,
            "{\"type\":\"request\",\"id\":1,\"command\":\"hello\",\"arguments\":{\"protocol\":{"
            "\"major\":2,\"minor\":1},\"contractVersion\":\"0.2.0\",\"requiredCapabilities\":[]}}",
            65536);
    assert(successful());
    request_id++;
    request(1,
            "{\"type\":\"request\",\"id\":2,\"command\":\"attach\",\"arguments\":{\"sessionId\":"
            "null,\"role\":\"controller\"}}",
            65536);
    assert(successful());
    assert(json_string(&document, json_member(&document, body(), "sessionId"), sid, sizeof sid));
    generation = field64("generation");
    assert(generation == 1);
}
static struct emuif_progress pump(unsigned n, emuif_status expected)
{
    struct emuif_progress p;
    INIT(p);
    assert(emusa80535_session_pump_v1(session, n, &p) == expected);
    return p;
}
static void finish(void)
{
    emusa80535_session_destroy_v1(session);
    session = NULL;
    assert(creates == destroys);
}
static void validation_tests(void)
{
    struct emuif_session_config c = config();
    unsigned before = creates;
    struct emuif_session *s = (void *)1;
    const char *bad[] = {"{\"x\":1 2}", "{\"x\":1,\"x\":2}", "{\"x\":\"\\ud800\"}"};
    size_t i;
    for (i = 0; i < 3; i++)
    {
        c.board_config_utf8 = (const uint8_t *)bad[i];
        c.board_config_bytes = (uint32_t)strlen(bad[i]);
        assert(emusa80535_session_create_v1(&c, &s) == EMUIF_INVALID && s == NULL);
    }
    c.board_config_utf8 = (const uint8_t *)"{\0}";
    c.board_config_bytes = 3;
    assert(emusa80535_session_create_v1(&c, &s) == EMUIF_INVALID);
    assert(creates == before);
    c = config();
    api.required_cpu_capabilities = EMUIF_CPU_CAP_UART_FRAME;
    c.variant = 0;
    assert(emusa80535_session_create_v1(&c, &s) == EMUIF_UNSUPPORTED);
    api.required_cpu_capabilities = 0;
    c = config();
    c.api_version = 2;
    assert(emusa80535_session_create_v1(&c, &s) == EMUIF_VERSION);
}
static void execution_tests(void)
{
    struct emuif_progress p;
    uint64_t revision;
    unsigned boundaries;
    const uint8_t program[] = {0x90, 0x12, 0x34, 0x74, 0x05, 0xf0,
                               0xe0, 0x75, 0x90, 0xaa, 0x80, 0xfe};
    memset(code, 0, sizeof code);
    memcpy(code, program, sizeof program);
    schedule_test = true;
    start(true);
    assert(board->boundaries == 1);
    boundaries = board->boundaries;
    command("status", NULL);
    command("status", NULL);
    assert(board->boundaries == boundaries);
    mutate("boardInput", "\"cycle\":\"1\",\"schema\":\"fixture.v1\",\"payload\":{}");
    assert(successful() && field64("order") == 3);
    mutate("replaceCodeBreakpoints", "\"addresses\":[0]");
    assert(successful());
    mutate("resume", NULL);
    p = pump(20, 0);
    assert(p.cycles == 0 && p.stop_reason == 4);
    command("status", NULL);
    revision = field64("stopRevision");
    mutate("step", "\"maxCycles\":1");
    assert(successful() && field64("cycle") == 2 && field64("instructions") == 1);
    assert(board->stimuli == 1 && board->boundaries == 3);
    mutate("resume", NULL);
    reentrant = true;
    p = pump(20, 0);
    reentrant = false;
    assert(p.cycles >= 20 && p.cycles <= 36);
    assert(board->accesses == 2 && board->ports == 1 && board->memory[0x1234] == 5);
    assert(board->boundaries == board->cycle + 1);
    pump(100, 0); /* cycle100/order1 follows cycle2/order2 without stale rejection. */
    mutate("pause", NULL);
    assert(successful());
    assert(field64("stopRevision") > revision);
    mutate("boardInspect", "\"schema\":\"fixture.v1\",\"payload\":{}");
    assert(successful());
    mutate("readMemory", "\"space\":\"XDATA\",\"address\":4660,\"count\":1");
    assert(successful());
    mutate("decodeCode", "\"address\":0,\"byteOffset\":0,\"instructionOffset\":0,\"count\":5");
    assert(successful());
    mutate("reset", "\"seed\":9,\"entry\":0");
    assert(successful() && field64("generation") == 2 && field64("cycle") == 0);
    generation = 2;
    assert(board->boundaries == 1 && board->resets == 2);
    mutate("boardInput", "\"cycle\":\"1\",\"schema\":\"fixture.v1\",\"payload\":{}");
    assert(successful() && field64("order") > 4);
    finish();
    schedule_test = false;
}
static void fault_tests(void)
{
    struct emuif_progress p;
    memset(code, 0, sizeof code);
    code[0] = 0xe0;
    start(true);
    access_fault = true;
    mutate("resume", NULL);
    p = pump(10, EMUIF_IO);
    assert(p.state == EMUIF_STATE_FAULTED && p.cycles == 0 && board->accesses == 1);
    command("status", NULL);
    assert(successful());
    mutate("boardInspect", "\"schema\":\"fixture.v1\",\"payload\":{}");
    assert(successful());
    mutate("resume", NULL);
    error_is("STATE");
    mutate("decodeCode", "\"address\":0,\"byteOffset\":0,\"instructionOffset\":0,\"count\":1");
    error_is("STATE");
    assert(board->accesses == 1);
    access_fault = false;
    mutate("reset", "\"seed\":0,\"entry\":0");
    assert(successful());
    generation = 2;
    mutate("resume", NULL);
    pump(2, 0);
    assert(board->accesses == 1);
    finish();
}
static void idle_tests(void)
{
    memset(code, 0, sizeof code);
    code[0] = 0x43;
    code[1] = 0x87;
    code[2] = 1;
    start(false);
    mutate("step", "\"maxCycles\":10");
    assert(successful() && field64("instructions") == 1);
    mutate("step", "\"maxCycles\":7");
    assert(successful() && field64("instructions") == 1);
    {
        char reason[32];
        assert(json_string(&document, json_member(&document, body(), "reason"), reason,
                           sizeof reason) &&
               !strcmp(reason, "cycle-budget"));
    }
    finish();
}
static void queue_tests(void)
{
    unsigned i;
    start(false);
    for (i = 0; i < 1024; i++)
    {
        mutate("boardInput",
               "\"cycle\":\"18446744073709551615\",\"schema\":\"fixture.v1\",\"payload\":{}");
        assert(successful());
    }
    mutate("boardInput", "\"cycle\":null,\"schema\":\"fixture.v1\",\"payload\":{}");
    error_is("CAPACITY");
    mutate("reset", "\"seed\":0,\"entry\":0");
    assert(successful());
    generation = 2;
    mutate("boardInput", "\"cycle\":null,\"schema\":\"fixture.v1\",\"payload\":{}");
    assert(successful() && field64("order") == 1025);
    stimulus_fault = true;
    mutate("resume", NULL);
    pump(2, EMUIF_RANGE);
    assert(board->stimuli == 1);
    stimulus_fault = false;
    finish();
}
static void protocol_tests(void)
{
    char text[1000];
    struct emuif_client c;
    uint64_t revision;
    unsigned boundaries;
    start(false);
    command("status", NULL);
    revision = field64("stopRevision");
    boundaries = board->boundaries;
    snprintf(text, sizeof text,
             "{\"type\":\"request\",\"id\":%u,\"command\":\"resume\",\"arguments\":{\"sessionId\":"
             "\"%s\",\"generation\":\"1\"}}",
             request_id + 1, sid);
    assert(request(1, text, 12) == EMUIF_CAPACITY);
    command("status", NULL);
    assert(field64("stopRevision") == revision && board->boundaries == boundaries);
    mutate("reset", "\"seed\":0,\"entry\":0");
    assert(successful());
    mutate("resume", NULL);
    error_is("STALE");
    generation = 2;
    open_client(2, 0);
    request(2,
            "{\"type\":\"request\",\"id\":1,\"command\":\"hello\",\"arguments\":{\"protocol\":{"
            "\"major\":2,\"minor\":1},\"contractVersion\":\"0.2.0\",\"requiredCapabilities\":[]}}",
            65536);
    request(2,
            "{\"type\":\"request\",\"id\":2,\"command\":\"attach\",\"arguments\":{\"sessionId\":"
            "null,\"role\":\"controller\"}}",
            65536);
    error_is("DENIED");
    assert(!emusa80535_debug_close_v1(session, 2));
    INIT(c);
    c.connection_id = 2;
    assert(emusa80535_debug_open_v1(session, &c) == EMUIF_STALE);
    mutate("replaceCodeBreakpoints", "\"addresses\":[0,0]");
    error_is("INVALID");
    assert(emusa80535_debug_close_v1(session, 1) == EMUIF_NOT_FOUND);
    finish();
    start(false);
    mutate("resume", NULL);
    assert(!emusa80535_debug_close_v1(session, 1));
    pump(1, 0);
    open_client(99, 1);
    request(99, "{\"type\":\"request\",\"id\":1,\"command\":\"unknown\",\"arguments\":{}}", 65536);
    assert(!response_bytes[0]);
    assert(emusa80535_debug_close_v1(session, 99) == EMUIF_NOT_FOUND);
    finish();
}
static void lifecycle_tests(void)
{
    struct emuif_session_config c = config();
    struct emuif_session *created = NULL;
    struct emuif_progress p;
    struct emuif_client cl;
    unsigned i;
    reset_fault = true;
    assert(emusa80535_session_create_v1(&c, &created) == EMUIF_IO && !created);
    reset_fault = false;
    assert(creates == destroys);
    memset(code, 0, sizeof code);
    start(false);
    assert(emu_session_allow_fault_inspection(session, 1) == EMUIF_STATE);
    INIT(p);
    p.flags = 1;
    assert(emusa80535_session_pump_v1(session, 1, &p) == EMUIF_INVALID);
    INIT(p);
    p.api_version = 2;
    assert(emusa80535_session_pump_v1(session, 1, &p) == EMUIF_VERSION);
    INIT(p);
    assert(emusa80535_session_pump_v1(session, 0, &p) == EMUIF_RANGE);
    INIT(cl);
    cl.connection_id = 42;
    cl.role = 2;
    assert(emusa80535_debug_open_v1(session, &cl) == EMUIF_INVALID);
    for (i = 2; i <= 8; i++)
        open_client(i, 0);
    cl.connection_id = 9;
    cl.role = 0;
    assert(emusa80535_debug_open_v1(session, &cl) == EMUIF_CAPACITY);
    for (i = 2; i <= 8; i++)
        assert(!emusa80535_debug_close_v1(session, i));
    for (i = 9; i <= 8192; i++)
    {
        cl.connection_id = UINT64_MAX - i;
        assert(!emusa80535_debug_open_v1(session, &cl));
        assert(!emusa80535_debug_close_v1(session, cl.connection_id));
    }
    cl.connection_id = 99;
    assert(emusa80535_debug_open_v1(session, &cl) == EMUIF_CAPACITY);
    cl.connection_id = 2;
    assert(emusa80535_debug_open_v1(session, &cl) == EMUIF_STALE);
    command("status", NULL);
    assert(successful());
    reset_fault = true;
    mutate("reset", "\"seed\":0,\"entry\":0");
    error_is("FAULT");
    reset_fault = false;
    generation = 2;
    mutate("boardInspect", "\"schema\":\"fixture.v1\",\"payload\":{}");
    error_is("STATE");
    mutate("reset", "\"seed\":0,\"entry\":0");
    assert(successful());
    generation = 3;
    inspect_destroy = true;
    mutate("boardInspect", "\"schema\":\"fixture.v1\",\"payload\":{}");
    error_is("FAULT");
    inspect_destroy = false;
    finish();
    /* Power-down freezes CPU and board time; reset is the only recovery. */
    memset(code, 0, sizeof code);
    code[0] = 0x43;
    code[1] = 0x87;
    code[2] = 2;
    start(false);
    mutate("resume", NULL);
    p = pump(10, 0);
    assert(p.stop_reason == 8 && p.cycles == 2 && board->boundaries == 3);
    mutate("resume", NULL);
    error_is("STATE");
    finish();
    /* A scheduled interrupt wakes IDLE through the CPU's normal arbitration. */
    memset(code, 0, sizeof code);
    code[0] = 2;
    code[2] = 0x20;
    code[3] = 0x32;
    {
        const uint8_t program[] = {0x75, 0xa8, 0x81, 0x75, 0x88, 1, 0x43, 0x87, 1, 0x80, 0xfe};
        memcpy(code + 0x20, program, sizeof program);
    }
    wake_test = true;
    start(false);
    mutate("resume", NULL);
    p = pump(40, 0);
    assert(p.instructions > 4 && board->boundaries == board->cycle + 1);
    wake_test = false;
    finish();
}
static void uart_timing_test(void)
{
    const uint8_t program[] = {0x75, 0x89, 0x20, 0x75, 0x8d, 0xff, 0x75, 0x8b,
                               0xff, 0x75, 0x98, 0xd8, 0x75, 0x87, 0x80, 0x75,
                               0x88, 0x40, 0x75, 0x99, 0x5a, 0x75, 0x87, 0x81};
    memset(code, 0, sizeof code);
    memcpy(code, program, sizeof program);
    signal_count = 0;
    signal_schedule = true;
    start(false);
    mutate("resume", NULL);
    pump(300, 0);
    assert(signal_count == 1);
    mutate("boardInput", "\"cycle\":null,\"schema\":\"fixture.v1\",\"payload\":{}");
    assert(successful() && field64("order") == 2);
    finish();
    signal_schedule = false;
}
static void json_tests(void)
{
    const char *valid = "{\"x\":\"\\u0000\",\"\\u0000\":1,\"\\u0000x\":2}";
    const char *duplicate = "{\"\\u0000\":1,\"\\u0000\":2}";
    struct emuif_session_config c = config();
    struct emuif_session *created;
    assert(json_document_parse(&document, valid, strlen(valid), true));
    assert(!json_document_parse(&document, duplicate, strlen(duplicate), true));
    c.board_config_utf8 = (const uint8_t *)valid;
    c.board_config_bytes = (uint32_t)strlen(valid);
    assert(!emusa80535_session_create_v1(&c, &created));
    emusa80535_session_destroy_v1(created);
}
static void termination_and_prefix_tests(void)
{
    struct emuif_client c;
    struct emuif_progress p;
    struct
    {
        struct emuif_progress p;
        uint8_t tail[32];
    } large;
    unsigned k;
    memset(code, 0, sizeof code);
    start(false);
    inspect_deep = true;
    mutate("boardInspect", "\"schema\":\"fixture.v1\",\"payload\":{}");
    error_is("CAPACITY");
    inspect_deep = false;
    command("status", NULL);
    assert(successful());
    mutate("resume", NULL);
    memset(&large, 0xa5, sizeof large);
    INIT(large.p);
    large.p.struct_size = sizeof large;
    assert(!emusa80535_session_pump_v1(session, 1, &large.p));
    for (k = 0; k < sizeof large.tail; k++)
        assert(large.tail[k] == 0xa5);
    mutate("terminate", NULL);
    assert(successful());
    assert(emusa80535_debug_close_v1(session, 1) == EMUIF_NOT_FOUND);
    INIT(c);
    c.connection_id = 9;
    assert(emusa80535_debug_open_v1(session, &c) == EMUIF_STATE);
    INIT(p);
    assert(emusa80535_session_pump_v1(session, 1, &p) == EMUIF_STATE);
    finish();
    memset(code, 0, sizeof code);
    code[0] = 0xe0;
    start(false);
    access_fault = true;
    mutate("resume", NULL);
    pump(1, EMUIF_IO);
    access_fault = false;
    mutate("terminate", NULL);
    assert(response_bytes[0] == 0);
    assert(emusa80535_debug_close_v1(session, 1) == EMUIF_NOT_FOUND);
    assert(emusa80535_debug_open_v1(session, &c) == EMUIF_STATE);
    assert(emusa80535_session_pump_v1(session, 1, &p) == EMUIF_STATE);
    finish();
}
static void typed_fault_test(void)
{
    memset(code, 0, sizeof code);
    code[0] = 0xe0;
    start(false);
    access_fault = true;
    access_error = EMUIF_STATE;
    mutate("step", "\"maxCycles\":1");
    error_is("FAULT");
    mutate("resume", NULL);
    error_is("STATE");
    access_fault = false;
    access_error = EMUIF_IO;
    finish();
}
static void inspection_capacity_tests(void)
{
    unsigned mode;
    uint64_t cycle, instructions, revision;
    memset(code, 0, sizeof code);
    start(true);
    command("status", NULL);
    cycle = field64("cycle");
    instructions = field64("instructions");
    revision = field64("stopRevision");
    inspect_required_mode = 1;
    mutate("boardInspect", "\"schema\":\"fixture.v1\",\"payload\":{}");
    assert(successful());
    mutate("boardInspect", "\"schema\":\"fixture.v1\",\"payload\":{}");
    assert(successful());
    command("status", NULL);
    assert(successful() && field64("cycle") == cycle && field64("instructions") == instructions &&
           field64("stopRevision") == revision);
    assert(board->accesses == 0 && board->stimuli == 0 && board->boundaries == 1);
    finish();
    for (mode = 2; mode <= 3; ++mode)
    {
        start(true);
        inspect_required_mode = mode;
        mutate("boardInspect", "\"schema\":\"fixture.v1\",\"payload\":{}");
        error_is("FAULT");
        command("status", NULL);
        assert(successful());
        assert(field64("cycle") == 0 && board->accesses == 0 && board->boundaries == 1);
        finish();
    }
    inspect_required_mode = 0;
}
int main(void)
{
    setup();
    validation_tests();
    execution_tests();
    fault_tests();
    idle_tests();
    queue_tests();
    protocol_tests();
    lifecycle_tests();
    uart_timing_test();
    json_tests();
    termination_and_prefix_tests();
    typed_fault_test();
    inspection_capacity_tests();
    puts("runtime session tests passed");
    return 0;
}

/* Real native control3 coverage, reusing the board ABI fixture. MIT. */
#define main baseline_main
#include "test_runtime_session.c"
#undef main
#include <emusa80535/contracts/control3.h>
static struct emuif_control3_api control;
static uint64_t lease;
static uint32_t ids[16];
static uint64_t boundary_failure;
static emuif_status boundary3(void *p, const struct emuif_board_boundary *r)
{
    emuif_status e = boundary(p, r);
    return r->cycle && r->cycle == boundary_failure ? EMUIF_IO : e;
}
static void send3(uint64_t id, const char *cmd, const char *args)
{
    char q[65536];
    struct emuif_json_request req;
    struct emuif_json_response res;
    int n = snprintf(q, sizeof q,
                     "{\"type\":\"request\",\"id\":%u,\"command\":\"%s\",\"arguments\":%s}",
                     ++ids[id], cmd, args);
    INIT(req);
    INIT(res);
    req.bytes = (uint8_t *)q;
    req.byte_count = (uint32_t)n;
    res.bytes = (uint8_t *)response_bytes;
    res.capacity = 65536;
    assert(!control.request(session, id, &req, &res));
    response_bytes[res.written] = 0;
    if (res.written)
    {
        const char *path = getenv("CONTROL3_OUTPUT");
        assert(json_document_parse(&document, response_bytes, res.written, true));
        if (path)
        {
            FILE *f = fopen(path, "ab");
            assert(f);
            fwrite(response_bytes, 1, res.written, f);
            fputc('\n', f);
            fclose(f);
        }
    }
}
static void open3(uint64_t id, unsigned role)
{
    struct emuif_client c;
    INIT(c);
    c.connection_id = id;
    c.role = role;
    c.may_terminate = 1;
    assert(!control.open(session, &c));
    assert(!emu_session_client_ready(session, id));
    send3(id, "hello",
          "{\"protocol\":{\"major\":3,\"minor\":0},\"contractVersion\":\"0.3.0\","
          "\"requiredCapabilities\":[\"status\"]}");
    assert(successful());
    assert(emu_session_client_ready(session, id));
}
static void attach3(uint64_t id, bool controller)
{
    send3(id, "attach",
          controller ? "{\"sessionId\":null,\"role\":\"controller\"}"
                     : "{\"sessionId\":null,\"role\":\"observer\"}");
    if (successful())
    {
        assert(
            json_string(&document, json_member(&document, body(), "sessionId"), sid, sizeof sid));
        generation = field64("generation");
        if (controller)
            lease = field64("leaseId");
    }
}
static void cmd3(uint64_t id, const char *cmd, const char *extra)
{
    char args[16000];
    snprintf(args, sizeof args, "{\"sessionId\":\"%s\"%s%s}", sid, extra ? "," : "",
             extra ? extra : "");
    send3(id, cmd, args);
}
static void mut3(uint64_t id, const char *cmd, const char *extra)
{
    char args[15000];
    snprintf(args, sizeof args, "\"generation\":\"%" PRIu64 "\",\"leaseId\":\"%" PRIu64 "\"%s%s",
             generation, lease, extra ? "," : "", extra ? extra : "");
    cmd3(id, cmd, args);
}
static void create3(uint32_t mode)
{
    struct emuif_session_config c = config();
    assert(!emusa80535_session_create_v1(&c, &session));
    assert(!emu_session_set_execution_mode(session, mode));
    assert(!emu_session_allow_fault_inspection(session, 1));
    memset(ids, 0, sizeof ids);
    open3(1, 1);
    attach3(1, true);
    assert(successful());
    assert(emu_session_set_execution_mode(session, mode) == EMUIF_STATE);
}
static void native_advance(uint32_t budget, struct emuif_control_progress *p)
{
    struct emuif_control_advance a;
    INIT(a);
    INIT(*p);
    a.connection_id = 1;
    a.generation = generation;
    a.lease_id = lease;
    a.cycle_budget = budget;
    assert(!control.advance(session, &a, p));
}
static void core_tests3(void)
{
    uint64_t first;
    struct emuif_control_progress p;
    memset(code, 0, sizeof code);
    code[0] = 0xa4; /* MUL takes four cycles. */
    create3(0);
    first = lease;
    cmd3(1, "stopDetail", NULL);
    assert(successful());
    assert(field64("stopRevision") > 0);
    cmd3(1, "status", NULL);
    assert(field64("cycle") == 0);
    mut3(1, "resume", NULL);
    assert(successful());
    cmd3(1, "status", NULL);
    assert(field64("cycle") == 0); /* no hidden pump */
    mut3(1, "advance", "\"cycleBudget\":1");
    assert(successful());
    assert(field64("cycles") == 4);
    assert(field64("retiredInstructions") == 1);
    native_advance(2, &p);
    assert(p.cycles == 2 && p.instructions == 2 && p.retired_instructions == 2 &&
           p.state == EMUIF_STATE_RUNNING);
    mut3(1, "pause", NULL);
    assert(successful());
    cmd3(1, "decodeCode",
         "\"generation\":\"1\",\"address\":0,\"byteOffset\":0,\"instructionOffset\":0,\"count\":2");
    assert(successful());
    cmd3(1, "readMemory", "\"generation\":\"1\",\"space\":\"CODE\",\"address\":0,\"count\":2");
    assert(successful());
    mut3(1, "replaceCodeBreakpoints", "\"addresses\":[8]");
    assert(successful());
    mut3(1, "step", "\"maxCycles\":1");
    assert(successful());
    mut3(1, "boardInput", "\"cycle\":null,\"schema\":\"fixture.v1\",\"payload\":{}");
    assert(successful());
    assert(field64("parentStimulusId") > 0);
    cmd3(1, "boardInspect", "\"generation\":\"1\",\"schema\":\"fixture.v1\",\"payload\":{}");
    assert(successful());
    open3(2, 1);
    attach3(2, true);
    error_is("BUSY");
    attach3(2, false);
    assert(successful());
    mut3(2, "pause", NULL);
    error_is("DENIED");
    mut3(1, "releaseControl", NULL);
    assert(successful());
    assert(field64("leaseId") == 0);
    cmd3(2, "acquireControl", "\"generation\":\"1\"");
    assert(successful());
    lease = field64("leaseId");
    assert(lease > first);
    mut3(1, "resume", NULL);
    error_is("DENIED");
    mut3(2, "reset", "\"seed\":0,\"entry\":0");
    assert(successful());
    assert(field64("generation") == 2);
    mut3(2, "resume", NULL);
    error_is("STALE");
    generation = 2;
    mut3(2, "resume", NULL);
    assert(successful());
    cmd3(2, "detach", NULL);
    assert(successful());
    cmd3(1, "acquireControl", "\"generation\":\"2\"");
    assert(successful());
    lease = field64("leaseId");
    mut3(1, "advance", "\"cycleBudget\":1");
    assert(successful());
    mut3(1, "pause", NULL);
    assert(successful());
    mut3(1, "terminate", NULL);
    assert(successful());
    assert(!emu_session_client_open(session, 1));
    emusa80535_session_destroy_v1(session);
}
static void faults3(void)
{
    struct emuif_control_progress p;
    memset(code, 0, sizeof code);
    code[0] = 0xa4;
    api.boundary = boundary3;
    boundary_failure = 1;
    create3(0);
    mut3(1, "resume", NULL);
    native_advance(10, &p);
    assert(p.cycles == 1 && p.instructions == 1 && p.retired_instructions == 0 && p.state == 2 &&
           p.partial && !p.safe_boundary);
    emusa80535_session_destroy_v1(session);
    boundary_failure = 4;
    create3(0);
    mut3(1, "resume", NULL);
    native_advance(10, &p);
    assert(p.cycles == 4 && p.instructions == 1 && p.retired_instructions == 1 && p.state == 2 &&
           p.partial);
    emusa80535_session_destroy_v1(session);
    boundary_failure = 0;
    api.boundary = boundary;
    code[0] = 0xe0;
    access_fault = true;
    create3(0);
    mut3(1, "resume", NULL);
    mut3(1, "advance", "\"cycleBudget\":5");
    assert(successful());
    assert(field64("cycles") == 0 && field64("retiredInstructions") == 0);
    cmd3(1, "stopDetail", NULL);
    assert(successful());
    mut3(1, "reset", "\"seed\":0,\"entry\":0");
    assert(successful());
    generation = field64("generation");
    access_fault = false;
    emusa80535_session_destroy_v1(session);
}
static void errors3(void)
{
    char big[5000], args[6000];
    struct emuif_control_advance a;
    struct emuif_control_progress p;
    memset(code, 0, sizeof code);
    create3(0);
    INIT(a);
    INIT(p);
    a.connection_id = 1;
    a.generation = 1;
    a.lease_id = lease;
    a.cycle_budget = 0;
    assert(control.advance(session, &a, &p) == EMUIF_STATE);
    mut3(1, "resume", NULL);
    assert(control.advance(session, &a, &p) == EMUIF_RANGE);
    a.generation = 2;
    a.lease_id = 0;
    assert(control.advance(session, &a, &p) == EMUIF_STALE);
    a.generation = 1;
    assert(control.advance(session, &a, &p) == EMUIF_DENIED);
    cmd3(1, "traceConfig", NULL);
    error_is("UNSUPPORTED");
    cmd3(1, "tracePage",
         "\"destinationId\":0,\"epoch\":\"1\",\"afterRecordSequence\":\"0\",\"maxRecords\":16");
    error_is("UNSUPPORTED");
    cmd3(1, "outcomePage", "\"epoch\":\"1\",\"afterRecordSequence\":\"0\",\"maxRecords\":16");
    error_is("UNSUPPORTED");
    open3(2, 0);
    attach3(2, false);
    strcpy(sid, "ffffffffffffffffffffffffffffffff");
    cmd3(2, "traceConfig", NULL);
    error_is("UNSUPPORTED");
    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = 0;
    snprintf(args, sizeof args,
             "{\"sessionId\":\"%s\",\"generation\":\"1\",\"schema\":\"fixture.v1\",\"payload\":{"
             "\"x\":\"%s\"}}",
             sid, big);
    send3(2, "boardInspect", args);
    error_is("INVALID");
    assert(!emu_session_client_open(session, 2));
    open3(3, 1);
    attach3(3, false);
    mut3(3, "captureStart",
         "\"traceId\":1,\"mode\":0,\"contextId\":\"1\",\"entryPc\":0,\"maxRecords\":\"1\","
         "\"maxCycles\":\"1\",\"rearmOnReset\":false,\"stopCpu\":false");
    error_is("INVALID");
    assert(!emu_session_client_open(session, 3));
    open3(4, 1);
    attach3(4, false);
    cmd3(4, "readMemory",
         "\"generation\":\"18446744073709551616\",\"space\":\"CODE\",\"address\":0,\"count\":0");
    error_is("INVALID");
    emusa80535_session_destroy_v1(session);
    create3(1);
    mut3(1, "advance", "\"cycleBudget\":1");
    error_is("UNSUPPORTED");
    emusa80535_session_destroy_v1(session);
}

static emuif_status peripheral_failure(void *ctx, const struct emuif_signal_output *r)
{
    (void)ctx;
    (void)r;
    return EMUIF_IO;
}
static void peripheral3(void)
{
    const uint8_t uart[] = {0x75, 0x89, 0x20, 0x75, 0x8d, 0xff, 0x75, 0x8b, 0xff, 0x75, 0x98,
                            0xd8, 0x75, 0x87, 0x80, 0x75, 0x88, 0x40, 0x75, 0x99, 0x5a};
    unsigned offset, i;
    bool delayed = false;
    api.signal_output = peripheral_failure;
    for (offset = 0; offset < 4; offset++)
    {
        struct emuif_control_progress p;
        memset(code, 0xa4, sizeof code);
        memcpy(code, uart, sizeof uart);
        memset(code + sizeof uart, 0, offset);
        create3(0);
        for (i = 0; i < 7 + offset; i++)
        {
            mut3(1, "step", "\"maxCycles\":10");
            assert(successful());
        }
        mut3(1, "resume", NULL);
        for (i = 0; i < 256; i++)
        {
            native_advance(1, &p);
            if (p.state == EMUIF_STATE_FAULTED)
                break;
            assert(p.instructions == 1 && p.retired_instructions == 1 && p.cycles == 4);
        }
        assert(i < 256 && p.instructions == 1 && p.retired_instructions == 0 && p.partial &&
               !p.safe_boundary);
        if (p.cycles > 0)
            delayed = true;
        emusa80535_session_destroy_v1(session);
    }
    assert(delayed);
    api.signal_output = signal_out;
}
static void profiles3(void)
{
    struct emuif_json_request q;
    struct emuif_json_response r;
    struct emuif_client cl;
    uint64_t first;
    memset(code, 0, sizeof code);
    create3(0);
    first = lease;
    open_client(2, 1);
    request(2,
            "{\"type\":\"request\",\"id\":1,\"command\":\"hello\",\"arguments\":{\"protocol\":{"
            "\"major\":2,\"minor\":1},\"contractVersion\":\"0.2.0\",\"requiredCapabilities\":[]}}",
            65536);
    assert(successful());
    request(2,
            "{\"type\":\"request\",\"id\":2,\"command\":\"attach\",\"arguments\":{\"sessionId\":"
            "null,\"role\":\"controller\"}}",
            65536);
    error_is("BUSY");
    mut3(1, "releaseControl", NULL);
    assert(successful());
    request(2,
            "{\"type\":\"request\",\"id\":3,\"command\":\"attach\",\"arguments\":{\"sessionId\":"
            "null,\"role\":\"controller\"}}",
            65536);
    assert(successful());
    cmd3(1, "acquireControl", "\"generation\":\"1\"");
    error_is("BUSY");
    INIT(q);
    INIT(r);
    q.bytes = (const uint8_t *)"{}";
    q.byte_count = 2;
    r.bytes = (uint8_t *)response_bytes;
    r.capacity = 65536;
    assert(control.request(session, 2, &q, &r) == EMUIF_VERSION);
    assert(emusa80535_debug_request_v1(session, 1, &q, &r) == EMUIF_VERSION);
    assert(!emusa80535_debug_close_v1(session, 2));
    cmd3(1, "acquireControl", "\"generation\":\"1\"");
    assert(successful());
    lease = field64("leaseId");
    assert(lease > first + 1);
    assert(!control.close(session, 1));
    INIT(cl);
    cl.connection_id = 1;
    assert(control.open(session, &cl) == EMUIF_STALE);
    for (uint64_t id = 3; id <= 8192; id++)
    {
        cl.connection_id = id;
        assert(!control.open(session, &cl));
        assert(!control.close(session, id));
    }
    cl.connection_id = 8193;
    assert(control.open(session, &cl) == EMUIF_CAPACITY);
    cl.connection_id = 2;
    assert(control.open(session, &cl) == EMUIF_STALE);
    emusa80535_session_destroy_v1(session);
}
static void buffers3(void)
{
    struct
    {
        struct emuif_control_progress p;
        uint8_t tail[13];
    } out;
    struct emuif_control_advance a;
    struct emuif_json_request q;
    struct emuif_json_response r;
    char text[512];
    memset(code, 0, sizeof code);
    create3(0);
    snprintf(text, sizeof text,
             "{\"type\":\"request\",\"id\":3,\"command\":\"resume\",\"arguments\":{\"sessionId\":"
             "\"%s\",\"generation\":\"1\",\"leaseId\":\"%" PRIu64 "\"}}",
             sid, lease);
    INIT(q);
    INIT(r);
    q.bytes = (uint8_t *)text;
    q.byte_count = (uint32_t)strlen(text);
    r.bytes = (uint8_t *)response_bytes;
    r.capacity = 65535;
    assert(control.request(session, 1, &q, &r) == EMUIF_CAPACITY && r.required == 65536 &&
           r.written == 0);
    assert(emu_session_state(session) == EMUIF_STATE_PAUSED);
    mut3(1, "resume", NULL);
    assert(successful());
    memset(&out, 0xa5, sizeof out);
    INIT(out.p);
    out.p.struct_size = sizeof out;
    INIT(a);
    a.connection_id = 1;
    a.lease_id = lease;
    a.generation = 1;
    a.cycle_budget = 2;
    a.reserved = 1;
    assert(control.advance(session, &a, &out.p) == EMUIF_INVALID);
    a.reserved = 0;
    out.p.api_version = 2;
    assert(control.advance(session, &a, &out.p) == EMUIF_VERSION);
    out.p.api_version = 1;
    out.p.flags = 1;
    assert(control.advance(session, &a, &out.p) == EMUIF_INVALID);
    out.p.flags = 0;
    assert(!control.advance(session, &a, &out.p));
    assert(out.p.struct_size == sizeof out && out.p.cycle == 2 && out.p.retired_instructions == 2);
    for (unsigned i = 0; i < sizeof out.tail; i++)
        assert(out.tail[i] == 0xa5);
    emusa80535_session_destroy_v1(session);
}

static void unavailable3(void)
{
    static const struct
    {
        const char *cmd, *extra;
    } commands[] = {
        {"replaceTraceConfig", "\"expectedRevision\":\"0\",\"config\":{\"traces\":[],"
                               "\"destinations\":[],\"points\":[],\"gates\":[],\"watches\":[]}"},
        {"clearTrace", "\"destinationId\":1"},
        {"captureStart",
         "\"traceId\":1,\"mode\":0,\"contextId\":\"0\",\"entryPc\":0,\"maxRecords\":\"1\","
         "\"maxCycles\":\"1\",\"rearmOnReset\":false,\"stopCpu\":true"},
        {"captureStop", "\"captureId\":\"1\""},
        {"sinkOpen", "\"destinationId\":1,\"destinationToken\":\"out\",\"maxSegmentBytes\":"
                     "\"65536\",\"maxSegments\":1,\"maxTotalRecords\":\"1\""},
        {"sinkFlush", "\"destinationId\":1"},
        {"sinkClose", "\"destinationId\":1"}};
    create3(0);
    for (unsigned i = 0; i < sizeof commands / sizeof commands[0]; i++)
    {
        mut3(1, commands[i].cmd, commands[i].extra);
        error_is("UNSUPPORTED");
        assert(emu_session_client_open(session, 1));
    }
    cmd3(1, "captureStatus", "\"captureId\":\"1\"");
    error_is("UNSUPPORTED");
    cmd3(1, "sinkStatus", "\"destinationId\":1");
    error_is("UNSUPPORTED");
    mut3(1, "replaceTraceConfig",
         "\"expectedRevision\":\"0\",\"config\":{\"traces\":[],\"destinations\":[{"
         "\"destinationId\":1,\"kind\":0,\"capacity\":2,\"stopOnFull\":false}],\"points\":[],"
         "\"gates\":[],\"watches\":[]}");
    error_is("UNSUPPORTED");
    mut3(1, "captureStart",
         "\"traceId\":1,\"mode\":0,\"contextId\":\"0\",\"entryPc\":0,\"maxRecords\":\"1\","
         "\"maxCycles\":\"1\",\"rearmOnReset\":\"false\",\"stopCpu\":true");
    error_is("INVALID");
    assert(!emu_session_client_open(session, 1));
    emusa80535_session_destroy_v1(session);
}

static void stop_priority3(void)
{
    struct emuif_control_progress p;
    memset(code, 0, sizeof code);
    code[0] = 0x75;
    code[1] = 0x87;
    code[2] = 2;
    create3(0);
    mut3(1, "replaceCodeBreakpoints", "\"addresses\":[3]");
    assert(successful());
    mut3(1, "resume", NULL);
    native_advance(1, &p);
    assert(p.stop_reason == 5 && p.state == EMUIF_STATE_PAUSED && p.cycles == 2 &&
           p.retired_instructions == 1);
    cmd3(1, "stopDetail", NULL);
    assert(strstr(response_bytes, "power-down-unsupported"));
    emusa80535_session_destroy_v1(session);
    memset(code, 0, sizeof code);
    code[0] = 0xa4;
    create3(0);
    mut3(1, "replaceCodeBreakpoints", "\"addresses\":[1]");
    mut3(1, "resume", NULL);
    native_advance(10, &p);
    assert(p.stop_reason == 2 && p.cycles == 4 && p.retired_instructions == 1 && p.partial);
    cmd3(1, "stopDetail", NULL);
    assert(strstr(response_bytes, "breakpoint"));
    mut3(1, "step", "\"maxCycles\":1");
    assert(successful());
    assert(field64("cycle") == 5);
    emusa80535_session_destroy_v1(session);
    memset(code, 0, sizeof code);
    code[0] = 0xe0;
    access_fault = true;
    create3(0);
    mut3(1, "resume", NULL);
    native_advance(1, &p);
    assert(p.state == EMUIF_STATE_FAULTED);
    mut3(1, "terminate", NULL);
    assert(response_bytes[0] == 0 && !emu_session_client_open(session, 1) &&
           emu_session_state(session) == EMUIF_STATE_TERMINATED);
    emusa80535_session_destroy_v1(session);
    access_fault = false;
}
static void prefixes3(void)
{
    struct
    {
        struct emuif_control3_api api;
        unsigned char tail[17];
    } x;
    memset(&x, 0xa5, sizeof x);
    x.api.struct_size = sizeof x;
    x.api.api_version = 1;
    x.api.flags = 0;
    assert(!emusa80535_control_query_v3(&x.api));
    assert(x.api.struct_size == sizeof x);
    for (unsigned i = 0; i < sizeof x.tail; i++)
        assert(x.tail[i] == 0xa5);
    x.api.flags = 1;
    assert(emusa80535_control_query_v3(&x.api) == EMUIF_INVALID);
    x.api.flags = 0;
    x.api.api_version = 2;
    assert(emusa80535_control_query_v3(&x.api) == EMUIF_VERSION);
    x.api.api_version = 1;
    x.api.struct_size = 8;
    assert(emusa80535_control_query_v3(&x.api) == EMUIF_INVALID);
}
int main(void)
{
    setup();
    INIT(control);
    assert(!emusa80535_control_query_v3(&control));
    assert(control.open && control.request && control.close && control.advance);
    prefixes3();
    core_tests3();
    faults3();
    errors3();
    peripheral3();
    profiles3();
    buffers3();
    unavailable3();
    stop_priority3();
    puts("control3 native tests passed");
    return 0;
}

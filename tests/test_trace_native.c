/* Sized trace facade over the real shared engine, including adversarial limits. MIT. */
#include "../emu_trace_internal.h"
#include <assert.h>
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
extern void emu_trace_test_sequence(struct emu_trace_state *, uint32_t, uint64_t);
static struct em8051_debug_runtime *engine;
static struct emu_trace_state *state;
static struct emuif_trace_config config;
static struct emuif_trace_revision revision;
static struct emuif_trace_session traces[16];
static struct emuif_trace_destination dests[8];
static struct emuif_trace_point points[32];
static struct emuif_trace_gate gates[32];
static struct emuif_trace_watch watches[64];
static struct emuif_trace_page_request request;
static struct emuif_trace_page page;
static struct emuif_trace_record records[16];
static struct emuif_observation event;
static const char sid[] = "0123456789abcdef0123456789abcdef";
static void observe(void *context, const struct emuif_observation *o)
{
    emu_trace_observe(context, o);
}
static void record_init(struct emuif_trace_record *r)
{
    INIT(*r);
    INIT(r->event);
    INIT(r->event.old_value);
    INIT(r->event.new_value);
}
static void fixture(uint64_t coverage)
{
    struct emu_trace_metadata metadata = {0};
    unsigned i, j;
    memcpy(metadata.session_id, sid, sizeof sid);
    assert(!em8051_debug_runtime_create(&engine));
    state = emu_trace_create(engine, &metadata, coverage);
    assert(state);
    assert(!emu_debug_runtime_observer(engine, observe, state));
    INIT(config);
    INIT(revision);
    config.generation = 1;
#define ARRAY(field, values)                                                                       \
    INIT(config.field);                                                                            \
    config.field.elements = values;                                                                \
    config.field.element_size = sizeof values[0]
    ARRAY(traces, traces);
    ARRAY(destinations, dests);
    ARRAY(points, points);
    ARRAY(gates, gates);
    ARRAY(watches, watches);
#undef ARRAY
    for (i = 0; i < 16; i++)
    {
        INIT(traces[i]);
        traces[i].trace_id = i + 1;
        traces[i].destination_id = 1;
        traces[i].enabled = 1;
    }
    for (i = 0; i < 8; i++)
    {
        INIT(dests[i]);
        dests[i].destination_id = i + 1;
        dests[i].capacity = 64;
    }
    for (i = 0; i < 32; i++)
    {
        INIT(points[i]);
        INIT(points[i].selector);
        points[i].point_id = i + 1;
        points[i].enabled = 1;
        points[i].route_count = 1;
        points[i].trace_ids[0] = 1;
        INIT(gates[i]);
        INIT(gates[i].selector);
        gates[i].gate_id = i + 1;
    }
    for (i = 0; i < 64; i++)
    {
        INIT(watches[i]);
        watches[i].watch_id = i + 1;
        watches[i].enabled = 1;
        watches[i].space = EMUIF_SPACE_XDATA;
        watches[i].access_mask = EMUIF_OBS_ACCESS_WRITE;
        watches[i].address_first = watches[i].address_last = 0x1234;
        watches[i].action_count = 1;
        for (j = 0; j < 8; j++)
        {
            INIT(watches[i].actions[j]);
            INIT(watches[i].actions[j].condition);
            watches[i].actions[j].condition.width = 8;
        }
    }
    config.traces.count = config.destinations.count = config.points.count = 1;
    INIT(event);
    INIT(event.old_value);
    INIT(event.new_value);
    event.generation = 1;
    event.old_value.width = event.new_value.width = 8;
    event.kind = EMUIF_OBS_INSTRUCTION_RETIRE;
    event.instruction_id = 1;
    INIT(request);
    request.epoch = 1;
    request.destination_id = 1;
    request.max_records = 16;
    memcpy(request.session_id, sid, sizeof sid);
    INIT(page);
    page.records = records;
    page.capacity = 16;
    page.element_size = sizeof records[0];
    for (i = 0; i < 16; i++)
        record_init(&records[i]);
}
static void done(void)
{
    emu_trace_destroy(state);
    em8051_debug_runtime_destroy(engine);
}
static void replace(void)
{
    assert(!emu_trace_replace(state, &config, &revision));
    config.expected_revision = revision.revision;
}
static void emit(void) { assert(!emu_debug_runtime_observation(engine, &event, NULL)); }
static void read_page(void) { assert(!emu_trace_page(state, &request, &page)); }
static struct output output;
static void command(const char *name, const char *args)
{
    struct json_document *d = malloc(sizeof *d);
    assert(d);
    assert(json_document_parse(d, args, strlen(args), true));
    memset(&output, 0, sizeof output);
    assert(!emu_trace_command(state, name, d, 0, &output));
    free(d);
}
static void clear(void)
{
    command("clearTrace", "{\"destinationId\":1}");
    request.epoch++;
    request.after_record_sequence = 0;
}
static void atomic_and_prefixes(void)
{
    struct
    {
        struct emuif_trace_revision known;
        unsigned char tail[8];
    } large;
    struct
    {
        struct emuif_trace_session known;
        unsigned char tail[8];
    } element;
    struct
    {
        struct emuif_trace_record known;
        unsigned char tail[8];
    } slot;
    uint32_t short_prefix = 4;
    fixture(125);
    replace();
    emit();
    read_page();
    assert(page.written == 1);
    assert(emu_trace_replace(state, (void *)&short_prefix, &revision) == EMUIF_INVALID);
    config.expected_revision--;
    assert(emu_trace_replace(state, &config, &revision) == EMUIF_STALE);
    config.expected_revision++;
    points[0].selector.match_mask = 8;
    points[0].selector.space = EMUIF_SPACE_IRAM;
    assert(emu_trace_replace(state, &config, &revision) == EMUIF_UNSUPPORTED);
    read_page();
    assert(page.written == 1);
    points[0].selector.match_mask = 0;
    points[0].selector.space = 0;
    traces[0].destination_id = 99;
    assert(emu_trace_replace(state, &config, &revision) == EMUIF_INVALID);
    traces[0].destination_id = 1;
    watches[0].actions[7].condition.width = 0;
    config.watches.count = 1;
    assert(emu_trace_replace(state, &config, &revision) == EMUIF_INVALID);
    watches[0].actions[7].condition.width = 8;
    watches[0].actions[7].condition.api_version = 2;
    assert(emu_trace_replace(state, &config, &revision) == EMUIF_VERSION);
    watches[0].actions[7].condition.api_version = 1;
    watches[0].actions[0].api_version = 2;
    assert(emu_trace_replace(state, &config, &revision) == EMUIF_VERSION);
    watches[0].actions[0].api_version = 1;
    memset(&element, 0xa5, sizeof element);
    element.known = traces[0];
    element.known.struct_size = sizeof element;
    config.traces.elements = &element;
    config.traces.element_size = sizeof element;
    memset(&large, 0xa5, sizeof large);
    INIT(large.known);
    large.known.struct_size = sizeof large;
    assert(!emu_trace_replace(state, &config, &large.known));
    assert(large.tail[0] == 0xa5);
    config.expected_revision = large.known.revision;
    element.known.enabled = 0;
    emit();
    read_page();
    assert(page.written == 2); /* deep copy */
    memset(&slot, 0xa5, sizeof slot);
    record_init(&slot.known);
    slot.known.struct_size = sizeof slot;
    page.records = &slot.known;
    page.element_size = sizeof slot;
    page.capacity = 1;
    assert(emu_trace_page(state, &request, &page) == EMUIF_CAPACITY && page.required == 2 &&
           slot.tail[0] == 0xa5);
    request.max_records = 1;
    assert(!emu_trace_page(state, &request, &page));
    assert(page.written == 1 && slot.tail[7] == 0xa5 && slot.known.struct_size == sizeof slot);
    {
        unsigned char slots[2 * (sizeof(struct emuif_trace_record) + 1)];
        struct emuif_trace_record local;
        unsigned j;
        memset(slots, 0xa5, sizeof slots);
        record_init(&local);
        local.struct_size = sizeof local + 1;
        for (j = 0; j < 2; j++)
            memcpy(slots + j * (sizeof local + 1), &local, sizeof local);
        page.records = (void *)slots;
        page.capacity = 2;
        page.element_size = sizeof local + 1;
        request.max_records = 2;
        assert(!emu_trace_page(state, &request, &page));
        assert(page.written == 2);
        for (j = 0; j < 2; j++)
            assert(slots[(j + 1) * (sizeof local + 1) - 1] == 0xa5);
    }
    config.traces.elements = traces;
    config.traces.element_size = sizeof traces[0];
    config.traces.count = config.points.count = config.watches.count = 0;
    replace();
    config.traces.count = 1;
    assert(emu_trace_replace(state, &config, &revision) == EMUIF_INVALID);
    done();
}
static void ring_and_capture(void)
{
    struct emuif_capture_request q;
    struct emuif_capture_status status;
    fixture(125);
    dests[0].capacity = 2;
    replace();
    event.cycle = 1;
    emit();
    event.cycle = 2;
    emit();
    event.cycle = 3;
    emit();
    read_page();
    assert(page.written == 2 && page.overwritten == 2 && records[0].kind == EMUIF_RECORD_LOSS &&
           records[0].count == 2 && records[1].event.sequence == 3 &&
           page.cursor_state == EMUIF_CURSOR_EXPIRED);
    clear();
    dests[0].stop_on_full = 1;
    replace();
    emit();
    emit();
    emit();
    read_page();
    assert(page.written == 2 && page.dropped == 1 && emu_trace_stop_pending(state) == 2);
    emu_trace_applied_stop(state, 2);
    clear();
    dests[0].capacity = 64;
    dests[0].stop_on_full = 0;
    replace();
    INIT(q);
    INIT(status);
    q.generation = 1;
    q.trace_id = 1;
    q.max_records = 2;
    q.max_cycles = 100;
    q.stop_cpu = 1;
    assert(!emu_trace_capture(state, &q, &status));
    assert(status.state == EMUIF_SCOPE_BOUND);
    emit();
    emit();
    read_page();
    assert(page.written == 2);
    emit();
    read_page();
    assert(page.written == 2 && emu_trace_stop_pending(state) == 7 &&
           emu_trace_stop_source(state) == 8);
    command("captureStatus", "{\"captureId\":\"1\"}");
    assert(strstr(output.data, "\"records\":\"2\""));
    emu_trace_applied_stop(state, 7);
    q.max_records = 100;
    q.max_cycles = 5;
    assert(!emu_trace_capture(state, &q, &status));
    emu_trace_tick(state, 1, 5);
    event.cycle = 5;
    emit();
    event.generation = 2;
    event.cycle = 0;
    event.kind = EMUIF_OBS_RESET;
    emit();
    emu_trace_tick(state, 2, 2);
    command("captureStatus", "{\"captureId\":\"2\"}");
    assert(strstr(output.data, "\"cycles\":\"4\"") && strstr(output.data, "\"generation\":\"2\""));
    emu_trace_tick(state, 2, 3);
    assert(emu_trace_stop_pending(state) == 7 && emu_trace_stop_source(state) == 0 &&
           emu_trace_stop_instruction(state) == 0);
    emu_trace_applied_stop(state, 7);
    q.generation = 2;
    q.max_records = 1;
    q.max_cycles = 100;
    assert(!emu_trace_capture(state, &q, &status));
    emu_trace_test_sequence(state, 1, 0);
    event.cycle = 4;
    emit();
    command("captureStatus", "{\"captureId\":\"3\"}");
    assert(strstr(output.data, "\"state\":3") && strstr(output.data, "\"reason\":6"));
    done();
}
static void canonical_matching(void)
{
    unsigned i;
    fixture(127);
    config.points.count = 28;
    for (i = 0; i < 28; i++)
    {
        points[i].selector.match_mask = 1;
        points[i].selector.kind = i + 1;
        if (i == 3 || i == 4)
        {
            points[i].selector.match_mask |= 8;
            points[i].selector.space = EMUIF_SPACE_IRAM;
        }
    }
    replace();
    for (i = 1; i < 28; i++)
    {
        event.kind = i;
        event.space = EMUIF_SPACE_IRAM;
        event.access = EMUIF_OBS_ACCESS_WRITE;
        emit();
    }
    clear();
    config.points.count = 1;
    points[0].selector.match_mask = 8;
    points[0].selector.kind = 0;
    points[0].selector.space = EMUIF_SPACE_CODE;
    replace();
    event.kind = EMUIF_OBS_INSTRUCTION_RETIRE;
    event.space = EMUIF_SPACE_CODE;
    emit();
    read_page();
    assert(page.written == 0);
    clear();
    points[0].selector.space = EMUIF_SPACE_IRAM;
    config.watches.count = 1;
    watches[0].space = EMUIF_SPACE_IRAM;
    watches[0].address_first = 0x7f;
    watches[0].address_last = 0x80;
    watches[0].actions[0].kind = EMUIF_ACTION_ROUTE;
    watches[0].actions[0].route_count = 1;
    watches[0].actions[0].trace_ids[0] = 1;
    watches[0].actions[0].condition.present = 1;
    watches[0].actions[0].condition.operand = 1;
    watches[0].actions[0].condition.is_signed = 1;
    watches[0].actions[0].condition.constant = 255;
    replace();
    event.kind = EMUIF_OBS_ACCESS_RESULT;
    event.space = EMUIF_SPACE_IRAM;
    event.access = EMUIF_OBS_ACCESS_WRITE;
    event.new_value.known = 1;
    event.new_value.is_signed = 1;
    event.new_value.value = 255;
    event.address = 0x7f;
    emit();
    event.address = 0x80;
    emit();
    read_page();
    assert(page.written == 4 && records[1].event.kind == 28 && records[3].event.kind == 28);
    event.kind = EMUIF_OBS_ACCESS_ATTEMPT;
    emit();
    event.kind = EMUIF_OBS_ACCESS_RESULT;
    event.status = EMUIF_IO;
    emit();
    read_page();
    assert(page.written == 6); /* attempts/failures route but no watches */
    done();
}
static void gates_and_watch_limit(void)
{
    unsigned i;
    fixture(125);
    config.watches.count = 64;
    replace();
    event.kind = EMUIF_OBS_ACCESS_RESULT;
    event.space = EMUIF_SPACE_XDATA;
    event.access = EMUIF_OBS_ACCESS_WRITE;
    event.address = 0x1234;
    emit();
    assert(emu_trace_stop_pending(state) == 5);
    emu_trace_applied_stop(state, 5);
    memset(&output, 0, sizeof output);
    emu_trace_stop_detail(state, &output);
    assert(strstr(output.data, "\"watchId\":64"));
    clear();
    config.watches.count = 0;
    config.gates.count = 1;
    gates[0].enabled = 1;
    gates[0].action = 1;
    gates[0].timing = 0;
    gates[0].route_count = 1;
    gates[0].trace_ids[0] = 1;
    replace();
    event.kind = EMUIF_OBS_INSTRUCTION_RETIRE;
    emit();
    read_page();
    assert(page.written == 0);
    command("traceConfig", "{}");
    assert(
        strstr(output.data, "\"traces\":[{\"comment\":\"\",\"destinationId\":1,\"enabled\":false"));
    for (i = 0; i < 1; i++)
    { /* explicit replacement re-enables, terminal capture does not */
        traces[0].enabled = 1;
        gates[0].enabled = 0;
        replace();
    }
    done();
}
static void retention_and_suppression(void)
{
    unsigned i;
    struct emuif_capture_request q;
    struct emuif_capture_status status;
    fixture(125);
    config.traces.count = config.destinations.count = 2;
    traces[1].destination_id = 2;
    traces[0].interrupt_policy = traces[1].interrupt_policy = 1;
    points[0].route_count = 2;
    points[0].trace_ids[1] = 2;
    replace();
    event.kind = EMUIF_OBS_IRQ_ENTER;
    event.irq_depth = 1;
    emit();
    event.kind = EMUIF_OBS_INSTRUCTION_RETIRE;
    emit();
    emit();
    request.destination_id = 2;
    read_page();
    assert(page.written == 1);
    clear();
    request.destination_id = 2;
    request.epoch = 1;
    read_page();
    assert(page.written == 1); /* unrelated summary remains pending */
    event.kind = EMUIF_OBS_RETI;
    emit();
    read_page();
    assert(page.written == 3 && records[1].kind == EMUIF_RECORD_SUPPRESSION &&
           records[1].count == 2);
    request.destination_id = 1;
    request.epoch = 2;
    read_page();
    assert(page.written == 1 && records[0].kind == EMUIF_RECORD_EVENT);
    done();
    fixture(125);
    replace();
    emit();
    emit();
    emit();
    dests[0].capacity = 2;
    assert(emu_trace_replace(state, &config, &revision) == EMUIF_BUSY);
    dests[0].capacity = 3;
    replace();
    read_page();
    assert(page.written == 3);
    config.points.count = 0;
    replace();
    INIT(q);
    INIT(status);
    q.generation = 1;
    q.trace_id = 1;
    q.max_records = q.max_cycles = 100;
    for (i = 1; i <= 64; i++)
    {
        char args[64];
        assert(!emu_trace_capture(state, &q, &status) && status.capture_id == i);
        snprintf(args, sizeof args, "{\"captureId\":\"%u\"}", i);
        command("captureStop", args);
    }
    assert(emu_trace_capture(state, &q, &status) == EMUIF_EXHAUSTED);
    command("captureStatus", "{\"captureId\":\"1\"}");
    assert(strstr(output.data, "\"state\":4"));
    done();
    fixture(125);
    config.points.count = 0;
    for (i = 1; i <= 64; i++)
    {
        traces[0].trace_id = i;
        replace();
    }
    traces[0].trace_id = 65;
    assert(emu_trace_replace(state, &config, &revision) == EMUIF_EXHAUSTED);
    traces[0].trace_id = 1;
    assert(emu_trace_replace(state, &config, &revision) == EMUIF_INVALID);
    done();
}
static void explicit_stop_exhaustion(void)
{
    struct emuif_capture_request q;
    struct emuif_capture_status status;
    fixture(125);
    traces[0].interrupt_policy = 1;
    replace();
    INIT(q);
    INIT(status);
    q.generation = 1;
    q.trace_id = 1;
    q.max_records = q.max_cycles = 100;
    assert(!emu_trace_capture(state, &q, &status));
    event.kind = EMUIF_OBS_IRQ_ENTER;
    event.irq_depth = 1;
    emit();
    event.kind = EMUIF_OBS_INSTRUCTION_RETIRE;
    emit();
    emu_trace_test_sequence(state, 1, 0);
    command("captureStop", "{\"captureId\":\"1\"}");
    assert(strstr(output.data, "\"state\":3") && strstr(output.data, "\"reason\":6"));
    done();
}
static emuif_status mock_open(void *context, const struct emuif_sink_request *q,
                              const struct emu_trace_file_metadata *m, struct emuif_sink_status *v)
{
    return EMUIF_OK;
}
static emuif_status mock_exhausted(void *context, const struct emuif_trace_record *r)
{
    return EMUIF_EXHAUSTED;
}
static void file_exhaustion(void)
{
    struct emu_trace_file_provider provider = {0};
    struct emuif_capture_request q;
    struct emuif_capture_status status;
    fixture(125);
    provider.open = mock_open;
    provider.enqueue = mock_exhausted;
    emu_trace_set_provider(state, &provider);
    dests[0].kind = EMUIF_DEST_FILE;
    dests[0].capacity = 256;
    replace();
    INIT(q);
    INIT(status);
    q.generation = 1;
    q.trace_id = 1;
    q.max_records = 1;
    q.max_cycles = 100;
    assert(!emu_trace_capture(state, &q, &status));
    emit();
    command("captureStatus", "{\"captureId\":\"1\"}");
    assert(strstr(output.data, "\"state\":3") && strstr(output.data, "\"reason\":6"));
    assert(!emu_trace_stop_pending(state));
    done();
}
int main(void)
{
    explicit_stop_exhaustion();
    file_exhaustion();
    retention_and_suppression();
    atomic_and_prefixes();
    ring_and_capture();
    canonical_matching();
    gates_and_watch_limit();
    puts("canonical trace native tests passed");
    return 0;
}

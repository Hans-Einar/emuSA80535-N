/* Canonical sequencing and stable operational retention adversarial tests. MIT. */
#include "../emu_debug_observation_internal.h"
#include "../emu_outcome_internal.h"
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
static const char identity[] = "0123456789abcdef0123456789abcdef";
static void initialize_record(struct emuif_trace_record *r)
{
    INIT(*r);
    INIT(r->event);
    INIT(r->event.old_value);
    INIT(r->event.new_value);
}
static unsigned observed;
static struct emuif_observation latest;
static void observe(void *ctx, const struct emuif_observation *o)
{
    (void)ctx;
    observed++;
    latest = *o;
}
static void canonical(void)
{
    struct em8051_debug_runtime *runtime;
    struct emuif_observation o;
    uint64_t assigned;
    struct em8051_debug_trace_session trace = {0};
    struct em8051_debug_trace_destination dest = {0};
    struct em8051_debug_trace_point point = {0};
    struct em8051_debug_trace_record records[2];
    size_t read, remaining;
    assert(!em8051_debug_runtime_create(&runtime));
    assert(!emu_debug_runtime_observer(runtime, observe, NULL));
    dest.destination_id = 1;
    assert(!em8051_debug_runtime_replace_destinations(runtime, &dest, 1));
    trace.trace_id = 1;
    trace.destination_id = 1;
    trace.enabled = 1;
    assert(!em8051_debug_runtime_replace_traces(runtime, &trace, 1));
    point.point_id = 1;
    point.enabled = 1;
    point.trace_id_count = 1;
    point.trace_ids[0] = 1;
    point.selector.match_observation_kind = 1;
    point.selector.observation_kind = EMUIF_OBS_RX_DROP;
    assert(!em8051_debug_runtime_replace_points(runtime, &point, 1));
    INIT(o);
    INIT(o.old_value);
    INIT(o.new_value);
    o.old_value.width = o.new_value.width = 8;
    o.generation = UINT64_C(0x100000001);
    o.kind = EMUIF_OBS_RX_START;
    o.sequence = 999;
    assert(!emu_debug_runtime_observation(runtime, &o, &assigned));
    assert(assigned == 1 && latest.sequence == 1 && latest.generation == o.generation);
    o.kind = EMUIF_OBS_RX_DROP;
    assert(!emu_debug_runtime_observation(runtime, &o, &assigned));
    assert(assigned == 2 && observed == 2 && latest.kind == EMUIF_OBS_RX_DROP);
    assert(!em8051_debug_runtime_read_ring(runtime, 1, 0, records, 2, &read, &remaining));
    assert(read == 1 && !remaining && records[0].event.observation.kind == EMUIF_OBS_RX_DROP &&
           records[0].event.observation.sequence == 2 &&
           records[0].event.generation == o.generation);
    o.kind = EMUIF_OBS_RESET;
    o.generation++;
    assert(!emu_debug_runtime_observation(runtime, &o, &assigned));
    assert(assigned == 3 && latest.generation == o.generation);
    {
        uint32_t trace_id = 1;
        struct em8051_debug_watch watch = {0};
        struct em8051_debug_runtime_stop stop;
        unsigned before;
        assert(!em8051_debug_runtime_set_trace_enabled(runtime, &trace_id, 1, false));
        watch.id = 7;
        watch.enabled = 1;
        watch.address_space = EM8051_DEBUG_SPACE_IRAM_LOWER;
        watch.address_first = watch.address_last = 0x20;
        watch.access_mask = EM8051_DEBUG_ACCESS_WRITE;
        watch.action_count = 1;
        watch.actions[0].kind = EM8051_DEBUG_WATCH_STOP;
        assert(!em8051_debug_runtime_replace_watches(runtime, &watch, 1));
        o.kind = EMUIF_OBS_ACCESS_ATTEMPT;
        o.access = EMUIF_OBS_ACCESS_WRITE;
        o.space = EMUIF_SPACE_IRAM;
        o.address = 0x20;
        o.instruction_id = 1;
        o.access_id = 1;
        o.new_value.known = 1;
        o.new_value.value = 0x55;
        before = observed;
        assert(!emu_debug_runtime_observation(runtime, &o, &assigned));
        assert(observed == before + 1);
        o.kind = EMUIF_OBS_ACCESS_RESULT;
        o.status = EMUIF_IO;
        assert(!emu_debug_runtime_observation(runtime, &o, &assigned));
        assert(observed == before + 2);
        o.status = EMUIF_OK;
        assert(!emu_debug_runtime_observation(runtime, &o, &assigned));
        assert(observed == before + 4 && latest.kind == EMUIF_OBS_WATCH &&
               latest.source_event_sequence == assigned && latest.watch_id == 7 &&
               latest.fired_actions == 1);
        assert(!em8051_debug_runtime_consume_stop(runtime, &stop));
        assert(stop.pending && stop.watch_id == 7 && stop.source_event_sequence == assigned);
        assert(!em8051_debug_runtime_read_ring(runtime, 1, 0, records, 2, &read, &remaining));
        assert(read == 2 && !remaining);
    }
    em8051_debug_runtime_destroy(runtime);
}
static void pages(void)
{
    struct emu_outcome_ring *ring = calloc(1, sizeof *ring);
    struct emuif_observation o;
    struct emuif_trace_page_request q;
    struct
    {
        struct emuif_trace_page p;
        unsigned char tail[16];
    } page;
    struct
    {
        struct emuif_trace_record r;
        unsigned char tail[16];
    } records[16], before;
    uint32_t tiny = 4;
    unsigned i;
    assert(ring);
    emu_outcome_init(ring);
    INIT(q);
    memcpy(q.session_id, identity, 33);
    q.epoch = 1;
    q.max_records = 16;
    memset(&page, 0xa5, sizeof page);
    INIT(page.p);
    page.p.struct_size = sizeof page;
    page.p.records = &records[0].r;
    page.p.element_size = sizeof records[0];
    page.p.capacity = 16;
    for (i = 0; i < 16; i++)
    {
        memset(&records[i], 0xa5, sizeof records[i]);
        initialize_record(&records[i].r);
        records[i].r.struct_size = sizeof records[i];
    }
    assert(!emu_outcome_page(ring, identity, &q, &page.p));
    assert(!page.p.oldest && !page.p.newest && !page.p.more);
    assert(emu_outcome_page(ring, identity, (const void *)&tiny, &page.p) == EMUIF_INVALID);
    assert(emu_outcome_page(ring, identity, &q, (void *)&tiny) == EMUIF_INVALID);
    q.session_id[0] = 'G';
    assert(emu_outcome_page(ring, identity, &q, &page.p) == EMUIF_INVALID);
    q.session_id[0] = identity[0];
    q.session_id[2] = 0;
    assert(emu_outcome_page(ring, identity, &q, &page.p) == EMUIF_INVALID);
    memcpy(q.session_id, identity, 33);
    q.flags = 1;
    assert(emu_outcome_page(ring, identity, &q, &page.p) == EMUIF_INVALID);
    q.flags = 0;
    q.api_version = 2;
    assert(emu_outcome_page(ring, identity, &q, &page.p) == EMUIF_VERSION);
    q.api_version = 1;
    INIT(o);
    INIT(o.old_value);
    INIT(o.new_value);
    o.old_value.width = o.new_value.width = 8;
    o.kind = EMUIF_OBS_INPUT_ADMITTED;
    o.input_stage = 1;
    o.cpu_input_order = 1;
    o.generation = UINT64_C(0x100000001);
    o.sequence = 100;
    for (i = 0; i < 3000; i++)
    {
        o.sequence = 100 + i;
        o.cpu_input_order = i + 1;
        o.cycle = i;
        assert(!emu_outcome_append(ring, &o));
    }
    assert(ring->count == 1024 && ring->overwritten == 3952 && ring->next_sequence == 4977);
    assert(!emu_outcome_page(ring, identity, &q, &page.p));
    assert(page.p.cursor_state == EMUIF_CURSOR_EXPIRED && page.p.oldest == 3953 &&
           page.p.newest == 4976 && page.p.lost_before_page == 3952 && page.p.written == 16 &&
           page.p.remaining == 1008);
    assert(records[0].r.kind == EMUIF_RECORD_LOSS && records[1].r.kind == EMUIF_RECORD_EVENT);
    assert(records[0].r.count == 1 && page.p.dropped == 0);
    assert(page.p.struct_size == sizeof page && page.p.records == &records[0].r &&
           page.p.element_size == sizeof records[0]);
    for (i = 0; i < 16; i++)
    {
        assert(page.tail[i] == 0xa5);
        assert(records[0].tail[i] == 0xa5);
    }
    assert(records[0].r.struct_size == sizeof records[0]);
    before = records[0];
    page.p.capacity = 1;
    assert(emu_outcome_page(ring, identity, &q, &page.p) == EMUIF_CAPACITY &&
           page.p.required == 16 && !page.p.written && page.p.remaining == 1024);
    assert(!memcmp(&before, &records[0], sizeof before));
    page.p.capacity = 16;
    records[1].r.api_version = 2;
    assert(emu_outcome_page(ring, identity, &q, &page.p) == EMUIF_VERSION);
    assert(!memcmp(&before, &records[0], sizeof before));
    records[1].r.api_version = 1;
    q.max_records = 0;
    assert(!emu_outcome_page(ring, identity, &q, &page.p));
    assert(page.p.more && page.p.remaining == 1024 && !page.p.written && !page.p.next_after);
    q.max_records = 16;
    q.after_record_sequence = 4976;
    assert(!emu_outcome_page(ring, identity, &q, &page.p));
    assert(!page.p.written && !page.p.more && page.p.next_after == 4976);
    q.after_record_sequence = 4977;
    assert(!emu_outcome_page(ring, identity, &q, &page.p));
    assert(page.p.cursor_state == EMUIF_CURSOR_AHEAD && !page.p.more && !page.p.remaining);
    q.epoch = 2;
    assert(!emu_outcome_page(ring, identity, &q, &page.p));
    assert(page.p.cursor_state == EMUIF_CURSOR_WRONG_EPOCH && !page.p.lost_before_page);
    q.session_id[0] = 'f';
    assert(!emu_outcome_page(ring, identity, &q, &page.p));
    assert(page.p.cursor_state == EMUIF_CURSOR_WRONG_SESSION);
    ring->next_sequence = UINT64_MAX;
    assert(emu_outcome_append(ring, &o) == EMUIF_EXHAUSTED && ring->overwritten == 3952);
    free(ring);
}
int main(void)
{
    canonical();
    pages();
    puts("canonical outcomes: sequencing, uint64 generation, paging, loss, prefixes passed");
    return 0;
}

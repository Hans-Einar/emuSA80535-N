#include "../emu_debug_trace.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", \
    __FILE__, __LINE__, #x); return 1; } } while (0)

static struct em8051_debug_trace_selector kind_selector(uint8_t kind)
{
    struct em8051_debug_trace_selector s;
    memset(&s, 0, sizeof(s)); s.match_kind = 1u; s.kind = kind; return s;
}

static struct em8051_debug_event event(uint64_t seq, uint8_t kind)
{
    struct em8051_debug_event e;
    memset(&e, 0, sizeof(e)); e.sequence = seq; e.kind = kind; return e;
}

static struct em8051_debug_trace_config base_config(void)
{
    struct em8051_debug_trace_config c;
    memset(&c, 0, sizeof(c));
    c.destination_count = 2u;
    c.destinations[0].destination_id = 4u;
    c.destinations[1].destination_id = 9u;
    c.trace_count = 3u;
    c.traces[0].trace_id = 7u; c.traces[0].destination_id = 4u;
    c.traces[0].enabled = 1u; strcpy(c.traces[0].tag, "all");
    c.traces[0].interrupt_policy = EM8051_DEBUG_TRACE_INCLUDE;
    c.traces[1].trace_id = 23u; c.traces[1].destination_id = 4u;
    c.traces[1].enabled = 1u; strcpy(c.traces[1].tag, "mainline");
    c.traces[1].interrupt_policy = EM8051_DEBUG_TRACE_SUPPRESS_DURING_INTERRUPT;
    c.traces[2].trace_id = 42u; c.traces[2].destination_id = 9u;
    c.traces[2].enabled = 1u; strcpy(c.traces[2].tag, "irq");
    c.traces[2].interrupt_policy = EM8051_DEBUG_TRACE_INTERRUPT_ONLY;
    c.point_count = 1u;
    c.points[0].point_id = 10u; c.points[0].enabled = 1u;
    c.points[0].trace_id_count = 3u;
    c.points[0].trace_ids[0] = 7u; c.points[0].trace_ids[1] = 23u;
    c.points[0].trace_ids[2] = 42u;
    return c;
}

static int test_atomic_and_coalesced(void)
{
    struct em8051_debug_trace_router r;
    struct em8051_debug_trace_config c = base_config(), bad;
    const struct em8051_debug_trace_ring *ring;
    struct em8051_debug_event e = event(1u, EM8051_DEBUG_EVENT_INSTRUCTION_END);
    em8051_debug_trace_router_init(&r);
    CHECK(em8051_debug_trace_router_replace(&r, &c) == EM8051_DEBUG_EVENT_OK);
    bad = c; bad.traces[1].trace_id = 7u;
    CHECK(em8051_debug_trace_router_replace(&r, &bad) ==
          EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    CHECK(r.config.traces[1].trace_id == 23u);
    CHECK(em8051_debug_trace_router_event(&r, &e) == EM8051_DEBUG_EVENT_OK);
    ring = em8051_debug_trace_router_ring(&r, 4u);
    CHECK(ring && ring->count == 1u);
    CHECK(ring->records[0].trace_id_count == 2u);
    CHECK(ring->records[0].trace_ids[0] == 7u &&
          ring->records[0].trace_ids[1] == 23u);
    CHECK(em8051_debug_trace_router_ring(&r, 9u)->count == 0u);
    return 0;
}

static int test_gates(void)
{
    struct em8051_debug_trace_router r;
    struct em8051_debug_trace_config c = base_config();
    struct em8051_debug_event e;
    c.trace_count = 1u; c.point_count = 1u;
    c.points[0].trace_id_count = 1u; c.points[0].trace_ids[0] = 7u;
    c.gate_count = 4u;
    c.gates[0].gate_id = 1u; c.gates[0].enabled = 1u;
    c.gates[0].selector = kind_selector(EM8051_DEBUG_EVENT_CONTROL_CALL);
    c.gates[0].trace_ids[0] = 7u; c.gates[0].trace_id_count = 1u;
    c.gates[0].action = EM8051_DEBUG_TRACE_GATE_OFF;
    c.gates[0].timing = EM8051_DEBUG_TRACE_GATE_BEFORE;
    c.gates[1] = c.gates[0]; c.gates[1].gate_id = 2u;
    c.gates[1].action = EM8051_DEBUG_TRACE_GATE_ON; /* last before wins */
    c.gates[2] = c.gates[0]; c.gates[2].gate_id = 3u;
    c.gates[2].timing = EM8051_DEBUG_TRACE_GATE_AFTER;
    c.gates[2].action = EM8051_DEBUG_TRACE_GATE_ON;
    c.gates[3] = c.gates[2]; c.gates[3].gate_id = 4u;
    c.gates[3].action = EM8051_DEBUG_TRACE_GATE_OFF; /* last after wins */
    em8051_debug_trace_router_init(&r);
    CHECK(em8051_debug_trace_router_replace(&r, &c) == EM8051_DEBUG_EVENT_OK);
    e = event(1u, EM8051_DEBUG_EVENT_CONTROL_CALL);
    CHECK(em8051_debug_trace_router_event(&r, &e) == EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_trace_router_ring(&r, 4u)->count == 1u);
    CHECK(r.config.traces[0].enabled == 0u);
    e = event(2u, EM8051_DEBUG_EVENT_INSTRUCTION_END);
    CHECK(em8051_debug_trace_router_event(&r, &e) == EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_trace_router_ring(&r, 4u)->count == 1u);
    return 0;
}

static int test_each_gate_timing(void)
{
    struct em8051_debug_trace_router r;
    struct em8051_debug_trace_config c;
    struct em8051_debug_event e;
    const struct em8051_debug_trace_ring *ring;
    uint8_t action, timing;
    uint64_t sequence = 1u;

    for (action = EM8051_DEBUG_TRACE_GATE_ON;
         action <= EM8051_DEBUG_TRACE_GATE_OFF; ++action) {
        for (timing = EM8051_DEBUG_TRACE_GATE_BEFORE;
             timing <= EM8051_DEBUG_TRACE_GATE_AFTER; ++timing) {
            c = base_config();
            c.trace_count = 1u;
            c.point_count = 1u;
            c.points[0].trace_id_count = 1u;
            c.points[0].trace_ids[0] = 7u;
            c.traces[0].enabled = (uint8_t)
                (action == EM8051_DEBUG_TRACE_GATE_ON ? 0u : 1u);
            c.gate_count = 1u;
            c.gates[0].gate_id = 1u;
            c.gates[0].enabled = 1u;
            c.gates[0].selector =
                kind_selector(EM8051_DEBUG_EVENT_CONTROL_CALL);
            c.gates[0].trace_ids[0] = 7u;
            c.gates[0].trace_id_count = 1u;
            c.gates[0].action = action;
            c.gates[0].timing = timing;

            em8051_debug_trace_router_init(&r);
            CHECK(em8051_debug_trace_router_replace(&r, &c) ==
                  EM8051_DEBUG_EVENT_OK);
            e = event(sequence++, EM8051_DEBUG_EVENT_CONTROL_CALL);
            CHECK(em8051_debug_trace_router_event(&r, &e) ==
                  EM8051_DEBUG_EVENT_OK);
            ring = em8051_debug_trace_router_ring(&r, 4u);
            CHECK(ring != NULL);
            if (timing == EM8051_DEBUG_TRACE_GATE_BEFORE)
                CHECK(ring->count ==
                      (action == EM8051_DEBUG_TRACE_GATE_ON ? 1u : 0u));
            else
                CHECK(ring->count ==
                      (action == EM8051_DEBUG_TRACE_GATE_ON ? 0u : 1u));
            CHECK(r.config.traces[0].enabled ==
                  (action == EM8051_DEBUG_TRACE_GATE_ON ? 1u : 0u));
        }
    }
    return 0;
}

static int test_nested_interrupts_and_suppression(void)
{
    struct em8051_debug_trace_router r;
    struct em8051_debug_trace_config c = base_config();
    struct em8051_debug_event events[] = {
        { .sequence=1u, .kind=EM8051_DEBUG_EVENT_INSTRUCTION_END },
        { .sequence=2u, .kind=EM8051_DEBUG_EVENT_INTERRUPT_ENTER },
        { .sequence=3u, .kind=EM8051_DEBUG_EVENT_INSTRUCTION_END },
        { .sequence=4u, .kind=EM8051_DEBUG_EVENT_INTERRUPT_ENTER },
        { .sequence=5u, .kind=EM8051_DEBUG_EVENT_INSTRUCTION_END },
        { .sequence=6u, .kind=EM8051_DEBUG_EVENT_INTERRUPT_EXIT },
        { .sequence=7u, .kind=EM8051_DEBUG_EVENT_INTERRUPT_EXIT },
        { .sequence=8u, .kind=EM8051_DEBUG_EVENT_INSTRUCTION_END }
    };
    const struct em8051_debug_trace_ring *mainring, *irqring;
    size_t i; unsigned summaries = 0u;
    em8051_debug_trace_router_init(&r);
    CHECK(em8051_debug_trace_router_replace(&r, &c) == EM8051_DEBUG_EVENT_OK);
    for (i = 0u; i < sizeof(events)/sizeof(events[0]); ++i)
        CHECK(em8051_debug_trace_router_event(&r, &events[i]) ==
              EM8051_DEBUG_EVENT_OK);
    CHECK(r.interrupt_depth == 0u);
    mainring = em8051_debug_trace_router_ring(&r, 4u);
    irqring = em8051_debug_trace_router_ring(&r, 9u);
    CHECK(mainring->count == 9u); /* shared retained views plus one summary */
    for (i = 0u; i < mainring->count; ++i)
        if (mainring->records[i].kind == EM8051_DEBUG_TRACE_RECORD_SUPPRESSION) {
            ++summaries;
            CHECK(mainring->records[i].suppression.first_sequence == 3u);
            CHECK(mainring->records[i].suppression.last_sequence == 6u);
            CHECK(mainring->records[i].suppression.count == 4u);
            CHECK(mainring->records[i].suppression.maximum_depth == 2u);
        }
    CHECK(summaries == 1u);
    CHECK(irqring->count == 7u); /* depth-zero gap plus six IRQ events */
    CHECK(irqring->records[0].kind == EM8051_DEBUG_TRACE_RECORD_SUPPRESSION);
    CHECK(irqring->records[1].event.sequence == 2u);
    CHECK(irqring->records[6].event.sequence == 7u);
    return 0;
}

static int test_watch_and_ring_loss(void)
{
    struct em8051_debug_trace_router r;
    struct em8051_debug_trace_config c = base_config();
    struct em8051_debug_watch_result wr;
    struct em8051_debug_event e;
    const struct em8051_debug_trace_ring *ring;
    size_t i;
    em8051_debug_trace_router_init(&r);
    CHECK(em8051_debug_trace_router_replace(&r, &c) == EM8051_DEBUG_EVENT_OK);
    memset(&wr, 0, sizeof(wr)); wr.watch_id = 5u;
    wr.trace_id_count = 2u; wr.trace_ids[0] = 7u; wr.trace_ids[1] = 23u;
    e = event(1u, EM8051_DEBUG_EVENT_WATCH_MATCH);
    CHECK(em8051_debug_trace_router_watch(&r, &e, &wr) ==
          EM8051_DEBUG_EVENT_OK);
    ring = em8051_debug_trace_router_ring(&r, 4u);
    CHECK(ring->count == 1u && ring->records[0].trace_id_count == 2u);
    CHECK(ring->records[0].watch_result.watch_id == 5u);
    CHECK(em8051_debug_trace_router_event(&r, &e) ==
          EM8051_DEBUG_EVENT_INVALID_ARGUMENT); /* no recursive ordinary match */
    for (i = 2u; i < EM8051_DEBUG_TRACE_RING_CAPACITY + 12u; ++i) {
        e = event(i, EM8051_DEBUG_EVENT_INSTRUCTION_END);
        CHECK(em8051_debug_trace_router_event(&r, &e) == EM8051_DEBUG_EVENT_OK);
    }
    ring = em8051_debug_trace_router_ring(&r, 4u);
    CHECK(ring->count == EM8051_DEBUG_TRACE_RING_CAPACITY);
    CHECK(ring->overwritten == 11u);
    return 0;
}

static int test_limits_and_flush(void)
{
    struct em8051_debug_trace_router r;
    struct em8051_debug_trace_config c = base_config();
    struct em8051_debug_event e;
    struct em8051_debug_trace_record record;
    struct em8051_debug_trace_ring *ring;
    c.trace_count = EM8051_DEBUG_TRACE_MAX_TRACES + 1u;
    em8051_debug_trace_router_init(&r);
    CHECK(em8051_debug_trace_router_replace(&r, &c) ==
          EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    c = base_config();
    CHECK(em8051_debug_trace_router_replace(&r, &c) == EM8051_DEBUG_EVENT_OK);
    e = event(1u, EM8051_DEBUG_EVENT_INTERRUPT_ENTER);
    CHECK(em8051_debug_trace_router_event(&r, &e) == EM8051_DEBUG_EVENT_OK);
    e = event(2u, EM8051_DEBUG_EVENT_INSTRUCTION_END);
    CHECK(em8051_debug_trace_router_event(&r, &e) == EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_trace_router_flush(&r) == EM8051_DEBUG_EVENT_OK);
    ring = (struct em8051_debug_trace_ring *)
        em8051_debug_trace_router_ring(&r, 4u);
    CHECK(ring && em8051_debug_trace_ring_pop(ring, &record));
    while (record.kind != EM8051_DEBUG_TRACE_RECORD_SUPPRESSION)
        CHECK(em8051_debug_trace_ring_pop(ring, &record));
    CHECK(record.suppression.count == 1u);
    return 0;
}

static int test_all_configuration_bounds_and_rejection_neutrality(void)
{
    struct em8051_debug_trace_router r;
    struct em8051_debug_trace_config good = base_config(), bad;
    struct em8051_debug_watch_result wr;
    struct em8051_debug_event e;
    const struct em8051_debug_trace_ring *ring;
    em8051_debug_trace_router_init(&r);
    CHECK(em8051_debug_trace_router_replace(&r, &good) == EM8051_DEBUG_EVENT_OK);

    bad = good; bad.destination_count = EM8051_DEBUG_TRACE_MAX_DESTINATIONS + 1u;
    CHECK(em8051_debug_trace_router_replace(&r, &bad) == EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    bad = good; bad.point_count = EM8051_DEBUG_TRACE_MAX_POINTS + 1u;
    CHECK(em8051_debug_trace_router_replace(&r, &bad) == EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    bad = good; bad.gate_count = EM8051_DEBUG_TRACE_MAX_GATES + 1u;
    CHECK(em8051_debug_trace_router_replace(&r, &bad) == EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    bad = good; bad.points[0].trace_id_count = EM8051_DEBUG_TRACE_MAX_ROUTES + 1u;
    CHECK(em8051_debug_trace_router_replace(&r, &bad) == EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    bad = good; memset(bad.traces[0].tag, 'x', sizeof(bad.traces[0].tag));
    CHECK(em8051_debug_trace_router_replace(&r, &bad) == EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    bad = good; bad.traces[0].destination_id = 99u;
    CHECK(em8051_debug_trace_router_replace(&r, &bad) == EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    CHECK(r.config.traces[0].destination_id == 4u);

    e = event(1u, EM8051_DEBUG_EVENT_INTERRUPT_EXIT);
    CHECK(em8051_debug_trace_router_event(&r, &e) == EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    CHECK(r.last_sequence == 0u && r.interrupt_depth == 0u);
    CHECK(em8051_debug_trace_router_ring(&r, 4u)->count == 0u);

    memset(&wr, 0, sizeof(wr)); wr.trace_id_count = 2u;
    wr.trace_ids[0] = 23u; wr.trace_ids[1] = 7u; /* deliberately unsorted */
    e = event(1u, EM8051_DEBUG_EVENT_WATCH_MATCH);
    CHECK(em8051_debug_trace_router_watch(&r, &e, &wr) ==
          EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    ring = em8051_debug_trace_router_ring(&r, 4u);
    CHECK(ring->count == 0u && r.last_sequence == 0u);
    return 0;
}

static int test_source_transaction_guards_and_saturating_counters(void)
{
    struct em8051_debug_trace_router r;
    struct em8051_debug_trace_config c = base_config();
    struct em8051_debug_watch_result wr;
    struct em8051_debug_event source;
    struct em8051_debug_event derived;
    const struct em8051_debug_trace_ring *ring;

    c.trace_count = 1u;
    c.destination_count = 1u;
    c.point_count = 1u;
    c.points[0].trace_id_count = 1u;
    c.points[0].trace_ids[0] = 7u;
    em8051_debug_trace_router_init(&r);
    CHECK(em8051_debug_trace_router_replace(&r, &c) == EM8051_DEBUG_EVENT_OK);
    source = event(1u, EM8051_DEBUG_EVENT_MEMORY_WRITE);
    source.generation = 2u;
    CHECK(em8051_debug_trace_router_source_begin(&r, &source) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_trace_router_source_begin(&r, &source) ==
          EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    CHECK(em8051_debug_trace_router_replace(&r, &c) ==
          EM8051_DEBUG_EVENT_BUSY);
    CHECK(em8051_debug_trace_router_flush(&r) == EM8051_DEBUG_EVENT_BUSY);

    memset(&wr, 0, sizeof(wr));
    wr.watch_id = 5u;
    wr.source_event_sequence = 99u;
    wr.trace_id_count = 1u;
    wr.trace_ids[0] = 7u;
    derived = source;
    derived.sequence = 2u;
    derived.kind = EM8051_DEBUG_EVENT_WATCH_MATCH;
    CHECK(em8051_debug_trace_router_watch(&r, &derived, &wr) ==
          EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    ring = em8051_debug_trace_router_ring(&r, 4u);
    CHECK(ring != NULL && ring->count == 1u && r.last_sequence == 1u);

    wr.source_event_sequence = 1u;
    CHECK(em8051_debug_trace_router_watch(&r, &derived, &wr) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_trace_router_source_end(&r) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_trace_router_source_end(&r) ==
          EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    CHECK(r.last_sequence == 2u && ring->count == 2u);

    r.rings[0].overwritten = UINT64_MAX;
    r.rings[0].count = EM8051_DEBUG_TRACE_RING_CAPACITY;
    source = event(3u, EM8051_DEBUG_EVENT_INSTRUCTION_END);
    CHECK(em8051_debug_trace_router_event(&r, &source) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(r.rings[0].overwritten == UINT64_MAX);

    r.config.traces[0].interrupt_policy =
        EM8051_DEBUG_TRACE_SUPPRESS_DURING_INTERRUPT;
    source = event(4u, EM8051_DEBUG_EVENT_INTERRUPT_ENTER);
    CHECK(em8051_debug_trace_router_event(&r, &source) ==
          EM8051_DEBUG_EVENT_OK);
    source = event(5u, EM8051_DEBUG_EVENT_INSTRUCTION_END);
    CHECK(em8051_debug_trace_router_event(&r, &source) ==
          EM8051_DEBUG_EVENT_OK);
    r.suppression[0].value.count = UINT64_MAX;
    source = event(6u, EM8051_DEBUG_EVENT_INSTRUCTION_END);
    CHECK(em8051_debug_trace_router_event(&r, &source) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(r.suppression[0].value.count == UINT64_MAX);
    return 0;
}

static int test_lifecycle_marker_bypasses_points_and_interrupt_filter(void)
{
    struct em8051_debug_trace_router r;
    struct em8051_debug_trace_config c = base_config();
    struct em8051_debug_event e;
    const struct em8051_debug_trace_ring *ring;

    c.trace_count = 1u;
    c.destination_count = 1u;
    c.destinations[0].destination_id = 9u;
    c.traces[0] = c.traces[2];
    c.point_count = 0u;
    em8051_debug_trace_router_init(&r);
    CHECK(em8051_debug_trace_router_replace(&r, &c) == EM8051_DEBUG_EVENT_OK);
    e = event(1u, EM8051_DEBUG_EVENT_LOAD);
    CHECK(em8051_debug_trace_router_event(&r, &e) == EM8051_DEBUG_EVENT_OK);
    ring = em8051_debug_trace_router_ring(&r, 9u);
    CHECK(ring != NULL && ring->count == 1u);
    CHECK(ring->records[0].event.kind == EM8051_DEBUG_EVENT_LOAD &&
          ring->records[0].trace_id_count == 1u &&
          ring->records[0].trace_ids[0] == 42u);
    return 0;
}

static int test_lifecycle_gate_entry_snapshot(void)
{
    struct em8051_debug_trace_router r;
    struct em8051_debug_trace_config c;
    struct em8051_debug_event e;
    const struct em8051_debug_trace_ring *ring;
    uint8_t kind;
    size_t i;

    for (kind = EM8051_DEBUG_EVENT_RESET;
         kind <= EM8051_DEBUG_EVENT_LOAD; ++kind) {
        memset(&c, 0, sizeof(c));
        c.destination_count = 1u;
        c.destinations[0].destination_id = 4u;
        c.trace_count = 4u;
        c.traces[0].trace_id = 7u;
        c.traces[0].destination_id = 4u;
        c.traces[0].enabled = 1u;
        c.traces[1].trace_id = 23u;
        c.traces[1].destination_id = 4u;
        c.traces[2].trace_id = 42u;
        c.traces[2].destination_id = 4u;
        c.traces[2].enabled = 1u;
        c.traces[3].trace_id = 50u;
        c.traces[3].destination_id = 4u;
        c.gate_count = 4u;
        for (i = 0u; i < c.gate_count; ++i) {
            c.gates[i].gate_id = (uint32_t)i + 1u;
            c.gates[i].enabled = 1u;
            c.gates[i].selector = kind_selector(kind);
            c.gates[i].trace_ids[0] = c.traces[i].trace_id;
            c.gates[i].trace_id_count = 1u;
        }
        c.gates[0].action = EM8051_DEBUG_TRACE_GATE_OFF;
        c.gates[0].timing = EM8051_DEBUG_TRACE_GATE_BEFORE;
        c.gates[1].action = EM8051_DEBUG_TRACE_GATE_ON;
        c.gates[1].timing = EM8051_DEBUG_TRACE_GATE_BEFORE;
        c.gates[2].action = EM8051_DEBUG_TRACE_GATE_OFF;
        c.gates[2].timing = EM8051_DEBUG_TRACE_GATE_AFTER;
        c.gates[3].action = EM8051_DEBUG_TRACE_GATE_ON;
        c.gates[3].timing = EM8051_DEBUG_TRACE_GATE_AFTER;

        em8051_debug_trace_router_init(&r);
        CHECK(em8051_debug_trace_router_replace(&r, &c) ==
              EM8051_DEBUG_EVENT_OK);
        e = event(1u, kind);
        CHECK(em8051_debug_trace_router_event(&r, &e) ==
              EM8051_DEBUG_EVENT_OK);
        ring = em8051_debug_trace_router_ring(&r, 4u);
        CHECK(ring != NULL && ring->count == 1u);
        CHECK(ring->records[0].event.kind == kind &&
              ring->records[0].trace_id_count == 2u &&
              ring->records[0].trace_ids[0] == 7u &&
              ring->records[0].trace_ids[1] == 42u);
        CHECK(r.config.traces[0].enabled == 0u &&
              r.config.traces[1].enabled == 1u &&
              r.config.traces[2].enabled == 0u &&
              r.config.traces[3].enabled == 1u);
    }
    return 0;
}

static int test_suppression_replacement_requires_flush(void)
{
    struct em8051_debug_trace_router r;
    struct em8051_debug_trace_config c = base_config(), candidate;
    struct em8051_debug_event e;
    const struct em8051_debug_trace_ring *ring;

    c.traces[0] = c.traces[1];
    c.trace_count = 1u;
    c.points[0].trace_ids[0] = 23u;
    c.points[0].trace_id_count = 1u;
    c.gate_count = 0u;
    em8051_debug_trace_router_init(&r);
    CHECK(em8051_debug_trace_router_replace(&r, &c) ==
          EM8051_DEBUG_EVENT_OK);
    e = event(1u, EM8051_DEBUG_EVENT_INTERRUPT_ENTER);
    CHECK(em8051_debug_trace_router_event(&r, &e) == EM8051_DEBUG_EVENT_OK);
    e = event(2u, EM8051_DEBUG_EVENT_INSTRUCTION_END);
    CHECK(em8051_debug_trace_router_event(&r, &e) == EM8051_DEBUG_EVENT_OK);
    CHECK(r.suppression[0].active == 1u &&
          r.suppression[0].value.count == 1u);

    candidate = c;
    candidate.trace_count = 0u;
    candidate.point_count = 0u;
    CHECK(em8051_debug_trace_router_replace(&r, &candidate) ==
          EM8051_DEBUG_EVENT_BUSY);
    candidate = c;
    candidate.traces[0].destination_id = 9u;
    CHECK(em8051_debug_trace_router_replace(&r, &candidate) ==
          EM8051_DEBUG_EVENT_BUSY);
    candidate = c;
    candidate.traces[0].interrupt_policy = EM8051_DEBUG_TRACE_INCLUDE;
    CHECK(em8051_debug_trace_router_replace(&r, &candidate) ==
          EM8051_DEBUG_EVENT_BUSY);
    CHECK(r.config.trace_count == 1u &&
          r.config.traces[0].destination_id == 4u &&
          r.config.traces[0].interrupt_policy ==
              EM8051_DEBUG_TRACE_SUPPRESS_DURING_INTERRUPT &&
          r.suppression[0].active == 1u);

    CHECK(em8051_debug_trace_router_flush(&r) == EM8051_DEBUG_EVENT_OK);
    ring = em8051_debug_trace_router_ring(&r, 4u);
    CHECK(ring != NULL && ring->count == 2u &&
          ring->records[1].kind == EM8051_DEBUG_TRACE_RECORD_SUPPRESSION);
    candidate = r.config;
    candidate.traces[0].interrupt_policy = EM8051_DEBUG_TRACE_INCLUDE;
    CHECK(em8051_debug_trace_router_replace(&r, &candidate) ==
          EM8051_DEBUG_EVENT_OK);
    candidate = r.config;
    candidate.traces[0].destination_id = 9u;
    CHECK(em8051_debug_trace_router_replace(&r, &candidate) ==
          EM8051_DEBUG_EVENT_OK);
    candidate = r.config;
    candidate.trace_count = 0u;
    candidate.point_count = 0u;
    CHECK(em8051_debug_trace_router_replace(&r, &candidate) ==
          EM8051_DEBUG_EVENT_OK);
    return 0;
}

static int test_trace_id_lifetime_and_registry_bound(void)
{
    struct em8051_debug_trace_router r;
    struct em8051_debug_trace_config c, candidate;
    size_t i;

    memset(&c, 0, sizeof(c));
    c.destination_count = 1u;
    c.destinations[0].destination_id = 1u;
    c.trace_count = 1u;
    c.traces[0].trace_id = 7u;
    c.traces[0].destination_id = 1u;
    c.traces[0].enabled = 1u;
    em8051_debug_trace_router_init(&r);
    CHECK(em8051_debug_trace_router_replace(&r, &c) ==
          EM8051_DEBUG_EVENT_OK);
    c.traces[0].enabled = 0u;
    CHECK(em8051_debug_trace_router_replace(&r, &c) ==
          EM8051_DEBUG_EVENT_OK); /* a live ID may be updated */
    candidate = c;
    candidate.trace_count = 2u;
    candidate.traces[1] = candidate.traces[0];
    candidate.traces[1].trace_id = 8u;
    CHECK(em8051_debug_trace_router_replace(&r, &candidate) ==
          EM8051_DEBUG_EVENT_OK); /* a new ID is admitted */
    c = r.config;
    c.traces[0] = c.traces[1];
    memset(&c.traces[1], 0, sizeof(c.traces[1]));
    c.trace_count = 1u;
    CHECK(em8051_debug_trace_router_replace(&r, &c) ==
          EM8051_DEBUG_EVENT_OK);
    candidate = c;
    candidate.trace_count = 2u;
    candidate.traces[1] = candidate.traces[0];
    candidate.traces[0].trace_id = 7u;
    CHECK(em8051_debug_trace_router_replace(&r, &candidate) ==
          EM8051_DEBUG_EVENT_DUPLICATE_ID);
    CHECK(r.config.trace_count == 1u && r.config.traces[0].trace_id == 8u);
    em8051_debug_trace_router_init(&r);
    CHECK(em8051_debug_trace_router_replace(&r, &candidate) ==
          EM8051_DEBUG_EVENT_OK); /* initialization starts a new session */

    memset(&c, 0, sizeof(c));
    c.destination_count = 1u;
    c.destinations[0].destination_id = 1u;
    c.trace_count = 1u;
    c.traces[0].destination_id = 1u;
    c.traces[0].enabled = 1u;
    em8051_debug_trace_router_init(&r);
    for (i = 0u; i < EM8051_DEBUG_TRACE_MAX_SEEN_IDS; ++i) {
        c.trace_count = 1u;
        c.traces[0].trace_id = (uint32_t)i + 1u;
        CHECK(em8051_debug_trace_router_replace(&r, &c) ==
              EM8051_DEBUG_EVENT_OK);
        c.trace_count = 0u;
        CHECK(em8051_debug_trace_router_replace(&r, &c) ==
              EM8051_DEBUG_EVENT_OK);
    }
    c.trace_count = 1u;
    c.traces[0].trace_id = (uint32_t)EM8051_DEBUG_TRACE_MAX_SEEN_IDS + 1u;
    CHECK(em8051_debug_trace_router_replace(&r, &c) ==
          EM8051_DEBUG_EVENT_LIMIT);
    CHECK(r.config.trace_count == 0u &&
          r.seen_trace_id_count == EM8051_DEBUG_TRACE_MAX_SEEN_IDS);
    em8051_debug_trace_router_init(&r);
    CHECK(em8051_debug_trace_router_replace(&r, &c) ==
          EM8051_DEBUG_EVENT_OK);
    return 0;
}

static int test_utf8_metadata_validation(void)
{
    static const unsigned char malformed[][4] = {
        { 0x80u, 0u, 0u, 0u },
        { 0xc0u, 0x80u, 0u, 0u },
        { 0xe2u, 0x82u, 0u, 0u },
        { 0xe0u, 0x80u, 0x80u, 0u },
        { 0xedu, 0xa0u, 0x80u, 0u },
        { 0xf4u, 0x90u, 0x80u, 0x80u }
    };
    static const size_t malformed_length[] = { 1u, 2u, 2u, 3u, 3u, 4u };
    static const unsigned char valid_four_byte[] = {
        0xf0u, 0x9fu, 0x98u, 0x80u
    };
    struct em8051_debug_trace_router r;
    struct em8051_debug_trace_config c = base_config(), candidate;
    size_t i;

    em8051_debug_trace_router_init(&r);
    CHECK(em8051_debug_trace_router_replace(&r, &c) ==
          EM8051_DEBUG_EVENT_OK);
    candidate = c;
    memset(candidate.traces[0].tag, 0, sizeof(candidate.traces[0].tag));
    memcpy(candidate.traces[0].tag, valid_four_byte,
           sizeof(valid_four_byte));
    CHECK(em8051_debug_trace_router_replace(&r, &candidate) ==
          EM8051_DEBUG_EVENT_OK);

    c = r.config;
    memset(c.traces[0].tag, 'a', sizeof(c.traces[0].tag));
    c.traces[0].tag[sizeof(c.traces[0].tag) - 1u] = '\0';
    memset(c.traces[0].comment, 'b', sizeof(c.traces[0].comment));
    c.traces[0].comment[sizeof(c.traces[0].comment) - 1u] = '\0';
    CHECK(em8051_debug_trace_router_replace(&r, &c) ==
          EM8051_DEBUG_EVENT_OK); /* valid 64-byte tag and 256-byte comment */
    for (i = 0u; i < sizeof(malformed) / sizeof(malformed[0]); ++i) {
        candidate = c;
        memset(candidate.traces[0].tag, 0,
               sizeof(candidate.traces[0].tag));
        memcpy(candidate.traces[0].tag, malformed[i], malformed_length[i]);
        CHECK(em8051_debug_trace_router_replace(&r, &candidate) ==
              EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
        CHECK(r.config.traces[0].tag[0] == 'a' &&
              r.config.traces[0].comment[0] == 'b');
    }
    candidate = c;
    memset(candidate.traces[0].comment, 0,
           sizeof(candidate.traces[0].comment));
    candidate.traces[0].comment[0] = (char)0xf5;
    CHECK(em8051_debug_trace_router_replace(&r, &candidate) ==
          EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    CHECK(r.config.traces[0].tag[0] == 'a' &&
          r.config.traces[0].comment[0] == 'b');
    return 0;
}

int main(void)
{
    CHECK(test_atomic_and_coalesced() == 0);
    CHECK(test_gates() == 0);
    CHECK(test_each_gate_timing() == 0);
    CHECK(test_nested_interrupts_and_suppression() == 0);
    CHECK(test_watch_and_ring_loss() == 0);
    CHECK(test_limits_and_flush() == 0);
    CHECK(test_all_configuration_bounds_and_rejection_neutrality() == 0);
    CHECK(test_source_transaction_guards_and_saturating_counters() == 0);
    CHECK(test_lifecycle_marker_bypasses_points_and_interrupt_filter() == 0);
    CHECK(test_lifecycle_gate_entry_snapshot() == 0);
    CHECK(test_suppression_replacement_requires_flush() == 0);
    CHECK(test_trace_id_lifetime_and_registry_bound() == 0);
    CHECK(test_utf8_metadata_validation() == 0);
    puts("debug multi-trace router tests passed");
    return 0;
}

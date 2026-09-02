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

int main(void)
{
    CHECK(test_atomic_and_coalesced() == 0);
    CHECK(test_gates() == 0);
    CHECK(test_each_gate_timing() == 0);
    CHECK(test_nested_interrupts_and_suppression() == 0);
    CHECK(test_watch_and_ring_loss() == 0);
    CHECK(test_limits_and_flush() == 0);
    CHECK(test_all_configuration_bounds_and_rejection_neutrality() == 0);
    puts("debug multi-trace router tests passed");
    return 0;
}

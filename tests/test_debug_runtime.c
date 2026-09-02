#include "../emu_debug_runtime.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", \
    __FILE__, __LINE__, #x); return 1; } } while (0)

static struct em8051_debug_watch_action unconditional_action(uint8_t aKind)
{
    struct em8051_debug_watch_action action;
    memset(&action, 0, sizeof(action));
    action.kind = aKind;
    return action;
}

static struct em8051_debug_watch make_watch(uint32_t aId, uint8_t aActions,
                                             bool aRoute)
{
    struct em8051_debug_watch watch;
    memset(&watch, 0, sizeof(watch));
    watch.id = aId;
    watch.enabled = 1u;
    watch.address_space = EM8051_DEBUG_SPACE_XDATA;
    watch.address_first = 0x1200u;
    watch.address_last = 0x1200u;
    watch.access_mask = EM8051_DEBUG_ACCESS_WRITE;
    watch.action_count = aActions;
    watch.actions[0] = unconditional_action(EM8051_DEBUG_WATCH_STOP);
    if (aActions > 1u) {
        watch.actions[1] = unconditional_action(EM8051_DEBUG_WATCH_CONSOLE);
    }
    if (aRoute) {
        uint8_t at = aActions > 2u ? 2u : (uint8_t)(aActions - 1u);
        watch.actions[at] = unconditional_action(EM8051_DEBUG_WATCH_ROUTE);
        watch.actions[at].trace_ids[0] = 7u;
        watch.actions[at].trace_id_count = 1u;
    }
    return watch;
}

static struct em8051_debug_event write_event(void)
{
    struct em8051_debug_event event;
    memset(&event, 0, sizeof(event));
    event.kind = EM8051_DEBUG_EVENT_MEMORY_WRITE;
    event.address_space = EM8051_DEBUG_SPACE_XDATA;
    event.access = EM8051_DEBUG_ACCESS_WRITE;
    event.address = 0x1200u;
    event.pc = 0x3456u;
    event.instruction_count = 12u;
    event.machine_cycle_count = 34u;
    event.old_value.known = 1u;
    event.old_value.width = 8u;
    event.old_value.value = 1u;
    event.new_value.known = 1u;
    event.new_value.width = 8u;
    event.new_value.value = 2u;
    return event;
}

static int configure_routed_runtime(struct em8051_debug_runtime *aRuntime,
                                    bool aWithGates)
{
    struct em8051_debug_trace_destination destination;
    struct em8051_debug_trace_session trace;
    struct em8051_debug_trace_point point;
    struct em8051_debug_trace_gate gates[2];

    memset(&destination, 0, sizeof(destination));
    destination.destination_id = 4u;
    CHECK(em8051_debug_runtime_replace_destinations(aRuntime, &destination,
                                                     1u) ==
          EM8051_DEBUG_EVENT_OK);
    memset(&trace, 0, sizeof(trace));
    trace.trace_id = 7u;
    trace.destination_id = 4u;
    trace.enabled = (uint8_t)(aWithGates ? 0u : 1u);
    trace.interrupt_policy = EM8051_DEBUG_TRACE_INCLUDE;
    strcpy(trace.tag, "control");
    CHECK(em8051_debug_runtime_replace_traces(aRuntime, &trace, 1u) ==
          EM8051_DEBUG_EVENT_OK);
    memset(&point, 0, sizeof(point));
    point.point_id = 10u;
    point.enabled = 1u;
    point.trace_ids[0] = 7u;
    point.trace_id_count = 1u;
    CHECK(em8051_debug_runtime_replace_points(aRuntime, &point, 1u) ==
          EM8051_DEBUG_EVENT_OK);
    if (!aWithGates)
        return 0;
    memset(gates, 0, sizeof(gates));
    gates[0].gate_id = 1u;
    gates[0].enabled = 1u;
    gates[0].selector.match_kind = 1u;
    gates[0].selector.kind = EM8051_DEBUG_EVENT_MEMORY_WRITE;
    gates[0].trace_ids[0] = 7u;
    gates[0].trace_id_count = 1u;
    gates[0].action = EM8051_DEBUG_TRACE_GATE_ON;
    gates[0].timing = EM8051_DEBUG_TRACE_GATE_BEFORE;
    gates[1] = gates[0];
    gates[1].gate_id = 2u;
    gates[1].action = EM8051_DEBUG_TRACE_GATE_OFF;
    gates[1].timing = EM8051_DEBUG_TRACE_GATE_AFTER;
    CHECK(em8051_debug_runtime_replace_gates(aRuntime, gates, 2u) ==
          EM8051_DEBUG_EVENT_OK);
    return 0;
}

static int test_source_derived_order_gates_stop_and_pages(void)
{
    struct em8051_debug_runtime *runtime = NULL;
    struct em8051_debug_watch watches[2];
    struct em8051_debug_event event = write_event();
    struct em8051_debug_trace_record records[2];
    struct em8051_debug_runtime_stop stop;
    struct em8051_debug_runtime_status status;
    struct em8051_debug_trace_session listed_trace;
    size_t read, remaining, written, total;
    uint64_t sequence = 0u;

    CHECK(em8051_debug_runtime_create(&runtime) == EM8051_DEBUG_EVENT_OK);
    CHECK(configure_routed_runtime(runtime, true) == 0);
    watches[0] = make_watch(20u, 4u, true);
    watches[0].actions[3] = unconditional_action(EM8051_DEBUG_WATCH_QUIET);
    watches[1] = make_watch(3u, 3u, true);
    CHECK(em8051_debug_runtime_replace_watches(runtime, watches, 2u) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_runtime_ingest(runtime, &event, &sequence) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(sequence == 1u);
    CHECK(em8051_debug_runtime_get_status(runtime, &status) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(status.next_sequence == 4u && status.accepted_source_events == 1u);
    CHECK(status.derived_events == 2u && status.console_matches == 1u);
    CHECK(status.stop_pending == 1u);
    CHECK(em8051_debug_runtime_list_traces(runtime, 0u, &listed_trace, 1u,
                                           &written, &total) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(written == 1u && total == 1u && listed_trace.enabled == 0u);

    CHECK(em8051_debug_runtime_read_ring(runtime, 4u, 0u, records, 2u, &read,
                                         &remaining) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(read == 2u && remaining == 1u);
    CHECK(records[0].event.sequence == 1u &&
          records[0].event.kind == EM8051_DEBUG_EVENT_MEMORY_WRITE);
    CHECK(records[1].event.sequence == 2u &&
          records[1].event.kind == EM8051_DEBUG_EVENT_WATCH_MATCH);
    CHECK(records[1].watch_result.watch_id == 3u &&
          records[1].watch_result.source_event_sequence == 1u);
    CHECK(em8051_debug_runtime_read_ring(runtime, 4u, 2u, records, 2u, &read,
                                         &remaining) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(read == 1u && remaining == 0u);
    CHECK(records[0].event.sequence == 3u &&
          records[0].watch_result.watch_id == 20u);
    CHECK(records[0].watch_result.quiet == 1u &&
          records[0].watch_result.console == 0u);
    CHECK(em8051_debug_runtime_read_ring(runtime, 4u, 0u, records, 2u, &read,
                                         &remaining) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(read == 2u && remaining == 1u && records[0].event.sequence == 1u &&
          records[1].event.sequence == 2u); /* reads are non-destructive */
    CHECK(em8051_debug_runtime_read_ring(runtime, 4u, 1u, NULL, 0u, &read,
                                         &remaining) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(read == 0u && remaining == 2u);

    CHECK(em8051_debug_runtime_consume_stop(runtime, &stop) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(stop.pending == 1u && stop.watch_id == 3u &&
          stop.source_event_sequence == 1u &&
          stop.matching_watch_count == 2u);
    CHECK(em8051_debug_runtime_consume_stop(runtime, &stop) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(stop.pending == 0u);

    event.kind = EM8051_DEBUG_EVENT_INSTRUCTION_END;
    event.access = EM8051_DEBUG_ACCESS_NONE;
    CHECK(em8051_debug_runtime_ingest(runtime, &event, &sequence) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(sequence == 4u);
    CHECK(em8051_debug_runtime_read_ring(runtime, 4u, 3u, records, 2u, &read,
                                         &remaining) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(read == 0u && remaining == 0u); /* off-after controls next source */
    em8051_debug_runtime_destroy(runtime);
    return 0;
}

static int test_pending_overflow_is_neutral(void)
{
    struct em8051_debug_runtime *runtime = NULL;
    struct em8051_debug_watch watches[EM8051_DEBUG_RUNTIME_PENDING_CAPACITY + 1u];
    struct em8051_debug_event event = write_event();
    struct em8051_debug_runtime_status status;
    struct em8051_debug_runtime_stop stop;
    struct em8051_debug_trace_session trace;
    struct em8051_debug_trace_record record;
    size_t i, written, total, read, remaining;
    uint64_t sequence = 0u;

    CHECK(em8051_debug_runtime_create(&runtime) == EM8051_DEBUG_EVENT_OK);
    CHECK(configure_routed_runtime(runtime, true) == 0);
    for (i = 0u; i < sizeof(watches) / sizeof(watches[0]); ++i)
        watches[i] = make_watch((uint32_t)i + 1u, 1u, false);
    CHECK(em8051_debug_runtime_replace_watches(
              runtime, watches, sizeof(watches) / sizeof(watches[0])) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_runtime_ingest(runtime, &event, &sequence) ==
          EM8051_DEBUG_EVENT_LIMIT);
    CHECK(sequence == 1u); /* rejected dispatcher event still owns its ID */
    CHECK(em8051_debug_runtime_get_status(runtime, &status) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(status.next_sequence == 2u && status.accepted_source_events == 0u &&
          status.rejected_source_events == 1u &&
          status.pending_overflows == 1u && status.derived_events == 0u);
    CHECK(em8051_debug_runtime_list_traces(runtime, 0u, &trace, 1u,
                                           &written, &total) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(written == 1u && trace.enabled == 0u); /* before gate did not run */
    CHECK(em8051_debug_runtime_read_ring(runtime, 4u, 0u, &record, 1u, &read,
                                         &remaining) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(read == 0u && remaining == 0u);
    CHECK(em8051_debug_runtime_consume_stop(runtime, &stop) ==
          EM8051_DEBUG_EVENT_OK && stop.pending == 0u);
    em8051_debug_runtime_destroy(runtime);
    return 0;
}

static int test_replacement_atomicity_and_enable(void)
{
    struct em8051_debug_runtime *runtime = NULL;
    struct em8051_debug_watch watch, bad_watch, listed_watch;
    struct em8051_debug_trace_session trace, bad_trace, listed_trace;
    struct em8051_debug_trace_destination destination;
    struct em8051_debug_trace_point bad_point, listed_point;
    struct em8051_debug_trace_gate bad_gate, listed_gate;
    size_t written, total;
    uint32_t ids[2];

    CHECK(em8051_debug_runtime_create(&runtime) == EM8051_DEBUG_EVENT_OK);
    CHECK(configure_routed_runtime(runtime, false) == 0);
    watch = make_watch(5u, 2u, true);
    CHECK(em8051_debug_runtime_replace_watches(runtime, &watch, 1u) ==
          EM8051_DEBUG_EVENT_OK);
    bad_watch = watch;
    bad_watch.actions[1].trace_ids[0] = 99u;
    CHECK(em8051_debug_runtime_replace_watches(runtime, &bad_watch, 1u) ==
          EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    CHECK(em8051_debug_runtime_list_watches(runtime, 0u, &listed_watch, 1u,
                                            &written, &total) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(written == 1u && listed_watch.actions[1].trace_ids[0] == 7u);

    memset(&bad_trace, 0, sizeof(bad_trace));
    bad_trace.trace_id = 8u;
    bad_trace.destination_id = 4u;
    bad_trace.enabled = 1u;
    CHECK(em8051_debug_runtime_replace_traces(runtime, &bad_trace, 1u) ==
          EM8051_DEBUG_EVENT_INVALID_ARGUMENT); /* points/watch still use 7 */
    CHECK(em8051_debug_runtime_list_traces(runtime, 0u, &trace, 1u,
                                           &written, &total) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(trace.trace_id == 7u && trace.enabled == 1u);

    ids[0] = 7u;
    CHECK(em8051_debug_runtime_set_trace_enabled(runtime, ids, 1u, false) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_runtime_list_traces(runtime, 0u, &listed_trace, 1u,
                                           &written, &total) ==
          EM8051_DEBUG_EVENT_OK && listed_trace.enabled == 0u);
    ids[0] = 7u;
    ids[1] = 7u;
    CHECK(em8051_debug_runtime_set_trace_enabled(runtime, ids, 2u, true) ==
          EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    CHECK(em8051_debug_runtime_list_traces(runtime, 0u, &listed_trace, 1u,
                                           &written, &total) ==
          EM8051_DEBUG_EVENT_OK && listed_trace.enabled == 0u);

    memset(&destination, 0, sizeof(destination));
    destination.destination_id = 9u;
    CHECK(em8051_debug_runtime_replace_destinations(runtime, &destination,
                                                     1u) ==
          EM8051_DEBUG_EVENT_INVALID_ARGUMENT); /* referenced destination */
    CHECK(em8051_debug_runtime_list_destinations(runtime, 1u, NULL, 0u,
                                                 &written, &total) ==
          EM8051_DEBUG_EVENT_OK && written == 0u && total == 1u);

    memset(&bad_point, 0, sizeof(bad_point));
    bad_point.point_id = 10u;
    bad_point.enabled = 1u;
    bad_point.trace_ids[0] = 99u;
    bad_point.trace_id_count = 1u;
    CHECK(em8051_debug_runtime_replace_points(runtime, &bad_point, 1u) ==
          EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    CHECK(em8051_debug_runtime_list_points(runtime, 0u, &listed_point, 1u,
                                           &written, &total) ==
          EM8051_DEBUG_EVENT_OK && written == 1u && total == 1u &&
          listed_point.point_id == 10u && listed_point.trace_ids[0] == 7u);

    memset(&bad_gate, 0, sizeof(bad_gate));
    bad_gate.gate_id = 1u;
    bad_gate.enabled = 1u;
    bad_gate.trace_ids[0] = 99u;
    bad_gate.trace_id_count = 1u;
    CHECK(em8051_debug_runtime_replace_gates(runtime, &bad_gate, 1u) ==
          EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    CHECK(em8051_debug_runtime_list_gates(runtime, 0u, &listed_gate, 1u,
                                          &written, &total) ==
          EM8051_DEBUG_EVENT_OK && written == 0u && total == 0u);
    em8051_debug_runtime_destroy(runtime);
    return 0;
}

static int test_lifecycle_and_code_invalidation(void)
{
    struct em8051_debug_runtime *runtime = NULL;
    struct em8051_debug_watch watch, listed_watch;
    struct em8051_debug_trace_point points[3], listed_points[3];
    struct em8051_debug_trace_gate gates[3], listed_gates[3];
    struct em8051_debug_runtime_status status;
    struct em8051_debug_runtime_stop stop;
    struct em8051_debug_trace_record lifecycle_records[3];
    size_t written, total, read, remaining;
    uint64_t sequence;
    struct em8051_debug_event interrupt_event;

    CHECK(em8051_debug_runtime_create(&runtime) == EM8051_DEBUG_EVENT_OK);
    CHECK(configure_routed_runtime(runtime, false) == 0);
    memset(points, 0, sizeof(points));
    points[0].point_id = 11u;
    points[0].enabled = 1u;
    points[0].selector.match_pc = 1u;
    points[0].selector.pc_first = 0u;
    points[0].selector.pc_last = UINT16_MAX;
    points[0].trace_ids[0] = 7u;
    points[0].trace_id_count = 1u;
    points[1] = points[0];
    points[1].point_id = 12u;
    memset(&points[1].selector, 0, sizeof(points[1].selector));
    points[1].selector.match_address = 1u;
    points[1].selector.address_last = UINT16_MAX;
    points[2] = points[1];
    points[2].point_id = 13u;
    points[2].selector.match_address_space = 1u;
    points[2].selector.address_space = EM8051_DEBUG_SPACE_XDATA;
    CHECK(em8051_debug_runtime_replace_points(runtime, points, 3u) ==
          EM8051_DEBUG_EVENT_OK);
    memset(gates, 0, sizeof(gates));
    gates[0].gate_id = 14u;
    gates[0].enabled = 1u;
    gates[0].selector.match_address_space = 1u;
    gates[0].selector.address_space = EM8051_DEBUG_SPACE_CODE;
    gates[0].trace_ids[0] = 7u;
    gates[0].trace_id_count = 1u;
    gates[0].action = EM8051_DEBUG_TRACE_GATE_ON;
    gates[0].timing = EM8051_DEBUG_TRACE_GATE_BEFORE;
    gates[1] = gates[0];
    gates[1].gate_id = 15u;
    memset(&gates[1].selector, 0, sizeof(gates[1].selector));
    gates[1].selector.match_address = 1u;
    gates[1].selector.address_last = UINT16_MAX;
    gates[2] = gates[1];
    gates[2].gate_id = 16u;
    gates[2].selector.match_address_space = 1u;
    gates[2].selector.address_space = EM8051_DEBUG_SPACE_XDATA;
    CHECK(em8051_debug_runtime_replace_gates(runtime, gates, 3u) ==
          EM8051_DEBUG_EVENT_OK);
    watch = make_watch(17u, 1u, false);
    watch.address_space = EM8051_DEBUG_SPACE_CODE;
    watch.access_mask = EM8051_DEBUG_ACCESS_FETCH;
    CHECK(em8051_debug_runtime_replace_watches(runtime, &watch, 1u) ==
          EM8051_DEBUG_EVENT_OK);

    memset(&interrupt_event, 0, sizeof(interrupt_event));
    interrupt_event.kind = EM8051_DEBUG_EVENT_INTERRUPT_ENTER;
    CHECK(em8051_debug_runtime_ingest(runtime, &interrupt_event, &sequence) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(sequence == 1u);
    CHECK(em8051_debug_runtime_get_status(runtime, &status) ==
          EM8051_DEBUG_EVENT_OK && status.interrupt_depth == 1u);
    CHECK(em8051_debug_runtime_reset(runtime, 10u, 20u, 0x100u, &sequence) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(sequence == 2u);
    CHECK(em8051_debug_runtime_get_status(runtime, &status) ==
          EM8051_DEBUG_EVENT_OK && status.generation == 2u &&
          status.next_sequence == 3u && status.interrupt_depth == 0u);
    CHECK(em8051_debug_runtime_load(runtime, 11u, 21u, 0x101u, &sequence) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(sequence == 3u);
    CHECK(em8051_debug_runtime_list_points(runtime, 0u, listed_points, 3u,
                                           &written, &total) ==
          EM8051_DEBUG_EVENT_OK && written == 3u && total == 3u &&
          listed_points[0].enabled == 0u &&
          listed_points[1].enabled == 0u &&
          listed_points[2].enabled == 1u);
    CHECK(em8051_debug_runtime_list_gates(runtime, 0u, listed_gates, 3u,
                                          &written, &total) ==
          EM8051_DEBUG_EVENT_OK && written == 3u && total == 3u &&
          listed_gates[0].enabled == 0u &&
          listed_gates[1].enabled == 0u &&
          listed_gates[2].enabled == 1u);
    CHECK(em8051_debug_runtime_list_watches(runtime, 0u, &listed_watch, 1u,
                                            &written, &total) ==
          EM8051_DEBUG_EVENT_OK && listed_watch.enabled == 0u);
    CHECK(em8051_debug_runtime_get_status(runtime, &status) ==
          EM8051_DEBUG_EVENT_OK && status.generation == 3u &&
          status.next_sequence == 4u);
    CHECK(em8051_debug_runtime_read_ring(runtime, 4u, 0u, lifecycle_records,
                                         3u, &read, &remaining) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(read == 3u && remaining == 0u);
    CHECK(lifecycle_records[1].event.kind == EM8051_DEBUG_EVENT_RESET &&
          lifecycle_records[1].event.sequence == 2u &&
          lifecycle_records[1].event.generation == 2u);
    CHECK(lifecycle_records[2].event.kind == EM8051_DEBUG_EVENT_LOAD &&
          lifecycle_records[2].event.sequence == 3u &&
          lifecycle_records[2].event.generation == 3u);

    CHECK(em8051_debug_runtime_clear_session(runtime) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_runtime_get_status(runtime, &status) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(status.generation == 1u && status.next_sequence == 1u &&
          status.trace_count == 0u && status.destination_count == 0u &&
          status.point_count == 0u && status.gate_count == 0u &&
          status.watch_count == 0u && status.accepted_source_events == 0u);
    CHECK(em8051_debug_runtime_consume_stop(runtime, &stop) ==
          EM8051_DEBUG_EVENT_OK && stop.pending == 0u);
    em8051_debug_runtime_destroy(runtime);
    return 0;
}

static int test_bounds_and_invalid_ingest(void)
{
    struct em8051_debug_runtime *runtime = NULL;
    struct em8051_debug_watch
        watches[EM8051_DEBUG_WATCH_MAX_WATCHES + 1u];
    struct em8051_debug_trace_session
        traces[EM8051_DEBUG_TRACE_MAX_TRACES + 1u];
    struct em8051_debug_trace_destination
        destinations[EM8051_DEBUG_TRACE_MAX_DESTINATIONS + 1u];
    struct em8051_debug_trace_point
        points[EM8051_DEBUG_TRACE_MAX_POINTS + 1u];
    struct em8051_debug_trace_gate
        gates[EM8051_DEBUG_TRACE_MAX_GATES + 1u];
    struct em8051_debug_event event = write_event();
    struct em8051_debug_runtime_status before, after;
    uint32_t trace_ids[EM8051_DEBUG_TRACE_MAX_ROUTES + 1u];
    uint64_t assigned = UINT64_MAX;
    size_t written, total;

    memset(watches, 0, sizeof(watches));
    memset(traces, 0, sizeof(traces));
    memset(destinations, 0, sizeof(destinations));
    memset(points, 0, sizeof(points));
    memset(gates, 0, sizeof(gates));
    memset(trace_ids, 0, sizeof(trace_ids));
    CHECK(em8051_debug_runtime_create(&runtime) == EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_runtime_replace_watches(
              runtime, watches, EM8051_DEBUG_WATCH_MAX_WATCHES + 1u) ==
          EM8051_DEBUG_EVENT_LIMIT);
    CHECK(em8051_debug_runtime_replace_traces(
              runtime, traces, EM8051_DEBUG_TRACE_MAX_TRACES + 1u) ==
          EM8051_DEBUG_EVENT_LIMIT);
    CHECK(em8051_debug_runtime_replace_destinations(
              runtime, destinations,
              EM8051_DEBUG_TRACE_MAX_DESTINATIONS + 1u) ==
          EM8051_DEBUG_EVENT_LIMIT);
    CHECK(em8051_debug_runtime_replace_points(
              runtime, points, EM8051_DEBUG_TRACE_MAX_POINTS + 1u) ==
          EM8051_DEBUG_EVENT_LIMIT);
    CHECK(em8051_debug_runtime_replace_gates(
              runtime, gates, EM8051_DEBUG_TRACE_MAX_GATES + 1u) ==
          EM8051_DEBUG_EVENT_LIMIT);
    CHECK(em8051_debug_runtime_set_trace_enabled(
              runtime, trace_ids, EM8051_DEBUG_TRACE_MAX_ROUTES + 1u, true) ==
          EM8051_DEBUG_EVENT_LIMIT);
    CHECK(em8051_debug_runtime_list_destinations(runtime, 0u, NULL, 0u,
                                                 &written, &total) ==
          EM8051_DEBUG_EVENT_OK && written == 0u && total == 0u);
    CHECK(em8051_debug_runtime_get_status(runtime, &before) ==
          EM8051_DEBUG_EVENT_OK);
    event.kind = EM8051_DEBUG_EVENT_RESET;
    CHECK(em8051_debug_runtime_ingest(runtime, &event, &assigned) ==
          EM8051_DEBUG_EVENT_INVALID_ARGUMENT && assigned == UINT64_MAX);
    event.kind = EM8051_DEBUG_EVENT_LOAD;
    CHECK(em8051_debug_runtime_ingest(runtime, &event, &assigned) ==
          EM8051_DEBUG_EVENT_INVALID_ARGUMENT && assigned == UINT64_MAX);
    event.kind = EM8051_DEBUG_EVENT_MEMORY_WRITE;
    event.sequence = 9u;
    CHECK(em8051_debug_runtime_ingest(runtime, &event, NULL) ==
          EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    event.sequence = 0u;
    event.kind = EM8051_DEBUG_EVENT_WATCH_MATCH;
    CHECK(em8051_debug_runtime_ingest(runtime, &event, NULL) ==
          EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    event.kind = UINT8_MAX;
    CHECK(em8051_debug_runtime_ingest(runtime, &event, NULL) ==
          EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    event.kind = EM8051_DEBUG_EVENT_MEMORY_WRITE;
    event.address_space = UINT8_MAX;
    CHECK(em8051_debug_runtime_ingest(runtime, &event, NULL) ==
          EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    event.address_space = EM8051_DEBUG_SPACE_XDATA;
    event.new_value.known = 2u;
    CHECK(em8051_debug_runtime_ingest(runtime, &event, NULL) ==
          EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    CHECK(em8051_debug_runtime_get_status(runtime, &after) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(before.next_sequence == after.next_sequence &&
          after.rejected_source_events == 0u);
    em8051_debug_runtime_destroy(runtime);
    return 0;
}

static int test_trace_id_reuse_resets_only_on_clear(void)
{
    struct em8051_debug_runtime *runtime = NULL;
    struct em8051_debug_trace_destination destination;
    struct em8051_debug_trace_session trace;
    size_t written, total;

    memset(&destination, 0, sizeof(destination));
    destination.destination_id = 4u;
    memset(&trace, 0, sizeof(trace));
    trace.trace_id = 7u;
    trace.destination_id = 4u;
    trace.enabled = 1u;
    CHECK(em8051_debug_runtime_create(&runtime) == EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_runtime_replace_destinations(runtime, &destination,
                                                     1u) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_runtime_replace_traces(runtime, &trace, 1u) ==
          EM8051_DEBUG_EVENT_OK);
    trace.enabled = 0u;
    CHECK(em8051_debug_runtime_replace_traces(runtime, &trace, 1u) ==
          EM8051_DEBUG_EVENT_OK); /* live update */
    CHECK(em8051_debug_runtime_replace_traces(runtime, NULL, 0u) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_runtime_replace_traces(runtime, &trace, 1u) ==
          EM8051_DEBUG_EVENT_DUPLICATE_ID);
    CHECK(em8051_debug_runtime_list_traces(runtime, 0u, NULL, 0u, &written,
                                           &total) ==
          EM8051_DEBUG_EVENT_OK && written == 0u && total == 0u);
    CHECK(em8051_debug_runtime_clear_session(runtime) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_runtime_replace_destinations(runtime, &destination,
                                                     1u) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_runtime_replace_traces(runtime, &trace, 1u) ==
          EM8051_DEBUG_EVENT_OK);
    em8051_debug_runtime_destroy(runtime);
    return 0;
}

static int test_stop_priority_across_pending_sources(void)
{
    struct em8051_debug_runtime *runtime = NULL;
    struct em8051_debug_watch watch;
    struct em8051_debug_event event = write_event();
    struct em8051_debug_runtime_stop stop;
    uint64_t sequence;

    CHECK(em8051_debug_runtime_create(&runtime) == EM8051_DEBUG_EVENT_OK);
    watch = make_watch(20u, 1u, false);
    CHECK(em8051_debug_runtime_replace_watches(runtime, &watch, 1u) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_runtime_ingest(runtime, &event, &sequence) ==
          EM8051_DEBUG_EVENT_OK && sequence == 1u);
    watch = make_watch(3u, 1u, false);
    CHECK(em8051_debug_runtime_replace_watches(runtime, &watch, 1u) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_runtime_ingest(runtime, &event, &sequence) ==
          EM8051_DEBUG_EVENT_OK && sequence == 3u);
    CHECK(em8051_debug_runtime_consume_stop(runtime, &stop) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(stop.pending == 1u && stop.watch_id == 3u &&
          stop.source_event_sequence == 3u &&
          stop.matching_watch_count == 2u);
    em8051_debug_runtime_destroy(runtime);
    return 0;
}

int main(void)
{
    CHECK(test_source_derived_order_gates_stop_and_pages() == 0);
    CHECK(test_pending_overflow_is_neutral() == 0);
    CHECK(test_replacement_atomicity_and_enable() == 0);
    CHECK(test_lifecycle_and_code_invalidation() == 0);
    CHECK(test_bounds_and_invalid_ingest() == 0);
    CHECK(test_stop_priority_across_pending_sources() == 0);
    CHECK(test_trace_id_reuse_resets_only_on_clear() == 0);
    puts("debug dispatcher/runtime tests passed");
    return 0;
}

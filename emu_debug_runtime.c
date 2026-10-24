/* Opaque in-memory deterministic debugger trace runtime.
 * Copyright 2026 Hans-Einar
 * Distributed under the MIT license; see LICENSE.
 */
#include "emu_debug_runtime.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

enum dispatch_phase {
    DISPATCH_IDLE = 0,
    DISPATCH_SOURCE,
    DISPATCH_DERIVED
};

struct em8051_debug_runtime {
    struct em8051_debug_event_bus bus;
    struct em8051_debug_watch_table watches;
    struct em8051_debug_trace_router router;
    struct em8051_debug_watch_result
        pending[EM8051_DEBUG_RUNTIME_PENDING_CAPACITY];
    struct em8051_debug_event source_event;
    struct em8051_debug_runtime_stop stop;
    uint64_t accepted_source_events;
    uint64_t rejected_source_events;
    uint64_t derived_events;
    uint64_t pending_overflows;
    uint64_t console_matches;
    size_t pending_count;
    size_t derived_index;
    uint32_t observer_id;
    uint32_t generation;
    enum em8051_debug_event_status dispatch_status;
    uint8_t phase;
    uint8_t busy;
};

static void saturated_increment(uint64_t *aValue)
{
    if (*aValue != UINT64_MAX)
        ++*aValue;
}

static void saturated_add(uint64_t *aValue, uint64_t aAddend)
{
    if (UINT64_MAX - *aValue < aAddend)
        *aValue = UINT64_MAX;
    else
        *aValue += aAddend;
}

static int runtime_trace_index(const struct em8051_debug_trace_config *aConfig,
                               uint32_t aTraceId)
{
    size_t i;

    for (i = 0u; i < aConfig->trace_count; ++i)
        if (aConfig->traces[i].trace_id == aTraceId)
            return (int)i;
    return -1;
}

static int runtime_destination_index(
    const struct em8051_debug_trace_config *aConfig, uint32_t aDestinationId)
{
    size_t i;

    for (i = 0u; i < aConfig->destination_count; ++i)
        if (aConfig->destinations[i].destination_id == aDestinationId)
            return (int)i;
    return -1;
}

static bool watch_routes_valid(
    const struct em8051_debug_watch_table *aWatches,
    const struct em8051_debug_trace_config *aConfig)
{
    size_t wi;

    for (wi = 0u; wi < aWatches->watch_count; ++wi) {
        const struct em8051_debug_watch *watch = &aWatches->watches[wi];
        size_t ai;

        for (ai = 0u; ai < watch->action_count; ++ai) {
            const struct em8051_debug_watch_action *action =
                &watch->actions[ai];
            size_t ri;

            if (action->kind != EM8051_DEBUG_WATCH_ROUTE)
                continue;
            for (ri = 0u; ri < action->trace_id_count; ++ri)
                if (runtime_trace_index(aConfig, action->trace_ids[ri]) < 0)
                    return false;
        }
    }
    return true;
}

static bool sequence_room(const struct em8051_debug_event_bus *aBus,
                          size_t aAdditionalEvents)
{
    uint64_t additional;

    additional = (uint64_t)aAdditionalEvents;
    if (additional == 0u)
        return true;
    return aBus->next_sequence != 0u &&
           additional - 1u <= UINT64_MAX - aBus->next_sequence;
}

static bool event_value_valid(const struct em8051_debug_value *aValue)
{
    return aValue->known <= 1u && aValue->is_signed <= 1u &&
           (aValue->width == 0u || aValue->width == 8u ||
            aValue->width == 16u || aValue->width == 32u ||
            aValue->width == 64u) &&
           (aValue->known == 0u || aValue->width != 0u);
}

static bool source_event_valid(const struct em8051_debug_event *aEvent)
{
    const uint8_t all_access = EM8051_DEBUG_ACCESS_FETCH |
                               EM8051_DEBUG_ACCESS_READ |
                               EM8051_DEBUG_ACCESS_WRITE |
                               EM8051_DEBUG_ACCESS_RMW;

    return aEvent->kind <= EM8051_DEBUG_EVENT_DEBUGGER_MUTATION &&
           aEvent->address_space <= EM8051_DEBUG_SPACE_XDATA &&
           (aEvent->access & (uint8_t)~all_access) == 0u &&
           event_value_valid(&aEvent->old_value) &&
           event_value_valid(&aEvent->new_value);
}

static void update_stop_candidate(struct em8051_debug_runtime_stop *aStop,
                                  uint32_t aWatchId,
                                  uint64_t aSourceSequence)
{
    saturated_increment(&aStop->matching_watch_count);
    if (!aStop->pending || aWatchId < aStop->watch_id ||
        (aWatchId == aStop->watch_id &&
         aSourceSequence < aStop->source_event_sequence)) {
        aStop->watch_id = aWatchId;
        aStop->source_event_sequence = aSourceSequence;
    }
    aStop->pending = 1u;
}

static void runtime_observer(const struct em8051_debug_event *aEvent,
                             void *aUser)
{
    struct em8051_debug_runtime *runtime =
        (struct em8051_debug_runtime *)aUser;

    if (runtime->dispatch_status != EM8051_DEBUG_EVENT_OK)
        return;
    if (runtime->phase == DISPATCH_SOURCE) {
        size_t required = 0u;
        enum em8051_debug_event_status status;

        if (aEvent->kind == EM8051_DEBUG_EVENT_WATCH_MATCH) {
            runtime->dispatch_status = EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
            return;
        }
        status = em8051_debug_watch_table_match(
            &runtime->watches, aEvent, runtime->pending,
            EM8051_DEBUG_RUNTIME_PENDING_CAPACITY, &required);
        if (status == EM8051_DEBUG_EVENT_LIMIT ||
            required > EM8051_DEBUG_RUNTIME_PENDING_CAPACITY) {
            runtime->pending_count = 0u;
            saturated_increment(&runtime->pending_overflows);
            runtime->dispatch_status = EM8051_DEBUG_EVENT_LIMIT;
            return;
        }
        if (status != EM8051_DEBUG_EVENT_OK ||
            !sequence_room(&runtime->bus, required)) {
            runtime->pending_count = 0u;
            runtime->dispatch_status = status == EM8051_DEBUG_EVENT_OK ?
                EM8051_DEBUG_EVENT_LIMIT : status;
            return;
        }
        runtime->pending_count = required;
        runtime->source_event = *aEvent;
        runtime->dispatch_status = em8051_debug_trace_router_source_begin(
            &runtime->router, aEvent);
        return;
    }
    if (runtime->phase == DISPATCH_DERIVED) {
        if (runtime->derived_index >= runtime->pending_count ||
            aEvent->kind != EM8051_DEBUG_EVENT_WATCH_MATCH) {
            runtime->dispatch_status = EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
            return;
        }
        runtime->dispatch_status = em8051_debug_trace_router_watch(
            &runtime->router, aEvent,
            &runtime->pending[runtime->derived_index]);
        return;
    }
    runtime->dispatch_status = EM8051_DEBUG_EVENT_BUSY;
}

static enum em8051_debug_event_status runtime_initialize(
    struct em8051_debug_runtime *aRuntime)
{
    enum em8051_debug_event_status status;

    memset(aRuntime, 0, sizeof(*aRuntime));
    em8051_debug_event_bus_init(&aRuntime->bus);
    em8051_debug_watch_table_init(&aRuntime->watches);
    em8051_debug_trace_router_init(&aRuntime->router);
    aRuntime->generation = 1u;
    status = em8051_debug_event_bus_subscribe(
        &aRuntime->bus, runtime_observer, aRuntime, &aRuntime->observer_id);
    return status;
}

enum em8051_debug_event_status em8051_debug_runtime_create(
    struct em8051_debug_runtime **aRuntime)
{
    struct em8051_debug_runtime *runtime;
    enum em8051_debug_event_status status;

    if (aRuntime == NULL)
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    *aRuntime = NULL;
    runtime = (struct em8051_debug_runtime *)calloc(1u, sizeof(*runtime));
    if (runtime == NULL)
        return EM8051_DEBUG_EVENT_LIMIT;
    status = runtime_initialize(runtime);
    if (status != EM8051_DEBUG_EVENT_OK) {
        free(runtime);
        return status;
    }
    *aRuntime = runtime;
    return EM8051_DEBUG_EVENT_OK;
}

void em8051_debug_runtime_destroy(struct em8051_debug_runtime *aRuntime)
{
    free(aRuntime);
}

static enum em8051_debug_event_status replace_config(
    struct em8051_debug_runtime *aRuntime,
    const struct em8051_debug_trace_config *aCandidate)
{
    if (!watch_routes_valid(&aRuntime->watches, aCandidate))
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    return em8051_debug_trace_router_replace(&aRuntime->router, aCandidate);
}

enum em8051_debug_event_status em8051_debug_runtime_replace_watches(
    struct em8051_debug_runtime *aRuntime,
    const struct em8051_debug_watch *aWatches, size_t aCount)
{
    struct em8051_debug_watch_table candidate;
    enum em8051_debug_event_status status;

    if (aRuntime == NULL || aRuntime->busy)
        return aRuntime == NULL ? EM8051_DEBUG_EVENT_INVALID_ARGUMENT :
                                  EM8051_DEBUG_EVENT_BUSY;
    em8051_debug_watch_table_init(&candidate);
    status = em8051_debug_watch_table_replace(&candidate, aWatches, aCount);
    if (status != EM8051_DEBUG_EVENT_OK)
        return status;
    if (!watch_routes_valid(&candidate, &aRuntime->router.config))
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    aRuntime->watches = candidate;
    return EM8051_DEBUG_EVENT_OK;
}

#define DEFINE_REPLACE(name, type, member, count_member, maximum)                 \
enum em8051_debug_event_status em8051_debug_runtime_replace_##name(               \
    struct em8051_debug_runtime *aRuntime, const struct type *aValues,             \
    size_t aCount)                                                                 \
{                                                                                 \
    struct em8051_debug_trace_config candidate;                                   \
    if (aRuntime == NULL || (aCount != 0u && aValues == NULL))                    \
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;                               \
    if (aRuntime->busy)                                                            \
        return EM8051_DEBUG_EVENT_BUSY;                                            \
    if (aCount > (maximum))                                                        \
        return EM8051_DEBUG_EVENT_LIMIT;                                           \
    candidate = aRuntime->router.config;                                           \
    memset(candidate.member, 0, sizeof(candidate.member));                         \
    if (aCount != 0u)                                                              \
        memcpy(candidate.member, aValues, aCount * sizeof(candidate.member[0]));   \
    candidate.count_member = (uint8_t)aCount;                                     \
    return replace_config(aRuntime, &candidate);                                  \
}

DEFINE_REPLACE(traces, em8051_debug_trace_session, traces, trace_count,
               EM8051_DEBUG_TRACE_MAX_TRACES)
DEFINE_REPLACE(destinations, em8051_debug_trace_destination, destinations,
               destination_count, EM8051_DEBUG_TRACE_MAX_DESTINATIONS)
DEFINE_REPLACE(points, em8051_debug_trace_point, points, point_count,
               EM8051_DEBUG_TRACE_MAX_POINTS)
DEFINE_REPLACE(gates, em8051_debug_trace_gate, gates, gate_count,
               EM8051_DEBUG_TRACE_MAX_GATES)

#undef DEFINE_REPLACE

enum em8051_debug_event_status em8051_debug_runtime_set_trace_enabled(
    struct em8051_debug_runtime *aRuntime, const uint32_t *aTraceIds,
    size_t aCount, bool aEnabled)
{
    struct em8051_debug_trace_config candidate;
    size_t i;

    if (aRuntime == NULL || aTraceIds == NULL || aCount == 0u)
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    if (aRuntime->busy)
        return EM8051_DEBUG_EVENT_BUSY;
    if (aCount > EM8051_DEBUG_TRACE_MAX_ROUTES)
        return EM8051_DEBUG_EVENT_LIMIT;
    candidate = aRuntime->router.config;
    for (i = 0u; i < aCount; ++i) {
        int index;

        if (aTraceIds[i] == 0u ||
            (i != 0u && aTraceIds[i - 1u] >= aTraceIds[i]))
            return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
        index = runtime_trace_index(&candidate, aTraceIds[i]);
        if (index < 0)
            return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
        candidate.traces[index].enabled = aEnabled ? 1u : 0u;
    }
    return replace_config(aRuntime, &candidate);
}

#define DEFINE_LIST(name, type, container, array_member, count_member)             \
enum em8051_debug_event_status em8051_debug_runtime_list_##name(                   \
    const struct em8051_debug_runtime *aRuntime, size_t aOffset,                   \
    struct type *aValues, size_t aCapacity, size_t *aWritten, size_t *aTotal)      \
{                                                                                 \
    size_t available, written;                                                     \
    if (aRuntime == NULL || aWritten == NULL || aTotal == NULL ||                 \
        (aCapacity != 0u && aValues == NULL))                                      \
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;                               \
    *aTotal = aRuntime->container.count_member;                                    \
    available = aOffset < *aTotal ? *aTotal - aOffset : 0u;                       \
    written = available < aCapacity ? available : aCapacity;                      \
    if (written != 0u)                                                             \
        memcpy(aValues, &aRuntime->container.array_member[aOffset],                \
               written * sizeof(aValues[0]));                                     \
    *aWritten = written;                                                           \
    return EM8051_DEBUG_EVENT_OK;                                                  \
}

DEFINE_LIST(watches, em8051_debug_watch, watches, watches, watch_count)
DEFINE_LIST(traces, em8051_debug_trace_session, router.config, traces, trace_count)
DEFINE_LIST(destinations, em8051_debug_trace_destination, router.config,
            destinations,
            destination_count)
DEFINE_LIST(points, em8051_debug_trace_point, router.config, points, point_count)
DEFINE_LIST(gates, em8051_debug_trace_gate, router.config, gates, gate_count)

#undef DEFINE_LIST

static void merge_stops_and_counts(struct em8051_debug_runtime *aRuntime)
{
    size_t i;

    for (i = 0u; i < aRuntime->pending_count; ++i) {
        const struct em8051_debug_watch_result *result = &aRuntime->pending[i];

        if (result->stop)
            update_stop_candidate(&aRuntime->stop, result->watch_id,
                                  result->source_event_sequence);
        if (result->console)
            saturated_increment(&aRuntime->console_matches);
    }
    saturated_add(&aRuntime->derived_events,
                  (uint64_t)aRuntime->pending_count);
}

static enum em8051_debug_event_status runtime_ingest(
    struct em8051_debug_runtime *aRuntime,
    const struct em8051_debug_event *aSourceEvent,
    uint64_t *aAssignedSequence, bool aAllowLifecycle)
{
    struct em8051_debug_event source;
    enum em8051_debug_event_status status;
    size_t i;

    if (aRuntime == NULL || aSourceEvent == NULL ||
        aSourceEvent->sequence != 0u ||
        (!aAllowLifecycle &&
         (aSourceEvent->kind == EM8051_DEBUG_EVENT_RESET ||
          aSourceEvent->kind == EM8051_DEBUG_EVENT_LOAD)) ||
        !source_event_valid(aSourceEvent))
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    if (aRuntime->busy)
        return EM8051_DEBUG_EVENT_BUSY;
    aRuntime->busy = 1u;
    aRuntime->phase = DISPATCH_SOURCE;
    aRuntime->dispatch_status = EM8051_DEBUG_EVENT_OK;
    aRuntime->pending_count = 0u;
    aRuntime->derived_index = 0u;
    source = *aSourceEvent;
    source.generation = aRuntime->generation;
    status = em8051_debug_event_bus_emit(&aRuntime->bus, &source,
                                         aAssignedSequence);
    if (status == EM8051_DEBUG_EVENT_OK)
        status = aRuntime->dispatch_status;
    if (status != EM8051_DEBUG_EVENT_OK) {
        saturated_increment(&aRuntime->rejected_source_events);
        aRuntime->phase = DISPATCH_IDLE;
        aRuntime->busy = 0u;
        return status;
    }

    aRuntime->phase = DISPATCH_DERIVED;
    for (i = 0u; i < aRuntime->pending_count; ++i) {
        struct em8051_debug_event derived = aRuntime->source_event;

        derived.sequence = 0u;
        derived.kind = EM8051_DEBUG_EVENT_WATCH_MATCH;
        aRuntime->derived_index = i;
        aRuntime->dispatch_status = EM8051_DEBUG_EVENT_OK;
        status = em8051_debug_event_bus_emit(&aRuntime->bus, &derived, NULL);
        if (status == EM8051_DEBUG_EVENT_OK)
            status = aRuntime->dispatch_status;
        if (status != EM8051_DEBUG_EVENT_OK)
            break;
    }
    if (aRuntime->router.source_open) {
        enum em8051_debug_event_status end_status =
            em8051_debug_trace_router_source_end(&aRuntime->router);
        if (status == EM8051_DEBUG_EVENT_OK)
            status = end_status;
    }
    if (status == EM8051_DEBUG_EVENT_OK) {
        merge_stops_and_counts(aRuntime);
        saturated_increment(&aRuntime->accepted_source_events);
    } else {
        saturated_increment(&aRuntime->rejected_source_events);
    }
    aRuntime->phase = DISPATCH_IDLE;
    aRuntime->busy = 0u;
    return status;
}

enum em8051_debug_event_status em8051_debug_runtime_ingest(
    struct em8051_debug_runtime *aRuntime,
    const struct em8051_debug_event *aSourceEvent,
    uint64_t *aAssignedSequence)
{
    return runtime_ingest(aRuntime, aSourceEvent, aAssignedSequence, false);
}

static enum em8051_debug_event_status lifecycle_event(
    struct em8051_debug_runtime *aRuntime, uint8_t aKind,
    uint64_t aInstructionCount, uint64_t aMachineCycleCount, uint16_t aPc,
    uint64_t *aAssignedSequence)
{
    struct em8051_debug_event event;
    enum em8051_debug_event_status status;
    uint32_t old_generation;
    uint32_t old_interrupt_depth;

    if (aRuntime == NULL)
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    if (aRuntime->busy)
        return EM8051_DEBUG_EVENT_BUSY;
    if (aRuntime->generation == UINT32_MAX ||
        !sequence_room(&aRuntime->bus, 1u))
        return EM8051_DEBUG_EVENT_LIMIT;
    memset(&event, 0, sizeof(event));
    event.kind = aKind;
    event.instruction_count = aInstructionCount;
    event.machine_cycle_count = aMachineCycleCount;
    event.pc = aPc;
    old_generation = aRuntime->generation;
    old_interrupt_depth = aRuntime->router.interrupt_depth;
    ++aRuntime->generation;
    aRuntime->router.interrupt_depth = 0u;
    status = runtime_ingest(aRuntime, &event, aAssignedSequence, true);
    if (status != EM8051_DEBUG_EVENT_OK) {
        aRuntime->generation = old_generation;
        aRuntime->router.interrupt_depth = old_interrupt_depth;
    }
    return status;
}

enum em8051_debug_event_status em8051_debug_runtime_reset(
    struct em8051_debug_runtime *aRuntime, uint64_t aInstructionCount,
    uint64_t aMachineCycleCount, uint16_t aPc,
    uint64_t *aAssignedSequence)
{
    return lifecycle_event(aRuntime, EM8051_DEBUG_EVENT_RESET,
                           aInstructionCount, aMachineCycleCount, aPc,
                           aAssignedSequence);
}

static bool code_selector(const struct em8051_debug_trace_selector *aSelector)
{
    return aSelector->match_pc ||
           (aSelector->match_address &&
            (!aSelector->match_address_space ||
             aSelector->address_space == EM8051_DEBUG_SPACE_CODE)) ||
           (aSelector->match_address_space &&
            aSelector->address_space == EM8051_DEBUG_SPACE_CODE);
}

enum em8051_debug_event_status em8051_debug_runtime_load(
    struct em8051_debug_runtime *aRuntime, uint64_t aInstructionCount,
    uint64_t aMachineCycleCount, uint16_t aPc,
    uint64_t *aAssignedSequence)
{
    struct em8051_debug_trace_config candidate;
    struct em8051_debug_watch_table watches;
    enum em8051_debug_event_status status;
    size_t i;

    if (aRuntime == NULL)
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    if (aRuntime->busy)
        return EM8051_DEBUG_EVENT_BUSY;
    if (aRuntime->generation == UINT32_MAX ||
        !sequence_room(&aRuntime->bus, 1u))
        return EM8051_DEBUG_EVENT_LIMIT;
    candidate = aRuntime->router.config;
    watches = aRuntime->watches;
    for (i = 0u; i < candidate.point_count; ++i)
        if (code_selector(&candidate.points[i].selector))
            candidate.points[i].enabled = 0u;
    for (i = 0u; i < candidate.gate_count; ++i)
        if (code_selector(&candidate.gates[i].selector))
            candidate.gates[i].enabled = 0u;
    for (i = 0u; i < watches.watch_count; ++i)
        if (watches.watches[i].address_space == EM8051_DEBUG_SPACE_CODE)
            watches.watches[i].enabled = 0u;
    status = replace_config(aRuntime, &candidate);
    if (status != EM8051_DEBUG_EVENT_OK)
        return status;
    aRuntime->watches = watches;
    return lifecycle_event(aRuntime, EM8051_DEBUG_EVENT_LOAD,
                           aInstructionCount, aMachineCycleCount, aPc,
                           aAssignedSequence);
}

enum em8051_debug_event_status em8051_debug_runtime_clear_session(
    struct em8051_debug_runtime *aRuntime)
{
    if (aRuntime == NULL)
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    if (aRuntime->busy)
        return EM8051_DEBUG_EVENT_BUSY;
    return runtime_initialize(aRuntime);
}

enum em8051_debug_event_status em8051_debug_runtime_get_status(
    const struct em8051_debug_runtime *aRuntime,
    struct em8051_debug_runtime_status *aStatus)
{
    if (aRuntime == NULL || aStatus == NULL)
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    memset(aStatus, 0, sizeof(*aStatus));
    aStatus->next_sequence = aRuntime->bus.next_sequence;
    aStatus->accepted_source_events = aRuntime->accepted_source_events;
    aStatus->rejected_source_events = aRuntime->rejected_source_events;
    aStatus->derived_events = aRuntime->derived_events;
    aStatus->pending_overflows = aRuntime->pending_overflows;
    aStatus->console_matches = aRuntime->console_matches;
    aStatus->generation = aRuntime->generation;
    aStatus->interrupt_depth = aRuntime->router.interrupt_depth;
    aStatus->trace_count = aRuntime->router.config.trace_count;
    aStatus->destination_count = aRuntime->router.config.destination_count;
    aStatus->point_count = aRuntime->router.config.point_count;
    aStatus->gate_count = aRuntime->router.config.gate_count;
    aStatus->watch_count = aRuntime->watches.watch_count;
    aStatus->stop_pending = aRuntime->stop.pending;
    aStatus->busy = aRuntime->busy;
    return EM8051_DEBUG_EVENT_OK;
}

enum em8051_debug_event_status em8051_debug_runtime_consume_stop(
    struct em8051_debug_runtime *aRuntime,
    struct em8051_debug_runtime_stop *aStop)
{
    if (aRuntime == NULL || aStop == NULL)
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    if (aRuntime->busy)
        return EM8051_DEBUG_EVENT_BUSY;
    *aStop = aRuntime->stop;
    memset(&aRuntime->stop, 0, sizeof(aRuntime->stop));
    return EM8051_DEBUG_EVENT_OK;
}

enum em8051_debug_event_status em8051_debug_runtime_read_ring(
    const struct em8051_debug_runtime *aRuntime, uint32_t aDestinationId,
    size_t aOffset, struct em8051_debug_trace_record *aRecords, size_t aCapacity,
    size_t *aRead, size_t *aRemaining)
{
    int destination;
    const struct em8051_debug_trace_ring *ring;
    size_t available;
    size_t count;
    size_t i;

    if (aRuntime == NULL || aRead == NULL || aRemaining == NULL ||
        (aCapacity != 0u && aRecords == NULL))
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    if (aRuntime->busy)
        return EM8051_DEBUG_EVENT_BUSY;
    destination = runtime_destination_index(&aRuntime->router.config,
                                            aDestinationId);
    if (destination < 0)
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    ring = &aRuntime->router.rings[destination];
    available = aOffset < ring->count ? (size_t)ring->count - aOffset : 0u;
    count = available < aCapacity ? available : aCapacity;
    for (i = 0u; i < count; ++i) {
        size_t index = ((size_t)ring->head + aOffset + i) %
                       EM8051_DEBUG_TRACE_RING_CAPACITY;

        aRecords[i] = ring->records[index];
    }
    *aRead = count;
    *aRemaining = available - count;
    return EM8051_DEBUG_EVENT_OK;
}

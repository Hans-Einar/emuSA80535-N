/* Opaque in-memory deterministic debugger trace runtime.
 * Copyright 2026 Hans-Einar
 * Distributed under the MIT license; see LICENSE.
 */
#ifndef EMU_DEBUG_RUNTIME_H
#define EMU_DEBUG_RUNTIME_H

#include "emu_debug_trace.h"

#define EM8051_DEBUG_RUNTIME_PENDING_CAPACITY 32u
#define EM8051_DEBUG_RUNTIME_API_VERSION 1u

struct em8051_debug_runtime;

struct em8051_debug_runtime_status {
    uint64_t next_sequence;
    uint64_t accepted_source_events;
    uint64_t rejected_source_events;
    uint64_t derived_events;
    uint64_t pending_overflows;
    uint64_t console_matches;
    uint32_t generation;
    uint32_t interrupt_depth;
    uint8_t trace_count;
    uint8_t destination_count;
    uint8_t point_count;
    uint8_t gate_count;
    uint8_t watch_count;
    uint8_t stop_pending;
    uint8_t busy;
};

struct em8051_debug_runtime_stop {
    uint64_t source_event_sequence;
    uint64_t matching_watch_count;
    uint32_t watch_id;
    uint8_t pending;
};

enum em8051_debug_event_status em8051_debug_runtime_create(
    struct em8051_debug_runtime **aRuntime);
void em8051_debug_runtime_destroy(struct em8051_debug_runtime *aRuntime);

enum em8051_debug_event_status em8051_debug_runtime_replace_watches(
    struct em8051_debug_runtime *aRuntime,
    const struct em8051_debug_watch *aWatches, size_t aCount);
enum em8051_debug_event_status em8051_debug_runtime_replace_traces(
    struct em8051_debug_runtime *aRuntime,
    const struct em8051_debug_trace_session *aTraces, size_t aCount);
enum em8051_debug_event_status em8051_debug_runtime_replace_destinations(
    struct em8051_debug_runtime *aRuntime,
    const struct em8051_debug_trace_destination *aDestinations,
    size_t aCount);
enum em8051_debug_event_status em8051_debug_runtime_replace_points(
    struct em8051_debug_runtime *aRuntime,
    const struct em8051_debug_trace_point *aPoints, size_t aCount);
enum em8051_debug_event_status em8051_debug_runtime_replace_gates(
    struct em8051_debug_runtime *aRuntime,
    const struct em8051_debug_trace_gate *aGates, size_t aCount);
enum em8051_debug_event_status em8051_debug_runtime_set_trace_enabled(
    struct em8051_debug_runtime *aRuntime, const uint32_t *aTraceIds,
    size_t aCount, bool aEnabled);

enum em8051_debug_event_status em8051_debug_runtime_list_watches(
    const struct em8051_debug_runtime *aRuntime, size_t aOffset,
    struct em8051_debug_watch *aWatches, size_t aCapacity,
    size_t *aWritten, size_t *aTotal);
enum em8051_debug_event_status em8051_debug_runtime_list_traces(
    const struct em8051_debug_runtime *aRuntime, size_t aOffset,
    struct em8051_debug_trace_session *aTraces, size_t aCapacity,
    size_t *aWritten, size_t *aTotal);
enum em8051_debug_event_status em8051_debug_runtime_list_destinations(
    const struct em8051_debug_runtime *aRuntime, size_t aOffset,
    struct em8051_debug_trace_destination *aDestinations, size_t aCapacity,
    size_t *aWritten, size_t *aTotal);
enum em8051_debug_event_status em8051_debug_runtime_list_points(
    const struct em8051_debug_runtime *aRuntime, size_t aOffset,
    struct em8051_debug_trace_point *aPoints, size_t aCapacity,
    size_t *aWritten, size_t *aTotal);
enum em8051_debug_event_status em8051_debug_runtime_list_gates(
    const struct em8051_debug_runtime *aRuntime, size_t aOffset,
    struct em8051_debug_trace_gate *aGates, size_t aCapacity,
    size_t *aWritten, size_t *aTotal);

enum em8051_debug_event_status em8051_debug_runtime_ingest(
    struct em8051_debug_runtime *aRuntime,
    const struct em8051_debug_event *aSourceEvent,
    uint64_t *aAssignedSequence);
enum em8051_debug_event_status em8051_debug_runtime_reset(
    struct em8051_debug_runtime *aRuntime, uint64_t aInstructionCount,
    uint64_t aMachineCycleCount, uint16_t aPc,
    uint64_t *aAssignedSequence);
enum em8051_debug_event_status em8051_debug_runtime_load(
    struct em8051_debug_runtime *aRuntime, uint64_t aInstructionCount,
    uint64_t aMachineCycleCount, uint16_t aPc,
    uint64_t *aAssignedSequence);
enum em8051_debug_event_status em8051_debug_runtime_clear_session(
    struct em8051_debug_runtime *aRuntime);

enum em8051_debug_event_status em8051_debug_runtime_get_status(
    const struct em8051_debug_runtime *aRuntime,
    struct em8051_debug_runtime_status *aStatus);
enum em8051_debug_event_status em8051_debug_runtime_consume_stop(
    struct em8051_debug_runtime *aRuntime,
    struct em8051_debug_runtime_stop *aStop);
/* Copy a stable, non-destructive page in oldest-to-newest ring order. Offset
 * zero names the current oldest retained record. aRemaining is the number of
 * records following the returned page. Call only at a stopped safe boundary. */
enum em8051_debug_event_status em8051_debug_runtime_read_ring(
    const struct em8051_debug_runtime *aRuntime, uint32_t aDestinationId,
    size_t aOffset, struct em8051_debug_trace_record *aRecords, size_t aCapacity,
    size_t *aRead, size_t *aRemaining);

#endif

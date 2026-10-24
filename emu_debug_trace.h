/* Generic deterministic multi-trace router.
 * Copyright 2026 Hans-Einar
 * Distributed under the MIT license; see LICENSE.
 */
#ifndef EMU_DEBUG_TRACE_H
#define EMU_DEBUG_TRACE_H

#include "emu_debug_event.h"

#define EM8051_DEBUG_TRACE_MAX_TRACES 16u
#define EM8051_DEBUG_TRACE_MAX_DESTINATIONS 8u
#define EM8051_DEBUG_TRACE_MAX_POINTS 32u
#define EM8051_DEBUG_TRACE_MAX_GATES 32u
#define EM8051_DEBUG_TRACE_MAX_ROUTES 16u
#define EM8051_DEBUG_TRACE_MAX_SEEN_IDS 64u
#define EM8051_DEBUG_TRACE_RING_CAPACITY 128u
#define EM8051_DEBUG_TRACE_TAG_BYTES 65u
#define EM8051_DEBUG_TRACE_COMMENT_BYTES 257u

enum em8051_debug_trace_interrupt_policy {
    EM8051_DEBUG_TRACE_INCLUDE = 0,
    EM8051_DEBUG_TRACE_SUPPRESS_DURING_INTERRUPT,
    EM8051_DEBUG_TRACE_INTERRUPT_ONLY
};

enum em8051_debug_trace_gate_action {
    EM8051_DEBUG_TRACE_GATE_ON = 0,
    EM8051_DEBUG_TRACE_GATE_OFF
};

enum em8051_debug_trace_gate_timing {
    EM8051_DEBUG_TRACE_GATE_BEFORE = 0,
    EM8051_DEBUG_TRACE_GATE_AFTER
};

enum em8051_debug_trace_record_kind {
    EM8051_DEBUG_TRACE_RECORD_EVENT = 0,
    EM8051_DEBUG_TRACE_RECORD_SUPPRESSION
};

struct em8051_debug_trace_selector {
    uint16_t pc_first;
    uint16_t pc_last;
    uint16_t address_first;
    uint16_t address_last;
    uint8_t kind;
    uint8_t match_kind;
    uint8_t match_pc;
    uint8_t match_address;
    uint8_t address_space;
    uint8_t match_address_space;
};

struct em8051_debug_trace_session {
    uint32_t trace_id;
    uint32_t destination_id;
    char tag[EM8051_DEBUG_TRACE_TAG_BYTES];
    char comment[EM8051_DEBUG_TRACE_COMMENT_BYTES];
    uint8_t enabled;
    uint8_t interrupt_policy;
};

struct em8051_debug_trace_destination {
    uint32_t destination_id;
};

struct em8051_debug_trace_point {
    struct em8051_debug_trace_selector selector;
    uint32_t trace_ids[EM8051_DEBUG_TRACE_MAX_ROUTES];
    uint32_t point_id;
    uint8_t trace_id_count;
    uint8_t enabled;
};

struct em8051_debug_trace_gate {
    struct em8051_debug_trace_selector selector;
    uint32_t trace_ids[EM8051_DEBUG_TRACE_MAX_ROUTES];
    uint32_t gate_id;
    uint8_t trace_id_count;
    uint8_t enabled;
    uint8_t action;
    uint8_t timing;
};

struct em8051_debug_trace_config {
    struct em8051_debug_trace_session traces[EM8051_DEBUG_TRACE_MAX_TRACES];
    struct em8051_debug_trace_destination
        destinations[EM8051_DEBUG_TRACE_MAX_DESTINATIONS];
    struct em8051_debug_trace_point points[EM8051_DEBUG_TRACE_MAX_POINTS];
    struct em8051_debug_trace_gate gates[EM8051_DEBUG_TRACE_MAX_GATES];
    uint8_t trace_count;
    uint8_t destination_count;
    uint8_t point_count;
    uint8_t gate_count;
};

struct em8051_debug_trace_suppression {
    uint64_t first_sequence;
    uint64_t last_sequence;
    uint64_t count;
    uint32_t trace_id;
    uint32_t entry_depth;
    uint32_t maximum_depth;
    uint8_t interrupt_policy;
};

struct em8051_debug_trace_record {
    struct em8051_debug_event event;
    struct em8051_debug_watch_result watch_result;
    struct em8051_debug_trace_suppression suppression;
    uint32_t trace_ids[EM8051_DEBUG_TRACE_MAX_TRACES];
    uint32_t destination_id;
    uint8_t kind;
    uint8_t trace_id_count;
};

struct em8051_debug_trace_ring {
    struct em8051_debug_trace_record records[EM8051_DEBUG_TRACE_RING_CAPACITY];
    uint64_t overwritten;
    uint16_t head;
    uint16_t count;
};

struct em8051_debug_trace_suppression_state {
    struct em8051_debug_trace_suppression value;
    uint8_t active;
};

struct em8051_debug_trace_router {
    struct em8051_debug_trace_config config;
    struct em8051_debug_trace_ring rings[EM8051_DEBUG_TRACE_MAX_DESTINATIONS];
    struct em8051_debug_trace_suppression_state
        suppression[EM8051_DEBUG_TRACE_MAX_TRACES];
    uint32_t seen_trace_ids[EM8051_DEBUG_TRACE_MAX_SEEN_IDS];
    uint64_t last_sequence;
    uint32_t interrupt_depth;
    struct em8051_debug_event open_source_event;
    uint8_t seen_trace_id_count;
    uint8_t source_open;
};

void em8051_debug_trace_router_init(struct em8051_debug_trace_router *aRouter);
enum em8051_debug_event_status em8051_debug_trace_router_replace(
    struct em8051_debug_trace_router *aRouter,
    const struct em8051_debug_trace_config *aConfig);
enum em8051_debug_event_status em8051_debug_trace_router_event(
    struct em8051_debug_trace_router *aRouter,
    const struct em8051_debug_event *aEvent);
/* source_begin routes the source and leaves after-gates pending. Zero or more
 * correlated watch events may then be routed before source_end applies those
 * after-gates. Configuration replacement and flush are busy while open. */
enum em8051_debug_event_status em8051_debug_trace_router_source_begin(
    struct em8051_debug_trace_router *aRouter,
    const struct em8051_debug_event *aEvent);
enum em8051_debug_event_status em8051_debug_trace_router_source_end(
    struct em8051_debug_trace_router *aRouter);
enum em8051_debug_event_status em8051_debug_trace_router_watch(
    struct em8051_debug_trace_router *aRouter,
    const struct em8051_debug_event *aWatchEvent,
    const struct em8051_debug_watch_result *aResult);
enum em8051_debug_event_status em8051_debug_trace_router_flush(
    struct em8051_debug_trace_router *aRouter);
const struct em8051_debug_trace_ring *em8051_debug_trace_router_ring(
    const struct em8051_debug_trace_router *aRouter, uint32_t aDestinationId);
bool em8051_debug_trace_ring_pop(struct em8051_debug_trace_ring *aRing,
                                 struct em8051_debug_trace_record *aRecord);

#endif

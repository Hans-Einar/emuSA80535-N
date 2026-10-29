/* Copyright 2026 Hans-Einar. MIT; see LICENSE. TARGET ONLY. */
#ifndef EMUSA80535_CONTRACTS_TRACE_H
#define EMUSA80535_CONTRACTS_TRACE_H
#include "runtime.h"
#include "observation.h"
#ifdef __cplusplus
extern "C" {
#endif
#define EMUIF_TRACE_INCLUDE UINT32_C(0)
#define EMUIF_TRACE_SUPPRESS_IRQ UINT32_C(1)
#define EMUIF_TRACE_IRQ_ONLY UINT32_C(2)
#define EMUIF_DEST_RING UINT32_C(0)
#define EMUIF_DEST_FILE UINT32_C(1)
#define EMUIF_RECORD_EVENT UINT32_C(0)
#define EMUIF_RECORD_SUPPRESSION UINT32_C(1)
#define EMUIF_RECORD_LOSS UINT32_C(2)
#define EMUIF_RECORD_SINK UINT32_C(3)
#define EMUIF_CURSOR_CURRENT UINT32_C(0)
#define EMUIF_CURSOR_EXPIRED UINT32_C(1)
#define EMUIF_CURSOR_AHEAD UINT32_C(2)
#define EMUIF_CURSOR_WRONG_SESSION UINT32_C(3)
#define EMUIF_CURSOR_WRONG_EPOCH UINT32_C(4)
#define EMUIF_SCOPE_PERSISTENT UINT32_C(0)
#define EMUIF_SCOPE_NEXT_CALL UINT32_C(1)
#define EMUIF_SCOPE_ACTIVE_CONTEXT UINT32_C(2)
#define EMUIF_SCOPE_ARMED UINT32_C(0)
#define EMUIF_SCOPE_BOUND UINT32_C(1)
#define EMUIF_SCOPE_COMPLETE UINT32_C(2)
#define EMUIF_SCOPE_INCOMPLETE UINT32_C(3)
#define EMUIF_SCOPE_STOPPED UINT32_C(4)
#define EMUIF_SCOPE_RETURN UINT32_C(0)
#define EMUIF_SCOPE_EXPLICIT UINT32_C(1)
#define EMUIF_SCOPE_LIMIT UINT32_C(2)
#define EMUIF_SCOPE_RESET UINT32_C(3)
#define EMUIF_SCOPE_UNKNOWN UINT32_C(4)
#define EMUIF_SCOPE_MISMATCH UINT32_C(5)
#define EMUIF_SCOPE_OVERFLOW UINT32_C(6)
#define EMUIF_SCOPE_FAULT UINT32_C(7)
#define EMUIF_SCOPE_LOAD UINT32_C(8)
#define EMUIF_ACTION_STOP UINT32_C(0)
#define EMUIF_ACTION_CONSOLE UINT32_C(1)
#define EMUIF_ACTION_QUIET UINT32_C(2)
#define EMUIF_ACTION_ROUTE UINT32_C(3)
struct emuif_trace_selector {
    EMUIF_PREFIX;
    uint32_t match_mask; /* 1 kind,2 pc,4 address,8 space,16 access */
    uint32_t kind, space, access_mask;
    uint16_t pc_first, pc_last, address_first, address_last;
};
struct emuif_trace_condition {
    EMUIF_PREFIX;
    uint32_t present, operand; /* old=0,new=1; delta intentionally unsupported */
    uint32_t comparison; /* eq=0,ne=1,lt=2,le=3,gt=4,ge=5 */
    uint32_t width, is_signed, reserved;
    uint64_t constant; /* width-bit two's complement when signed */
};
struct emuif_trace_action {
    EMUIF_PREFIX;
    struct emuif_trace_condition condition;
    uint32_t kind, route_count;
    uint32_t trace_ids[16];
};
struct emuif_trace_watch {
    EMUIF_PREFIX;
    uint32_t watch_id, enabled, space, access_mask;
    uint16_t address_first, address_last;
    uint32_t action_count;
    struct emuif_trace_action actions[8];
};
struct emuif_trace_point {
    EMUIF_PREFIX;
    struct emuif_trace_selector selector;
    uint32_t point_id, enabled, route_count, reserved;
    uint32_t trace_ids[16];
};
struct emuif_trace_gate {
    EMUIF_PREFIX;
    struct emuif_trace_selector selector;
    uint32_t gate_id, enabled, action, timing; /* on=0,off=1; before=0,after=1 */
    uint32_t route_count, reserved;
    uint32_t trace_ids[16];
};
struct emuif_trace_session {
    EMUIF_PREFIX;
    uint32_t trace_id, destination_id, enabled, interrupt_policy;
    char tag[65], comment[257];
    uint8_t reserved[6];
};
struct emuif_trace_destination {
    EMUIF_PREFIX;
    uint32_t destination_id, kind, capacity, stop_on_full;
};
/* Caller-owned strided arrays; each element starts with EMUIF_PREFIX. */
struct emuif_trace_array {
    EMUIF_PREFIX;
    const void *elements;
    uint32_t element_size, count;
};
struct emuif_trace_config {
    EMUIF_PREFIX;
    uint64_t generation, expected_revision;
    struct emuif_trace_array traces, destinations, points, gates, watches;
};
struct emuif_trace_revision { EMUIF_PREFIX; uint64_t revision; };
struct emuif_trace_record {
    EMUIF_PREFIX;
    uint64_t epoch, record_sequence, generation;
    uint64_t first_source_sequence, last_source_sequence, count;
    uint32_t kind, destination_id, route_count, trace_ids[16];
    uint32_t trace_id, interrupt_policy, entry_depth, maximum_depth;
    emuif_status status;
    uint32_t reserved;
    struct emuif_observation event;
};
struct emuif_trace_page_request {
    EMUIF_PREFIX;
    char session_id[33];
    uint8_t reserved[7];
    uint32_t destination_id, max_records;
    uint64_t epoch, after_record_sequence;
};
struct emuif_trace_page {
    EMUIF_PREFIX;
    char session_id[33];
    uint8_t reserved[7];
    uint32_t destination_id, cursor_state;
    uint64_t epoch, requested_after, oldest, newest, first, last, next_after;
    uint64_t overwritten, dropped, lost_before_page;
    uint32_t remaining, more, written, required;
    struct emuif_trace_record *records;
    uint32_t element_size, capacity;
};
struct emuif_capture_request {
    EMUIF_PREFIX;
    uint64_t generation, context_id, max_records, max_cycles;
    uint32_t trace_id, mode, rearm_on_reset, stop_cpu;
    uint16_t entry_pc;
    uint16_t reserved;
};
struct emuif_capture_status {
    EMUIF_PREFIX;
    uint64_t capture_id, invocation_id, context_id, generation, successor_capture_id;
    uint64_t start_sequence, end_sequence, records, cycles;
    uint32_t trace_id, mode, state, reason, rearm_on_reset;
    emuif_status rearm_status;
};
#define EMUIF_SINK_OPEN UINT32_C(0)
#define EMUIF_SINK_FLUSH_PENDING UINT32_C(1)
#define EMUIF_SINK_CLOSE_PENDING UINT32_C(2)
#define EMUIF_SINK_CLOSED UINT32_C(3)
#define EMUIF_SINK_FAILED UINT32_C(4)
struct emuif_sink_request {
    EMUIF_PREFIX;
    uint64_t generation, max_segment_bytes, max_total_records;
    uint32_t destination_id, max_segments;
    char destination_token[65]; /* host policy token, never a caller path */
    uint8_t reserved[7];
};
struct emuif_sink_status {
    EMUIF_PREFIX;
    uint64_t operation_id, completed_operation_id, bytes, records, lost;
    uint32_t destination_id, state, segment, queued, partial_output;
    emuif_status error;
};
struct emuif_trace_api {
    EMUIF_PREFIX;
    emuif_status (*replace)(struct emuif_session *, const struct emuif_trace_config *, struct emuif_trace_revision *);
    emuif_status (*page)(struct emuif_session *, const struct emuif_trace_page_request *, struct emuif_trace_page *);
    emuif_status (*outcomes)(struct emuif_session *, const struct emuif_trace_page_request *, struct emuif_trace_page *);
    emuif_status (*capture_start)(struct emuif_session *, const struct emuif_capture_request *, struct emuif_capture_status *);
    emuif_status (*sink_open)(struct emuif_session *, const struct emuif_sink_request *, struct emuif_sink_status *);
};
#define EMUIF_TRACE_QUERY_SYMBOL "emusa80535_trace_query_v1"
emuif_status emusa80535_trace_query_v1(struct emuif_trace_api *);
#ifdef __cplusplus
}
#endif
#endif

/* Copyright 2026 Hans-Einar. MIT; see LICENSE. TARGET ONLY, no symbols supplied. */
#ifndef EMUSA80535_CONTRACTS_OBSERVATION_H
#define EMUSA80535_CONTRACTS_OBSERVATION_H
#include "cpu.h"
#include "target_version.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Frozen numeric mapping is also in observation.schema.json and docs/02. */
#define EMUIF_OBS_INSTRUCTION_BEGIN UINT32_C(1)
#define EMUIF_OBS_INSTRUCTION_RETIRE UINT32_C(2)
#define EMUIF_OBS_INSTRUCTION_ABORT UINT32_C(3)
#define EMUIF_OBS_ACCESS_ATTEMPT UINT32_C(4)
#define EMUIF_OBS_ACCESS_RESULT UINT32_C(5)
#define EMUIF_OBS_CALL UINT32_C(6)
#define EMUIF_OBS_RET UINT32_C(7)
#define EMUIF_OBS_IRQ_REQUEST UINT32_C(8)
#define EMUIF_OBS_IRQ_ENTER UINT32_C(9)
#define EMUIF_OBS_RETI UINT32_C(10)
#define EMUIF_OBS_RESET UINT32_C(11)
#define EMUIF_OBS_LOAD UINT32_C(12)
#define EMUIF_OBS_FAULT UINT32_C(13)
#define EMUIF_OBS_INPUT_ADMITTED UINT32_C(14)
#define EMUIF_OBS_INPUT_DELIVERED UINT32_C(15)
#define EMUIF_OBS_INPUT_FAILED UINT32_C(16)
#define EMUIF_OBS_INPUT_CANCELLED UINT32_C(17)
#define EMUIF_OBS_RX_START UINT32_C(18)
#define EMUIF_OBS_RX_ACCEPT UINT32_C(19)
#define EMUIF_OBS_RX_DROP UINT32_C(20)
#define EMUIF_OBS_RX_END UINT32_C(21)
#define EMUIF_OBS_TX_HOLD UINT32_C(22)
#define EMUIF_OBS_TX_REPLACE UINT32_C(23)
#define EMUIF_OBS_TX_START UINT32_C(24)
#define EMUIF_OBS_TX_STOP UINT32_C(25)
#define EMUIF_OBS_TX_TI UINT32_C(26)
#define EMUIF_OBS_TX_END UINT32_C(27)
#define EMUIF_OBS_WATCH UINT32_C(28)
#define EMUIF_OBS_PHASE_PRE UINT32_C(0)
#define EMUIF_OBS_PHASE_EFFECT UINT32_C(1)
#define EMUIF_OBS_PHASE_RETIRE UINT32_C(2)
#define EMUIF_OBS_PHASE_PERIPHERAL UINT32_C(3)
#define EMUIF_OBS_PHASE_BOUNDARY UINT32_C(4)
#define EMUIF_OBS_REASON_NONE UINT32_C(0)
#define EMUIF_OBS_REASON_REN UINT32_C(1)
#define EMUIF_OBS_REASON_BUSY UINT32_C(2)
#define EMUIF_OBS_REASON_RI UINT32_C(3)
#define EMUIF_OBS_REASON_SM2 UINT32_C(4)
#define EMUIF_OBS_REASON_REPLACED UINT32_C(5)
#define EMUIF_OBS_REASON_RESET UINT32_C(6)
#define EMUIF_OBS_REASON_UNSUPPORTED UINT32_C(7)
#define EMUIF_OBS_REASON_DEVICE UINT32_C(8)
#define EMUIF_OBS_REASON_CONTEXT UINT32_C(9)
#define EMUIF_OBS_REASON_EXHAUSTED UINT32_C(10)
#define EMUIF_OBS_COVER_INSTRUCTION UINT64_C(1)
#define EMUIF_OBS_COVER_ACCESS UINT64_C(2)
#define EMUIF_OBS_COVER_CONTROL UINT64_C(4)
#define EMUIF_OBS_COVER_IRQ UINT64_C(8)
#define EMUIF_OBS_COVER_LIFECYCLE UINT64_C(16)
#define EMUIF_OBS_COVER_INPUT UINT64_C(32)
#define EMUIF_OBS_COVER_UART UINT64_C(64)
#define EMUIF_OBS_ACCESS_FETCH UINT32_C(1)
#define EMUIF_OBS_ACCESS_READ UINT32_C(2)
#define EMUIF_OBS_ACCESS_WRITE UINT32_C(4)
#define EMUIF_OBS_ACCESS_RMW UINT32_C(8)
#define EMUIF_OBS_KNOWN_NEXT_PC UINT32_C(32)
#define EMUIF_OBS_KNOWN_PC UINT32_C(1)
#define EMUIF_OBS_KNOWN_TARGET UINT32_C(2)
#define EMUIF_OBS_KNOWN_CONTEXT UINT32_C(4)
#define EMUIF_OBS_KNOWN_UART_STATE UINT32_C(8)
#define EMUIF_OBS_KNOWN_TIMING UINT32_C(16)
#define EMUIF_OBS_FRAME_NONE UINT32_C(0)
#define EMUIF_OBS_FRAME_CALL UINT32_C(1)
#define EMUIF_OBS_FRAME_IRQ UINT32_C(2)
#define EMUIF_OBS_FRAME_UNKNOWN UINT32_C(3)
struct emuif_observed_value {
    EMUIF_PREFIX;
    uint64_t value;
    uint32_t width; /* 1,8,16,32,64 */
    uint32_t known;
    uint32_t is_signed;
    uint32_t reserved;
};
/* No native bytes are a wire format. Unused fields are zero; nested prefixes
 * still initialized. All observations are immutable borrowed callback facts. */
struct emuif_observation {
    EMUIF_PREFIX;
    uint64_t generation, sequence, cycle, instruction_id;
    uint64_t legacy_instructions, retired_instructions, access_id;
    uint64_t parent_stimulus_id, cpu_input_order, frame_id, related_frame_id;
    uint64_t context_id, parent_context_id, scheduled_cycle, start_cycle;
    uint64_t source_event_sequence, begin_cycle;
    uint32_t kind, phase, reason, known, input_stage; /* board=0, CPU=1 */
    emuif_status status;
    uint32_t partial_effects, space, access; /* space CPU API1; access 1 fetch,2 read,4 write,8 RMW */
    uint32_t frame_kind, irq_source, irq_depth, watch_id, fired_actions;
    uint16_t pc, target, address, return_pc, next_pc, reserved_pc;
    uint8_t sp_before, sp_after, byte_count, bytes[3];
    uint8_t uart_mode, uart_width, ninth, scon, tcon, reserved_bytes[3];
    struct emuif_observed_value old_value, new_value;
    char text[97]; /* captured-byte decode, UTF-8 <=96 bytes; empty if unavailable */
    uint8_t reserved_text[7];
};
struct emuif_observer {
    EMUIF_PREFIX;
    void *context;
    void (*observe)(void *, const struct emuif_observation *);
};
struct emuif_observation_binding {
    EMUIF_PREFIX;
    uint64_t generation;
    struct emuif_observer observer;
};
struct emuif_observed_input {
    EMUIF_PREFIX;
    uint64_t parent_stimulus_id;
    struct emuif_input input; /* controller-assigned nonzero order, delivered at due cycle */
};
struct emuif_input_receipt {
    EMUIF_PREFIX;
    uint64_t parent_stimulus_id, cpu_input_order, admission_sequence;
};
struct emuif_observation_api {
    EMUIF_PREFIX;
    uint64_t coverage;
    emuif_status (*bind)(struct emuif_cpu *, const struct emuif_observation_binding *);
    emuif_status (*unbind)(struct emuif_cpu *);
    emuif_status (*deliver)(struct emuif_cpu *, const struct emuif_observed_input *);
};
/* Resolve optionally by exact symbol name. Absent symbol means unavailable;
 * never infer availability from headers and never cast opaque CPU storage. */
#define EMUIF_OBSERVATION_QUERY_SYMBOL "emusa80535_cpu_observation_query_v1"
emuif_status emusa80535_cpu_observation_query_v1(struct emuif_observation_api *);
#ifdef __cplusplus
}
#endif
#endif

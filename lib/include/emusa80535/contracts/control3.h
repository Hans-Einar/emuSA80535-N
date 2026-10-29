/* Copyright 2026 Hans-Einar. MIT; see LICENSE. TARGET ONLY. */
#ifndef EMUSA80535_CONTRACTS_CONTROL3_H
#define EMUSA80535_CONTRACTS_CONTROL3_H
#include "debug.h"
#include "trace.h"
#ifdef __cplusplus
extern "C" {
#endif
#define EMUIF_EXECUTION_EXTERNALLY_DRIVEN UINT32_C(0)
#define EMUIF_EXECUTION_HOST_DRIVEN UINT32_C(1)
struct emuif_control_advance {
    EMUIF_PREFIX;
    uint64_t connection_id, lease_id, generation;
    uint32_t cycle_budget, reserved;
};
struct emuif_control_progress {
    EMUIF_PREFIX;
    uint64_t generation, cycles, instructions, retired_instructions;
    uint64_t cycle, stop_revision;
    uint32_t state, safe_boundary, stop_reason, partial;
};
struct emuif_control3_api {
    EMUIF_PREFIX;
    emuif_status (*open)(struct emuif_session *, const struct emuif_client *);
    emuif_status (*request)(struct emuif_session *, uint64_t connection_id,
        const struct emuif_json_request *, struct emuif_json_response *);
    emuif_status (*close)(struct emuif_session *, uint64_t connection_id);
    emuif_status (*advance)(struct emuif_session *, const struct emuif_control_advance *, struct emuif_control_progress *);
};
#define EMUIF_CONTROL3_QUERY_SYMBOL "emusa80535_control_query_v3"
emuif_status emusa80535_control_query_v3(struct emuif_control3_api *);
#ifdef __cplusplus
}
#endif
#endif

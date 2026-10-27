/* Copyright 2026 Hans-Einar. MIT; see LICENSE.
 * Contract bundle 0.1.0. TARGET DECLARATIONS ONLY; not implemented. */
#ifndef EMUSA80535_CONTRACTS_DEBUG_H
#define EMUSA80535_CONTRACTS_DEBUG_H
#include <stdint.h>
#include "runtime.h"

#ifdef __cplusplus
extern "C" {
#endif
/* Callback-free, caller-buffer facade. Wire payload is serialized JSON values.
 * This facade retains no input/output pointer and is called only by the owner. */
#define EMUIF_ROLE_OBSERVER UINT32_C(0)
#define EMUIF_ROLE_CONTROLLER UINT32_C(1)
struct emuif_client {
    EMUIF_PREFIX;
    uint64_t connection_id;
    uint32_t role;
    uint32_t may_terminate;
};
struct emuif_json_request {
    EMUIF_PREFIX;
    const uint8_t *bytes;
    uint32_t byte_count;
    uint32_t reserved;
};
struct emuif_json_response {
    EMUIF_PREFIX;
    uint8_t *bytes;
    uint32_t capacity;
    uint32_t written;
    uint32_t required;
    uint32_t reserved;
};
/* Response buffer capacity must be MAX_FRAME_BYTES before any mutation. */
emuif_status emusa80535_debug_open_v1(struct emuif_session *, const struct emuif_client *);
emuif_status emusa80535_debug_request_v1(struct emuif_session *, uint64_t connection_id,
    const struct emuif_json_request *, struct emuif_json_response *);
emuif_status emusa80535_debug_close_v1(struct emuif_session *, uint64_t connection_id);

#ifdef __cplusplus
}
#endif
#endif

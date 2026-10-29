/* Copyright 2026 Hans-Einar. MIT; see LICENSE.
 * Contract bundle 0.2.0. TARGET DECLARATIONS ONLY; not implemented. */
#ifndef EMUSA80535_CONTRACTS_BOARD_H
#define EMUSA80535_CONTRACTS_BOARD_H
#include <stdint.h>
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif
#define EMUIF_BOARD_CAP_PEEK UINT64_C(1)
#define EMUIF_BOARD_CAP_STIMULUS UINT64_C(2)
#define EMUIF_BOARD_CAP_INSPECT UINT64_C(4)
/* Only native Linux ELF64 little-endian x86-64 System V C ABI in this revision. */
#define EMUIF_PLATFORM_LINUX_X86_64 UINT32_C(1)
struct emuif_board_host {
    EMUIF_PREFIX;
    void *context;
    emuif_status (*schedule_input)(void *, const struct emuif_input *);
};
struct emuif_board_create {
    EMUIF_PREFIX;
    uint32_t variant;
    uint32_t oscillator_hz;
    const uint8_t *config_utf8;
    uint32_t config_bytes;
    uint32_t reserved;
};
struct emuif_board_reset {
    EMUIF_PREFIX;
    uint64_t generation;
    uint32_t seed;
    uint32_t reserved;
};
struct emuif_board_boundary {
    EMUIF_PREFIX;
    uint64_t cycle;
    uint64_t generation;
};
struct emuif_board_stimulus {
    EMUIF_PREFIX;
    uint64_t generation;
    uint64_t cycle;
    uint64_t order;
    char schema[64];
    const uint8_t *payload_utf8;
    uint32_t payload_bytes;
    uint32_t reserved;
};
struct emuif_board_inspect {
    EMUIF_PREFIX;
    uint64_t generation;
    char schema[64];
    const uint8_t *request_utf8;
    uint32_t request_bytes;
    uint32_t reserved;
};
struct emuif_board_inspection {
    EMUIF_PREFIX;
    uint8_t *response_utf8;
    uint32_t capacity;
    uint32_t written;
    uint32_t required;
    uint32_t reserved;
};
struct emuif_board_api {
    EMUIF_PREFIX;
    uint16_t abi_major;
    uint16_t abi_minor;
    uint32_t platform;
    uint64_t capabilities;
    uint64_t required_cpu_capabilities;
    char module_id[64];
    char module_version[32];
    char build_id[65];
    uint8_t reserved[7];
    emuif_status (*create)(const struct emuif_board_host *, const struct emuif_board_create *, void **);
    void (*destroy)(void *);
    emuif_status (*reset)(void *, const struct emuif_board_reset *);
    emuif_status (*boundary)(void *, const struct emuif_board_boundary *);
    emuif_status (*access)(void *, const struct emuif_bus_access *, struct emuif_bus_result *);
    emuif_status (*port_output)(void *, const struct emuif_port_output *);
    emuif_status (*peek)(const void *, const struct emuif_memory_request *, uint8_t *, uint32_t);
    emuif_status (*signal_output)(void *, const struct emuif_signal_output *);
    emuif_status (*validate_stimulus)(const void *, const struct emuif_board_stimulus *);
    emuif_status (*apply_stimulus)(void *, const struct emuif_board_stimulus *);
    emuif_status (*inspect)(const void *, const struct emuif_board_inspect *, struct emuif_board_inspection *);
};
typedef emuif_status (*emuif_board_query_fn)(struct emuif_board_api *);
/* Module exports exactly this C symbol with default visibility. */
emuif_status emusa80535_board_query_v1(struct emuif_board_api *out);

#ifdef __cplusplus
}
#endif
#endif

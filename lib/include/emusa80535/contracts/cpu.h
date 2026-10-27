/* Copyright 2026 Hans-Einar. MIT; see LICENSE.
 * Contract bundle 0.1.0. TARGET DECLARATIONS ONLY; not implemented. */
#ifndef EMUSA80535_CONTRACTS_CPU_H
#define EMUSA80535_CONTRACTS_CPU_H
#include <stdint.h>
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif
#define EMUIF_CPU_CAP_PORT_LATCH UINT64_C(1)
#define EMUIF_CPU_CAP_UART_FRAME UINT64_C(2)
#define EMUIF_CPU_CAP_EFFECTIVE_PIN UINT64_C(4)
struct emuif_cpu;
struct emuif_cpu_config {
    EMUIF_PREFIX;
    uint32_t variant;
    uint32_t oscillator_hz;
};
/* CPU retains callbacks/context until destroy; this is NOT the debug facade. */
struct emuif_cpu_hooks {
    EMUIF_PREFIX;
    void *context;
    emuif_status (*bus_access)(void *, const struct emuif_bus_access *, struct emuif_bus_result *);
    emuif_status (*port_output)(void *, const struct emuif_port_output *);
    emuif_status (*signal_output)(void *, const struct emuif_signal_output *);
};
struct emuif_reset {
    EMUIF_PREFIX;
    uint32_t seed;
    uint16_t entry;
    uint16_t reserved;
};
struct emuif_cycle_result {
    EMUIF_PREFIX;
    uint64_t cycle;
    uint64_t instructions;
    uint32_t progressed;
    uint32_t safe_boundary;
    uint32_t idle;
    uint32_t power_down;
};
struct emuif_decode_request {
    EMUIF_PREFIX;
    uint16_t address;
    uint16_t reserved;
    int32_t byte_offset;
    int32_t instruction_offset;
    uint32_t count;
};
struct emuif_decoded {
    EMUIF_PREFIX;
    uint16_t address;
    uint8_t size;
    uint8_t valid;
    char text[97];
    uint8_t reserved[3];
};
struct emuif_cpu_api {
    EMUIF_PREFIX;
    uint64_t capabilities;
    emuif_status (*create)(const struct emuif_cpu_config *, const struct emuif_cpu_hooks *, struct emuif_cpu **);
    void (*destroy)(struct emuif_cpu *);
    emuif_status (*reset)(struct emuif_cpu *, const struct emuif_reset *);
    emuif_status (*load_code)(struct emuif_cpu *, const uint8_t *, uint32_t);
    emuif_status (*apply_input)(struct emuif_cpu *, const struct emuif_input *);
    emuif_status (*cycle)(struct emuif_cpu *, struct emuif_cycle_result *);
    emuif_status (*snapshot)(const struct emuif_cpu *, struct emuif_snapshot *);
    emuif_status (*peek)(const struct emuif_cpu *, const struct emuif_memory_request *, uint8_t *, uint32_t);
    emuif_status (*decode)(struct emuif_cpu *, const struct emuif_decode_request *,
        struct emuif_decoded *, uint32_t element_size, uint32_t capacity);
};
emuif_status emusa80535_cpu_query_v1(struct emuif_cpu_api *out);

#ifdef __cplusplus
}
#endif
#endif

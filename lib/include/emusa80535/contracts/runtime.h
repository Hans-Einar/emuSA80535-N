/* Copyright 2026 Hans-Einar. MIT; see LICENSE.
 * Contract bundle 0.1.0. TARGET DECLARATIONS ONLY; not implemented. */
#ifndef EMUSA80535_CONTRACTS_RUNTIME_H
#define EMUSA80535_CONTRACTS_RUNTIME_H
#include <stdint.h>
#include "cpu.h"
#include "board.h"

#ifdef __cplusplus
extern "C" {
#endif
struct emuif_session;
struct emuif_session_config {
    EMUIF_PREFIX;
    uint32_t variant;
    uint32_t oscillator_hz;
    uint32_t seed;
    uint16_t entry;
    uint16_t reserved;
    const uint8_t *code;
    uint32_t code_bytes;
    uint32_t board_config_bytes;
    const uint8_t *board_config_utf8;
    char module_sha256[65];
    uint8_t reserved_identity[7];
    const struct emuif_cpu_api *cpu_api;
    const struct emuif_board_api *board_api;
};
struct emuif_progress {
    EMUIF_PREFIX;
    uint64_t cycles;
    uint64_t instructions;
    uint32_t state;
    uint32_t stop_reason;
};
/* Runtime copies code/config/tables; loaded module stays resident until destroy. */
emuif_status emusa80535_session_create_v1(const struct emuif_session_config *, struct emuif_session **);
void emusa80535_session_destroy_v1(struct emuif_session *);
/* Drives only a running session; returns after <= budget + 16 drain cycles. */
emuif_status emusa80535_session_pump_v1(struct emuif_session *, uint32_t cycle_budget, struct emuif_progress *);

#ifdef __cplusplus
}
#endif
#endif

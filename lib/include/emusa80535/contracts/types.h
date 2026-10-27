/* Copyright 2026 Hans-Einar. MIT; see LICENSE.
 * Contract bundle 0.1.0. TARGET DECLARATIONS ONLY; not implemented. */
#ifndef EMUSA80535_CONTRACTS_TYPES_H
#define EMUSA80535_CONTRACTS_TYPES_H
#include <stdint.h>
#include "version.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef uint32_t emuif_status;
#define EMUIF_OK UINT32_C(0)
#define EMUIF_INVALID UINT32_C(1)
#define EMUIF_VERSION UINT32_C(2)
#define EMUIF_STATE UINT32_C(3)
#define EMUIF_RANGE UINT32_C(4)
#define EMUIF_CAPACITY UINT32_C(5)
#define EMUIF_UNSUPPORTED UINT32_C(6)
#define EMUIF_BUSY UINT32_C(7)
#define EMUIF_STALE UINT32_C(8)
#define EMUIF_DENIED UINT32_C(9)
#define EMUIF_FAULT UINT32_C(10)
#define EMUIF_EXHAUSTED UINT32_C(11)
#define EMUIF_IO UINT32_C(12)
#define EMUIF_HASH UINT32_C(13)
#define EMUIF_NOT_FOUND UINT32_C(14)
#define EMUIF_VARIANT_8051 UINT32_C(0)
#define EMUIF_VARIANT_8052 UINT32_C(1)
#define EMUIF_VARIANT_SAB80535 UINT32_C(2)
#define EMUIF_SPACE_CODE UINT32_C(0)
#define EMUIF_SPACE_IRAM UINT32_C(1)
#define EMUIF_SPACE_SFR UINT32_C(2)
#define EMUIF_SPACE_XDATA UINT32_C(3)
#define EMUIF_ACCESS_READ UINT32_C(0)
#define EMUIF_ACCESS_WRITE UINT32_C(1)
#define EMUIF_INPUT_PORT UINT32_C(0)
#define EMUIF_INPUT_INTERRUPT UINT32_C(1)
#define EMUIF_INPUT_ADC UINT32_C(2)
#define EMUIF_INPUT_UART_RX UINT32_C(3)
#define EMUIF_INPUT_RELEASE UINT32_C(4)
#define EMUIF_STATE_PAUSED UINT32_C(0)
#define EMUIF_STATE_RUNNING UINT32_C(1)
#define EMUIF_STATE_FAULTED UINT32_C(2)
#define EMUIF_STATE_TERMINATED UINT32_C(3)

struct emuif_bus_access {
    EMUIF_PREFIX;
    uint64_t cycle;
    uint64_t sequence;
    uint32_t direction;
    uint16_t pc;
    uint16_t address;
    uint8_t value;
    uint8_t port_latches[6];
    uint8_t reserved;
};
struct emuif_bus_result {
    EMUIF_PREFIX;
    uint8_t value;
    uint8_t reserved[3];
    uint32_t diagnostics;
};
struct emuif_input {
    EMUIF_PREFIX;
    uint64_t cycle;
    uint64_t order;
    uint32_t kind;
    uint32_t index;
    uint32_t value;
    uint32_t mask;
};
struct emuif_port_output {
    EMUIF_PREFIX;
    uint64_t cycle;
    uint64_t sequence;
    uint32_t port;
    uint8_t previous_latch;
    uint8_t latch;
    uint8_t changed_mask;
    uint8_t reserved;
};
#define EMUIF_OUTPUT_PIN UINT32_C(0)
#define EMUIF_OUTPUT_UART_FRAME UINT32_C(1)
struct emuif_signal_output {
    EMUIF_PREFIX;
    uint64_t cycle;
    uint64_t start_cycle;
    uint64_t sequence;
    uint32_t kind;
    uint32_t index;
    uint32_t value;
    uint32_t width;
};
struct emuif_snapshot {
    EMUIF_PREFIX;
    uint64_t generation;
    uint64_t cycle;
    uint64_t instructions;
    uint32_t state;
    uint32_t variant;
    uint16_t pc;
    uint16_t dptr;
    uint8_t a, b, psw, sp;
    uint8_t r[8];
    uint8_t idle;
    uint8_t power_down;
    uint8_t safe_boundary;
    uint8_t reserved;
};
struct emuif_memory_request {
    EMUIF_PREFIX;
    uint32_t space;
    uint32_t address;
    uint32_t count;
    uint32_t reserved;
};

#ifdef __cplusplus
}
#endif
#endif

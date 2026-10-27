/* Copyright 2026 Hans-Einar. MIT; see LICENSE.
 * Contract bundle 0.1.0. TARGET DECLARATIONS ONLY; not implemented. */
#ifndef EMUSA80535_CONTRACTS_VERSION_H
#define EMUSA80535_CONTRACTS_VERSION_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#define EMUIF_BUNDLE_VERSION "0.1.0"
#define EMUIF_API_VERSION UINT16_C(1)
#define EMUIF_BOARD_ABI_MAJOR UINT16_C(1)
#define EMUIF_BOARD_ABI_MINOR UINT16_C(0)
#define EMUIF_WIRE_MAJOR UINT16_C(2)
#define EMUIF_WIRE_MINOR UINT16_C(0)
#define EMUIF_TARGET_IMPLEMENTED 0
#define EMUIF_BOARD_ENTRY_SYMBOL "emusa80535_board_query_v1"
#define EMUIF_MAX_FRAME_BYTES UINT32_C(65536)
#define EMUIF_MAX_CONFIG_BYTES UINT32_C(16384)
#define EMUIF_MAX_INPUTS UINT32_C(1024)
#define EMUIF_MAX_PAGE_RECORDS UINT32_C(128)
#define EMUIF_MAX_CYCLE_BUDGET UINT32_C(100000)
#define EMUIF_MAX_SAFE_DRAIN_CYCLES UINT32_C(16)
#define EMUIF_MAX_CLIENTS UINT32_C(8)
#define EMUIF_MAX_PENDING_REQUESTS UINT32_C(32)
#define EMUIF_MAX_BREAKPOINTS UINT32_C(1024)
#define EMUIF_MAX_MEMORY_BYTES UINT32_C(256)
#define EMUIF_MAX_DECODE_RECORDS UINT32_C(256)
/* Prefix required on every stable record, including array elements. */
#define EMUIF_PREFIX uint32_t struct_size; uint16_t api_version; uint16_t flags

#ifdef __cplusplus
}
#endif
#endif

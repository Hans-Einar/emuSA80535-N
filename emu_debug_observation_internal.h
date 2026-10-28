/* Private canonical bridge to the existing semantic runtime. Copyright 2026 Hans-Einar. MIT. */
#ifndef EMU_DEBUG_OBSERVATION_INTERNAL_H
#define EMU_DEBUG_OBSERVATION_INTERNAL_H
#include <emusa80535/emu_debug_runtime.h>
typedef void (*emu_debug_observation_fn)(void *, const struct emuif_observation *);
enum em8051_debug_event_status emu_debug_runtime_observer(struct em8051_debug_runtime *,
                                                          emu_debug_observation_fn, void *);
enum em8051_debug_event_status emu_debug_runtime_observation(struct em8051_debug_runtime *,
                                                             const struct emuif_observation *,
                                                             uint64_t *);
#endif

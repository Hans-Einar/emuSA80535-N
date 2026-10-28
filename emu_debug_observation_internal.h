/* Private canonical bridge to the existing semantic runtime. Copyright 2026 Hans-Einar. MIT. */
#ifndef EMU_DEBUG_OBSERVATION_INTERNAL_H
#define EMU_DEBUG_OBSERVATION_INTERNAL_H
#include <emusa80535/emu_debug_runtime.h>
typedef void (*emu_debug_observation_fn)(void *, const struct emuif_observation *);
enum em8051_debug_event_status emu_debug_runtime_observer(struct em8051_debug_runtime *,
                                                          emu_debug_observation_fn, void *);
enum em8051_debug_event_status emu_debug_runtime_after_observation(struct em8051_debug_runtime *,
    emu_debug_observation_fn, void *);
enum em8051_debug_event_status emu_debug_runtime_observation(struct em8051_debug_runtime *,
                                                             const struct emuif_observation *,
                                                             uint64_t *);
enum em8051_debug_event_status emu_debug_runtime_configure(struct em8051_debug_runtime *,
    const struct em8051_debug_trace_config *, const struct em8051_debug_watch *, size_t);
enum em8051_debug_event_status emu_debug_runtime_delivery(struct em8051_debug_runtime *,
    void *, void (*)(void *, const struct em8051_debug_trace_record *),
    bool (*)(void *, uint32_t, const struct em8051_debug_event *));
enum em8051_debug_event_status emu_debug_runtime_flush(struct em8051_debug_runtime *);
size_t emu_debug_runtime_pending_summaries(const struct em8051_debug_runtime *, uint32_t);
enum em8051_debug_event_status emu_debug_runtime_flush_selected(struct em8051_debug_runtime *, uint32_t destination_id, uint32_t trace_id);
enum em8051_debug_event_status emu_debug_runtime_discard_suppression(struct em8051_debug_runtime *, uint32_t trace_id);
#endif

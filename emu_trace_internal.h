/* Private canonical trace projection. Copyright 2026 Hans-Einar. MIT. */
#ifndef EMU_TRACE_INTERNAL_H
#define EMU_TRACE_INTERNAL_H
#include "emu_debug_observation_internal.h"
#include "emu_outcome_internal.h"
#include "emu_trace_file_internal.h"
struct emu_trace_state;
struct emu_trace_metadata
{
    char session_id[33], runtime_commit[41], manifest[65];
    char image_hash[65], module_hash[65], config_hash[65];
};
struct emu_trace_state *emu_trace_create(struct em8051_debug_runtime *,
                                         const struct emu_trace_metadata *, uint64_t);
void emu_trace_destroy(struct emu_trace_state *);
void emu_trace_observe(struct emu_trace_state *, const struct emuif_observation *);
void emu_trace_poll_files(struct emu_trace_state *);
void emu_trace_tick(struct emu_trace_state *, uint64_t generation, uint64_t cycle);
emuif_status emu_trace_replace(struct emu_trace_state *, const struct emuif_trace_config *,
                               struct emuif_trace_revision *);
emuif_status emu_trace_page(struct emu_trace_state *, const struct emuif_trace_page_request *,
                            struct emuif_trace_page *);
emuif_status emu_trace_capture(struct emu_trace_state *, const struct emuif_capture_request *,
                               struct emuif_capture_status *);
emuif_status emu_trace_sink_open(struct emu_trace_state *, const struct emuif_sink_request *,
                                 struct emuif_sink_status *);
emuif_status emu_trace_command(struct emu_trace_state *, const char *, const struct json_document *,
                               int, struct output *);
void emu_trace_set_provider(struct emu_trace_state *, const struct emu_trace_file_provider *);
int emu_trace_has_provider(const struct emu_trace_state *);
emuif_status emu_trace_close_files(struct emu_trace_state *);
int emu_trace_stop_pending(const struct emu_trace_state *);
void emu_trace_applied_stop(struct emu_trace_state *, uint32_t reason);
void emu_trace_stop_detail(struct emu_trace_state *, struct output *);
uint64_t emu_trace_stop_source(const struct emu_trace_state *);
uint64_t emu_trace_stop_instruction(const struct emu_trace_state *);
#endif

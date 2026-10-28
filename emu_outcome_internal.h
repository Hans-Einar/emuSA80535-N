/* Bounded canonical outcome retention, private same-build implementation. MIT. */
#ifndef EMU_OUTCOME_INTERNAL_H
#define EMU_OUTCOME_INTERNAL_H
#include "emu_json_internal.h"
#include <emusa80535/contracts/trace.h>
#define EMU_OUTCOME_CAPACITY 1024u
struct emu_outcome_ring
{
    struct emuif_trace_record records[EMU_OUTCOME_CAPACITY];
    uint64_t next_sequence, overwritten;
    uint32_t head, count;
};
void emu_outcome_init(struct emu_outcome_ring *);
emuif_status emu_outcome_append(struct emu_outcome_ring *, const struct emuif_observation *);
emuif_status emu_outcome_page(const struct emu_outcome_ring *, const char *,
                              const struct emuif_trace_page_request *, struct emuif_trace_page *);
void emu_observation_json(struct output *, const struct emuif_observation *);
void emu_trace_record_json(struct output *, const struct emuif_trace_record *);
void emu_trace_page_json(struct output *, const struct emuif_trace_page *);
#endif

/* Generic pull-only outcomes. Copyright 2026 Hans-Einar. MIT; see LICENSE. */
#include "emu_outcome_internal.h"
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#define INIT(x)                                                                                    \
    do                                                                                             \
    {                                                                                              \
        memset(&(x), 0, sizeof(x));                                                                \
        (x).struct_size = sizeof(x);                                                               \
        (x).api_version = 1;                                                                       \
    } while (0)
static bool zero(const void *p, size_t n)
{
    const unsigned char *b = p;
    while (n--)
        if (*b++)
            return false;
    return true;
}
static emuif_status sized(const void *p, size_t n)
{
    uint32_t size;
    uint16_t version, flags;
    if (!p)
        return EMUIF_INVALID;
    memcpy(&size, p, 4);
    if (size < 8)
        return EMUIF_INVALID;
    memcpy(&version, (const char *)p + 4, 2);
    memcpy(&flags, (const char *)p + 6, 2);
    if (version != 1)
        return EMUIF_VERSION;
    if (size < n || flags)
        return EMUIF_INVALID;
    return EMUIF_OK;
}
void emu_outcome_init(struct emu_outcome_ring *r)
{
    memset(r, 0, sizeof *r);
    r->next_sequence = 1;
}
static void insert(struct emu_outcome_ring *r, struct emuif_trace_record *v)
{
    v->record_sequence = r->next_sequence++;
    r->records[(r->head + r->count) % EMU_OUTCOME_CAPACITY] = *v;
    r->count++;
}
emuif_status emu_outcome_append(struct emu_outcome_ring *r, const struct emuif_observation *o)
{
    struct emuif_trace_record v;
    unsigned needed = r->count == EMU_OUTCOME_CAPACITY ? 2 : 1;
    if (!r->next_sequence || UINT64_MAX - r->next_sequence < needed - 1)
        return EMUIF_EXHAUSTED;
    if (r->count == EMU_OUTCOME_CAPACITY)
    {
        unsigned i;
        INIT(v);
        INIT(v.event);
        INIT(v.event.old_value);
        INIT(v.event.new_value);
        v.event.old_value.width = v.event.new_value.width = 8;
        v.epoch = 1;
        v.generation = o->generation;
        v.kind = EMUIF_RECORD_LOSS;
        for (i = 0; i < 2; i++)
        {
            const struct emuif_trace_record *old = &r->records[r->head];
            uint64_t first = old->kind == EMUIF_RECORD_EVENT ? old->event.sequence : 0;
            uint64_t last = old->kind == EMUIF_RECORD_EVENT ? old->event.sequence : 0;
            uint64_t count = old->kind == EMUIF_RECORD_EVENT ? 1 : 0;
            if (!v.first_source_sequence || (first && first < v.first_source_sequence))
                v.first_source_sequence = first;
            if (last > v.last_source_sequence)
                v.last_source_sequence = last;
            v.count = UINT64_MAX - v.count < count ? UINT64_MAX : v.count + count;
            r->head = (r->head + 1) % EMU_OUTCOME_CAPACITY;
            r->count--;
            if (r->overwritten != UINT64_MAX)
                r->overwritten++;
        }
        insert(r, &v);
    }
    INIT(v);
    v.epoch = 1;
    v.generation = o->generation;
    v.event = *o;
    insert(r, &v);
    return EMUIF_OK;
}
emuif_status emu_outcome_page(const struct emu_outcome_ring *r, const char *session,
                              const struct emuif_trace_page_request *q,
                              struct emuif_trace_page *out)
{
    struct emuif_trace_page p;
    emuif_status status;
    uint32_t i, offset = 0, available;
    status = sized(q, sizeof *q);
    if (status)
        return status;
    status = sized(out, sizeof *out);
    if (status)
        return status;
    if (q->destination_id || q->max_records > 16 || !q->epoch ||
        !zero(q->reserved, sizeof q->reserved) || !zero(out->reserved, sizeof out->reserved) ||
        !memchr(q->session_id, 0, sizeof q->session_id) ||
        (out->capacity && (!out->records || out->element_size < sizeof(struct emuif_trace_record))))
        return EMUIF_INVALID;
    for (i = 0; i < 32; i++)
        if (!((q->session_id[i] >= '0' && q->session_id[i] <= '9') ||
              (q->session_id[i] >= 'a' && q->session_id[i] <= 'f')))
            return EMUIF_INVALID;
    if (q->session_id[32])
        return EMUIF_INVALID;
    INIT(p);
    p.struct_size = out->struct_size;
    memcpy(p.session_id, session, 33);
    p.epoch = 1;
    p.requested_after = q->after_record_sequence;
    p.next_after = q->after_record_sequence;
    p.overwritten = r->overwritten;
    p.records = out->records;
    p.capacity = out->capacity;
    p.element_size = out->element_size;
    if (r->count)
    {
        p.oldest = r->records[r->head].record_sequence;
        p.newest = r->records[(r->head + r->count - 1) % EMU_OUTCOME_CAPACITY].record_sequence;
    }
    if (strcmp(q->session_id, session))
        p.cursor_state = EMUIF_CURSOR_WRONG_SESSION;
    else if (q->epoch != 1)
        p.cursor_state = EMUIF_CURSOR_WRONG_EPOCH;
    else if (q->after_record_sequence > p.newest)
        p.cursor_state = EMUIF_CURSOR_AHEAD;
    if (p.cursor_state)
    {
        *out = p;
        return EMUIF_OK;
    }
    if (p.oldest && q->after_record_sequence < p.oldest - 1)
    {
        p.cursor_state = EMUIF_CURSOR_EXPIRED;
        p.lost_before_page = p.oldest - q->after_record_sequence - 1;
    }
    else if (r->count && q->after_record_sequence >= p.oldest)
        offset = (uint32_t)(q->after_record_sequence - p.oldest + 1);
    available = r->count - offset;
    p.required = available < q->max_records ? available : q->max_records;
    if (out->capacity < p.required)
    {
        p.remaining = available;
        p.more = available != 0;
        *out = p;
        return EMUIF_CAPACITY;
    }
    for (i = 0; i < p.required; i++)
    {
        struct emuif_trace_record local;
        struct emuif_trace_record *slot=&local;
        memcpy(slot,(const char *)p.records+(size_t)i*p.element_size,sizeof *slot);
        status = sized(slot, sizeof *slot);
        if (status) return status;
        if (slot->struct_size > p.element_size || slot->reserved || slot->event.reserved_pc ||
            !zero(slot->event.reserved_bytes, sizeof slot->event.reserved_bytes) ||
            !zero(slot->event.reserved_text, sizeof slot->event.reserved_text) ||
            slot->event.old_value.reserved || slot->event.new_value.reserved) return EMUIF_INVALID;
        status = sized(&slot->event, sizeof slot->event);
        if (status) return status;
        status = sized(&slot->event.old_value, sizeof slot->event.old_value);
        if (status) return status;
        status = sized(&slot->event.new_value, sizeof slot->event.new_value);
        if (status) return status;
    }
    for (i = 0; i < p.required; i++)
    {
        struct emuif_trace_record previous,v=r->records[(r->head+offset+i)%EMU_OUTCOME_CAPACITY];
        unsigned char *slot=(unsigned char *)p.records+(size_t)i*p.element_size;
        memcpy(&previous,slot,sizeof previous);
        v.struct_size=previous.struct_size; v.event.struct_size=previous.event.struct_size;
        v.event.old_value.struct_size=previous.event.old_value.struct_size;
        v.event.new_value.struct_size=previous.event.new_value.struct_size;
        memcpy(slot,&v,sizeof v);
    }
    p.written = p.required;
    p.remaining = available - p.written;
    p.more = p.remaining != 0;
    if (p.written)
    {
        p.first = r->records[(r->head + offset) % EMU_OUTCOME_CAPACITY].record_sequence;
        p.last =
            r->records[(r->head + offset + p.written - 1) % EMU_OUTCOME_CAPACITY].record_sequence;
        p.next_after = p.last;
    }
    *out = p;
    return EMUIF_OK;
}
static const char *status_name(emuif_status s)
{
    static const char *names[] = {"OK",       "INVALID",     "VERSION", "STATE", "RANGE",
                                  "CAPACITY", "UNSUPPORTED", "BUSY",    "STALE", "DENIED",
                                  "FAULT",    "EXHAUSTED",   "IO",      "HASH",  "NOT_FOUND"};
    return s <= EMUIF_NOT_FOUND ? names[s] : "FAULT";
}
static void value_json(struct output *o, const struct emuif_observed_value *v)
{
    output_format(o, "{\"value\":\"%" PRIu64 "\",\"width\":%u,\"known\":%s,\"isSigned\":%s}",
                  v->value, v->width, v->known ? "true" : "false", v->is_signed ? "true" : "false");
}

void emu_observation_json(struct output *o, const struct emuif_observation *v)
{
    unsigned i;
    output_raw(o, "{");
    output_raw(o, "\"generation\":");
    output_format(o, "\"%" PRIu64 "\"", v->generation);
    output_raw(o, ",\"sequence\":");
    output_format(o, "\"%" PRIu64 "\"", v->sequence);
    output_raw(o, ",\"cycle\":");
    output_format(o, "\"%" PRIu64 "\"", v->cycle);
    output_raw(o, ",\"instructionId\":");
    output_format(o, "\"%" PRIu64 "\"", v->instruction_id);
    output_raw(o, ",\"legacyInstructions\":");
    output_format(o, "\"%" PRIu64 "\"", v->legacy_instructions);
    output_raw(o, ",\"retiredInstructions\":");
    output_format(o, "\"%" PRIu64 "\"", v->retired_instructions);
    output_raw(o, ",\"accessId\":");
    output_format(o, "\"%" PRIu64 "\"", v->access_id);
    output_raw(o, ",\"parentStimulusId\":");
    output_format(o, "\"%" PRIu64 "\"", v->parent_stimulus_id);
    output_raw(o, ",\"cpuInputOrder\":");
    output_format(o, "\"%" PRIu64 "\"", v->cpu_input_order);
    output_raw(o, ",\"frameId\":");
    output_format(o, "\"%" PRIu64 "\"", v->frame_id);
    output_raw(o, ",\"relatedFrameId\":");
    output_format(o, "\"%" PRIu64 "\"", v->related_frame_id);
    output_raw(o, ",\"contextId\":");
    output_format(o, "\"%" PRIu64 "\"", v->context_id);
    output_raw(o, ",\"parentContextId\":");
    output_format(o, "\"%" PRIu64 "\"", v->parent_context_id);
    output_raw(o, ",\"scheduledCycle\":");
    output_format(o, "\"%" PRIu64 "\"", v->scheduled_cycle);
    output_raw(o, ",\"startCycle\":");
    output_format(o, "\"%" PRIu64 "\"", v->start_cycle);
    output_raw(o, ",\"sourceEventSequence\":");
    output_format(o, "\"%" PRIu64 "\"", v->source_event_sequence);
    output_raw(o, ",\"beginCycle\":");
    output_format(o, "\"%" PRIu64 "\"", v->begin_cycle);
    output_raw(o, ",\"kind\":");
    output_format(o, "%u", (unsigned)v->kind);
    output_raw(o, ",\"inputStage\":");
    output_format(o, "%u", (unsigned)v->input_stage);
    output_raw(o, ",\"phase\":");
    output_format(o, "%u", (unsigned)v->phase);
    output_raw(o, ",\"reason\":");
    output_format(o, "%u", (unsigned)v->reason);
    output_raw(o, ",\"known\":");
    output_format(o, "%u", (unsigned)v->known);
    output_raw(o, ",\"status\":");
    output_string(o, status_name(v->status));
    output_raw(o, ",\"partialEffects\":");
    output_raw(o, v->partial_effects ? "true" : "false");
    output_raw(o, ",\"space\":");
    output_format(o, "%u", (unsigned)v->space);
    output_raw(o, ",\"access\":");
    output_format(o, "%u", (unsigned)v->access);
    output_raw(o, ",\"frameKind\":");
    output_format(o, "%u", (unsigned)v->frame_kind);
    output_raw(o, ",\"irqSource\":");
    output_format(o, "%u", (unsigned)v->irq_source);
    output_raw(o, ",\"irqDepth\":");
    output_format(o, "%u", (unsigned)v->irq_depth);
    output_raw(o, ",\"watchId\":");
    output_format(o, "%u", (unsigned)v->watch_id);
    output_raw(o, ",\"firedActions\":");
    output_format(o, "%u", (unsigned)v->fired_actions);
    output_raw(o, ",\"pc\":");
    output_format(o, "%u", (unsigned)v->pc);
    output_raw(o, ",\"target\":");
    output_format(o, "%u", (unsigned)v->target);
    output_raw(o, ",\"address\":");
    output_format(o, "%u", (unsigned)v->address);
    output_raw(o, ",\"returnPc\":");
    output_format(o, "%u", (unsigned)v->return_pc);
    output_raw(o, ",\"nextPc\":");
    output_format(o, "%u", (unsigned)v->next_pc);
    output_raw(o, ",\"spBefore\":");
    output_format(o, "%u", (unsigned)v->sp_before);
    output_raw(o, ",\"spAfter\":");
    output_format(o, "%u", (unsigned)v->sp_after);
    output_raw(o, ",\"bytes\":");
    output_raw(o, "[");
    for (i = 0; i < v->byte_count && i < 3; i++)
        output_format(o, "%s%u", i ? "," : "", v->bytes[i]);
    output_raw(o, "]");
    output_raw(o, ",\"uartMode\":");
    output_format(o, "%u", (unsigned)v->uart_mode);
    output_raw(o, ",\"uartWidth\":");
    output_format(o, "%u", (unsigned)v->uart_width);
    output_raw(o, ",\"ninth\":");
    output_format(o, "%u", (unsigned)v->ninth);
    output_raw(o, ",\"scon\":");
    output_format(o, "%u", (unsigned)v->scon);
    output_raw(o, ",\"tcon\":");
    output_format(o, "%u", (unsigned)v->tcon);
    output_raw(o, ",\"oldValue\":");
    value_json(o, &v->old_value);
    output_raw(o, ",\"newValue\":");
    value_json(o, &v->new_value);
    output_raw(o, ",\"text\":");
    output_string(o, v->text);
    output_raw(o, "}");
}
void emu_trace_record_json(struct output *o, const struct emuif_trace_record *r)
{
    unsigned i;
    output_format(o,
                  "{\"epoch\":\"%" PRIu64 "\",\"recordSequence\":\"%" PRIu64
                  "\",\"generation\":\"%" PRIu64 "\",\"destinationId\":%u,\"traceIds\":[",
                  r->epoch, r->record_sequence, r->generation, r->destination_id);
    for (i = 0; i < r->route_count; i++)
        output_format(o, "%s%u", i ? "," : "", r->trace_ids[i]);
    output_format(o, "],\"kind\":%u", r->kind);
    if (r->kind == EMUIF_RECORD_EVENT)
    {
        output_raw(o, ",\"event\":");
        emu_observation_json(o, &r->event);
    }
    else if (r->kind == EMUIF_RECORD_LOSS || r->kind == EMUIF_RECORD_SUPPRESSION)
        output_format(o,
                      ",\"firstSourceSequence\":\"%" PRIu64 "\",\"lastSourceSequence\":\"%" PRIu64
                      "\",\"count\":\"%" PRIu64 "\"",
                      r->first_source_sequence, r->last_source_sequence, r->count);
    if (r->kind == EMUIF_RECORD_SUPPRESSION)
        output_format(o,",\"traceId\":%u,\"interruptPolicy\":%u,\"entryDepth\":%u,\"maximumDepth\":%u",
                      r->trace_id,r->interrupt_policy,r->entry_depth,r->maximum_depth);
    if (r->kind == EMUIF_RECORD_SINK) {
        static const char *statuses[]={"OK","INVALID","VERSION","STATE","RANGE","CAPACITY","UNSUPPORTED","BUSY","STALE","DENIED","FAULT","EXHAUSTED","IO","HASH","NOT_FOUND"};
        output_format(o,",\"status\":\"%s\"",statuses[r->status <= EMUIF_NOT_FOUND ? r->status : EMUIF_FAULT]);
    }
    output_raw(o, "}");
}
void emu_trace_page_json(struct output *o, const struct emuif_trace_page *p)
{
    static const char *states[] = {"current", "expired", "ahead", "wrong-session", "wrong-epoch"};
    unsigned i;
    output_format(
        o,
        "{\"sessionId\":\"%s\",\"destinationId\":%u,\"epoch\":\"%" PRIu64
        "\",\"cursorState\":\"%s\",\"requestedAfter\":\"%" PRIu64 "\",\"oldest\":\"%" PRIu64
        "\",\"newest\":\"%" PRIu64 "\",\"first\":\"%" PRIu64 "\",\"last\":\"%" PRIu64
        "\",\"nextAfter\":\"%" PRIu64 "\",\"overwritten\":\"%" PRIu64 "\",\"dropped\":\"%" PRIu64
        "\",\"lostBeforePage\":\"%" PRIu64 "\",\"remaining\":%u,\"more\":%s,\"records\":[",
        p->session_id, p->destination_id, p->epoch, states[p->cursor_state], p->requested_after,
        p->oldest, p->newest, p->first, p->last, p->next_after, p->overwritten, p->dropped,
        p->lost_before_page, p->remaining, p->more ? "true" : "false");
    for (i = 0; i < p->written; i++)
    {
        if (i)
            output_raw(o, ",");
        emu_trace_record_json(o, (const struct emuif_trace_record *)((const char *)p->records +
                                                                     (size_t)i * p->element_size));
    }
    output_raw(o, "]}");
}

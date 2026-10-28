/* Canonical projection over the shared matcher/router. Copyright 2026
 * Hans-Einar. MIT. */
#include "emu_trace_internal.h"
#include "emu_sha_internal.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#define INIT(x)                                                                                    \
    do                                                                                             \
    {                                                                                              \
        memset(&(x), 0, sizeof(x));                                                                \
        (x).struct_size = sizeof(x);                                                               \
        (x).api_version = 1;                                                                       \
    } while (0)
struct trace_config
{
    struct emuif_trace_session traces[16];
    struct emuif_trace_destination destinations[8];
    struct emuif_trace_point points[32];
    struct emuif_trace_gate gates[32];
    struct emuif_trace_watch watches[64];
    uint32_t nt, nd, np, ng, nw;
};
struct trace_ring
{
    struct emuif_trace_record *records;
    uint32_t capacity, head, count;
    uint64_t epoch, next, overwritten, dropped;
};
struct capture
{
    struct emuif_capture_status value;
    uint64_t max_records, max_cycles, last_cycle, last_generation;
    uint32_t stop_cpu;
};
struct stop_match
{
    uint32_t id;
    uint64_t event, source;
};
struct emu_trace_state
{
    struct em8051_debug_runtime *runtime;
    struct emu_trace_metadata metadata;
    struct trace_config config;
    struct trace_ring rings[8];
    struct emu_trace_file_provider provider;
    uint32_t seen[5][64], seen_count[5];
    uint64_t revision, coverage, generation, cycle, sequence, current_instruction;
    struct capture captures[64];
    uint32_t capture_count;
    struct stop_match pending_matches[64], applied_matches[64];
    uint32_t pending_count, applied_count;
    uint64_t pending_source, pending_instruction, applied_source, applied_instruction;
    uint32_t pending_stop, event_tick;
};
static emuif_status sized(const void *p, size_t need)
{
    uint32_t n;
    uint16_t version, flags;
    if (!p)
        return EMUIF_INVALID;
    memcpy(&n, p, 4);
    if (n < 8)
        return EMUIF_INVALID;
    memcpy(&version, (const char *)p + 4, 2);
    memcpy(&flags, (const char *)p + 6, 2);
    if (n < need || flags)
        return EMUIF_INVALID;
    return version == 1 ? EMUIF_OK : EMUIF_VERSION;
}
static int zero(const void *p, size_t n)
{
    const unsigned char *v = p;
    while (n--)
        if (*v++)
            return 0;
    return 1;
}
static int string_valid(const char *s, size_t size)
{
    const char *end = memchr(s, 0, size);
    return end && utf8_valid((const unsigned char *)s, (size_t)(end - s)) &&
           zero(end, (size_t)(s + size - end));
}
static void saturate(uint64_t *n, uint64_t v) { *n = UINT64_MAX - *n < v ? UINT64_MAX : *n + v; }
static int trace_index(const struct trace_config *c, uint32_t id)
{
    uint32_t i;
    for (i = 0; i < c->nt; i++)
        if (c->traces[i].trace_id == id)
            return (int)i;
    return -1;
}
static int dest_index(const struct trace_config *c, uint32_t id)
{
    uint32_t i;
    for (i = 0; i < c->nd; i++)
        if (c->destinations[i].destination_id == id)
            return (int)i;
    return -1;
}
static struct capture *capture_for(struct emu_trace_state *s, uint32_t id)
{
    uint32_t i = s->capture_count;
    while (i)
    {
        struct capture *c = &s->captures[--i];
        if (c->value.trace_id == id)
            return c;
    }
    return NULL;
}
static int routes_valid(const struct trace_config *c, const uint32_t *ids, uint32_t n)
{
    uint32_t i;
    if (n > 16)
        return 0;
    for (i = 0; i < n; i++)
        if (!ids[i] || (i && ids[i] <= ids[i - 1]) || trace_index(c, ids[i]) < 0)
            return 0;
    return zero(ids + n, (16 - n) * sizeof(*ids));
}
static emuif_status access_supported(struct emu_trace_state *s, uint32_t space, uint16_t first,
                                     uint16_t last, uint32_t mask)
{
    if (s->coverage & EMUIF_OBS_COVER_ACCESS)
        return EMUIF_OK;
    if (space == EMUIF_SPACE_XDATA && !(mask & ~(EMUIF_OBS_ACCESS_READ | EMUIF_OBS_ACCESS_WRITE)))
        return EMUIF_OK;
    if (space == EMUIF_SPACE_SFR && first == 0x99 && last == 0x99 &&
        !(mask & ~(EMUIF_OBS_ACCESS_WRITE | EMUIF_OBS_ACCESS_RMW)))
        return EMUIF_OK;
    return EMUIF_UNSUPPORTED;
}
static emuif_status selector(struct emu_trace_state *s, const struct emuif_trace_selector *v,
                             struct em8051_debug_trace_selector *out)
{
    emuif_status e = sized(v, sizeof *v);
    uint32_t m;
    if (e)
        return e;
    m = v->match_mask;
    if (m > 31 || v->kind > 28 || v->space > 3 || v->access_mask > 15 || v->pc_first > v->pc_last ||
        v->address_first > v->address_last || (!(m & 1) && v->kind) || ((m & 1) && !v->kind) ||
        (!(m & 2) && (v->pc_first || v->pc_last)) ||
        (!(m & 4) && (v->address_first || v->address_last)) || (!(m & 8) && v->space) ||
        (!(m & 16) && v->access_mask) || ((m & 16) && !v->access_mask))
        return EMUIF_INVALID;
    if ((m & 28) || ((m & 1) && (v->kind == 4 || v->kind == 5)))
    {
        if (!(m & 8))
            return EMUIF_UNSUPPORTED;
        e = access_supported(s, v->space, (m & 4) ? v->address_first : 0,
                             (m & 4) ? v->address_last : 65535, (m & 16) ? v->access_mask : 15);
        if (e)
            return e;
    }
    memset(out, 0, sizeof *out);
    out->canonical = 1;
    out->match_observation_kind = (m & 1) != 0;
    out->observation_kind = (uint8_t)v->kind;
    out->match_pc = (m & 2) != 0;
    out->pc_first = v->pc_first;
    out->pc_last = v->pc_last;
    out->match_address = (m & 4) != 0;
    out->address_first = v->address_first;
    out->address_last = v->address_last;
    out->canonical_space = (m & 8) ? (uint8_t)(v->space + 1) : 0;
    out->match_access = (m & 16) != 0;
    out->access_mask = (uint8_t)v->access_mask;
    return EMUIF_OK;
}
static int condition_valid(const struct emuif_trace_condition *c)
{
    if (c->reserved || c->present > 1 || c->operand > 1 || c->comparison > 5 || c->is_signed > 1 ||
        !(c->width == 1 || c->width == 8 || c->width == 16 || c->width == 32 || c->width == 64))
        return 0;
    if (c->width < 64 && c->constant >> c->width)
        return 0;
    return c->present ||
           (!c->operand && !c->comparison && c->width == 8 && !c->is_signed && !c->constant);
}
static emuif_status copy_array(const struct emuif_trace_array *a, void *to, size_t size,
                               uint32_t max, uint32_t *n)
{
    uint32_t i;
    emuif_status e = sized(a, sizeof *a);
    if (e)
        return e;
    if (a->count > max || (a->count && (!a->elements || a->element_size < size)) ||
        (uint64_t)a->element_size * a->count > SIZE_MAX)
        return EMUIF_INVALID;
    for (i = 0; i < a->count; i++)
    {
        const char *p = (const char *)a->elements + (size_t)i * a->element_size;
        e = sized(p, size);
        if (e)
            return e;
        memcpy((char *)to + (size_t)i * size, p, size);
    }
    *n = a->count;
    return EMUIF_OK;
}
static emuif_status adapt(struct emu_trace_state *s, const struct trace_config *c,
                          struct em8051_debug_trace_config *out, struct em8051_debug_watch *w)
{
    uint32_t i, j;
    emuif_status e;
    memset(out, 0, sizeof *out);
    memset(w, 0, 64 * sizeof *w);
    out->trace_count = (uint8_t)c->nt;
    out->destination_count = (uint8_t)c->nd;
    out->point_count = (uint8_t)c->np;
    out->gate_count = (uint8_t)c->ng;
    for (i = 0; i < c->nd; i++)
    {
        const struct emuif_trace_destination *v = &c->destinations[i];
        if (!v->destination_id ||
            (i && v->destination_id <= c->destinations[i - 1].destination_id) || v->kind > 1 ||
            v->stop_on_full > 1 ||
            (v->kind == 0 ? (v->capacity < 2 || v->capacity > 4096)
                          : (v->capacity != 256 || v->stop_on_full)))
            return EMUIF_INVALID;
        if (v->kind == 1 && !s->provider.open)
            return EMUIF_UNSUPPORTED;
        out->destinations[i].destination_id = v->destination_id;
    }
    for (i = 0; i < c->nt; i++)
    {
        const struct emuif_trace_session *v = &c->traces[i];
        struct em8051_debug_trace_session *d = &out->traces[i];
        if (!v->trace_id || (i && v->trace_id <= c->traces[i - 1].trace_id) ||
            dest_index(c, v->destination_id) < 0 || v->enabled > 1 || v->interrupt_policy > 2 ||
            !string_valid(v->tag, sizeof v->tag) || !string_valid(v->comment, sizeof v->comment) ||
            !zero(v->reserved, sizeof v->reserved))
            return EMUIF_INVALID;
        d->trace_id = v->trace_id;
        d->destination_id = v->destination_id;
        d->enabled = (uint8_t)v->enabled;
        d->interrupt_policy = (uint8_t)v->interrupt_policy;
        memcpy(d->tag, v->tag, sizeof d->tag);
        memcpy(d->comment, v->comment, sizeof d->comment);
    }
    for (i = 0; i < c->np; i++)
    {
        const struct emuif_trace_point *v = &c->points[i];
        struct em8051_debug_trace_point *d = &out->points[i];
        if (!v->point_id || (i && v->point_id <= c->points[i - 1].point_id) || v->enabled > 1 ||
            v->reserved || !routes_valid(c, v->trace_ids, v->route_count))
            return EMUIF_INVALID;
        e = selector(s, &v->selector, &d->selector);
        if (e)
            return e;
        d->point_id = v->point_id;
        d->enabled = (uint8_t)v->enabled;
        d->trace_id_count = (uint8_t)v->route_count;
        memcpy(d->trace_ids, v->trace_ids, sizeof d->trace_ids);
    }
    for (i = 0; i < c->ng; i++)
    {
        const struct emuif_trace_gate *v = &c->gates[i];
        struct em8051_debug_trace_gate *d = &out->gates[i];
        if (!v->gate_id || (i && v->gate_id <= c->gates[i - 1].gate_id) || v->enabled > 1 ||
            v->reserved || v->action > 1 || v->timing > 1 ||
            !routes_valid(c, v->trace_ids, v->route_count))
            return EMUIF_INVALID;
        e = selector(s, &v->selector, &d->selector);
        if (e)
            return e;
        d->gate_id = v->gate_id;
        d->enabled = (uint8_t)v->enabled;
        d->action = (uint8_t)v->action;
        d->timing = (uint8_t)v->timing;
        d->trace_id_count = (uint8_t)v->route_count;
        memcpy(d->trace_ids, v->trace_ids, sizeof d->trace_ids);
    }
    for (i = 0; i < c->nw; i++)
    {
        const struct emuif_trace_watch *v = &c->watches[i];
        struct em8051_debug_watch *d = &w[i];
        if (!v->watch_id || (i && v->watch_id <= c->watches[i - 1].watch_id) || v->enabled > 1 ||
            v->space > 3 || !v->access_mask || v->access_mask > 15 ||
            v->address_first > v->address_last || !v->action_count || v->action_count > 8)
            return EMUIF_INVALID;
        e = access_supported(s, v->space, v->address_first, v->address_last, v->access_mask);
        if (e)
            return e;
        d->id = v->watch_id;
        d->enabled = (uint8_t)v->enabled;
        d->canonical_space = (uint8_t)(v->space + 1);
        d->address_space = EM8051_DEBUG_SPACE_XDATA;
        d->address_first = v->address_first;
        d->address_last = v->address_last;
        d->access_mask = (uint8_t)v->access_mask;
        d->action_count = (uint8_t)v->action_count;
        for (j = 0; j < 8; j++)
        {
            const struct emuif_trace_action *a = &v->actions[j];
            struct em8051_debug_watch_action *b = &d->actions[j];
            if ((e = sized(a, sizeof *a)) || (e = sized(&a->condition, sizeof a->condition)))
                return e;
            if (j >= v->action_count)
            {
                if (!condition_valid(&a->condition) || a->kind || a->route_count ||
                    a->condition.present || !zero(a->trace_ids, sizeof a->trace_ids))
                    return EMUIF_INVALID;
                continue;
            }
            if (!condition_valid(&a->condition) || a->kind > 3 ||
                !routes_valid(c, a->trace_ids, a->route_count) || (a->kind != 3 && a->route_count))
                return EMUIF_INVALID;
            b->canonical = 1;
            b->kind = (uint8_t)a->kind;
            b->trace_id_count = (uint8_t)a->route_count;
            memcpy(b->trace_ids, a->trace_ids, sizeof b->trace_ids);
            b->condition.present = (uint8_t)a->condition.present;
            b->condition.operand = (uint8_t)a->condition.operand;
            b->condition.comparison = (uint8_t)a->condition.comparison;
            b->condition.width = (uint8_t)a->condition.width;
            b->condition.is_signed = (uint8_t)a->condition.is_signed;
            b->condition.constant = a->condition.constant;
            if (b->condition.is_signed && b->condition.width < 64 &&
                (b->condition.constant & (UINT64_C(1) << (b->condition.width - 1))))
                b->condition.constant |= UINT64_MAX << b->condition.width;
        }
    }
    return EMUIF_OK;
}
static void routes_json(struct output *o, const uint32_t *ids, uint32_t n)
{
    uint32_t i;
    output_raw(o, "[");
    for (i = 0; i < n; i++)
        output_format(o, "%s%u", i ? "," : "", ids[i]);
    output_raw(o, "]");
}
static void selector_json(struct output *o, const struct emuif_trace_selector *s)
{
    output_format(o,
                  "{\"accessMask\":%u,\"addressFirst\":%u,\"addressLast\":%u,\"kind\":%u,"
                  "\"matchMask\":%u,\"pcFirst\":%u,\"pcLast\":%u,\"space\":%u}",
                  s->access_mask, s->address_first, s->address_last, s->kind, s->match_mask,
                  s->pc_first, s->pc_last, s->space);
}
static void config_json(struct output *o, const struct trace_config *c)
{
    uint32_t i, j;
    output_raw(o, "{\"destinations\":[");
    for (i = 0; i < c->nd; i++)
    {
        const struct emuif_trace_destination *v = &c->destinations[i];
        output_format(o,
                      "%s{\"capacity\":%u,\"destinationId\":%u,\"kind\":%u,"
                      "\"stopOnFull\":%s}",
                      i ? "," : "", v->capacity, v->destination_id, v->kind,
                      v->stop_on_full ? "true" : "false");
    }
    output_raw(o, "],\"gates\":[");
    for (i = 0; i < c->ng; i++)
    {
        const struct emuif_trace_gate *v = &c->gates[i];
        output_format(o,
                      "%s{\"action\":%u,\"enabled\":%s,\"gateId\":%u,\"selector\":", i ? "," : "",
                      v->action, v->enabled ? "true" : "false", v->gate_id);
        selector_json(o, &v->selector);
        output_format(o, ",\"timing\":%u,\"traceIds\":", v->timing);
        routes_json(o, v->trace_ids, v->route_count);
        output_raw(o, "}");
    }
    output_raw(o, "],\"points\":[");
    for (i = 0; i < c->np; i++)
    {
        const struct emuif_trace_point *v = &c->points[i];
        output_format(o, "%s{\"enabled\":%s,\"pointId\":%u,\"selector\":", i ? "," : "",
                      v->enabled ? "true" : "false", v->point_id);
        selector_json(o, &v->selector);
        output_raw(o, ",\"traceIds\":");
        routes_json(o, v->trace_ids, v->route_count);
        output_raw(o, "}");
    }
    output_raw(o, "],\"traces\":[");
    for (i = 0; i < c->nt; i++)
    {
        const struct emuif_trace_session *v = &c->traces[i];
        output_format(o, "%s{\"comment\":", i ? "," : "");
        output_string(o, v->comment);
        output_format(o, ",\"destinationId\":%u,\"enabled\":%s,\"interruptPolicy\":%u,\"tag\":",
                      v->destination_id, v->enabled ? "true" : "false", v->interrupt_policy);
        output_string(o, v->tag);
        output_format(o, ",\"traceId\":%u}", v->trace_id);
    }
    output_raw(o, "],\"watches\":[");
    for (i = 0; i < c->nw; i++)
    {
        const struct emuif_trace_watch *v = &c->watches[i];
        output_format(o, "%s{\"accessMask\":%u,\"actions\":[", i ? "," : "", v->access_mask);
        for (j = 0; j < v->action_count; j++)
        {
            const struct emuif_trace_action *a = &v->actions[j];
            const struct emuif_trace_condition *d = &a->condition;
            output_format(o,
                          "%s{\"condition\":{\"comparison\":%u,\"constant\":\"%" PRIu64
                          "\",\"isSigned\":%s,\"operand\":%u,\"present\":%s,\"width\":%u},"
                          "\"kind\":%u,\"traceIds\":",
                          j ? "," : "", d->comparison, d->constant, d->is_signed ? "true" : "false",
                          d->operand, d->present ? "true" : "false", d->width, a->kind);
            routes_json(o, a->trace_ids, a->route_count);
            output_raw(o, "}");
        }
        output_format(o,
                      "],\"addressFirst\":%u,\"addressLast\":%u,\"enabled\":%s,"
                      "\"space\":%u,\"watchId\":%u}",
                      v->address_first, v->address_last, v->enabled ? "true" : "false", v->space,
                      v->watch_id);
    }
    output_raw(o, "]}");
}
static uint32_t config_id(const struct trace_config *c, unsigned ns, unsigned i)
{
    return ns == 0   ? c->traces[i].trace_id
           : ns == 1 ? c->destinations[i].destination_id
           : ns == 2 ? c->points[i].point_id
           : ns == 3 ? c->gates[i].gate_id
                     : c->watches[i].watch_id;
}
static uint32_t config_count(const struct trace_config *c, unsigned ns)
{
    return ns == 0 ? c->nt : ns == 1 ? c->nd : ns == 2 ? c->np : ns == 3 ? c->ng : c->nw;
}
static int config_has(const struct trace_config *c, unsigned ns, uint32_t id)
{
    unsigned i;
    for (i = 0; i < config_count(c, ns); i++)
        if (config_id(c, ns, i) == id)
            return 1;
    return 0;
}
static void request_stop(struct emu_trace_state *s, uint32_t reason, uint64_t seq,
                         uint64_t instruction)
{
    if (!s->pending_stop)
    {
        s->pending_source = seq;
        s->pending_instruction = instruction;
        s->pending_stop = reason;
    }
    if (reason == 5)
        s->pending_stop = 5;
}
static void capture_complete(struct emu_trace_state *s, struct capture *c)
{
    c->value.state = EMUIF_SCOPE_COMPLETE;
    c->value.reason = EMUIF_SCOPE_LIMIT;
    c->value.end_sequence = s->sequence;
    if (c->stop_cpu)
        request_stop(s, 7, s->event_tick ? s->sequence : 0,
                     s->event_tick ? s->current_instruction : 0);
}
static void ring_insert(struct trace_ring *r, struct emuif_trace_record *v)
{
    v->epoch = r->epoch;
    v->record_sequence = r->next++;
    r->records[(r->head + r->count) % r->capacity] = *v;
    r->count++;
}
static void storage_exhausted(struct emu_trace_state *s, uint32_t destination_id)
{
    unsigned i;
    for (i = 0; i < s->config.nt; i++)
        if (s->config.traces[i].destination_id == destination_id)
        {
            struct capture *c = capture_for(s, s->config.traces[i].trace_id);
            if (c &&
                (c->value.state == EMUIF_SCOPE_BOUND ||
                 (c->value.state == EMUIF_SCOPE_COMPLETE && c->value.end_sequence == s->sequence)))
            {
                c->value.state = EMUIF_SCOPE_INCOMPLETE;
                c->value.reason = EMUIF_SCOPE_OVERFLOW;
                c->value.end_sequence = s->sequence;
            }
        }
}
void emu_trace_poll_files(struct emu_trace_state *s)
{
    unsigned i;
    if (!s || !s->provider.status)
        return;
    for (i = 0; i < s->config.nd; i++)
        if (s->config.destinations[i].kind == EMUIF_DEST_FILE)
        {
            struct emuif_sink_status v;
            INIT(v);
            if (!s->provider.status(s->provider.context, s->config.destinations[i].destination_id,
                                    &v) &&
                v.error == EMUIF_EXHAUSTED)
                storage_exhausted(s, v.destination_id);
        }
}
static int ring_store(struct emu_trace_state *s, unsigned di, struct emuif_trace_record *v)
{
    struct trace_ring *r = &s->rings[di];
    const struct emuif_trace_destination *d = &s->config.destinations[di];
    unsigned needed = r->count == r->capacity && !d->stop_on_full ? 2 : 1;
    if (!r->next || UINT64_MAX - r->next < needed - 1)
    {
        storage_exhausted(s, d->destination_id);
        request_stop(s, 2, s->sequence, s->current_instruction);
        return 0;
    }
    if (r->count == r->capacity)
    {
        if (d->stop_on_full)
        {
            saturate(&r->dropped, 1);
            request_stop(s, 2, s->sequence, s->current_instruction);
            return 0;
        }
        struct emuif_trace_record loss;
        unsigned i;
        INIT(loss);
        INIT(loss.event);
        INIT(loss.event.old_value);
        INIT(loss.event.new_value);
        loss.event.old_value.width = loss.event.new_value.width = 8;
        loss.kind = EMUIF_RECORD_LOSS;
        loss.destination_id = d->destination_id;
        loss.generation = s->generation;
        for (i = 0; i < 2; i++)
        {
            const struct emuif_trace_record *old = &r->records[r->head];
            uint64_t first = 0, last = 0, count = 0;
            if (old->kind == EMUIF_RECORD_EVENT)
            {
                first = last = old->event.sequence;
                count = 1;
            }
            else if (old->kind == EMUIF_RECORD_SUPPRESSION)
            {
                first = old->first_source_sequence;
                last = old->last_source_sequence;
                count = old->count;
            }
            if (first && (!loss.first_source_sequence || first < loss.first_source_sequence))
                loss.first_source_sequence = first;
            if (last > loss.last_source_sequence)
                loss.last_source_sequence = last;
            saturate(&loss.count, count);
            r->head = (r->head + 1) % r->capacity;
            r->count--;
            saturate(&r->overwritten, 1);
        }
        ring_insert(r, &loss);
    }
    ring_insert(r, v);
    return 1;
}
static bool route_eligible(void *context, uint32_t id, const struct em8051_debug_event *e)
{
    struct emu_trace_state *s = context;
    struct capture *c = capture_for(s, id);
    (void)e;
    return !c || c->value.state == EMUIF_SCOPE_BOUND;
}
static void routed(void *context, const struct em8051_debug_trace_record *source)
{
    struct emu_trace_state *s = context;
    struct emuif_trace_record r;
    uint32_t i;
    int di = dest_index(&s->config, source->destination_id);
    if (di < 0)
        return;
    INIT(r);
    INIT(r.event);
    INIT(r.event.old_value);
    INIT(r.event.new_value);
    r.event.old_value.width = r.event.new_value.width = 8;
    r.generation = s->generation;
    r.destination_id = source->destination_id;
    r.route_count = source->trace_id_count;
    memcpy(r.trace_ids, source->trace_ids, r.route_count * sizeof(uint32_t));
    if (source->kind == EM8051_DEBUG_TRACE_RECORD_EVENT)
    {
        r.kind = EMUIF_RECORD_EVENT;
        r.event = source->event.observation;
        r.generation = r.event.generation;
        for (i = 0; i < r.route_count; i++)
        {
            struct capture *c = capture_for(s, r.trace_ids[i]);
            if (c && c->value.state == EMUIF_SCOPE_BOUND)
            {
                c->value.records++;
                if (c->value.records >= c->max_records)
                {
                    s->event_tick = 1;
                    capture_complete(s, c);
                    s->event_tick = 0;
                }
            }
        }
    }
    else
    {
        r.kind = EMUIF_RECORD_SUPPRESSION;
        r.trace_id = source->suppression.trace_id;
        r.interrupt_policy = source->suppression.interrupt_policy;
        r.entry_depth = source->suppression.entry_depth;
        r.maximum_depth = source->suppression.maximum_depth;
        r.first_source_sequence = source->suppression.first_sequence;
        r.last_source_sequence = source->suppression.last_sequence;
        r.count = source->suppression.count;
    }
    if (s->config.destinations[di].kind == EMUIF_DEST_FILE)
    {
        if (s->provider.enqueue && s->provider.enqueue(s->provider.context, &r) == EMUIF_EXHAUSTED)
            storage_exhausted(s, r.destination_id);
    }
    else
        ring_store(s, (unsigned)di, &r);
}
struct emu_trace_state *emu_trace_create(struct em8051_debug_runtime *runtime,
                                         const struct emu_trace_metadata *metadata,
                                         uint64_t coverage)
{
    struct emu_trace_state *s = calloc(1, sizeof *s);
    if (!s)
        return NULL;
    s->runtime = runtime;
    s->metadata = *metadata;
    s->coverage = coverage;
    s->generation = 1;
    if (emu_debug_runtime_delivery(runtime, s, routed, route_eligible))
    {
        free(s);
        return NULL;
    }
    return s;
}
void emu_trace_destroy(struct emu_trace_state *s)
{
    unsigned i;
    if (!s)
        return;
    for (i = 0; i < 8; i++)
        free(s->rings[i].records);
    free(s);
}
void emu_trace_set_provider(struct emu_trace_state *s, const struct emu_trace_file_provider *p)
{
    if (s)
        s->provider = *p;
}
int emu_trace_has_provider(const struct emu_trace_state *s) { return s && s->provider.open; }
emuif_status emu_trace_close_files(struct emu_trace_state *s)
{
    if (!s || !s->provider.close_all)
        return EMUIF_OK;
    emu_debug_runtime_flush(s->runtime);
    return s->provider.close_all(s->provider.context, s->generation);
}
void emu_trace_tick(struct emu_trace_state *s, uint64_t generation, uint64_t cycle)
{
    unsigned i;
    if (!s)
        return;
    if (generation != s->generation || cycle > s->cycle)
        s->cycle = cycle;
    s->generation = generation;
    for (i = 0; i < s->capture_count; i++)
    {
        struct capture *c = &s->captures[i];
        if (c->value.state != EMUIF_SCOPE_BOUND)
            continue;
        c->value.generation = generation;
        if (generation == c->last_generation && cycle >= c->last_cycle)
        {
            uint64_t add = cycle - c->last_cycle;
            if (add > c->max_cycles - c->value.cycles)
                add = c->max_cycles - c->value.cycles;
            c->value.cycles += add;
        }
        if (generation != c->last_generation || cycle > c->last_cycle)
            c->last_cycle = cycle;
        c->last_generation = generation;
        if (c->value.cycles >= c->max_cycles)
            capture_complete(s, c);
    }
}
void emu_trace_observe(struct emu_trace_state *s, const struct emuif_observation *o)
{
    unsigned i;
    if (!s)
        return;
    s->sequence = o->sequence;
    s->current_instruction = o->instruction_id;
    s->event_tick = 1;
    emu_trace_tick(s, o->generation, o->cycle);
    s->event_tick = 0;
    if (o->kind == EMUIF_OBS_WATCH)
    {
        for (i = 0; i < s->config.nw; i++)
            if (s->config.watches[i].watch_id == o->watch_id)
            {
                unsigned j;
                int stopping = 0;
                for (j = 0; j < s->config.watches[i].action_count; j++)
                    if ((o->fired_actions & (1u << j)) &&
                        s->config.watches[i].actions[j].kind == EMUIF_ACTION_STOP)
                        stopping = 1;
                if (stopping)
                {
                    unsigned at = 0;
                    while (at < s->pending_count && s->pending_matches[at].id < o->watch_id)
                        at++;
                    if (at == s->pending_count || s->pending_matches[at].id != o->watch_id)
                    {
                        memmove(s->pending_matches + at + 1, s->pending_matches + at,
                                (s->pending_count - at) * sizeof(s->pending_matches[0]));
                        s->pending_matches[at] =
                            (struct stop_match){o->watch_id, o->sequence, o->source_event_sequence};
                        s->pending_count++;
                    }
                    request_stop(s, 5, o->source_event_sequence, o->instruction_id);
                }
                break;
            }
    }
}
int emu_trace_stop_pending(const struct emu_trace_state *s) { return s ? (int)s->pending_stop : 0; }
uint64_t emu_trace_stop_source(const struct emu_trace_state *s)
{
    return !s ? 0 : s->pending_stop ? s->pending_source : s->applied_source;
}
uint64_t emu_trace_stop_instruction(const struct emu_trace_state *s)
{
    return !s ? 0 : s->pending_stop ? s->pending_instruction : s->applied_instruction;
}
void emu_trace_applied_stop(struct emu_trace_state *s, uint32_t reason)
{
    struct em8051_debug_runtime_stop old;
    if (!s)
        return;
    (void)reason;
    s->applied_count = s->pending_count;
    memcpy(s->applied_matches, s->pending_matches,
           s->pending_count * sizeof(s->pending_matches[0]));
    s->applied_source = s->pending_source;
    s->applied_instruction = s->pending_instruction;
    s->pending_count = s->pending_stop = 0;
    s->pending_source = s->pending_instruction = 0;
    em8051_debug_runtime_consume_stop(s->runtime, &old);
}
void emu_trace_stop_detail(struct emu_trace_state *s, struct output *o)
{
    unsigned i;
    output_raw(o, "[");
    for (i = 0; s && i < s->applied_count; i++)
        output_format(
            o, "%s{\"watchId\":%u,\"sequence\":\"%" PRIu64 "\",\"sourceSequence\":\"%" PRIu64 "\"}",
            i ? "," : "", s->applied_matches[i].id, s->applied_matches[i].event,
            s->applied_matches[i].source);
    output_raw(o, "]");
}
static void sync_enabled(struct emu_trace_state *s)
{
    struct em8051_debug_trace_session traces[16];
    size_t written, total, i;
    if (!em8051_debug_runtime_list_traces(s->runtime, 0, traces, 16, &written, &total))
        for (i = 0; i < written; i++)
        {
            int at = trace_index(&s->config, traces[i].trace_id);
            if (at >= 0)
                s->config.traces[at].enabled = traces[i].enabled;
        }
}
emuif_status emu_trace_replace(struct emu_trace_state *s, const struct emuif_trace_config *q,
                               struct emuif_trace_revision *out)
{
    struct trace_config *c;
    struct em8051_debug_trace_config *legacy;
    struct em8051_debug_watch *w;
    struct output *encoded;
    struct trace_ring next[8];
    uint32_t added[5][64], counts[5] = {0};
    unsigned i, j, k;
    emuif_status e;
    if (!s)
        return EMUIF_UNSUPPORTED;
    if ((e = sized(q, sizeof *q)) || (e = sized(out, sizeof *out)))
        return e;
    if (q->generation != s->generation || q->expected_revision != s->revision)
        return EMUIF_STALE;
    if (s->revision == UINT64_MAX)
        return EMUIF_EXHAUSTED;
    if (s->provider.active && s->provider.active(s->provider.context))
        return EMUIF_BUSY;
    c = calloc(1, sizeof *c);
    legacy = calloc(1, sizeof *legacy);
    w = calloc(64, sizeof *w);
    encoded = calloc(1, sizeof *encoded);
    memset(next, 0, sizeof next);
    if (!c || !legacy || !w || !encoded)
    {
        e = EMUIF_CAPACITY;
        goto done;
    }
#define COPY(member, field, max, count)                                                            \
    if ((e = copy_array(&q->member, c->field, sizeof c->field[0], max, &c->count)))                \
    goto done
    COPY(traces, traces, 16, nt);
    COPY(destinations, destinations, 8, nd);
    COPY(points, points, 32, np);
    COPY(gates, gates, 32, ng);
    COPY(watches, watches, 64, nw);
#undef COPY
    e = adapt(s, c, legacy, w);
    if (e)
        goto done;
    config_json(encoded, c);
    if (encoded->failed || encoded->length > 16384)
    {
        e = EMUIF_CAPACITY;
        goto done;
    }
    for (i = 0; i < 5; i++)
        for (j = 0; j < config_count(c, i); j++)
        {
            uint32_t id = config_id(c, i, j);
            if (config_has(&s->config, i, id))
                continue;
            for (k = 0; k < s->seen_count[i]; k++)
                if (s->seen[i][k] == id)
                {
                    e = EMUIF_INVALID;
                    goto done;
                }
            if (s->seen_count[i] + counts[i] == 64)
            {
                e = EMUIF_EXHAUSTED;
                goto done;
            }
            added[i][counts[i]++] = id;
        }
    for (i = 0; i < c->nd; i++)
    {
        int old = dest_index(&s->config, c->destinations[i].destination_id);
        uint32_t capacity = c->destinations[i].capacity;
        if (old >= 0)
        {
            size_t count = s->rings[old].count + emu_debug_runtime_pending_summaries(
                                                     s->runtime, c->destinations[i].destination_id);
            if (c->destinations[i].kind != s->config.destinations[old].kind)
            {
                e = EMUIF_INVALID;
                goto done;
            }
            if (count > s->rings[old].capacity)
                count = s->rings[old].capacity;
            if (c->destinations[i].kind == 0 && capacity < count)
            {
                e = EMUIF_BUSY;
                goto done;
            }
        }
        if (c->destinations[i].kind == 0)
        {
            next[i].records = calloc(capacity, sizeof(struct emuif_trace_record));
            if (!next[i].records)
            {
                e = EMUIF_CAPACITY;
                goto done;
            }
            next[i].capacity = capacity;
            next[i].epoch = next[i].next = 1;
        }
    }
    switch (emu_debug_runtime_configure(s->runtime, legacy, w, c->nw))
    {
    case EM8051_DEBUG_EVENT_OK:
        break;
    case EM8051_DEBUG_EVENT_BUSY:
        e = EMUIF_BUSY;
        goto done;
    case EM8051_DEBUG_EVENT_LIMIT:
        e = EMUIF_CAPACITY;
        goto done;
    default:
        e = EMUIF_INVALID;
        goto done;
    }
    for (i = 0; i < c->nd; i++)
    {
        int old = dest_index(&s->config, c->destinations[i].destination_id);
        if (old >= 0 && next[i].records)
        {
            struct trace_ring *r = &s->rings[old];
            next[i].epoch = r->epoch;
            next[i].next = r->next;
            next[i].overwritten = r->overwritten;
            next[i].dropped = r->dropped;
            next[i].count = r->count;
            for (j = 0; j < r->count; j++)
                next[i].records[j] = r->records[(r->head + j) % r->capacity];
        }
    }
    for (i = 0; i < s->config.nt; i++)
        if (trace_index(c, s->config.traces[i].trace_id) < 0)
        {
            struct capture *cap = capture_for(s, s->config.traces[i].trace_id);
            if (cap && cap->value.state <= 1)
            {
                cap->value.state = EMUIF_SCOPE_STOPPED;
                cap->value.reason = EMUIF_SCOPE_EXPLICIT;
                cap->value.end_sequence = s->sequence;
            }
        }
    for (i = 0; i < 8; i++)
        free(s->rings[i].records);
    memcpy(s->rings, next, sizeof next);
    memset(next, 0, sizeof next);
    s->config = *c;
    for (i = 0; i < 5; i++)
        for (j = 0; j < counts[i]; j++)
            s->seen[i][s->seen_count[i]++] = added[i][j];
    s->revision++;
    out->revision = s->revision;
    e = EMUIF_OK;
done:
    for (i = 0; i < 8; i++)
        free(next[i].records);
    free(c);
    free(legacy);
    free(w);
    free(encoded);
    return e;
}
emuif_status emu_trace_page(struct emu_trace_state *s, const struct emuif_trace_page_request *q,
                            struct emuif_trace_page *out)
{
    struct emuif_trace_page p;
    struct trace_ring *r;
    uint32_t i, offset = 0, available;
    int di;
    emuif_status e;
    if (!s)
        return EMUIF_UNSUPPORTED;
    if ((e = sized(q, sizeof *q)) || (e = sized(out, sizeof *out)))
        return e;
    if (q->max_records > 16 || !q->epoch || !zero(q->reserved, sizeof q->reserved) ||
        !zero(out->reserved, sizeof out->reserved) ||
        !string_valid(q->session_id, sizeof q->session_id) || strlen(q->session_id) != 32 ||
        (out->capacity &&
         (!out->records || out->element_size < sizeof(struct emuif_trace_record))) ||
        (uint64_t)out->capacity * out->element_size > SIZE_MAX)
        return EMUIF_INVALID;
    for (i = 0; i < 32; i++)
        if (!((q->session_id[i] >= '0' && q->session_id[i] <= '9') ||
              (q->session_id[i] >= 'a' && q->session_id[i] <= 'f')))
            return EMUIF_INVALID;
    di = dest_index(&s->config, q->destination_id);
    if (di < 0)
        return EMUIF_NOT_FOUND;
    if (s->config.destinations[di].kind != 0)
        return EMUIF_UNSUPPORTED;
    r = &s->rings[di];
    INIT(p);
    p.struct_size = out->struct_size;
    memcpy(p.session_id, s->metadata.session_id, 33);
    p.destination_id = q->destination_id;
    p.epoch = r->epoch;
    p.requested_after = p.next_after = q->after_record_sequence;
    p.overwritten = r->overwritten;
    p.dropped = r->dropped;
    p.records = out->records;
    p.capacity = out->capacity;
    p.element_size = out->element_size;
    if (r->count)
    {
        p.oldest = r->records[r->head].record_sequence;
        p.newest = r->records[(r->head + r->count - 1) % r->capacity].record_sequence;
    }
    if (strcmp(q->session_id, s->metadata.session_id))
        p.cursor_state = EMUIF_CURSOR_WRONG_SESSION;
    else if (q->epoch != r->epoch)
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
        const unsigned char *slot =
            (const unsigned char *)out->records + (size_t)i * out->element_size;
        struct emuif_trace_record *v = &local;
        memcpy(v, slot, sizeof *v);
        if ((e = sized(v, sizeof *v)) || (e = sized(&v->event, sizeof v->event)) ||
            (e = sized(&v->event.old_value, sizeof v->event.old_value)) ||
            (e = sized(&v->event.new_value, sizeof v->event.new_value)))
            return e;
        if (v->struct_size > out->element_size || v->reserved || v->event.reserved_pc ||
            !zero(v->event.reserved_bytes, sizeof v->event.reserved_bytes) ||
            !zero(v->event.reserved_text, sizeof v->event.reserved_text) ||
            v->event.old_value.reserved || v->event.new_value.reserved)
            return EMUIF_INVALID;
    }
    for (i = 0; i < p.required; i++)
    {
        struct emuif_trace_record previous, v = r->records[(r->head + offset + i) % r->capacity];
        unsigned char *slot = (unsigned char *)out->records + (size_t)i * out->element_size;
        memcpy(&previous, slot, sizeof previous);
        v.struct_size = previous.struct_size;
        v.event.struct_size = previous.event.struct_size;
        v.event.old_value.struct_size = previous.event.old_value.struct_size;
        v.event.new_value.struct_size = previous.event.new_value.struct_size;
        memcpy(slot, &v, sizeof v);
    }
    p.written = p.required;
    p.remaining = available - p.written;
    p.more = p.remaining != 0;
    if (p.written)
    {
        p.first = r->records[(r->head + offset) % r->capacity].record_sequence;
        p.last = r->records[(r->head + offset + p.written - 1) % r->capacity].record_sequence;
        p.next_after = p.last;
    }
    *out = p;
    return EMUIF_OK;
}
emuif_status emu_trace_capture(struct emu_trace_state *s, const struct emuif_capture_request *q,
                               struct emuif_capture_status *out)
{
    struct capture *c;
    emuif_status e;
    uint32_t size;
    if (!s)
        return EMUIF_UNSUPPORTED;
    if ((e = sized(q, sizeof *q)) || (e = sized(out, sizeof *out)))
        return e;
    if (q->generation != s->generation)
        return EMUIF_STALE;
    if (q->reserved || q->stop_cpu > 1 || q->rearm_on_reset > 1 || q->mode > 2 || !q->max_records ||
        !q->max_cycles)
        return EMUIF_INVALID;
    if (q->mode)
        return EMUIF_UNSUPPORTED;
    if (q->context_id || q->entry_pc || q->rearm_on_reset)
        return EMUIF_INVALID;
    if (trace_index(&s->config, q->trace_id) < 0)
        return EMUIF_NOT_FOUND;
    c = capture_for(s, q->trace_id);
    if (c && c->value.state <= 1)
        return EMUIF_BUSY;
    if (s->capture_count == 64)
        return EMUIF_EXHAUSTED;
    c = &s->captures[s->capture_count++];
    memset(c, 0, sizeof *c);
    INIT(c->value);
    c->value.capture_id = s->capture_count;
    c->value.generation = s->generation;
    c->value.trace_id = q->trace_id;
    c->value.state = EMUIF_SCOPE_BOUND;
    c->value.start_sequence = s->sequence;
    c->max_records = q->max_records;
    c->max_cycles = q->max_cycles;
    c->last_cycle = s->cycle;
    c->last_generation = s->generation;
    c->stop_cpu = q->stop_cpu;
    size = out->struct_size;
    *out = c->value;
    out->struct_size = size;
    return EMUIF_OK;
}
static const char *status_text(emuif_status e)
{
    static const char *names[] = {"OK",       "INVALID",     "VERSION", "STATE", "RANGE",
                                  "CAPACITY", "UNSUPPORTED", "BUSY",    "STALE", "DENIED",
                                  "FAULT",    "EXHAUSTED",   "IO",      "HASH",  "NOT_FOUND"};
    return e <= EMUIF_NOT_FOUND ? names[e] : "FAULT";
}
static void capture_json(struct output *o, const struct emuif_capture_status *v)
{
    output_format(
        o,
        "{\"captureId\":\"%" PRIu64 "\",\"successorCaptureId\":\"%" PRIu64
        "\",\"rearmStatus\":\"%s\",\"invocationId\":\"%" PRIu64 "\",\"contextId\":\"%" PRIu64
        "\",\"generation\":\"%" PRIu64 "\",\"startSequence\":\"%" PRIu64
        "\",\"endSequence\":\"%" PRIu64 "\",\"records\":\"%" PRIu64 "\",\"cycles\":\"%" PRIu64
        "\",\"traceId\":%u,\"mode\":%u,\"state\":%u,\"reason\":%u,"
        "\"rearmOnReset\":%s}",
        v->capture_id, v->successor_capture_id, status_text(v->rearm_status), v->invocation_id,
        v->context_id, v->generation, v->start_sequence, v->end_sequence, v->records, v->cycles,
        v->trace_id, v->mode, v->state, v->reason, v->rearm_on_reset ? "true" : "false");
}
static void sink_json(struct output *o, const struct emuif_sink_status *v)
{
    output_format(o,
                  "{\"operationId\":\"%" PRIu64 "\",\"completedOperationId\":\"%" PRIu64
                  "\",\"bytes\":\"%" PRIu64 "\",\"records\":\"%" PRIu64 "\",\"lost\":\"%" PRIu64
                  "\",\"destinationId\":%u,\"state\":%u,\"segment\":%u,\"queued\":%u,"
                  "\"partialOutput\":%s,\"error\":\"%s\"}",
                  v->operation_id, v->completed_operation_id, v->bytes, v->records, v->lost,
                  v->destination_id, v->state, v->segment, v->queued,
                  v->partial_output ? "true" : "false", status_text(v->error));
}
emuif_status emu_trace_sink_open(struct emu_trace_state *s, const struct emuif_sink_request *q,
                                 struct emuif_sink_status *out)
{
    emuif_status e;
    int di;
    struct output *configuration;
    char hash[65];
    struct emu_trace_file_metadata m;
    if (!s || !s->provider.open)
        return EMUIF_UNSUPPORTED;
    if ((e = sized(q, sizeof *q)) || (e = sized(out, sizeof *out)))
        return e;
    if (q->generation != s->generation)
        return EMUIF_STALE;
    if (q->max_segment_bytes < 65536 || q->max_segment_bytes > 1073741824 || !q->max_segments ||
        q->max_segments > 16 || !q->max_total_records || !zero(q->reserved, sizeof q->reserved) ||
        !string_valid(q->destination_token, sizeof q->destination_token))
        return EMUIF_INVALID;
    di = dest_index(&s->config, q->destination_id);
    if (di < 0)
        return EMUIF_NOT_FOUND;
    if (s->config.destinations[di].kind != EMUIF_DEST_FILE)
        return EMUIF_INVALID;
    configuration = calloc(1, sizeof *configuration);
    if (!configuration)
        return EMUIF_CAPACITY;
    sync_enabled(s);
    config_json(configuration, &s->config);
    if (configuration->failed || configuration->length > 16384)
    {
        free(configuration);
        return EMUIF_CAPACITY;
    }
    sha256_hex((const uint8_t *)configuration->data, configuration->length, hash);
    memset(&m, 0, sizeof m);
    m.runtime_commit = s->metadata.runtime_commit;
    m.contract_manifest_sha256 = s->metadata.manifest;
    m.session_id = s->metadata.session_id;
    m.image_sha256 = s->metadata.image_hash;
    m.module_sha256 = s->metadata.module_hash;
    m.config_sha256 = s->metadata.config_hash;
    m.trace_config_sha256 = hash;
    m.configuration_json = configuration->data;
    m.configuration_bytes = (uint32_t)configuration->length;
    m.generation = s->generation;
    e = s->provider.open(s->provider.context, q, &m, out);
    free(configuration);
    return e;
}
static uint32_t num(const struct json_document *d, int a, const char *key)
{
    int64_t n = 0;
    json_integer(d, json_member(d, a, key), 0, UINT32_MAX, &n);
    return (uint32_t)n;
}
static uint64_t u64(const struct json_document *d, int a, const char *key)
{
    uint64_t n = 0;
    json_u64(d, json_member(d, a, key), &n);
    return n;
}
static uint32_t boolean(const struct json_document *d, int a, const char *key)
{
    int t = json_member(d, a, key);
    return t >= 0 && d->tokens[t].type == JSON_PRIMITIVE &&
           d->tokens[t].end - d->tokens[t].start == 4 &&
           !memcmp(d->text + d->tokens[t].start, "true", 4);
}
static uint32_t parse_routes(const struct json_document *d, int a, uint32_t ids[16])
{
    uint32_t n = 0;
    int t;
    for (t = a + 1; t < d->count && d->tokens[t].start < d->tokens[a].end; t = json_next(d, t))
    {
        int64_t v = 0;
        if (n == 16)
            return 17;
        json_integer(d, t, 1, UINT32_MAX, &v);
        ids[n++] = (uint32_t)v;
    }
    return n;
}
static void parse_selector(const struct json_document *d, int a, struct emuif_trace_selector *s)
{
    INIT(*s);
    s->match_mask = num(d, a, "matchMask");
    s->kind = num(d, a, "kind");
    s->space = num(d, a, "space");
    s->access_mask = num(d, a, "accessMask");
    s->pc_first = (uint16_t)num(d, a, "pcFirst");
    s->pc_last = (uint16_t)num(d, a, "pcLast");
    s->address_first = (uint16_t)num(d, a, "addressFirst");
    s->address_last = (uint16_t)num(d, a, "addressLast");
}
static emuif_status parse_config(const struct json_document *d, int a, struct trace_config *c)
{
    int array, t;
    unsigned i;
#define EACH(key, max, n)                                                                          \
    array = json_member(d, a, key);                                                                \
    if (array < 0)                                                                                 \
        return EMUIF_INVALID;                                                                      \
    for (t = array + 1; t < d->count && d->tokens[t].start < d->tokens[array].end;                 \
         t = json_next(d, t))                                                                      \
    {                                                                                              \
        if (c->n == max)                                                                           \
            return EMUIF_CAPACITY;                                                                 \
        i = c->n++;
    EACH("traces", 16, nt) struct emuif_trace_session *v = &c->traces[i];
    INIT(*v);
    v->trace_id = num(d, t, "traceId");
    v->destination_id = num(d, t, "destinationId");
    v->enabled = boolean(d, t, "enabled");
    v->interrupt_policy = num(d, t, "interruptPolicy");
    if (!json_string(d, json_member(d, t, "tag"), v->tag, sizeof v->tag) ||
        !json_string(d, json_member(d, t, "comment"), v->comment, sizeof v->comment))
        return EMUIF_INVALID;
}
EACH("destinations", 8, nd)
struct emuif_trace_destination *v = &c->destinations[i];
INIT(*v);
v->destination_id = num(d, t, "destinationId");
v->kind = num(d, t, "kind");
v->capacity = num(d, t, "capacity");
v->stop_on_full = boolean(d, t, "stopOnFull");
}
EACH("points", 32, np) struct emuif_trace_point *v = &c->points[i];
INIT(*v);
v->point_id = num(d, t, "pointId");
v->enabled = boolean(d, t, "enabled");
parse_selector(d, json_member(d, t, "selector"), &v->selector);
v->route_count = parse_routes(d, json_member(d, t, "traceIds"), v->trace_ids);
}
EACH("gates", 32, ng) struct emuif_trace_gate *v = &c->gates[i];
INIT(*v);
v->gate_id = num(d, t, "gateId");
v->enabled = boolean(d, t, "enabled");
v->action = num(d, t, "action");
v->timing = num(d, t, "timing");
parse_selector(d, json_member(d, t, "selector"), &v->selector);
v->route_count = parse_routes(d, json_member(d, t, "traceIds"), v->trace_ids);
}
EACH("watches", 64, nw) struct emuif_trace_watch *v = &c->watches[i];
int actions, j;
unsigned k;
INIT(*v);
v->watch_id = num(d, t, "watchId");
v->enabled = boolean(d, t, "enabled");
v->space = num(d, t, "space");
v->access_mask = num(d, t, "accessMask");
v->address_first = (uint16_t)num(d, t, "addressFirst");
v->address_last = (uint16_t)num(d, t, "addressLast");
for (k = 0; k < 8; k++)
{
    INIT(v->actions[k]);
    INIT(v->actions[k].condition);
    v->actions[k].condition.width = 8;
}
actions = json_member(d, t, "actions");
for (j = actions + 1; j < d->count && d->tokens[j].start < d->tokens[actions].end;
     j = json_next(d, j))
{
    struct emuif_trace_action *b;
    int cond;
    if (v->action_count == 8)
        return EMUIF_CAPACITY;
    b = &v->actions[v->action_count++];
    b->kind = num(d, j, "kind");
    b->route_count = parse_routes(d, json_member(d, j, "traceIds"), b->trace_ids);
    cond = json_member(d, j, "condition");
    b->condition.present = boolean(d, cond, "present");
    b->condition.operand = num(d, cond, "operand");
    b->condition.comparison = num(d, cond, "comparison");
    b->condition.width = num(d, cond, "width");
    b->condition.is_signed = boolean(d, cond, "isSigned");
    b->condition.constant = u64(d, cond, "constant");
}
}
#undef EACH
return EMUIF_OK;
}
static void native_config(struct emuif_trace_config *q, const struct trace_config *c,
                          uint64_t generation, uint64_t revision)
{
    INIT(*q);
    q->generation = generation;
    q->expected_revision = revision;
#define ARRAY(member, field, n)                                                                    \
    INIT(q->member);                                                                               \
    q->member.elements = c->field;                                                                 \
    q->member.element_size = sizeof(c->field[0]);                                                  \
    q->member.count = c->n
    ARRAY(traces, traces, nt);
    ARRAY(destinations, destinations, nd);
    ARRAY(points, points, np);
    ARRAY(gates, gates, ng);
    ARRAY(watches, watches, nw);
#undef ARRAY
}
emuif_status emu_trace_command(struct emu_trace_state *s, const char *cmd,
                               const struct json_document *d, int a, struct output *o)
{
    emuif_status e;
    if (!s)
        return EMUIF_UNSUPPORTED;
    emu_trace_poll_files(s);
    if (!strcmp(cmd, "traceConfig"))
    {
        sync_enabled(s);
        output_format(o, "{\"revision\":\"%" PRIu64 "\",\"config\":", s->revision);
        config_json(o, &s->config);
        output_raw(o, "}");
        return EMUIF_OK;
    }
    if (!strcmp(cmd, "replaceTraceConfig"))
    {
        struct trace_config *c = calloc(1, sizeof *c);
        struct emuif_trace_config q;
        struct emuif_trace_revision r;
        if (!c)
            return EMUIF_CAPACITY;
        e = parse_config(d, json_member(d, a, "config"), c);
        if (!e)
        {
            native_config(&q, c, u64(d, a, "generation"), u64(d, a, "expectedRevision"));
            INIT(r);
            e = emu_trace_replace(s, &q, &r);
            if (!e)
                output_format(o, "{\"revision\":\"%" PRIu64 "\"}", r.revision);
        }
        free(c);
        return e;
    }
    if (!strcmp(cmd, "tracePage"))
    {
        struct emuif_trace_page_request q;
        struct emuif_trace_page p;
        struct emuif_trace_record records[16];
        unsigned i;
        INIT(q);
        INIT(p);
        for (i = 0; i < 16; i++)
        {
            INIT(records[i]);
            INIT(records[i].event);
            INIT(records[i].event.old_value);
            INIT(records[i].event.new_value);
        }
        q.destination_id = num(d, a, "destinationId");
        q.max_records = num(d, a, "maxRecords");
        q.epoch = u64(d, a, "epoch");
        q.after_record_sequence = u64(d, a, "afterRecordSequence");
        json_string(d, json_member(d, a, "sessionId"), q.session_id, sizeof q.session_id);
        p.records = records;
        p.element_size = sizeof records[0];
        p.capacity = 16;
        e = emu_trace_page(s, &q, &p);
        if (!e)
            emu_trace_page_json(o, &p);
        return e;
    }
    if (!strcmp(cmd, "clearTrace"))
    {
        int di = dest_index(&s->config, num(d, a, "destinationId"));
        struct trace_ring *r;
        if (di < 0)
            return EMUIF_NOT_FOUND;
        if (s->config.destinations[di].kind)
            return EMUIF_INVALID;
        r = &s->rings[di];
        if (r->epoch == UINT64_MAX)
            return EMUIF_EXHAUSTED;
        emu_debug_runtime_flush_selected(s->runtime, s->config.destinations[di].destination_id, 0);
        r->head = r->count = 0;
        r->overwritten = r->dropped = 0;
        r->epoch++;
        r->next = 1;
        output_format(o, "{\"epoch\":\"%" PRIu64 "\"}", r->epoch);
        return EMUIF_OK;
    }
    if (!strcmp(cmd, "captureStart"))
    {
        struct emuif_capture_request q;
        struct emuif_capture_status v;
        INIT(q);
        INIT(v);
        q.generation = u64(d, a, "generation");
        q.context_id = u64(d, a, "contextId");
        q.max_records = u64(d, a, "maxRecords");
        q.max_cycles = u64(d, a, "maxCycles");
        q.trace_id = num(d, a, "traceId");
        q.mode = num(d, a, "mode");
        q.entry_pc = (uint16_t)num(d, a, "entryPc");
        q.rearm_on_reset = boolean(d, a, "rearmOnReset");
        q.stop_cpu = boolean(d, a, "stopCpu");
        e = emu_trace_capture(s, &q, &v);
        if (!e)
            capture_json(o, &v);
        return e;
    }
    if (!strcmp(cmd, "captureStop") || !strcmp(cmd, "captureStatus"))
    {
        uint64_t id = u64(d, a, "captureId");
        struct capture *c;
        if (!id || id > s->capture_count)
            return EMUIF_NOT_FOUND;
        c = &s->captures[id - 1];
        if (!strcmp(cmd, "captureStop") && c->value.state <= 1)
        {
            emu_debug_runtime_flush_selected(s->runtime, 0, c->value.trace_id);
            if (c->value.state <= EMUIF_SCOPE_BOUND)
            {
                c->value.state = EMUIF_SCOPE_STOPPED;
                c->value.reason = EMUIF_SCOPE_EXPLICIT;
                c->value.end_sequence = s->sequence;
            }
        }
        capture_json(o, &c->value);
        return EMUIF_OK;
    }
    if (!strcmp(cmd, "sinkOpen"))
    {
        struct emuif_sink_request q;
        struct emuif_sink_status v;
        INIT(q);
        INIT(v);
        q.generation = u64(d, a, "generation");
        q.destination_id = num(d, a, "destinationId");
        q.max_segment_bytes = u64(d, a, "maxSegmentBytes");
        q.max_total_records = u64(d, a, "maxTotalRecords");
        q.max_segments = num(d, a, "maxSegments");
        json_string(d, json_member(d, a, "destinationToken"), q.destination_token,
                    sizeof q.destination_token);
        e = emu_trace_sink_open(s, &q, &v);
        if (!e)
            sink_json(o, &v);
        return e;
    }
    if (!strcmp(cmd, "sinkStatus") || !strcmp(cmd, "sinkFlush") || !strcmp(cmd, "sinkClose"))
    {
        uint32_t id = num(d, a, "destinationId");
        struct emuif_sink_status v;
        if (!s->provider.open)
            return EMUIF_UNSUPPORTED;
        INIT(v);
        if (!strcmp(cmd, "sinkStatus"))
            e = s->provider.status(s->provider.context, id, &v);
        else
        {
            emu_debug_runtime_flush_selected(s->runtime, id, 0);
            e = s->provider.operation(s->provider.context, id, !strcmp(cmd, "sinkClose"),
                                      s->generation, &v);
        }
        if (!e)
        {
            if (v.error == EMUIF_EXHAUSTED)
                storage_exhausted(s, id);
            sink_json(o, &v);
        }
        return e;
    }
    return EMUIF_UNSUPPORTED;
}
#ifdef EMU_TRACE_TEST
/* Test-only identity exhaustion injection, absent from production artifacts. */
void emu_trace_test_sequence(struct emu_trace_state *s, uint32_t id, uint64_t next)
{
    int i = dest_index(&s->config, id);
    if (i >= 0)
        s->rings[i].next = next;
}
#endif

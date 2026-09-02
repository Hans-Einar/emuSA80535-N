/* Generic deterministic multi-trace router.
 * Copyright 2026 Hans-Einar
 * Distributed under the MIT license; see LICENSE.
 */
#include "emu_debug_trace.h"

#include <string.h>

static bool utf8_terminated(const char *s, size_t n)
{
    const unsigned char *bytes = (const unsigned char *)s;
    size_t at = 0u;

    while (at < n && bytes[at] != 0u) {
        unsigned char lead = bytes[at];

        if (lead <= 0x7fu) {
            ++at;
        } else if (lead >= 0xc2u && lead <= 0xdfu) {
            if (at + 1u >= n || bytes[at + 1u] < 0x80u ||
                bytes[at + 1u] > 0xbfu)
                return false;
            at += 2u;
        } else if (lead >= 0xe0u && lead <= 0xefu) {
            unsigned char second;

            if (at + 2u >= n)
                return false;
            second = bytes[at + 1u];
            if (bytes[at + 2u] < 0x80u || bytes[at + 2u] > 0xbfu ||
                (lead == 0xe0u && second < 0xa0u) ||
                (lead == 0xedu && second > 0x9fu) ||
                second < 0x80u || second > 0xbfu)
                return false;
            at += 3u;
        } else if (lead >= 0xf0u && lead <= 0xf4u) {
            unsigned char second;

            if (at + 3u >= n)
                return false;
            second = bytes[at + 1u];
            if (bytes[at + 2u] < 0x80u || bytes[at + 2u] > 0xbfu ||
                bytes[at + 3u] < 0x80u || bytes[at + 3u] > 0xbfu ||
                (lead == 0xf0u && second < 0x90u) ||
                (lead == 0xf4u && second > 0x8fu) ||
                second < 0x80u || second > 0xbfu)
                return false;
            at += 4u;
        } else {
            return false;
        }
    }
    return at < n;
}

static void saturated_increment(uint64_t *value)
{
    if (*value != UINT64_MAX)
        ++*value;
}

static bool selector_valid(const struct em8051_debug_trace_selector *s)
{
    return s->match_kind <= 1u && s->match_pc <= 1u &&
           s->match_address <= 1u && s->match_address_space <= 1u &&
           (!s->match_kind || s->kind <= EM8051_DEBUG_EVENT_WATCH_MATCH) &&
           (!s->match_address_space ||
            (s->address_space >= EM8051_DEBUG_SPACE_CODE &&
             s->address_space <= EM8051_DEBUG_SPACE_XDATA)) &&
           (!s->match_pc || s->pc_first <= s->pc_last) &&
           (!s->match_address || s->address_first <= s->address_last);
}

static bool selector_match(const struct em8051_debug_trace_selector *s,
                           const struct em8051_debug_event *e)
{
    return (!s->match_kind || s->kind == e->kind) &&
           (!s->match_address_space ||
            s->address_space == e->address_space) &&
           (!s->match_pc || (e->pc >= s->pc_first && e->pc <= s->pc_last)) &&
           (!s->match_address || (e->address >= s->address_first &&
                                  e->address <= s->address_last));
}

static int trace_index(const struct em8051_debug_trace_config *c, uint32_t id)
{
    size_t i;
    for (i = 0u; i < c->trace_count; ++i)
        if (c->traces[i].trace_id == id)
            return (int)i;
    return -1;
}

static int destination_index(const struct em8051_debug_trace_config *c,
                             uint32_t id)
{
    size_t i;
    for (i = 0u; i < c->destination_count; ++i)
        if (c->destinations[i].destination_id == id)
            return (int)i;
    return -1;
}

static bool ids_valid(const struct em8051_debug_trace_config *c,
                      const uint32_t *ids, size_t count)
{
    size_t i;
    if (count == 0u || count > EM8051_DEBUG_TRACE_MAX_ROUTES)
        return false;
    for (i = 0u; i < count; ++i)
        if (trace_index(c, ids[i]) < 0 ||
            (i && ids[i - 1u] >= ids[i]))
            return false;
    return true;
}

static bool config_valid(const struct em8051_debug_trace_config *c)
{
    size_t i;
    if (c == NULL || c->trace_count > EM8051_DEBUG_TRACE_MAX_TRACES ||
        c->destination_count > EM8051_DEBUG_TRACE_MAX_DESTINATIONS ||
        c->point_count > EM8051_DEBUG_TRACE_MAX_POINTS ||
        c->gate_count > EM8051_DEBUG_TRACE_MAX_GATES)
        return false;
    for (i = 0u; i < c->destination_count; ++i)
        if (!c->destinations[i].destination_id ||
            (i && c->destinations[i - 1u].destination_id >=
                  c->destinations[i].destination_id))
            return false;
    for (i = 0u; i < c->trace_count; ++i)
        if (!c->traces[i].trace_id || c->traces[i].enabled > 1u ||
            c->traces[i].interrupt_policy > EM8051_DEBUG_TRACE_INTERRUPT_ONLY ||
            !utf8_terminated(c->traces[i].tag,
                             sizeof(c->traces[i].tag)) ||
            !utf8_terminated(c->traces[i].comment,
                             sizeof(c->traces[i].comment)) ||
            destination_index(c, c->traces[i].destination_id) < 0 ||
            (i && c->traces[i - 1u].trace_id >= c->traces[i].trace_id))
            return false;
    for (i = 0u; i < c->point_count; ++i)
        if (!c->points[i].point_id || c->points[i].enabled > 1u ||
            !selector_valid(&c->points[i].selector) ||
            !ids_valid(c, c->points[i].trace_ids,
                       c->points[i].trace_id_count) ||
            (i && c->points[i - 1u].point_id >= c->points[i].point_id))
            return false;
    for (i = 0u; i < c->gate_count; ++i)
        if (!c->gates[i].gate_id || c->gates[i].enabled > 1u ||
            c->gates[i].action > EM8051_DEBUG_TRACE_GATE_OFF ||
            c->gates[i].timing > EM8051_DEBUG_TRACE_GATE_AFTER ||
            !selector_valid(&c->gates[i].selector) ||
            !ids_valid(c, c->gates[i].trace_ids, c->gates[i].trace_id_count) ||
            (i && c->gates[i - 1u].gate_id >= c->gates[i].gate_id))
            return false;
    return true;
}

static void ring_push(struct em8051_debug_trace_ring *r,
                      const struct em8051_debug_trace_record *record)
{
    uint16_t at;
    if (r->count == EM8051_DEBUG_TRACE_RING_CAPACITY) {
        r->head = (uint16_t)(((uint32_t)r->head + 1u) %
                             EM8051_DEBUG_TRACE_RING_CAPACITY);
        --r->count;
        saturated_increment(&r->overwritten);
    }
    at = (uint16_t)(((uint32_t)r->head + (uint32_t)r->count) %
                    EM8051_DEBUG_TRACE_RING_CAPACITY);
    r->records[at] = *record;
    ++r->count;
}

static void apply_gates(struct em8051_debug_trace_router *r,
                        const struct em8051_debug_event *e, uint8_t timing)
{
    size_t i, j;
    for (i = 0u; i < r->config.gate_count; ++i) {
        const struct em8051_debug_trace_gate *g = &r->config.gates[i];
        if (!g->enabled || g->timing != timing || !selector_match(&g->selector, e))
            continue;
        for (j = 0u; j < g->trace_id_count; ++j) {
            int ti = trace_index(&r->config, g->trace_ids[j]);
            r->config.traces[ti].enabled =
                g->action == EM8051_DEBUG_TRACE_GATE_ON ? 1u : 0u;
        }
    }
}

static bool policy_accepts(uint8_t policy, uint8_t kind, uint32_t depth)
{
    bool outer_enter = kind == EM8051_DEBUG_EVENT_INTERRUPT_ENTER && depth == 0u;
    bool outer_exit = kind == EM8051_DEBUG_EVENT_INTERRUPT_EXIT && depth == 1u;
    if (kind == EM8051_DEBUG_EVENT_RESET || kind == EM8051_DEBUG_EVENT_LOAD)
        return true;
    if (policy == EM8051_DEBUG_TRACE_INCLUDE)
        return true;
    if (policy == EM8051_DEBUG_TRACE_SUPPRESS_DURING_INTERRUPT)
        return depth == 0u || outer_enter || outer_exit;
    return depth != 0u || outer_enter;
}

static void suppression_add(struct em8051_debug_trace_router *r, size_t ti,
                            const struct em8051_debug_event *e)
{
    struct em8051_debug_trace_suppression_state *s = &r->suppression[ti];
    if (!s->active) {
        memset(s, 0, sizeof(*s));
        s->active = 1u;
        s->value.first_sequence = e->sequence;
        s->value.trace_id = r->config.traces[ti].trace_id;
        s->value.entry_depth = r->interrupt_depth;
        s->value.interrupt_policy = r->config.traces[ti].interrupt_policy;
    }
    s->value.last_sequence = e->sequence;
    saturated_increment(&s->value.count);
    if (r->interrupt_depth > s->value.maximum_depth)
        s->value.maximum_depth = r->interrupt_depth;
}

static void suppression_emit(struct em8051_debug_trace_router *r, size_t ti)
{
    struct em8051_debug_trace_suppression_state *s = &r->suppression[ti];
    struct em8051_debug_trace_record record;
    int di;
    if (!s->active)
        return;
    di = destination_index(&r->config, r->config.traces[ti].destination_id);
    memset(&record, 0, sizeof(record));
    record.kind = EM8051_DEBUG_TRACE_RECORD_SUPPRESSION;
    record.destination_id = r->config.traces[ti].destination_id;
    record.trace_ids[0] = r->config.traces[ti].trace_id;
    record.trace_id_count = 1u;
    record.suppression = s->value;
    ring_push(&r->rings[di], &record);
    memset(s, 0, sizeof(*s));
}

static enum em8051_debug_event_status route_ids(
    struct em8051_debug_trace_router *r, const struct em8051_debug_event *e,
    const uint32_t *ids, size_t count,
    const struct em8051_debug_watch_result *watch_result,
    const uint8_t *enabled_snapshot)
{
    struct em8051_debug_trace_record records[EM8051_DEBUG_TRACE_MAX_DESTINATIONS];
    uint8_t used[EM8051_DEBUG_TRACE_MAX_DESTINATIONS];
    size_t i;
    for (i = 0u; i < count; ++i)
        if (!ids[i] || trace_index(&r->config, ids[i]) < 0 ||
            (i && ids[i - 1u] >= ids[i]))
            return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    memset(records, 0, sizeof(records));
    memset(used, 0, sizeof(used));
    for (i = 0u; i < count; ++i) {
        int ti = trace_index(&r->config, ids[i]);
        int di;
        struct em8051_debug_trace_session *t;
        t = &r->config.traces[ti];
        if (!(enabled_snapshot != NULL ? enabled_snapshot[ti] : t->enabled))
            continue;
        if (!policy_accepts(t->interrupt_policy, e->kind, r->interrupt_depth)) {
            suppression_add(r, (size_t)ti, e);
            continue;
        }
        suppression_emit(r, (size_t)ti);
        di = destination_index(&r->config, t->destination_id);
        if (!used[di]) {
            used[di] = 1u;
            records[di].kind = EM8051_DEBUG_TRACE_RECORD_EVENT;
            records[di].destination_id = t->destination_id;
            records[di].event = *e;
            if (watch_result) records[di].watch_result = *watch_result;
        }
        records[di].trace_ids[records[di].trace_id_count++] = t->trace_id;
    }
    for (i = 0u; i < r->config.destination_count; ++i)
        if (used[i])
            ring_push(&r->rings[i], &records[i]);
    return EM8051_DEBUG_EVENT_OK;
}

void em8051_debug_trace_router_init(struct em8051_debug_trace_router *r)
{
    if (r) memset(r, 0, sizeof(*r));
}

enum em8051_debug_event_status em8051_debug_trace_router_replace(
    struct em8051_debug_trace_router *r,
    const struct em8051_debug_trace_config *c)
{
    struct em8051_debug_trace_ring rings[EM8051_DEBUG_TRACE_MAX_DESTINATIONS];
    struct em8051_debug_trace_suppression_state
        suppression[EM8051_DEBUG_TRACE_MAX_TRACES];
    uint32_t new_ids[EM8051_DEBUG_TRACE_MAX_TRACES];
    size_t i;
    size_t new_id_count = 0u;
    if (!r || !config_valid(c))
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    if (r->source_open)
        return EM8051_DEBUG_EVENT_BUSY;
    for (i = 0u; i < r->config.trace_count; ++i) {
        int replacement;

        if (!r->suppression[i].active)
            continue;
        replacement = trace_index(c, r->config.traces[i].trace_id);
        if (replacement < 0 ||
            c->traces[(size_t)replacement].destination_id !=
                r->config.traces[i].destination_id ||
            c->traces[(size_t)replacement].interrupt_policy !=
                r->config.traces[i].interrupt_policy)
            return EM8051_DEBUG_EVENT_BUSY;
    }
    for (i = 0u; i < c->trace_count; ++i) {
        size_t seen;

        if (trace_index(&r->config, c->traces[i].trace_id) >= 0)
            continue;
        for (seen = 0u; seen < r->seen_trace_id_count; ++seen)
            if (r->seen_trace_ids[seen] == c->traces[i].trace_id)
                return EM8051_DEBUG_EVENT_DUPLICATE_ID;
        new_ids[new_id_count++] = c->traces[i].trace_id;
    }
    if (new_id_count > EM8051_DEBUG_TRACE_MAX_SEEN_IDS -
                           (size_t)r->seen_trace_id_count)
        return EM8051_DEBUG_EVENT_LIMIT;
    memset(rings, 0, sizeof(rings));
    memset(suppression, 0, sizeof(suppression));
    for (i = 0u; i < c->destination_count; ++i) {
        int old = destination_index(&r->config,
                                    c->destinations[i].destination_id);
        if (old >= 0) rings[i] = r->rings[old];
    }
    for (i = 0u; i < c->trace_count; ++i) {
        int old = trace_index(&r->config, c->traces[i].trace_id);
        if (old >= 0 &&
            r->config.traces[old].destination_id ==
                c->traces[i].destination_id &&
            r->config.traces[old].interrupt_policy ==
                c->traces[i].interrupt_policy)
            suppression[i] = r->suppression[old];
    }
    r->config = *c;
    memcpy(r->rings, rings, sizeof(rings));
    memcpy(r->suppression, suppression, sizeof(suppression));
    for (i = 0u; i < new_id_count; ++i)
        r->seen_trace_ids[r->seen_trace_id_count++] = new_ids[i];
    return EM8051_DEBUG_EVENT_OK;
}

enum em8051_debug_event_status em8051_debug_trace_router_event(
    struct em8051_debug_trace_router *r, const struct em8051_debug_event *e)
{
    enum em8051_debug_event_status status;

    status = em8051_debug_trace_router_source_begin(r, e);
    if (status != EM8051_DEBUG_EVENT_OK)
        return status;
    return em8051_debug_trace_router_source_end(r);
}

enum em8051_debug_event_status em8051_debug_trace_router_source_begin(
    struct em8051_debug_trace_router *r, const struct em8051_debug_event *e)
{
    uint32_t ids[EM8051_DEBUG_TRACE_MAX_TRACES];
    uint8_t enabled_snapshot[EM8051_DEBUG_TRACE_MAX_TRACES];
    size_t count = 0u, i, j;
    enum em8051_debug_event_status status;
    if (!r || !e || r->source_open || !e->sequence ||
        e->sequence <= r->last_sequence ||
        e->kind == EM8051_DEBUG_EVENT_WATCH_MATCH ||
        (e->kind == EM8051_DEBUG_EVENT_INTERRUPT_EXIT &&
         r->interrupt_depth == 0u) ||
        (e->kind == EM8051_DEBUG_EVENT_INTERRUPT_ENTER &&
         r->interrupt_depth == UINT32_MAX))
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    if (e->kind == EM8051_DEBUG_EVENT_RESET ||
        e->kind == EM8051_DEBUG_EVENT_LOAD) {
        for (i = 0u; i < r->config.trace_count; ++i) {
            enabled_snapshot[i] = r->config.traces[i].enabled;
            ids[count++] = r->config.traces[i].trace_id;
        }
        apply_gates(r, e, EM8051_DEBUG_TRACE_GATE_BEFORE);
    } else {
        apply_gates(r, e, EM8051_DEBUG_TRACE_GATE_BEFORE);
        for (i = 0u; i < r->config.point_count; ++i) {
            const struct em8051_debug_trace_point *p = &r->config.points[i];
            if (!p->enabled || !selector_match(&p->selector, e)) continue;
            for (j = 0u; j < p->trace_id_count; ++j) {
                size_t at = 0u, move;
                while (at < count && ids[at] < p->trace_ids[j]) ++at;
                if (at < count && ids[at] == p->trace_ids[j]) continue;
                for (move = count; move > at; --move)
                    ids[move] = ids[move - 1u];
                ids[at] = p->trace_ids[j]; ++count;
            }
        }
    }
    status = route_ids(r, e, ids, count, NULL,
                       (e->kind == EM8051_DEBUG_EVENT_RESET ||
                        e->kind == EM8051_DEBUG_EVENT_LOAD) ?
                           enabled_snapshot : NULL);
    if (status != EM8051_DEBUG_EVENT_OK) return status;
    if (e->kind == EM8051_DEBUG_EVENT_INTERRUPT_ENTER) {
        ++r->interrupt_depth;
    } else if (e->kind == EM8051_DEBUG_EVENT_INTERRUPT_EXIT) {
        --r->interrupt_depth;
    }
    r->last_sequence = e->sequence;
    r->open_source_event = *e;
    r->source_open = 1u;
    return EM8051_DEBUG_EVENT_OK;
}

enum em8051_debug_event_status em8051_debug_trace_router_source_end(
    struct em8051_debug_trace_router *r)
{
    if (!r)
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    if (!r->source_open)
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    apply_gates(r, &r->open_source_event, EM8051_DEBUG_TRACE_GATE_AFTER);
    memset(&r->open_source_event, 0, sizeof(r->open_source_event));
    r->source_open = 0u;
    return EM8051_DEBUG_EVENT_OK;
}

enum em8051_debug_event_status em8051_debug_trace_router_watch(
    struct em8051_debug_trace_router *r,
    const struct em8051_debug_event *e,
    const struct em8051_debug_watch_result *result)
{
    enum em8051_debug_event_status status;
    if (!r || !e || !result || e->kind != EM8051_DEBUG_EVENT_WATCH_MATCH ||
        !e->sequence || e->sequence <= r->last_sequence ||
        result->trace_id_count > EM8051_DEBUG_WATCH_MAX_ROUTES ||
        (r->source_open &&
         (result->source_event_sequence != r->open_source_event.sequence ||
          e->generation != r->open_source_event.generation)))
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    status = route_ids(r, e, result->trace_ids, result->trace_id_count, result,
                       NULL);
    if (status == EM8051_DEBUG_EVENT_OK) r->last_sequence = e->sequence;
    return status;
}

enum em8051_debug_event_status em8051_debug_trace_router_flush(
    struct em8051_debug_trace_router *r)
{
    size_t i;
    if (!r) return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    if (r->source_open) return EM8051_DEBUG_EVENT_BUSY;
    for (i = 0u; i < r->config.trace_count; ++i) suppression_emit(r, i);
    return EM8051_DEBUG_EVENT_OK;
}

const struct em8051_debug_trace_ring *em8051_debug_trace_router_ring(
    const struct em8051_debug_trace_router *r, uint32_t id)
{
    int di;
    if (!r) return NULL;
    di = destination_index(&r->config, id);
    return di < 0 ? NULL : &r->rings[di];
}

bool em8051_debug_trace_ring_pop(struct em8051_debug_trace_ring *r,
                                 struct em8051_debug_trace_record *record)
{
    if (!r || !record || !r->count) return false;
    *record = r->records[r->head];
    r->head = (uint16_t)(((uint32_t)r->head + 1u) %
                         EM8051_DEBUG_TRACE_RING_CAPACITY);
    --r->count;
    return true;
}

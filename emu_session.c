/* Deterministic session and callback-free wire2.1/control3 services.
 * Copyright 2026 Hans-Einar. MIT; see LICENSE.
 * Only the opaque CPU contract is used here. No board decode or physical I/O.
 */
#include "emu_breakpoint_internal.h"
#include "emu_json_internal.h"
#include "emu_session_internal.h"
#include "emu_sha_internal.h"
#include "emu_cpu_lifecycle_internal.h"
#include "emu_debug_observation_internal.h"
#include "emu_outcome_internal.h"
#include <emusa80535/contracts/control3.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#ifndef EMU_RUNTIME_COMMIT
#define EMU_RUNTIME_COMMIT "0000000000000000000000000000000000000000"
#endif
#ifndef EMU_CONTRACT_MANIFEST_SHA256
#define EMU_CONTRACT_MANIFEST_SHA256                                                               \
    "0000000000000000000000000000000000000000000000000000000000000000"
#endif
#ifndef EMU_CONTROL3_MANIFEST_SHA256
#define EMU_CONTROL3_MANIFEST_SHA256                                                               \
    "0000000000000000000000000000000000000000000000000000000000000000"
#endif
#define INIT(x)                                                                                    \
    do                                                                                             \
    {                                                                                              \
        memset(&(x), 0, sizeof(x));                                                                \
        (x).struct_size = sizeof(x);                                                               \
        (x).api_version = 1;                                                                       \
    } while (0)
#define PREFIX(p) prefix((p), sizeof(*(p)))
struct prefix_record
{
    uint32_t size;
    uint16_t version, flags;
};
static emuif_status prefix(const void *p, size_t size)
{
    struct prefix_record v;
    if (!p)
        return EMUIF_INVALID;
    memcpy(&v.size, p, sizeof v.size);
    if (v.size < sizeof v) return EMUIF_INVALID;
    memcpy(&v, p, sizeof v);
    if (v.version != 1)
        return EMUIF_VERSION;
    if (v.size < size || v.flags)
        return EMUIF_INVALID;
    return EMUIF_OK;
}
static bool zeros(const void *p, size_t n)
{
    const uint8_t *b = p;
    while (n--)
        if (*b++)
            return false;
    return true;
}
static bool identifier(const char *p, size_t n, bool empty)
{
    size_t i = 0;
    while (i < n && p[i])
    {
        unsigned char c = (unsigned char)p[i++];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '.' || c == '_' || c == '-'))
            return false;
    }
    return i < n && (empty || i) && zeros(p + i, n - i);
}
static bool hex_string(const char *p, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        if (!((p[i] >= '0' && p[i] <= '9') || (p[i] >= 'a' && p[i] <= 'f')))
            return false;
    return p[n] == 0;
}
static emuif_status normalize(emuif_status s)
{
    return s <= EMUIF_NOT_FOUND ? s : EMUIF_FAULT;
}
struct queued_input
{
    bool board;
    uint64_t parent;
    struct emuif_input cpu;
    struct emuif_board_stimulus stimulus;
    uint8_t payload[4097];
};
struct client_state
{
    uint64_t id;
    uint32_t maximum_role, may_terminate, last_id, role;
    bool hello, attached, control3;
    uint64_t lease_id;
};
struct emuif_session
{
    struct emuif_cpu_api cpu_api;
    struct emuif_board_api board_api;
    struct emuif_cpu *cpu;
    struct emuif_observation_api observation_api;
    struct em8051_debug_runtime *events;
    struct emu_outcome_ring outcomes;
    uint64_t observation_coverage, parent_context, next_parent;
    uint64_t last_source_sequence, last_instruction_id, observed_legacy;
    uint64_t last_retire_sequence, last_retire_id, stop_source_sequence, stop_instruction_id;
    uint64_t fault_source_sequence, fault_instruction_id;
    uint64_t pending_instruction_id, pending_begin_cycle;
    uint16_t pending_pc;
    bool observation_bound, fault_published;
    emuif_status observation_error;
    void *board;
    uint8_t code[65536], config[16385], breakpoints[8192];
    uint32_t config_bytes, variant, oscillator, seed, state, reason;
    uint16_t entry;
    uint64_t generation, revision, next_order, cycle, scheduling_cycle;
    emuif_status cause;
    bool busy, scheduling, fault_inspect, suppress, callback_violation, request_fault;
    uint16_t suppress_pc;
    uint64_t suppress_instruction;
    char id[33], image_hash[65], config_hash[65], module_hash[65];
    struct queued_input queue[1024];
    size_t queued;
    struct client_state clients[8];
    uint64_t lease, next_lease, retired;
    uint32_t execution_mode;
    bool retirement_pending, progressed;
    uint64_t *used_clients;
    size_t used_count;
};
static emuif_status fault(struct emuif_session *s, emuif_status code)
{
    s->request_fault = true;
    code = normalize(code);
    if (code == EMUIF_OK)
        code = EMUIF_FAULT;
    if (s->state != EMUIF_STATE_FAULTED)
    {
        if (s->revision != UINT64_MAX)
            s->revision++;
        s->cause = code;
    }
    s->state = EMUIF_STATE_FAULTED;
    s->reason = 6;
    return code;
}
/* Canonical source identity belongs to the existing semantic event bus. */
static void retained_observation(void *ctx, const struct emuif_observation *o)
{
    struct emuif_session *s = ctx;
    s->last_source_sequence = o->sequence;
    s->last_instruction_id = o->instruction_id;
    s->observed_legacy = o->legacy_instructions;
    if (o->kind == EMUIF_OBS_INSTRUCTION_BEGIN) {
        s->pending_instruction_id = o->instruction_id;
        s->pending_begin_cycle = o->begin_cycle;
        s->pending_pc = o->pc;
    }
    if (o->kind == EMUIF_OBS_INSTRUCTION_RETIRE || o->kind == EMUIF_OBS_INSTRUCTION_ABORT ||
        o->kind == EMUIF_OBS_RESET || o->kind == EMUIF_OBS_LOAD) {
        s->pending_instruction_id = s->pending_begin_cycle = 0;
        s->pending_pc = 0;
    }
    if (o->kind == EMUIF_OBS_INSTRUCTION_RETIRE) {
        s->retired = o->retired_instructions;
        s->last_retire_sequence = o->sequence;
        s->last_retire_id = o->instruction_id;
    }
    if (o->kind == EMUIF_OBS_INSTRUCTION_ABORT ||
        (o->kind == EMUIF_OBS_FAULT && !s->fault_source_sequence)) {
        s->fault_source_sequence = o->sequence;
        s->fault_instruction_id = o->instruction_id;
    }
    if (o->kind == EMUIF_OBS_FAULT)
        s->fault_published = true;
    if (o->kind >= EMUIF_OBS_INPUT_ADMITTED && o->kind <= EMUIF_OBS_TX_END) {
        emuif_status e = emu_outcome_append(&s->outcomes, o);
        if (e) s->observation_error = e;
    }
}
static void cpu_observation(void *ctx, const struct emuif_observation *o)
{
    struct emuif_session *s = ctx;
    if (s->observation_error) return;
    if (emu_debug_runtime_observation(s->events, o, NULL) != EM8051_DEBUG_EVENT_OK)
        s->observation_error = EMUIF_EXHAUSTED;
}
static emuif_status bind_observation(struct emuif_session *s)
{
    struct emuif_observation_binding b;
    emuif_status e;
    INIT(b); INIT(b.observer); b.generation = s->generation;
    b.observer.context = s; b.observer.observe = cpu_observation;
    e = s->observation_api.bind(s->cpu, &b);
    if (!e) s->observation_bound = true;
    return e;
}
static void input_observation(struct emuif_session *s, const struct queued_input *q,
                              uint32_t kind, emuif_status status)
{
    struct emuif_observation o;
    if (!s->observation_bound) return;
    INIT(o); INIT(o.old_value); INIT(o.new_value);
    o.old_value.width = o.new_value.width = 8;
    o.generation = s->generation; o.cycle = kind == EMUIF_OBS_INPUT_ADMITTED && !q->board ? s->scheduling_cycle : s->cycle; o.kind = kind;
    o.legacy_instructions = s->observed_legacy;
    o.phase = kind == EMUIF_OBS_INPUT_ADMITTED || kind == EMUIF_OBS_INPUT_CANCELLED ?
        EMUIF_OBS_PHASE_BOUNDARY : EMUIF_OBS_PHASE_PERIPHERAL;
    o.reason = kind == EMUIF_OBS_INPUT_FAILED ? EMUIF_OBS_REASON_DEVICE :
        kind == EMUIF_OBS_INPUT_CANCELLED ? EMUIF_OBS_REASON_RESET : EMUIF_OBS_REASON_NONE;
    o.status = status; o.parent_stimulus_id = q->parent;
    o.input_stage = q->board ? 0 : 1; o.cpu_input_order = q->board ? 0 : q->cpu.order;
    o.scheduled_cycle = q->board ? q->stimulus.cycle : q->cpu.cycle;
    o.known = EMUIF_OBS_KNOWN_TIMING;
    if (s->pending_instruction_id) {
        o.instruction_id = s->pending_instruction_id;
        o.begin_cycle = s->pending_begin_cycle;
        o.pc = s->pending_pc;
        o.known |= EMUIF_OBS_KNOWN_PC;
    }
    o.retired_instructions = s->retired;
    cpu_observation(s, &o);
}
static void publish_fault(struct emuif_session *s)
{
    if (s->observation_bound && !s->fault_published && s->state == EMUIF_STATE_FAULTED)
        emu_cpu_observation_lifecycle(s->cpu, EMUIF_OBS_FAULT, s->cause);
    if (s->state == EMUIF_STATE_FAULTED) {
        s->stop_source_sequence = s->fault_source_sequence;
        s->stop_instruction_id = s->fault_instruction_id;
    }
}
static emuif_status snapshot(struct emuif_session *s, struct emuif_snapshot *v)
{
    emuif_status e;
    INIT(*v);
    e = s->cpu_api.snapshot(s->cpu, v);
    if (e)
        return fault(s, e);
    v->generation = s->generation;
    v->state = s->state;
    if (s->state == EMUIF_STATE_FAULTED)
        v->safe_boundary = 0;
    return EMUIF_OK;
}
static emuif_status stop(struct emuif_session *s, uint32_t reason)
{
    if (s->revision == UINT64_MAX)
        return fault(s, EMUIF_EXHAUSTED);
    s->revision++;
    s->reason = reason;
    s->stop_source_sequence = reason == 3 ? s->last_retire_sequence : 0;
    s->stop_instruction_id = reason == 3 ? s->last_retire_id : 0;
    s->state = reason == 9 ? EMUIF_STATE_TERMINATED : EMUIF_STATE_PAUSED;
    return EMUIF_OK;
}
static uint64_t qcycle(const struct queued_input *q)
{
    return q->board ? q->stimulus.cycle : q->cpu.cycle;
}
static uint64_t qorder(const struct queued_input *q)
{
    return q->board ? q->stimulus.order : q->cpu.order;
}
static void insert(struct emuif_session *s, const struct queued_input *q)
{
    size_t i = s->queued++;
    while (i && (qcycle(&s->queue[i - 1]) > qcycle(q) ||
                 (qcycle(&s->queue[i - 1]) == qcycle(q) && qorder(&s->queue[i - 1]) > qorder(q))))
    {
        s->queue[i] = s->queue[i - 1];
        i--;
    }
    s->queue[i] = *q;
}
static emuif_status input_valid(struct emuif_session *s, const struct emuif_input *i)
{
    emuif_status e = PREFIX(i);
    uint32_t port;
    if (e)
        return e;
    if (i->order)
        return EMUIF_INVALID;
    if (i->cycle <= s->scheduling_cycle)
        return EMUIF_RANGE;
    if (s->variant != EMUIF_VARIANT_SAB80535)
        return EMUIF_UNSUPPORTED;
    switch (i->kind)
    {
    case EMUIF_INPUT_PORT:
        if (i->index > 5 || i->value > 255 || i->mask > 255)
            return EMUIF_RANGE;
        port = i->index;
        break;
    case EMUIF_INPUT_INTERRUPT:
        return i->index <= 6 && i->value <= 1 && i->mask == 1 ? EMUIF_OK : EMUIF_RANGE;
    case EMUIF_INPUT_ADC:
        return i->index <= 7 && i->value <= 65535 && i->mask == 65535 ? EMUIF_OK : EMUIF_RANGE;
    case EMUIF_INPUT_UART_RX:
        return i->index == 0 && i->value <= 511 && i->mask == 511 ? EMUIF_OK : EMUIF_RANGE;
    case EMUIF_INPUT_RELEASE:
        if (i->value)
            return EMUIF_RANGE;
        if (i->index >= 256)
            return i->index <= 262 && i->mask == 1 ? EMUIF_OK : EMUIF_RANGE;
        if (i->index > 5 || i->mask > 255)
            return EMUIF_RANGE;
        port = i->index;
        break;
    default:
        return EMUIF_INVALID;
    }
    return port == 1 || port == 3 || port == 4 || port == 5 ? EMUIF_OK : EMUIF_UNSUPPORTED;
}
static emuif_status schedule_input(void *ctx, const struct emuif_input *i)
{
    struct emuif_session *s = ctx;
    struct queued_input q;
    emuif_status e;
    if (!s->scheduling)
        return EMUIF_BUSY;
    e = input_valid(s, i);
    if (e)
        return e;
    if (s->queued == 1024)
        return EMUIF_CAPACITY;
    if (s->next_order == UINT64_MAX)
        return fault(s, EMUIF_EXHAUSTED);
    memset(&q, 0, sizeof q);
    q.cpu = *i;
    q.cpu.struct_size = sizeof q.cpu;
    q.cpu.order = ++s->next_order;
    q.parent = s->parent_context;
    insert(s, &q);
    input_observation(s, &q, EMUIF_OBS_INPUT_ADMITTED, EMUIF_OK);
    return EMUIF_OK;
}
static emuif_status bus(void *ctx, const struct emuif_bus_access *a, struct emuif_bus_result *r)
{
    struct emuif_session *s = ctx;
    emuif_status e;
    s->scheduling_cycle = a->cycle;
    s->scheduling = true;
    e = s->board_api.access(s->board, a, r);
    s->scheduling = false;
    if (s->state == EMUIF_STATE_FAULTED)
        return s->cause;
    return e ? fault(s, e) : EMUIF_OK;
}
static emuif_status port(void *ctx, const struct emuif_port_output *a)
{
    struct emuif_session *s = ctx;
    emuif_status e;
    s->scheduling_cycle = a->cycle;
    s->scheduling = true;
    e = s->board_api.port_output(s->board, a);
    s->scheduling = false;
    if (s->state == EMUIF_STATE_FAULTED)
        return s->cause;
    return e ? fault(s, e) : EMUIF_OK;
}
static emuif_status signal_output(void *ctx, const struct emuif_signal_output *a)
{
    struct emuif_session *s = ctx;
    emuif_status e;
    s->scheduling_cycle = a->cycle;
    s->scheduling = true;
    e = s->board_api.signal_output(s->board, a);
    s->scheduling = false;
    if (s->state == EMUIF_STATE_FAULTED)
        return s->cause;
    return e ? fault(s, e) : EMUIF_OK;
}
static emuif_status boundary(struct emuif_session *s)
{
    struct emuif_board_boundary b;
    emuif_status e;
    while (s->queued && qcycle(&s->queue[0]) <= s->cycle)
    {
        struct queued_input q = s->queue[0];
        memmove(s->queue, s->queue + 1, (--s->queued) * sizeof s->queue[0]);
        if (qcycle(&q) != s->cycle)
            return fault(s, EMUIF_STALE);
        if (q.board)
        {
            q.stimulus.payload_utf8 = q.payload;
            s->scheduling_cycle = q.stimulus.cycle;
            s->scheduling = true;
            s->parent_context = q.parent;
            e = s->board_api.apply_stimulus(s->board, &q.stimulus);
            s->parent_context = 0;
            s->scheduling = false;
        }
        else if (s->observation_bound) {
            struct emuif_observed_input input;
            INIT(input); input.parent_stimulus_id = q.parent; input.input = q.cpu;
            e = s->observation_api.deliver(s->cpu, &input);
        } else
            e = s->cpu_api.apply_input(s->cpu, &q.cpu);
        e = normalize(e);
        input_observation(s, &q, e ? EMUIF_OBS_INPUT_FAILED : EMUIF_OBS_INPUT_DELIVERED, q.board && e ? EMUIF_FAULT : e);
        if (e)
            return fault(s, e);
        if (s->observation_error)
            return fault(s, s->observation_error);
        if (s->state == EMUIF_STATE_FAULTED)
            return s->cause;
    }
    INIT(b);
    b.cycle = s->cycle;
    b.generation = s->generation;
    s->scheduling_cycle = b.cycle;
    s->scheduling = true;
    e = s->board_api.boundary(s->board, &b);
    s->scheduling = false;
    if (e)
        return fault(s, e);
    return s->state == EMUIF_STATE_FAULTED ? s->cause : EMUIF_OK;
}
static emuif_status reset_session(struct emuif_session *s, uint32_t seed, uint16_t entry,
                                  bool first)
{
    struct emuif_reset r;
    struct emuif_board_reset b;
    emuif_status e;
    if (!first && (s->generation == UINT64_MAX || s->revision == UINT64_MAX))
        return EMUIF_EXHAUSTED;
    /* Queue order is cycle-sorted; cancellation order is the global admission order. */
    while (s->queued) {
        size_t i, at = 0;
        for (i = 1; i < s->queued; ++i)
            if (qorder(&s->queue[i]) < qorder(&s->queue[at])) at = i;
        input_observation(s, &s->queue[at], EMUIF_OBS_INPUT_CANCELLED, EMUIF_OK);
        memmove(s->queue + at, s->queue + at + 1, (s->queued - at - 1) * sizeof s->queue[0]);
        --s->queued;
    }
    s->retirement_pending = false;
    s->suppress = false;
    s->cycle = 0;
    s->state = EMUIF_STATE_PAUSED;
    s->cause = 0;
    s->seed = seed;
    s->entry = entry;
    s->fault_published = false;
    s->fault_source_sequence = s->fault_instruction_id = 0;
    s->last_retire_sequence = s->last_retire_id = 0;
    INIT(r);
    r.seed = seed;
    r.entry = entry;
    e = s->cpu_api.reset(s->cpu, &r);
    if (e) return fault(s, e);
    if (s->observation_bound) {
        e = s->observation_api.unbind(s->cpu);
        if (e) return fault(s, e);
        s->observation_bound = false;
    }
    if (!first) s->generation++;
    if (s->observation_coverage) {
        e = bind_observation(s);
        if (e) return fault(s, e);
        e = emu_cpu_observation_lifecycle(s->cpu, EMUIF_OBS_RESET, EMUIF_OK);
        if (e) return fault(s, e);
    }
    INIT(b);
    b.generation = s->generation;
    b.seed = seed;
    e = s->board_api.reset(s->board, &b);
    if (e)
        return fault(s, e);
    if (s->state == EMUIF_STATE_FAULTED)
        return s->cause;
    e = s->cpu_api.load_code(s->cpu, s->code, 65536);
    if (e) return fault(s, e);
    if (first && s->observation_bound) {
        e = emu_cpu_observation_lifecycle(s->cpu, EMUIF_OBS_LOAD, EMUIF_OK);
        if (e) return fault(s, e);
    }
    e = boundary(s);
    if (e)
        return e;
    return stop(s, 1);
}
static emuif_status board_table(const struct emuif_board_api *b, const struct emuif_cpu_api *cpu,
                                uint32_t variant)
{
    emuif_status e = PREFIX(b);
    if (e)
        return e;
    if (b->abi_major != 1 || b->platform != 1)
        return EMUIF_VERSION;
    if (!zeros(b->reserved, sizeof b->reserved) ||
        !identifier(b->module_id, sizeof b->module_id, false) ||
        !identifier(b->module_version, sizeof b->module_version, false) ||
        !identifier(b->build_id, sizeof b->build_id, false))
        return EMUIF_INVALID;
    if (!((strlen(b->build_id) == 40 && hex_string(b->build_id, 40)) ||
          (strlen(b->build_id) == 64 && hex_string(b->build_id, 64))))
        return EMUIF_INVALID;
    if (!b->create || !b->destroy || !b->reset || !b->boundary || !b->access || !b->port_output ||
        !b->signal_output)
        return EMUIF_INVALID;
    if (!!(b->capabilities & EMUIF_BOARD_CAP_PEEK) != !!b->peek ||
        !!(b->capabilities & EMUIF_BOARD_CAP_INSPECT) != !!b->inspect)
        return EMUIF_INVALID;
    if ((b->capabilities & EMUIF_BOARD_CAP_STIMULUS) ? (!b->validate_stimulus || !b->apply_stimulus)
                                                     : (b->validate_stimulus || b->apply_stimulus))
        return EMUIF_INVALID;
    if ((b->required_cpu_capabilities & ~cpu->capabilities) ||
        ((b->required_cpu_capabilities & EMUIF_CPU_CAP_UART_FRAME) && variant != 2))
        return EMUIF_UNSUPPORTED;
    return EMUIF_OK;
}
emuif_status emusa80535_session_create_v1(const struct emuif_session_config *c,
                                          struct emuif_session **out)
{
    struct emuif_session *s;
    struct emuif_board_host h;
    struct emuif_board_create b;
    struct emuif_cpu_config cc;
    struct emuif_cpu_hooks hooks;
    struct json_document *d;
    emuif_status e;
    uint8_t random[16];
    size_t i;
    const uint8_t *config;
    uint32_t n;
    if (!out)
        return EMUIF_INVALID;
    *out = NULL;
    e = PREFIX(c);
    if (e)
        return e;
    if (c->reserved || !zeros(c->reserved_identity, sizeof c->reserved_identity) ||
        c->variant > 2 || !c->oscillator_hz || c->oscillator_hz > 100000000 || !c->code ||
        c->code_bytes != 65536 || c->board_config_bytes > 16384 ||
        (!c->board_config_utf8 && c->board_config_bytes) || !hex_string(c->module_sha256, 64))
        return EMUIF_INVALID;
    e = PREFIX(c->cpu_api);
    if (e)
        return e;
    if (!c->cpu_api->create || !c->cpu_api->destroy || !c->cpu_api->reset ||
        !c->cpu_api->load_code || !c->cpu_api->apply_input || !c->cpu_api->cycle ||
        !c->cpu_api->snapshot || !c->cpu_api->peek || !c->cpu_api->decode ||
        !(c->cpu_api->capabilities & EMUIF_CPU_CAP_PORT_LATCH))
        return EMUIF_INVALID;
    e = board_table(c->board_api, c->cpu_api, c->variant);
    if (e)
        return e;
    config = c->board_config_bytes ? c->board_config_utf8 : (const uint8_t *)"{}";
    n = c->board_config_bytes ? c->board_config_bytes : 2;
    d = malloc(sizeof *d);
    if (!d)
        return EMUIF_CAPACITY;
    e = json_document_parse_multiline(d, (const char *)config, n) ? EMUIF_OK : EMUIF_INVALID;
    free(d);
    if (e)
        return e;
    if (getrandom(random, sizeof random, 0) != (ssize_t)sizeof random)
        return EMUIF_IO;
    s = calloc(1, sizeof *s);
    if (!s)
        return EMUIF_CAPACITY;
    s->cpu_api = *c->cpu_api;
    s->board_api = *c->board_api;
    s->variant = c->variant;
    s->oscillator = c->oscillator_hz;
    s->generation = 1;
    emu_outcome_init(&s->outcomes);
    if (em8051_debug_runtime_create(&s->events) != EM8051_DEBUG_EVENT_OK) {
        free(s); return EMUIF_CAPACITY;
    }
    emu_debug_runtime_observer(s->events, retained_observation, s);
    s->busy = true;
    memcpy(s->code, c->code, 65536);
    memcpy(s->config, config, n);
    s->config_bytes = n;
    memcpy(s->module_hash, c->module_sha256, 65);
    sha256_hex(s->code, 65536, s->image_hash);
    sha256_hex(s->config, n, s->config_hash);
    for (i = 0; i < 16; i++)
        snprintf(s->id + i * 2, 3, "%02x", random[i]);
    INIT(h);
    h.context = s;
    h.schedule_input = schedule_input;
    INIT(b);
    b.variant = s->variant;
    b.oscillator_hz = s->oscillator;
    b.config_utf8 = s->config;
    b.config_bytes = n;
    e = normalize(s->board_api.create(&h, &b, &s->board));
    if (e)
        goto fail;
    if (s->state == EMUIF_STATE_FAULTED)
    {
        e = s->cause;
        goto fail;
    }
    if (!s->board)
    {
        e = EMUIF_FAULT;
        goto fail;
    }
    INIT(cc);
    cc.variant = s->variant;
    cc.oscillator_hz = s->oscillator;
    INIT(hooks);
    hooks.context = s;
    hooks.bus_access = bus;
    hooks.port_output = port;
    hooks.signal_output = signal_output;
    e = s->cpu_api.create(&cc, &hooks, &s->cpu);
    if (e)
        goto fail;
    /* A foreign CPU API can use the frozen session API without being cast into
     * this producer's private observation implementation. */
    if (s->variant == EMUIF_VARIANT_SAB80535) {
        struct emuif_cpu_api native;
        INIT(native); INIT(s->observation_api);
        if (!emusa80535_cpu_query_v1(&native) && native.create == s->cpu_api.create &&
            native.cycle == s->cpu_api.cycle && native.reset == s->cpu_api.reset &&
            !emusa80535_cpu_observation_query_v1(&s->observation_api))
            s->observation_coverage = s->observation_api.coverage;
    }
    e = reset_session(s, c->seed, c->entry, true);
    if (e)
        goto fail;
    s->busy = false;
    *out = s;
    return EMUIF_OK;
fail:
    if (s->board)
        s->board_api.destroy(s->board);
    if (s->cpu)
        s->cpu_api.destroy(s->cpu);
    em8051_debug_runtime_destroy(s->events);
    free(s);
    return normalize(e);
}
void emusa80535_session_destroy_v1(struct emuif_session *s)
{
    if (!s)
        return;
    if (s->busy)
    {
        s->callback_violation = true;
        fault(s, EMUIF_FAULT);
        return;
    }
    s->busy = true;
    s->state = EMUIF_STATE_TERMINATED;
    s->board_api.destroy(s->board);
    s->cpu_api.destroy(s->cpu);
    free(s->used_clients);
    em8051_debug_runtime_destroy(s->events);
    free(s);
}
emuif_status emu_session_allow_fault_inspection(struct emuif_session *s, uint32_t allow)
{
    if (!s || allow > 1)
        return EMUIF_INVALID;
    if (s->busy)
        return EMUIF_BUSY;
    if (s->cycle || s->used_count || s->state != EMUIF_STATE_PAUSED)
        return EMUIF_STATE;
    if (allow && !(s->board_api.capabilities & EMUIF_BOARD_CAP_INSPECT))
        return EMUIF_UNSUPPORTED;
    s->fault_inspect = allow != 0;
    return EMUIF_OK;
}
emuif_status emu_session_set_execution_mode(struct emuif_session *s, uint32_t mode)
{
    if (!s || mode > EMUIF_EXECUTION_HOST_DRIVEN)
        return EMUIF_INVALID;
    if (s->busy)
        return EMUIF_BUSY;
    if (s->progressed || s->used_count || s->state != EMUIF_STATE_PAUSED)
        return EMUIF_STATE;
    s->execution_mode = mode;
    return EMUIF_OK;
}
uint32_t emu_session_state(const struct emuif_session *s)
{
    return s ? s->state : EMUIF_STATE_TERMINATED;
}
uint32_t emu_session_client_open(const struct emuif_session *s, uint64_t id)
{
    unsigned i;
    if (s && id)
        for (i = 0; i < 8; i++)
            if (s->clients[i].id == id)
                return 1;
    return 0;
}
uint32_t emu_session_client_ready(const struct emuif_session *s, uint64_t id)
{
    unsigned i;
    if (s && id)
        for (i = 0; i < 8; i++)
            if (s->clients[i].id == id)
                return s->clients[i].hello;
    return 0;
}
static emuif_status execute(struct emuif_session *s, uint32_t budget, bool stepping,
                            struct emuif_progress *p)
{
    struct emuif_snapshot v;
    struct emuif_cycle_result r;
    uint64_t start, instructions;
    emuif_status e = snapshot(s, &v);
    uint32_t turns = 0;
    if (e)
        return e;
    start = v.cycle;
    instructions = v.instructions;
    while (s->state == EMUIF_STATE_RUNNING)
    {
        bool safe = v.safe_boundary != 0;
        if (safe)
        {
            /* Clock-domain halt outranks CODE and user/budget stops, including
             * an instruction that sets PD on the budget's final cycle. */
            if (v.power_down)
            {
                e = stop(s, 8);
                break;
            }
            if (s->suppress && v.instructions > s->suppress_instruction)
                s->suppress = false;
            if (emu_breakpoint_at(s->breakpoints, v.pc) && !(s->suppress && s->suppress_pc == v.pc))
            {
                e = stop(s, 4);
                break;
            }
            if (stepping && v.instructions > instructions)
            {
                e = stop(s, 3);
                break;
            }
            if (turns >= budget)
            {
                e = stepping ? stop(s, 7) : EMUIF_OK;
                break;
            }
        }
        if (turns >= budget + 16)
        {
            e = fault(s, EMUIF_FAULT);
            break;
        }
        INIT(r);
        s->progressed = true;
        e = s->cpu_api.cycle(s->cpu, &r);
        if (e)
        {
            if (e == EMUIF_UNSUPPORTED)
            {
                struct emuif_snapshot pd;
                if (!snapshot(s, &pd) && pd.power_down)
                {
                    e = stop(s, 8);
                    break;
                }
            }
            e = fault(s, e);
            break;
        }
        if (!r.progressed || r.cycle != s->cycle + 1)
        {
            e = fault(s, EMUIF_FAULT);
            break;
        }
        /* A successful cycle includes peripheral advancement. A legacy count
         * increase only starts a candidate: retire after its last delay cycle,
         * before the subsequent board/input boundary (which may itself fail).
         * IRQ-entry/IDLE cycles have no count increase and cannot retire. Failed
         * CPU cycles never complete this path, including bus/peripheral faults. */
        if (!s->observation_bound && r.instructions > v.instructions)
            s->retirement_pending = true;
        if (!s->observation_bound && r.safe_boundary && s->retirement_pending)
        {
            if (s->retired == UINT64_MAX)
            {
                e = fault(s, EMUIF_EXHAUSTED);
                break;
            }
            s->retired++;
            s->retirement_pending = false;
        }
        s->cycle = r.cycle;
        s->observed_legacy = r.instructions;
        if (s->observation_error) { e = fault(s, s->observation_error); break; }
        turns++;
        e = boundary(s);
        if (e)
            break;
        e = snapshot(s, &v);
        if (e)
            break;
    }
    publish_fault(s);
    snapshot(s, &v);
    p->cycles = v.cycle - start;
    p->instructions = v.instructions - instructions;
    p->state = s->state;
    p->stop_reason = s->state == EMUIF_STATE_RUNNING ? 0 : s->reason;
    return e;
}
emuif_status emusa80535_session_pump_v1(struct emuif_session *s, uint32_t budget,
                                        struct emuif_progress *out)
{
    emuif_status e;
    struct emuif_progress p;
    if (!s)
        return EMUIF_INVALID;
    if (s->busy)
        return EMUIF_BUSY;
    e = PREFIX(out);
    if (e)
        return e;
    if (!budget || budget > 100000)
        return EMUIF_RANGE;
    if (s->state != EMUIF_STATE_RUNNING)
        return EMUIF_STATE;
    INIT(p);
    s->busy = true;
    e = execute(s, budget, false, &p);
    s->busy = false;
    /* Even a fault supplies actual partial progress; never a fabricated success. */
    *out = p;
    return e;
}

static bool wire_null(const struct json_document *d, int t)
{
    return t >= 0 && t < d->count && d->tokens[t].type == JSON_PRIMITIVE &&
           d->tokens[t].end - d->tokens[t].start == 4 &&
           !memcmp(d->text + d->tokens[t].start, "null", 4);
}
static bool wire_string_is(const struct json_document *d, int t, const char *v)
{
    char b[128];
    return json_string(d, t, b, sizeof b) && !strcmp(b, v);
}
static bool wire_equal(const struct json_document *d, int a, int b)
{
    char x[128], y[128];
    int64_t n, m;
    if (d->tokens[a].type == JSON_STRING)
        return json_string(d, a, x, sizeof x) && json_string(d, b, y, sizeof y) && !strcmp(x, y);
    return json_integer(d, a, 0, UINT32_MAX, &n) && json_integer(d, b, 0, UINT32_MAX, &m) && n == m;
}
#include "emu_wire_schema_internal.h"
static bool wire3_literal(const struct json_document *d, int t, const char *value)
{
    return t >= 0 && t < d->count && d->tokens[t].type == JSON_PRIMITIVE &&
           (size_t)(d->tokens[t].end - d->tokens[t].start) == strlen(value) &&
           !memcmp(d->text + d->tokens[t].start, value, strlen(value));
}
#include "emu_control3_schema_internal.h"
static struct client_state *client(struct emuif_session *s, uint64_t id)
{
    unsigned i;
    for (i = 0; i < 8; i++)
        if (s->clients[i].id == id)
            return &s->clients[i];
    return NULL;
}
emuif_status emusa80535_debug_open_v1(struct emuif_session *s, const struct emuif_client *c)
{
    emuif_status e;
    size_t i;
    unsigned slot;
    uint64_t *used;
    if (!s)
        return EMUIF_INVALID;
    if (s->busy)
        return EMUIF_BUSY;
    e = PREFIX(c);
    if (e)
        return e;
    if (!c->connection_id || c->role > 1 || c->may_terminate > 1)
        return EMUIF_INVALID;
    if (s->state == EMUIF_STATE_TERMINATED)
        return EMUIF_STATE;
    for (i = 0; i < s->used_count; i++)
        if (s->used_clients[i] == c->connection_id)
            return EMUIF_STALE;
    for (slot = 0; slot < 8; slot++)
        if (!s->clients[slot].id)
            break;
    if (slot == 8)
        return EMUIF_CAPACITY;
    /* Bound lifetime identity retention without requiring increasing host IDs. */
    if (s->used_count == 8192)
        return EMUIF_CAPACITY;
    used = realloc(s->used_clients, (s->used_count + 1) * sizeof(uint64_t));
    if (!used)
        return EMUIF_CAPACITY;
    s->used_clients = used;
    used[s->used_count++] = c->connection_id;
    memset(&s->clients[slot], 0, sizeof s->clients[slot]);
    s->clients[slot].id = c->connection_id;
    s->clients[slot].maximum_role = c->role;
    s->clients[slot].may_terminate = c->may_terminate;
    return EMUIF_OK;
}
static emuif_status acquire_lease(struct emuif_session *s, struct client_state *c)
{
    if (!c->maximum_role)
        return EMUIF_DENIED;
    if (s->lease)
        return EMUIF_BUSY;
    if (s->next_lease == UINT64_MAX)
        return EMUIF_EXHAUSTED;
    c->lease_id = ++s->next_lease;
    c->role = EMUIF_ROLE_CONTROLLER;
    s->lease = c->id;
    return EMUIF_OK;
}
static void close_client(struct emuif_session *s, struct client_state *c)
{
    if (s->lease == c->id)
        s->lease = 0;
    memset(c, 0, sizeof *c);
}
emuif_status emusa80535_debug_close_v1(struct emuif_session *s, uint64_t id)
{
    struct client_state *c;
    if (!s || !id)
        return EMUIF_INVALID;
    if (s->busy)
        return EMUIF_BUSY;
    c = client(s, id);
    if (!c)
        return EMUIF_NOT_FOUND;
    close_client(s, c);
    return EMUIF_OK;
}
static const char *status_name(emuif_status e)
{
    static const char *names[] = {"OK",       "INVALID",     "VERSION", "STATE", "RANGE",
                                  "CAPACITY", "UNSUPPORTED", "BUSY",    "STALE", "DENIED",
                                  "FAULT",    "EXHAUSTED",   "IO",      "HASH",  "NOT_FOUND"};
    return names[normalize(e)];
}
static void snapshot_json(struct emuif_session *s, struct client_state *c, struct output *o,
                          bool include_lease)
{
    struct emuif_snapshot v;
    unsigned i;
    static const char *states[] = {"paused", "running", "faulted", "terminated"};
    static const char *variants[] = {"8051", "8052", "sab80535"};
    static const char *reasons[] = {"entry",     "entry",        "pause",
                                    "step",      "breakpoint",   "watchpoint",
                                    "exception", "cycle-budget", "power-down-unsupported",
                                    "terminated"};
    snapshot(s, &v);
    output_format(
        o,
        "{\"sessionId\":\"%s\",\"generation\":\"%" PRIu64
        "\",\"state\":\"%s\",\"variant\":\"%s\",\"cycle\":\"%" PRIu64
        "\",\"instructions\":\"%" PRIu64
        "\",\"pc\":%u,\"registers\":{\"a\":%u,\"b\":%u,\"psw\":%u,\"sp\":%u,\"dptr\":%u,\"r\":[",
        s->id, s->generation, states[s->state], variants[s->variant], v.cycle, v.instructions, v.pc,
        v.a, v.b, v.psw, v.sp, v.dptr);
    for (i = 0; i < 8; i++)
        output_format(o, "%s%u", i ? "," : "", v.r[i]);
    output_format(o,
                  "]},\"idle\":%s,\"powerDown\":%s,\"safeBoundary\":%s,\"stopRevision\":\"%" PRIu64
                  "\",\"reason\":\"%s\",\"imageSha256\":\"%s\",\"moduleSha256\":\"%s\","
                  "\"configSha256\":\"%s\",\"moduleId\":\"%s\",\"moduleVersion\":\"%s\","
                  "\"controlRole\":\"%s\",\"canTerminate\":%s}",
                  v.idle ? "true" : "false", v.power_down ? "true" : "false",
                  v.safe_boundary ? "true" : "false", s->revision, reasons[s->reason],
                  s->image_hash, s->module_hash, s->config_hash, s->board_api.module_id,
                  s->board_api.module_version, c->role ? "controller" : "observer",
                  c->role && c->may_terminate ? "true" : "false");
    if (c->control3 && include_lease && !o->failed)
    {
        o->length--;
        output_format(o, ",\"leaseId\":\"%" PRIu64 "\"}", c->lease_id);
    }
}
static bool capability(struct emuif_session *s, const char *v)
{
    if (!strcmp(v, "attachSession") || !strcmp(v, "status") || !strcmp(v, "control") ||
        !strcmp(v, "codeBreakpoints") || !strcmp(v, "decodeCode") || !strcmp(v, "memoryRead"))
        return true;
    if (!strcmp(v, "boardInput"))
        return (s->board_api.capabilities & EMUIF_BOARD_CAP_STIMULUS) != 0;
    if (!strcmp(v, "boardInspect"))
        return (s->board_api.capabilities & EMUIF_BOARD_CAP_INSPECT) != 0;
    return !strcmp(v, "boardInspectFault") && s->fault_inspect;
}
static int64_t number(const struct json_document *d, int a, const char *key)
{
    int64_t v = 0;
    json_integer(d, json_member(d, a, key), INT32_MIN, UINT32_MAX, &v);
    return v;
}
#include "emu_control3_validation_internal.h"
static emuif_status dispatch(struct emuif_session *s, struct client_state *c,
                             const struct json_document *d, int a, const char *cmd,
                             struct output *o, bool *close)
{
    char text[128];
    int t;
    uint64_t gen;
    struct emuif_snapshot v;
    emuif_status e;
    if (!strcmp(cmd, "hello"))
    {
        static const char *caps[] = {"attachSession",   "status",       "control",
                                     "codeBreakpoints", "decodeCode",   "memoryRead",
                                     "boardInput",      "boardInspect", "boardInspectFault"};
        unsigned i;
        bool comma = false;
        int protocol = json_member(d, a, "protocol"),
            required = json_member(d, a, "requiredCapabilities");
        if (c->hello)
            return EMUIF_STATE;
        if (number(d, protocol, "major") != 2 || number(d, protocol, "minor") != 1 ||
            !wire_string_is(d, json_member(d, a, "contractVersion"), "0.2.0"))
        {
            *close = true;
            return EMUIF_VERSION;
        }
        for (t = required + 1; t < d->count && d->tokens[t].start < d->tokens[required].end;
             t = json_next(d, t))
        {
            json_string(d, t, text, sizeof text);
            if (!capability(s, text))
                return EMUIF_UNSUPPORTED;
            if (!strcmp(text, "boardInspectFault"))
            {
                int k;
                bool found = false;
                for (k = required + 1; k < d->count && d->tokens[k].start < d->tokens[required].end;
                     k = json_next(d, k))
                    if (wire_string_is(d, k, "boardInspect"))
                        found = true;
                if (!found)
                    return EMUIF_INVALID;
            }
        }
        c->hello = true;
        output_format(
            o,
            "{\"protocol\":{\"major\":2,\"minor\":1},\"contractVersion\":\"0.2.0\","
            "\"runtimeCommit\":\"%s\",\"contractManifestSha256\":\"%s\",\"capabilities\":[",
            EMU_RUNTIME_COMMIT, EMU_CONTRACT_MANIFEST_SHA256);
        for (i = 0; i < sizeof caps / sizeof caps[0]; i++)
            if (capability(s, caps[i]))
            {
                if (comma)
                    output_raw(o, ",");
                output_string(o, caps[i]);
                comma = true;
            }
        output_raw(
            o, "],\"limits\":{\"maxFrameBytes\":65536,\"maxConfigBytes\":16384,\"maxInputs\":1024,"
               "\"maxPageRecords\":128,\"maxCycleBudget\":100000,\"maxSafeDrainCycles\":16,"
               "\"maxClients\":8,\"maxPendingRequests\":32,\"maxBreakpoints\":1024,"
               "\"maxMemoryBytes\":256,\"maxDecodeRecords\":256,\"maxBoardPayloadBytes\":4096}}");
        return EMUIF_OK;
    }
    if (!c->hello)
    {
        *close = true;
        return EMUIF_STATE;
    }
    t = json_member(d, a, "sessionId");
    if (!(wire_null(d, t) && !strcmp(cmd, "attach")))
    {
        json_string(d, t, text, sizeof text);
        if (strcmp(text, s->id))
            return EMUIF_STALE;
    }
    if (!strcmp(cmd, "attach"))
    {
        uint32_t role = wire_string_is(d, json_member(d, a, "role"), "controller") ? 1 : 0;
        if (c->attached)
            return EMUIF_STATE;
        if (role > c->maximum_role)
            return EMUIF_DENIED;
        if (role)
        {
            e = acquire_lease(s, c);
            if (e)
                return e;
        }
        c->attached = true;
        c->role = role;
        if (role)
            s->lease = c->id;
        snapshot_json(s, c, o, true);
        return EMUIF_OK;
    }
    if (!c->attached)
        return EMUIF_STATE;
    if (!strcmp(cmd, "status"))
    {
        snapshot_json(s, c, o, true);
        return EMUIF_OK;
    }
    if (!strcmp(cmd, "detach"))
    {
        output_format(o, "{\"sessionId\":\"%s\",\"detached\":true}", s->id);
        if (s->lease == c->id)
            s->lease = 0;
        c->attached = false;
        c->role = 0;
        c->lease_id = 0;
        return EMUIF_OK;
    }
    if (!strcmp(cmd, "tracePage"))
        return EMUIF_UNSUPPORTED;
    json_u64(d, json_member(d, a, "generation"), &gen);
    if (gen != s->generation)
        return EMUIF_STALE;
    if (s->state == EMUIF_STATE_TERMINATED)
        return EMUIF_STATE;
    if (!strcmp(cmd, "boardInspect"))
    {
        struct emuif_board_inspect request;
        struct emuif_board_inspection response;
        struct output payload = {0};
        uint8_t bytes[4097];
        struct json_document *inspection;
        if (!capability(s, "boardInspect"))
            return EMUIF_UNSUPPORTED;
        if (s->state != EMUIF_STATE_PAUSED &&
            !(s->state == EMUIF_STATE_FAULTED && s->fault_inspect))
            return EMUIF_STATE;
        json_compact(d, json_member(d, a, "payload"), &payload);
        if (payload.failed || payload.length > 4096)
            return EMUIF_CAPACITY;
        INIT(request);
        request.generation = s->generation;
        json_string(d, json_member(d, a, "schema"), request.schema, sizeof request.schema);
        request.request_utf8 = (uint8_t *)payload.data;
        request.request_bytes = (uint32_t)payload.length;
        INIT(response);
        response.response_utf8 = bytes;
        response.capacity = 4096;
        {
            s->callback_violation = false;
            e = normalize(s->board_api.inspect(s->board, &request, &response));
            if (s->callback_violation)
                return s->cause;
        }
        if (e)
            return e;
        if (PREFIX(&response) || response.reserved || response.response_utf8 != bytes ||
            response.capacity != 4096 || response.written > 4096 ||
            response.required < response.written || response.required > response.capacity)
            return fault(s, EMUIF_FAULT);
        inspection = malloc(sizeof *inspection);
        if (!inspection)
            return EMUIF_CAPACITY;
        if (!json_document_parse_multiline(inspection, (char *)bytes, response.written))
        {
            free(inspection);
            return fault(s, EMUIF_FAULT);
        }
        {
            int k;
            for (k = 0; k < inspection->count; k++)
            {
                int parent = inspection->tokens[k].parent, depth = 1;
                while (parent >= 0)
                {
                    depth++;
                    parent = inspection->tokens[parent].parent;
                }
                if (depth > 14)
                {
                    free(inspection);
                    return EMUIF_CAPACITY;
                }
            }
        }
        output_format(o, "{\"generation\":\"%" PRIu64 "\",\"schema\":", s->generation);
        output_string(o, request.schema);
        output_raw(o, ",\"payload\":");
        json_compact(inspection, 0, o);
        output_raw(o, "}");
        free(inspection);
        return EMUIF_OK;
    }
    if (!strcmp(cmd, "decodeCode") || !strcmp(cmd, "readMemory"))
    {
        if (s->state != EMUIF_STATE_PAUSED)
            return EMUIF_STATE;
        if (!strcmp(cmd, "decodeCode"))
        {
            struct emuif_decode_request r;
            struct emuif_decoded decoded[256];
            uint32_t i;
            INIT(r);
            r.address = (uint16_t)number(d, a, "address");
            r.byte_offset = (int32_t)number(d, a, "byteOffset");
            r.instruction_offset = (int32_t)number(d, a, "instructionOffset");
            r.count = (uint32_t)number(d, a, "count");
            for (i = 0; i < r.count; i++)
                INIT(decoded[i]);
            e = s->cpu_api.decode(s->cpu, &r, decoded, sizeof decoded[0], r.count);
            if (e)
                return e;
            output_raw(o, "{\"records\":[");
            for (i = 0; i < r.count; i++)
            {
                output_format(
                    o, "%s{\"address\":%u,\"size\":%u,\"valid\":%s,\"text\":", i ? "," : "",
                    decoded[i].address, decoded[i].size, decoded[i].valid ? "true" : "false");
                output_string(o, decoded[i].text);
                output_format(o, ",\"reason\":\"%s\"}",
                              decoded[i].valid ? "decoded" : "unknown-predecessor");
            }
            output_raw(o, "]}");
            return EMUIF_OK;
        }
        else
        {
            struct emuif_memory_request r;
            uint8_t bytes[256];
            uint32_t i;
            INIT(r);
            r.address = (uint32_t)number(d, a, "address");
            r.count = (uint32_t)number(d, a, "count");
            t = json_member(d, a, "space");
            r.space = wire_string_is(d, t, "CODE")   ? 0
                      : wire_string_is(d, t, "IRAM") ? 1
                      : wire_string_is(d, t, "SFR")  ? 2
                                                     : 3;
            if (r.space == 3)
            {
                if (!s->board_api.peek)
                    return EMUIF_UNSUPPORTED;
                if (r.address + r.count > 65536)
                    return EMUIF_RANGE;
                e = normalize(s->board_api.peek(s->board, &r, bytes, sizeof bytes));
                if (s->state == EMUIF_STATE_FAULTED)
                    return s->cause;
            }
            else
                e = s->cpu_api.peek(s->cpu, &r, bytes, sizeof bytes);
            if (e)
                return e;
            output_format(o, "{\"address\":%u,\"bytesHex\":\"", r.address);
            for (i = 0; i < r.count; i++)
                output_format(o, "%02x", bytes[i]);
            output_raw(o, "\"}");
            return EMUIF_OK;
        }
    }
    if (c->role != EMUIF_ROLE_CONTROLLER || s->lease != c->id)
        return EMUIF_DENIED;
    if (!strcmp(cmd, "terminate"))
    {
        bool unsafe = s->state == EMUIF_STATE_FAULTED;
        if (!c->may_terminate)
            return EMUIF_DENIED;
        e = stop(s, 9);
        if (e)
            return e;
        if (unsafe)
            return EMUIF_OK; /* No truthful conforming final snapshot exists. */
        snapshot_json(s, c, o, false);
        return EMUIF_OK;
    }
    if (!strcmp(cmd, "reset"))
    {
        if (s->state != EMUIF_STATE_PAUSED && s->state != EMUIF_STATE_FAULTED)
            return EMUIF_STATE;
        e = reset_session(s, (uint32_t)number(d, a, "seed"), (uint16_t)number(d, a, "entry"),
                          false);
        if (e)
            return e;
        snapshot_json(s, c, o, false);
        return EMUIF_OK;
    }
    if (s->state == EMUIF_STATE_FAULTED)
        return EMUIF_STATE;
    if (!strcmp(cmd, "boardInput"))
    {
        struct queued_input q;
        struct output payload = {0};
        uint64_t when;
        if (!capability(s, "boardInput"))
            return EMUIF_UNSUPPORTED;
        t = json_member(d, a, "cycle");
        if (wire_null(d, t))
        {
            if (s->cycle == UINT64_MAX)
                return fault(s, EMUIF_EXHAUSTED);
            when = s->cycle + 1;
        }
        else
            json_u64(d, t, &when);
        if (when <= s->cycle)
            return EMUIF_RANGE;
        if (s->queued == 1024)
            return EMUIF_CAPACITY;
        if (s->next_order == UINT64_MAX || s->next_parent == UINT64_MAX)
            return fault(s, EMUIF_EXHAUSTED);
        memset(&q, 0, sizeof q);
        q.board = true;
        INIT(q.stimulus);
        q.stimulus.generation = s->generation;
        q.stimulus.cycle = when;
        q.stimulus.order = s->next_order + 1;
        json_string(d, json_member(d, a, "schema"), q.stimulus.schema, sizeof q.stimulus.schema);
        json_compact(d, json_member(d, a, "payload"), &payload);
        if (payload.failed || payload.length > 4096)
            return EMUIF_CAPACITY;
        memcpy(q.payload, payload.data, payload.length);
        q.stimulus.payload_utf8 = q.payload;
        q.stimulus.payload_bytes = (uint32_t)payload.length;
        e = normalize(s->board_api.validate_stimulus(s->board, &q.stimulus));
        if (e)
            return e;
        if (s->state == EMUIF_STATE_FAULTED)
            return s->cause;
        s->next_order++;
        q.parent = ++s->next_parent;
        insert(s, &q);
        input_observation(s, &q, EMUIF_OBS_INPUT_ADMITTED, EMUIF_OK);
        output_format(o,
                      "{\"generation\":\"%" PRIu64 "\",\"cycle\":\"%" PRIu64
                      "\",\"order\":\"%" PRIu64 "\"}",
                      s->generation, when, s->next_order);
        if (c->control3 && !o->failed)
        {
            o->length--;
            output_format(o, ",\"parentStimulusId\":\"%" PRIu64 "\"}", q.parent);
        }
        return EMUIF_OK;
    }
    if (!strcmp(cmd, "pause"))
    {
        if (s->state == EMUIF_STATE_RUNNING)
        {
            e = stop(s, 2);
            if (e)
                return e;
        }
        snapshot_json(s, c, o, false);
        return EMUIF_OK;
    }
    if (!strcmp(cmd, "resume") || !strcmp(cmd, "step"))
    {
        bool stepping = !strcmp(cmd, "step");
        if (stepping && s->state != EMUIF_STATE_PAUSED)
            return EMUIF_STATE;
        e = snapshot(s, &v);
        if (e)
            return e;
        if (v.power_down)
            return EMUIF_STATE;
        if (s->state == EMUIF_STATE_PAUSED && s->reason == 4)
        {
            s->suppress = true;
            s->suppress_pc = v.pc;
            s->suppress_instruction = v.instructions;
        }
        s->state = EMUIF_STATE_RUNNING;
        if (stepping)
        {
            struct emuif_progress progress;
            INIT(progress);
            e = execute(s, (uint32_t)number(d, a, "maxCycles"), true, &progress);
            if (e)
                return e;
        }
        snapshot_json(s, c, o, false);
        return EMUIF_OK;
    }
    if (!strcmp(cmd, "replaceCodeBreakpoints"))
    {
        uint16_t addresses[1024];
        size_t count = 0;
        int array = json_member(d, a, "addresses");
        int64_t value;
        if (s->state != EMUIF_STATE_PAUSED)
            return EMUIF_STATE;
        for (t = array + 1; t < d->count && d->tokens[t].start < d->tokens[array].end;
             t = json_next(d, t))
        {
            json_integer(d, t, 0, 65535, &value);
            addresses[count++] = (uint16_t)value;
        }
        if (!emu_breakpoint_replace(s->breakpoints, addresses, count))
            return EMUIF_INVALID;
        output_raw(o, "{\"addresses\":");
        json_compact(d, array, o);
        output_raw(o, "}");
        return EMUIF_OK;
    }
    return EMUIF_UNSUPPORTED;
}
/* All control3 operations use the same session and execution path as wire2.1. */
static emuif_status advance_control(struct emuif_session *s, struct client_state *c,
                                    const struct emuif_control_advance *a,
                                    struct emuif_control_progress *p)
{
    struct emuif_progress progress;
    struct emuif_snapshot v;
    uint64_t retired = s->retired;
    emuif_status e;
    static const uint32_t reasons[] = {0, 0, 1, 1, 2, 3, 4, 0, 5, 6};
    if (!c->hello || !c->attached)
        return EMUIF_STATE;
    if (s->execution_mode != EMUIF_EXECUTION_EXTERNALLY_DRIVEN)
        return EMUIF_UNSUPPORTED;
    if (a->generation != s->generation)
        return EMUIF_STALE;
    if (!c->role || s->lease != c->id || !a->lease_id || a->lease_id != c->lease_id)
        return EMUIF_DENIED;
    if (s->state != EMUIF_STATE_RUNNING)
        return EMUIF_STATE;
    if (!a->cycle_budget || a->cycle_budget > 100000)
        return EMUIF_RANGE;
    INIT(progress);
    e = execute(s, a->cycle_budget, false, &progress);
    if (e && s->state != EMUIF_STATE_FAULTED)
        return e;
    if (snapshot(s, &v))
        return EMUIF_FAULT;
    p->generation = s->generation;
    p->cycles = progress.cycles;
    p->instructions = progress.instructions;
    p->retired_instructions = s->retired - retired;
    p->cycle = v.cycle;
    p->stop_revision = s->revision;
    p->state = s->state;
    p->safe_boundary = v.safe_boundary;
    p->stop_reason = s->state == EMUIF_STATE_RUNNING ? 0 : reasons[s->reason];
    p->partial = progress.cycles < a->cycle_budget || !v.safe_boundary;
    return EMUIF_OK;
}
static emuif_status control3_advance(struct emuif_session *s, const struct emuif_control_advance *a,
                                     struct emuif_control_progress *out)
{
    struct client_state *c;
    struct emuif_control_progress p;
    emuif_status e;
    if (!s)
        return EMUIF_INVALID;
    if (s->busy)
        return EMUIF_BUSY;
    if ((e = PREFIX(a)) || (e = PREFIX(out)))
        return e;
    if (a->reserved || !a->connection_id)
        return EMUIF_INVALID;
    c = client(s, a->connection_id);
    if (!c)
        return EMUIF_NOT_FOUND;
    if (!c->control3)
        return EMUIF_VERSION;
    INIT(p);
    p.struct_size = out->struct_size;
    s->busy = true;
    e = advance_control(s, c, a, &p);
    s->busy = false;
    if (!e)
        *out = p;
    return e;
}
static bool capability3(struct emuif_session *s, const char *name)
{
    return capability(s, name) ||
           (!strcmp(name, "observations") && s->observation_coverage != 0) ||
           (!strcmp(name, "uartOutcomes") && (s->observation_coverage & 112u) == 112u) ||
           (!strcmp(name, "advance") && s->execution_mode == EMUIF_EXECUTION_EXTERNALLY_DRIVEN);
}
static const char *command_capability3(const char *cmd)
{
    if (!strcmp(cmd, "boardInput"))
        return "boardInput";
    if (!strcmp(cmd, "boardInspect"))
        return "boardInspect";
    if (!strcmp(cmd, "advance"))
        return "advance";
    if (!strcmp(cmd, "traceConfig") || !strcmp(cmd, "replaceTraceConfig"))
        return "traceConfig";
    if (!strcmp(cmd, "tracePage") || !strcmp(cmd, "clearTrace"))
        return "tracePage";
    if (!strcmp(cmd, "outcomePage"))
        return "uartOutcomes";
    if (!strncmp(cmd, "capture", 7))
        return "captureA";
    if (!strncmp(cmd, "sink", 4))
        return "traceFile";
    return "control";
}
static emuif_status dispatch3(struct emuif_session *s, struct client_state *c,
                              const struct json_document *d, int a, const char *cmd,
                              struct output *o, bool *close)
{
    char text[128];
    int t;
    uint64_t gen = 0, lease_id = 0;
    emuif_status e;
    if (!strcmp(cmd, "hello"))
    {
        static const char *caps[] = {
            "attachSession", "status",     "control",      "codeBreakpoints",   "decodeCode",
            "memoryRead",    "boardInput", "boardInspect", "boardInspectFault", "advance", "observations", "uartOutcomes"};
        unsigned i;
        bool comma = false;
        int protocol = json_member(d, a, "protocol"),
            required = json_member(d, a, "requiredCapabilities");
        if (c->hello)
            return EMUIF_STATE;
        if (number(d, protocol, "major") != 3 || number(d, protocol, "minor") != 0 ||
            !wire_string_is(d, json_member(d, a, "contractVersion"), "0.3.0"))
        {
            *close = true;
            return EMUIF_VERSION;
        }
        for (t = required + 1; t < d->count && d->tokens[t].start < d->tokens[required].end;
             t = json_next(d, t))
        {
            json_string(d, t, text, sizeof text);
            if (!capability3(s, text))
                return EMUIF_UNSUPPORTED;
            if (!strcmp(text, "boardInspectFault"))
            {
                int k;
                bool found = false;
                for (k = required + 1; k < d->count && d->tokens[k].start < d->tokens[required].end;
                     k = json_next(d, k))
                    if (wire_string_is(d, k, "boardInspect"))
                        found = true;
                if (!found)
                    return EMUIF_INVALID;
            }
        }
        c->hello = true;
        output_format(
            o,
            "{\"protocol\":{\"major\":3,\"minor\":0},\"contractVersion\":\"0.3.0\","
            "\"runtimeCommit\":\"%s\",\"contractManifestSha256\":\"%s\",\"capabilities\":[",
            EMU_RUNTIME_COMMIT, EMU_CONTROL3_MANIFEST_SHA256);
        for (i = 0; i < sizeof caps / sizeof caps[0]; i++)
            if (capability3(s, caps[i]))
            {
                if (comma)
                    output_raw(o, ",");
                output_string(o, caps[i]);
                comma = true;
            }
        output_format(o, "],\"executionMode\":\"%s\",\"observationCoverage\":[",
                      s->execution_mode ? "host-driven" : "externally-driven");
        {
            static const char *coverage[] = {"instruction","access","control","irq","lifecycle","input","uart"};
            bool separator = false;
            for (i=0; i<7; i++) if (s->observation_coverage & (UINT64_C(1)<<i)) {
                if(separator) output_raw(o,",");
                output_string(o,coverage[i]); separator=true;
            }
        }
        output_raw(o, "],\"limits\":");
        output_raw(o, "{\"maxFrameBytes\":65536,\"maxConfigBytes\":16384,\"maxInputs\":1024,"
                      "\"maxPageRecords\":16,\"maxCycleBudget\":100000,\"maxSafeDrainCycles\":16,"
                      "\"maxClients\":8,\"maxPendingRequests\":32,\"maxBreakpoints\":1024,"
                      "\"maxMemoryBytes\":256,\"maxDecodeRecords\":256,\"maxBoardPayloadBytes\":"
                      "4096,\"maxTraces\":16,\"maxDestinations\":8,\"maxPoints\":32,\"maxGates\":"
                      "32,\"maxWatches\":64,\"maxActions\":8,\"maxRoutes\":16,\"maxContextDepth\":"
                      "64,\"maxRingRecords\":4096,\"outcomeRecords\":1024,\"maxRecordBytes\":3072,"
                      "\"fileQueueRecords\":256,\"maxFileSegments\":16,\"frameTimeoutMs\":5000,"
                      "\"maxJsonDepth\":16,\"maxJsonValues\":8192}}");
        return EMUIF_OK;
    }
    if (!c->hello)
    {
        *close = true;
        return EMUIF_STATE;
    }
    if (strcmp(cmd, "attach") && !c->attached)
        return EMUIF_STATE;
    if (!capability3(s, command_capability3(cmd)))
        return EMUIF_UNSUPPORTED;
    if (!strcmp(cmd, "outcomePage")) {
        struct emuif_trace_page_request q;
        struct emuif_trace_page page;
        struct emuif_trace_record records[16];
        if (s->state == EMUIF_STATE_TERMINATED) return EMUIF_STATE;
        INIT(q); INIT(page);
        json_string(d,json_member(d,a,"sessionId"),q.session_id,sizeof q.session_id);
        json_u64(d,json_member(d,a,"epoch"),&q.epoch);
        json_u64(d,json_member(d,a,"afterRecordSequence"),&q.after_record_sequence);
        q.max_records=(uint32_t)number(d,a,"maxRecords");
        { unsigned j; for(j=0;j<16;j++) { INIT(records[j]); INIT(records[j].event); INIT(records[j].event.old_value); INIT(records[j].event.new_value); } }
        page.records=records; page.capacity=16; page.element_size=sizeof records[0];
        e=emu_outcome_page(&s->outcomes,s->id,&q,&page);
        if(!e) emu_trace_page_json(o,&page);
        return e;
    }
    t = json_member(d, a, "sessionId");
    if (!(wire_null(d, t) && !strcmp(cmd, "attach")))
    {
        json_string(d, t, text, sizeof text);
        if (strcmp(text, s->id))
            return EMUIF_STALE;
    }
    t = json_member(d, a, "generation");
    if (t >= 0 && (!json_u64(d, t, &gen) || gen != s->generation))
        return EMUIF_STALE;
    t = json_member(d, a, "leaseId");
    if (t >= 0 &&
        (!json_u64(d, t, &lease_id) || !c->role || s->lease != c->id || lease_id != c->lease_id))
        return EMUIF_DENIED;
    if (s->state == EMUIF_STATE_TERMINATED)
        return EMUIF_STATE;
    if (!strcmp(cmd, "acquireControl"))
    {
        e = acquire_lease(s, c);
        if (e)
            return e;
        output_format(o, "{\"leaseId\":\"%" PRIu64 "\"}", c->lease_id);
        return EMUIF_OK;
    }
    if (!strcmp(cmd, "releaseControl"))
    {
        s->lease = 0;
        c->lease_id = 0;
        c->role = EMUIF_ROLE_OBSERVER;
        output_raw(o, "{\"leaseId\":\"0\"}");
        return EMUIF_OK;
    }
    if (!strcmp(cmd, "stopDetail"))
    {
        static const char *reasons[] = {"entry",     "entry",      "pause",
                                        "step",      "breakpoint", "watchpoint",
                                        "fault",     "budget",     "power-down-unsupported",
                                        "terminated"};
        output_format(
            o,
            "{\"stopRevision\":\"%" PRIu64 "\",\"generation\":\"%" PRIu64
            "\",\"reason\":\"%s\",\"sourceSequence\":\"%" PRIu64 "\",\"instructionId\":\"%" PRIu64 "\",\"matches\":[]}",
            s->revision, s->generation, reasons[s->reason], s->stop_source_sequence, s->stop_instruction_id);
        return EMUIF_OK;
    }
    if (!strcmp(cmd, "advance"))
    {
        struct emuif_control_advance request;
        struct emuif_control_progress p;
        static const char *states[] = {"paused", "running", "faulted", "terminated"};
        static const char *reasons[] = {"budget",     "pause", "breakpoint",
                                        "watchpoint", "fault", "power-down-unsupported",
                                        "terminated"};
        INIT(request);
        INIT(p);
        request.connection_id = c->id;
        request.generation = gen;
        request.lease_id = lease_id;
        request.cycle_budget = (uint32_t)number(d, a, "cycleBudget");
        e = advance_control(s, c, &request, &p);
        if (e)
            return e;
        output_format(o,
                      "{\"generation\":\"%" PRIu64 "\",\"cycles\":\"%" PRIu64
                      "\",\"instructions\":\"%" PRIu64 "\",\"retiredInstructions\":\"%" PRIu64
                      "\",\"cycle\":\"%" PRIu64 "\",\"stopRevision\":\"%" PRIu64
                      "\",\"state\":\"%s\",\"safeBoundary\":%s,\"reason\":\"%s\",\"partial\":%s}",
                      p.generation, p.cycles, p.instructions, p.retired_instructions, p.cycle,
                      p.stop_revision, states[p.state], p.safe_boundary ? "true" : "false",
                      reasons[p.stop_reason], p.partial ? "true" : "false");
        return EMUIF_OK;
    }
    return dispatch(s, c, d, a, cmd, o, close);
}

static emuif_status request_profile(struct emuif_session *s, uint64_t id,
                                    const struct emuif_json_request *request,
                                    struct emuif_json_response *response, bool control3)
{
    struct client_state *c;
    struct json_document *d;
    struct output *body, *out;
    emuif_status e;
    int64_t rid = 0;
    char cmd[64] = {0};
    int a;
    bool close = false, valid;
    if (!s || !id)
        return EMUIF_INVALID;
    if (s->busy)
        return EMUIF_BUSY;
    e = PREFIX(request);
    if (e)
        return e;
    e = PREFIX(response);
    if (e)
        return e;
    if (request->reserved || response->reserved || !request->bytes || !request->byte_count ||
        request->byte_count > 65535 || !response->bytes)
        return EMUIF_INVALID;
    if (response->capacity < 65536)
    {
        response->required = 65536;
        response->written = 0;
        return EMUIF_CAPACITY;
    }
    c = client(s, id);
    if (!c)
        return EMUIF_NOT_FOUND;
    if (c->control3 != control3)
        return EMUIF_VERSION;
    d = malloc(sizeof *d);
    body = calloc(1, sizeof *body);
    out = calloc(1, sizeof *out);
    if (!d || !body || !out)
    {
        free(d);
        free(body);
        free(out);
        return EMUIF_CAPACITY;
    }
    s->busy = true;
    s->request_fault = false;
    response->written = 0;
    response->required = 0;
    valid = json_document_parse(d, (const char *)request->bytes, request->byte_count, true);
    if (!valid)
    {
        /* Recover only from a syntactically valid envelope, never guess IDs. */
        if (!json_document_parse(d, (const char *)request->bytes, request->byte_count, false))
        {
            close_client(s, c);
            goto done;
        }
    }
    if (!json_integer(d, json_member(d, 0, "id"), 1, UINT32_MAX, &rid) ||
        !json_string(d, json_member(d, 0, "command"), cmd, sizeof cmd) ||
        !(control3 ? wire3_command(cmd) : wire_command(cmd)))
    {
        close_client(s, c);
        goto done;
    }
    a = json_member(d, 0, "arguments");
    if (!valid || d->tokens[0].size != 8 ||
        !wire_string_is(d, json_member(d, 0, "type"), "request") ||
        !(control3 ? (wire3_arguments(d, a, cmd) && wire3_semantics(d, a, cmd))
                   : wire_arguments(d, a, cmd)))
    {
        e = EMUIF_INVALID;
        close = true;
    }
    else if ((uint32_t)rid <= c->last_id || (!c->last_id && rid != 1))
    {
        e = EMUIF_INVALID;
        close = true;
    }
    else
    {
        c->last_id = (uint32_t)rid;
        e = control3 ? dispatch3(s, c, d, a, cmd, body, &close)
                     : dispatch(s, c, d, a, cmd, body, &close);
    }
    if (!e && !strcmp(cmd, "terminate") && !body->length)
    {
        unsigned i;
        for (i = 0; i < 8; i++)
            close_client(s, &s->clients[i]);
        goto done;
    }
    if (body->failed || body->length > 64000)
        e = EMUIF_CAPACITY;
    output_format(out, "{\"type\":\"response\",\"id\":%" PRId64 ",\"command\":", rid);
    output_string(out, cmd);
    if (e)
    {
        output_format(
            out,
            ",\"success\":false,\"error\":{\"code\":\"%s\",\"message\":\"%s%s\",\"retryable\":%s}}",
            status_name(s->request_fault ? EMUIF_FAULT : e),
            s->request_fault ? "session fault: " : "request: ",
            status_name(s->request_fault ? s->cause : e),
            e == EMUIF_BUSY && !(s->request_fault) ? "true" : "false");
    }
    else
    {
        output_raw(out, ",\"success\":true,\"body\":");
        body->data[body->length] = 0;
        output_raw(out, body->data);
        output_raw(out, "}");
    }
    if (!out->failed && out->length <= 65535)
    {
        memcpy(response->bytes, out->data, out->length);
        response->written = (uint32_t)out->length;
        response->required = response->written;
    }
    else
    {
        close = true;
    }
    if (!e && !strcmp(cmd, "terminate"))
    {
        unsigned i;
        for (i = 0; i < 8; i++)
            close_client(s, &s->clients[i]);
    }
    else if (close)
        close_client(s, c);
done:
    s->busy = false;
    free(d);
    free(body);
    free(out);
    return EMUIF_OK;
}

emuif_status emusa80535_debug_request_v1(struct emuif_session *s, uint64_t id,
                                         const struct emuif_json_request *request,
                                         struct emuif_json_response *response)
{
    return request_profile(s, id, request, response, false);
}
static emuif_status control3_request(struct emuif_session *s, uint64_t id,
                                     const struct emuif_json_request *request,
                                     struct emuif_json_response *response)
{
    return request_profile(s, id, request, response, true);
}
static emuif_status control3_open(struct emuif_session *s, const struct emuif_client *c)
{
    emuif_status e = emusa80535_debug_open_v1(s, c);
    if (!e)
        client(s, c->connection_id)->control3 = true;
    return e;
}
emuif_status emusa80535_control_query_v3(struct emuif_control3_api *out)
{
    emuif_status e = PREFIX(out);
    if (e)
        return e;
    out->open = control3_open;
    out->request = control3_request;
    out->close = emusa80535_debug_close_v1;
    out->advance = control3_advance;
    return EMUIF_OK;
}

/* The trace table is discoverable while later trace/capture/file operations
 * remain explicitly unavailable. Outcomes are trusted serialized owner reads. */
static emuif_status trace_replace_unavailable(struct emuif_session *s,
    const struct emuif_trace_config *c, struct emuif_trace_revision *r)
{ (void)s; (void)c; (void)r; return EMUIF_UNSUPPORTED; }
static emuif_status trace_page_unavailable(struct emuif_session *s,
    const struct emuif_trace_page_request *q, struct emuif_trace_page *p)
{ (void)s; (void)q; (void)p; return EMUIF_UNSUPPORTED; }
static emuif_status trace_capture_unavailable(struct emuif_session *s,
    const struct emuif_capture_request *q, struct emuif_capture_status *p)
{ (void)s; (void)q; (void)p; return EMUIF_UNSUPPORTED; }
static emuif_status trace_sink_unavailable(struct emuif_session *s,
    const struct emuif_sink_request *q, struct emuif_sink_status *p)
{ (void)s; (void)q; (void)p; return EMUIF_UNSUPPORTED; }
static emuif_status trace_outcomes(struct emuif_session *s,
    const struct emuif_trace_page_request *q, struct emuif_trace_page *p)
{
    if (!s) return EMUIF_INVALID;
    if (s->busy) return EMUIF_BUSY;
    if ((s->observation_coverage & 112u) != 112u) return EMUIF_UNSUPPORTED;
    if (s->state == EMUIF_STATE_TERMINATED) return EMUIF_STATE;
    return emu_outcome_page(&s->outcomes, s->id, q, p);
}
emuif_status emusa80535_trace_query_v1(struct emuif_trace_api *out)
{
    emuif_status e = PREFIX(out);
    if (e) return e;
    out->replace = trace_replace_unavailable;
    out->page = trace_page_unavailable;
    out->outcomes = trace_outcomes;
    out->capture_start = trace_capture_unavailable;
    out->sink_open = trace_sink_unavailable;
    return EMUIF_OK;
}

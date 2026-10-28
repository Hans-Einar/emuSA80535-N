/* Opaque CPU API1 adapter over the preserved emu8051 engine.
 * Copyright 2006 Jari Komppa; Copyright 2026 Hans-Einar. MIT; see LICENSE. */
#include "emu_decode_internal.h"
#include "emu_cpu_observation_internal.h"
#include <emusa80535/contracts/cpu.h>
#include <emusa80535/emu8051.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Observation state is private to this opaque CPU and never reachable through
 * borrowed immutable records. IDs and retired totals survive resets/rebinding. */
struct observed_frame {
    uint64_t id, parent;
    uint16_t return_pc;
    uint8_t sp, kind, irq_source;
};
struct observed_uart {
    uint64_t id, parent, order, scheduled, start, instruction, access;
    uint16_t value;
    bool started;
};
struct cpu_observation_state {
    struct emu_cpu_raw_observer raw;
    struct emuif_observer sink;
    uint64_t generation, sequence, next_instruction, next_access, next_context, next_frame;
    uint64_t retired, instruction, begin, delivery_parent, delivery_order, delivery_cycle;
    uint64_t current_access, attempted_cycle, unbound_cycle, unbound_instructions;
    bool access_old_known;
    uint8_t access_old;
    uint32_t access_kind, access_space;
    uint16_t access_address;
    uint16_t pc, next_pc;
    uint8_t bytes[3], byte_mask, depth, irq_depth;
    bool bound, pending, opcode_done, closing, fault_emitted, unknown_base;
    struct observed_frame frames[64];
    struct observed_uart rx, tx, hold;
};

struct emuif_cpu {
    struct em8051 core; /* First member: native callbacks recover this owner. */
    struct cpu_observation_state observation;
    uint8_t code[65536];
    struct emu_decode_cache decode_cache;
    struct emuif_cpu_hooks hooks;
    uint64_t sequence;
    uint64_t input_order; /* Last order at input_cycle. */
    uint64_t input_cycle;
    uint64_t input_max_order;
    uint64_t input_reset_floor;
    bool input_seen;
    uint64_t tx_start;
    uint16_t observed_pc;
    uint8_t observed_size;
    uint8_t port_latches[6];
    emuif_status fault;
    bool busy;
    bool loaded;
    bool instruction_pending;
};

static const uint8_t port_addresses[6] = {0x80, 0x90, 0xa0, 0xb0, 0xe8, 0xf8};

static emuif_status prefix(const void *record, uint32_t minimum) {
    uint32_t size;
    uint16_t version, flags;
    if (!record)
        return EMUIF_INVALID;
    memcpy(&size, record, 4);
    if (size < 8)
        return EMUIF_INVALID;
    memcpy(&version, (const uint8_t *)record + 4, 2);
    memcpy(&flags, (const uint8_t *)record + 6, 2);
    if (version != EMUIF_API_VERSION)
        return EMUIF_VERSION;
    if (flags != 0 || size < minimum)
        return EMUIF_INVALID;
    return EMUIF_OK;
}
#define PREFIX(p) prefix((p), (uint32_t)sizeof(*(p)))
#define INIT(p)                                                                                    \
    do {                                                                                           \
        memset((p), 0, sizeof(*(p)));                                                              \
        (p)->struct_size = sizeof(*(p));                                                           \
        (p)->api_version = EMUIF_API_VERSION;                                                      \
    } while (0)

static emuif_status available(const struct emuif_cpu *cpu) {
    if (!cpu)
        return EMUIF_INVALID;
    if (cpu->busy)
        return EMUIF_BUSY;
    return EMUIF_OK;
}

static bool abort_execution(const struct em8051 *core, void *context) {
    const struct emuif_cpu *cpu = context;
    (void)core;
    return cpu->fault != EMUIF_OK;
}

static void fail(struct emuif_cpu *cpu, emuif_status status) {
    if (status != EMUIF_OK && cpu->fault == EMUIF_OK)
        cpu->fault = status > EMUIF_NOT_FOUND ? EMUIF_FAULT : status;
}

static uint32_t obs_failure_reason(emuif_status status) {
    return status == EMUIF_UNSUPPORTED ? EMUIF_OBS_REASON_UNSUPPORTED :
           status == EMUIF_EXHAUSTED ? EMUIF_OBS_REASON_EXHAUSTED : EMUIF_OBS_REASON_DEVICE;
}
static bool obs_id(struct emuif_cpu *cpu, uint64_t *counter, uint64_t *value) {
    if (*counter == UINT64_MAX) { fail(cpu, EMUIF_EXHAUSTED); return false; }
    *value = ++*counter;
    return true;
}
static void obs_init(struct emuif_cpu *cpu, struct emuif_observation *e,
                     uint32_t kind, uint32_t phase, uint64_t cycle) {
    struct cpu_observation_state *o = &cpu->observation;
    INIT(e); INIT(&e->old_value); INIT(&e->new_value);
    e->old_value.width = e->new_value.width = 8;
    e->generation = o->generation;
    e->cycle = cycle; e->kind = kind; e->phase = phase;
    e->legacy_instructions = cpu->core.mInstructionCount;
    e->retired_instructions = o->retired;
    e->instruction_id = o->pending ? o->instruction : 0;
    e->begin_cycle = o->pending ? o->begin : 0;
    e->pc = o->pending ? o->pc : cpu->core.mPC;
    e->known = EMUIF_OBS_KNOWN_PC;
    e->irq_depth = o->irq_depth;
}
static void obs_emit(struct emuif_cpu *cpu, struct emuif_observation *e) {
    struct cpu_observation_state *o = &cpu->observation;
    bool busy;
    if (!o->bound || !obs_id(cpu, &o->sequence, &e->sequence)) return;
    busy = cpu->busy; cpu->busy = true;
    o->sink.observe(o->sink.context, e);
    cpu->busy = busy;
}
static void obs_bytes(struct emuif_cpu *cpu, struct emuif_observation *e) {
    struct cpu_observation_state *o = &cpu->observation;
    while (e->byte_count < 3 && (o->byte_mask & (1u << e->byte_count))) {
        e->bytes[e->byte_count] = o->bytes[e->byte_count]; ++e->byte_count;
    }
    (void)em8051_decode_captured(&cpu->core, o->pc, e->bytes, e->byte_count, e->text);
}
static void obs_uart_event(struct emuif_cpu *cpu, struct observed_uart *f,
                           uint32_t kind, uint32_t reason, uint64_t cycle,
                           uint64_t displaced) {
    struct emuif_observation e;
    bool rx = kind >= EMUIF_OBS_RX_START && kind <= EMUIF_OBS_RX_END;
    if (!f->id) return;
    obs_init(cpu, &e, kind,
             kind == EMUIF_OBS_TX_HOLD || kind == EMUIF_OBS_TX_REPLACE ?
             EMUIF_OBS_PHASE_EFFECT : EMUIF_OBS_PHASE_PERIPHERAL, cycle);
    e.frame_id = f->id; e.related_frame_id = displaced; e.reason = reason;
    e.known |= EMUIF_OBS_KNOWN_UART_STATE | EMUIF_OBS_KNOWN_TIMING;
    e.uart_mode = cpu->core.mSFR[REG_SCON] >> 6;
    e.uart_width = e.uart_mode >= 2 ? 9 : 8; e.ninth = (f->value >> 8) & 1;
    e.scon = cpu->core.mSFR[REG_SCON]; e.tcon = cpu->core.mSFR[REG_TCON];
    e.start_cycle = f->start; e.scheduled_cycle = f->scheduled;
    if (rx) {
        e.parent_stimulus_id = f->parent; e.cpu_input_order = f->order; e.input_stage = 1;
    }
    if (kind == EMUIF_OBS_TX_HOLD || kind == EMUIF_OBS_TX_REPLACE) {
        e.instruction_id = f->instruction; e.access_id = f->access;
    }
    if (kind == EMUIF_OBS_RX_ACCEPT || kind == EMUIF_OBS_RX_START ||
        kind == EMUIF_OBS_TX_HOLD || kind == EMUIF_OBS_TX_REPLACE ||
        kind == EMUIF_OBS_TX_START) {
        e.new_value.known = 1; e.new_value.width = 16; e.new_value.value = f->value;
    }
    obs_emit(cpu, &e);
}
static bool obs_uart_new(struct emuif_cpu *cpu, struct observed_uart *f,
                          uint16_t value) {
    struct cpu_observation_state *o = &cpu->observation;
    memset(f, 0, sizeof(*f));
    if (!obs_id(cpu, &o->next_frame, &f->id)) return false;
    f->value = value;
    return true;
}
static void obs_context_push(struct emuif_cpu *cpu, struct emuif_observation *e,
                              uint8_t kind, uint16_t target, uint16_t return_pc,
                              uint8_t sp_before, uint8_t sp_after, unsigned source) {
    struct cpu_observation_state *o = &cpu->observation;
    struct observed_frame *f;
    e->frame_kind = kind; e->target = target; e->return_pc = return_pc;
    e->sp_before = sp_before; e->sp_after = sp_after;
    e->known |= EMUIF_OBS_KNOWN_TARGET;
    if (o->depth == 64) {
        e->reason = EMUIF_OBS_REASON_EXHAUSTED;
        /* None of the retained ancestors remains a usable top frame after
         * an unrepresented push. A later fully observed child may establish
         * its own known extent above this unknown base, never recover it. */
        o->unknown_base = true; o->depth = 0;
        (void)obs_id(cpu, &o->next_context, &e->context_id);
        e->sp_before = e->sp_after = 0;
        return;
    }
    f = &o->frames[o->depth]; memset(f, 0, sizeof(*f));
    if (!obs_id(cpu, &o->next_context, &f->id)) return;
    /* Entry itself supplies all facts needed for this new frame. Parent0
     * means no known parent, including when unknown_base survives a gap,
     * mismatch or overflow. Popping this frame cannot recover that ancestry. */
    f->parent = o->depth ? o->frames[o->depth - 1].id : 0;
    f->return_pc = return_pc; f->sp = sp_after; f->kind = kind; f->irq_source = kind == EMUIF_OBS_FRAME_IRQ ? (uint8_t)source : 0;
    e->context_id = f->id; e->parent_context_id = f->parent;
    e->known |= EMUIF_OBS_KNOWN_CONTEXT; ++o->depth;
}
static uint64_t obs_raw_fact(void *context, unsigned kind, uint64_t cycle,
                             uint64_t a, uint32_t b, uint32_t c, uint32_t d) {
    struct emuif_cpu *cpu = context;
    struct cpu_observation_state *o = &cpu->observation;
    struct emuif_observation e;
    uint64_t id = 0;
    if (!o->bound) return 0;
    switch (kind) {
    case EMU_RAW_BEGIN:
        if (!obs_id(cpu, &o->next_instruction, &o->instruction)) return 0;
        o->pending = true; o->opcode_done = false; o->closing = false;
        o->pc = (uint16_t)a; o->begin = cycle; o->attempted_cycle = cycle; o->byte_mask = 1;
        memset(o->bytes, 0, sizeof(o->bytes)); o->bytes[0] = (uint8_t)b;
        obs_init(cpu, &e, EMUIF_OBS_INSTRUCTION_BEGIN, EMUIF_OBS_PHASE_PRE, cycle);
        obs_bytes(cpu, &e); obs_emit(cpu, &e); break;
    case EMU_RAW_OPERAND:
        if (o->pending && a < 3 && !(o->byte_mask & (1u << a))) {
            o->bytes[a] = (uint8_t)b; o->byte_mask |= (uint8_t)(1u << a);
        }
        break;
    case EMU_RAW_CYCLE_BEGIN: o->attempted_cycle = cycle; break;
    case EMU_RAW_OPCODE_END:
        if (o->pending && !cpu->fault) { o->opcode_done = true; o->next_pc = (uint16_t)a; }
        break;
    case EMU_RAW_CONTEXT_PREFLIGHT:
        if (o->next_context == UINT64_MAX) { fail(cpu, EMUIF_EXHAUSTED); return 0; }
        return 1;
    case EMU_RAW_CALL:
    case EMU_RAW_IRQ_ENTER:
        obs_init(cpu, &e, kind == EMU_RAW_CALL ? EMUIF_OBS_CALL : EMUIF_OBS_IRQ_ENTER,
                 kind == EMU_RAW_CALL ? EMUIF_OBS_PHASE_EFFECT : EMUIF_OBS_PHASE_BOUNDARY, cycle);
        obs_context_push(cpu, &e, kind == EMU_RAW_CALL ? EMUIF_OBS_FRAME_CALL : EMUIF_OBS_FRAME_IRQ,
                         kind == EMU_RAW_CALL ? (uint16_t)a : cpu->core.mPC,
                         (uint16_t)b, (uint8_t)c, (uint8_t)d, (unsigned)a);
        if (!e.context_id) break; /* Exhaustion is a fault, never a malformed control fact. */
        if (kind == EMU_RAW_IRQ_ENTER) e.irq_source = (uint32_t)a;
        obs_emit(cpu, &e);
        if (kind == EMU_RAW_IRQ_ENTER && o->irq_depth < 255) ++o->irq_depth;
        break;
    case EMU_RAW_RET:
    case EMU_RAW_RETI: {
        uint8_t expected = kind == EMU_RAW_RET ? EMUIF_OBS_FRAME_CALL : EMUIF_OBS_FRAME_IRQ;
        obs_init(cpu, &e, kind == EMU_RAW_RET ? EMUIF_OBS_RET : EMUIF_OBS_RETI,
                 EMUIF_OBS_PHASE_EFFECT, cycle);
        e.frame_kind = expected; e.target = e.return_pc = (uint16_t)a;
        e.known |= EMUIF_OBS_KNOWN_TARGET;
        if (o->depth) {
            struct observed_frame *f = &o->frames[o->depth - 1];
            e.context_id = f->id; e.parent_context_id = f->parent;
            e.sp_before = (uint8_t)c; e.sp_after = (uint8_t)d;
            e.known |= EMUIF_OBS_KNOWN_CONTEXT;
            if (f->kind != expected || f->return_pc != a || f->sp != c ||
                (uint8_t)(f->sp - 2) != d) {
                e.reason = EMUIF_OBS_REASON_CONTEXT;
                o->unknown_base = true; o->depth = 0;
                e.context_id = 0;
                if (!obs_id(cpu, &o->next_context, &e.context_id)) break;
                e.parent_context_id = 0; e.sp_before = e.sp_after = 0;
                e.known &= ~EMUIF_OBS_KNOWN_CONTEXT;
            } else o->closing = true;
            if (expected == EMUIF_OBS_FRAME_IRQ && f->kind == EMUIF_OBS_FRAME_IRQ) e.irq_source = f->irq_source;
        } else {
            if (!obs_id(cpu, &o->next_context, &e.context_id)) break;
            e.reason = EMUIF_OBS_REASON_CONTEXT; o->unknown_base = true; }
        obs_emit(cpu, &e);
        if (expected == EMUIF_OBS_FRAME_IRQ && o->irq_depth) --o->irq_depth;
        break;
    }
    case EMU_RAW_IRQ_REQUEST:
        obs_init(cpu, &e, EMUIF_OBS_IRQ_REQUEST, EMUIF_OBS_PHASE_PERIPHERAL, cycle);
        e.irq_source = (uint32_t)a; obs_emit(cpu, &e); break;
    case EMU_RAW_TX_HOLD: {
        uint64_t displaced = o->hold.id;
        if (!obs_uart_new(cpu, &o->hold, (uint16_t)(a | ((uint16_t)b << 8)))) break;
        o->hold.instruction = o->pending ? o->instruction : 0;
        o->hold.access = o->current_access;
        o->hold.scheduled = cycle;
        obs_uart_event(cpu, &o->hold, displaced ? EMUIF_OBS_TX_REPLACE : EMUIF_OBS_TX_HOLD,
                       displaced ? EMUIF_OBS_REASON_REPLACED : 0, cycle, displaced); break;
    }
    case EMU_RAW_TX_START:
        o->tx = o->hold; memset(&o->hold, 0, sizeof(o->hold));
        o->tx.start = cycle; o->tx.started = true;
        obs_uart_event(cpu, &o->tx, EMUIF_OBS_TX_START, 0, cycle, 0); break;
    case EMU_RAW_TX_STOP: obs_uart_event(cpu, &o->tx, EMUIF_OBS_TX_STOP, 0, cycle, 0); break;
    case EMU_RAW_TX_TI: obs_uart_event(cpu, &o->tx, EMUIF_OBS_TX_TI, 0, cycle, 0); break;
    case EMU_RAW_TX_END:
        obs_uart_event(cpu, &o->tx, EMUIF_OBS_TX_END, 0, cycle, 0);
        memset(&o->tx, 0, sizeof(o->tx)); break;
    case EMU_RAW_RX_START:
        if (!obs_uart_new(cpu, &o->rx, (uint16_t)(a | ((uint16_t)b << 8)))) break;
        o->rx.parent = o->delivery_parent; o->rx.order = o->delivery_order;
        o->rx.scheduled = o->delivery_cycle; o->rx.start = cycle; o->rx.started = true;
        obs_uart_event(cpu, &o->rx, EMUIF_OBS_RX_START, 0, cycle, 0); break;
    case EMU_RAW_RX_ACCEPT: obs_uart_event(cpu, &o->rx, EMUIF_OBS_RX_ACCEPT, 0, cycle, 0); break;
    case EMU_RAW_RX_DROP: obs_uart_event(cpu, &o->rx, EMUIF_OBS_RX_DROP, (uint32_t)a, cycle, 0); break;
    case EMU_RAW_RX_END:
        obs_uart_event(cpu, &o->rx, EMUIF_OBS_RX_END, 0, cycle, 0);
        memset(&o->rx, 0, sizeof(o->rx)); break;
    case EMU_RAW_ACCESS_BEGIN:
        if (b == EMUIF_SPACE_SFR && a == 0x99 &&
            (cpu->core.mSFR[REG_SCON] & 0xc0) == 0xc0 && o->next_frame == UINT64_MAX) {
            fail(cpu, EMUIF_EXHAUSTED); break;
        }
        if (o->current_access && o->access_space == b && o->access_address == a)
            return o->current_access;
        if (!o->pending || !obs_id(cpu, &o->next_access, &id)) break;
        o->current_access = id; o->access_kind = c; o->access_space = b;
        o->access_address = (uint16_t)a; o->access_old_known = false;
        obs_init(cpu, &e, EMUIF_OBS_ACCESS_ATTEMPT, EMUIF_OBS_PHASE_EFFECT, cycle);
        e.access_id = id; e.space = b; e.address = (uint16_t)a; e.access = c;
        if (c == EMUIF_OBS_ACCESS_WRITE) { e.new_value.known = 1; e.new_value.value = d; }
        obs_emit(cpu, &e); return id;
    case EMU_RAW_ACCESS_OLD:
        if (a && a == o->current_access) { o->access_old = (uint8_t)b; o->access_old_known = true; }
        break;
    case EMU_RAW_ACCESS_END:
        if (!a) break;
        obs_init(cpu, &e, EMUIF_OBS_ACCESS_RESULT, EMUIF_OBS_PHASE_EFFECT, cycle);
        e.access_id = a; e.address = (uint16_t)b; e.space = (c >> 16) & 255;
        e.access = a == o->current_access ? o->access_kind : c & 255;
        if (a == o->current_access && o->access_old_known) {
            e.old_value.known = 1; e.old_value.value = o->access_old;
        }
        e.status = cpu->fault; e.partial_effects = cpu->fault != 0;
        if (cpu->fault) e.reason = obs_failure_reason(cpu->fault);
        else { e.new_value.known = 1; e.new_value.value = d; }
        obs_emit(cpu, &e); o->current_access = 0; break;
    default: break;
    }
    return 0;
}
static void obs_finish(struct emuif_cpu *cpu) {
    struct cpu_observation_state *o = &cpu->observation;
    struct emuif_observation e;
    if (!o->bound) return;
    if (o->pending && (cpu->fault || (o->opcode_done && cpu->core.mTickDelay == 0))) {
        if (!cpu->fault && o->retired == UINT64_MAX) fail(cpu, EMUIF_EXHAUSTED);
        if (!cpu->fault) ++o->retired;
        obs_init(cpu, &e, cpu->fault ? EMUIF_OBS_INSTRUCTION_ABORT : EMUIF_OBS_INSTRUCTION_RETIRE,
                 cpu->fault ? EMUIF_OBS_PHASE_BOUNDARY : EMUIF_OBS_PHASE_RETIRE,
                 cpu->fault ? o->attempted_cycle : cpu->core.mMachineCycleCount);
        e.status = cpu->fault; e.partial_effects = cpu->fault != 0;
        if (cpu->fault) e.reason = obs_failure_reason(cpu->fault);
        if (o->opcode_done) { e.next_pc = o->next_pc; e.known |= EMUIF_OBS_KNOWN_NEXT_PC; }
        obs_bytes(cpu, &e); obs_emit(cpu, &e);
        if (!cpu->fault && o->closing && o->depth) {
            --o->depth;
        }
        o->pending = false; o->closing = false;
    }
    if (cpu->fault && !o->fault_emitted) {
        obs_init(cpu, &e, EMUIF_OBS_FAULT, EMUIF_OBS_PHASE_BOUNDARY, o->attempted_cycle);
        e.status = cpu->fault; e.partial_effects = 1; e.reason = obs_failure_reason(cpu->fault);
        o->fault_emitted = true; obs_emit(cpu, &e);
    }
}
static void obs_reset(struct emuif_cpu *cpu) {
    struct cpu_observation_state *o = &cpu->observation;
    uint64_t cycle = cpu->core.mMachineCycleCount;
    if (o->bound) {
        if (o->pending) {
            struct emuif_observation e;
            obs_init(cpu, &e, EMUIF_OBS_INSTRUCTION_ABORT, EMUIF_OBS_PHASE_BOUNDARY, cycle);
            e.status = EMUIF_STATE; e.reason = EMUIF_OBS_REASON_RESET; e.partial_effects = 1;
            if (o->opcode_done) { e.next_pc = o->next_pc; e.known |= EMUIF_OBS_KNOWN_NEXT_PC; }
            obs_bytes(cpu, &e); obs_emit(cpu, &e); o->pending = false;
        }
        obs_uart_event(cpu, &o->rx, EMUIF_OBS_RX_END, EMUIF_OBS_REASON_RESET, cycle, 0);
        obs_uart_event(cpu, &o->tx, EMUIF_OBS_TX_END, EMUIF_OBS_REASON_RESET, cycle, 0);
        obs_uart_event(cpu, &o->hold, EMUIF_OBS_TX_END, EMUIF_OBS_REASON_RESET, cycle, 0);
    }
    memset(&o->rx, 0, sizeof(o->rx)); memset(&o->tx, 0, sizeof(o->tx)); memset(&o->hold, 0, sizeof(o->hold));
    o->pending = false; o->closing = false; o->depth = o->irq_depth = 0;
    o->unknown_base = o->fault_emitted = false; o->current_access = 0;
    o->unbound_cycle = o->unbound_instructions = o->attempted_cycle = 0;
}

static bool sequence(struct emuif_cpu *cpu, uint64_t *out) {
    if (cpu->sequence == UINT64_MAX) {
        fail(cpu, EMUIF_EXHAUSTED);
        return false;
    }
    *out = ++cpu->sequence;
    return true;
}

static void capture_access(struct emuif_cpu *cpu, uint16_t address, uint32_t direction,
                           uint8_t value, struct emuif_bus_access *access) {
    unsigned p;
    INIT(access);
    access->cycle = cpu->core.mMachineCycleCount;
    access->pc = cpu->core.mPC;
    access->address = address;
    access->direction = direction;
    access->value = value;
    for (p = 0; p < 6; p++)
        access->port_latches[p] = p < 4 || cpu->core.mVariant == EM8051_VARIANT_SAB80535
                                      ? cpu->core.mSFR[port_addresses[p] - 0x80]
                                      : 0;
}

static uint8_t bus(struct emuif_cpu *cpu, uint16_t address, uint32_t direction, uint8_t value) {
    struct emuif_bus_access access;
    struct emuif_bus_result result;
    emuif_status status;
    uint64_t observed_access;
    uint32_t observed_direction = direction == EMUIF_ACCESS_READ ?
                                 EMUIF_OBS_ACCESS_READ : EMUIF_OBS_ACCESS_WRITE;
    uint8_t returned = 0;
    if (cpu->fault != EMUIF_OK) return 0;
    observed_access = emu_cpu_raw(&cpu->core, EMU_RAW_ACCESS_BEGIN, cpu->core.mMachineCycleCount,
                                   address, EMUIF_SPACE_XDATA, observed_direction, value);
    if (cpu->fault) return 0;
    capture_access(cpu, address, direction, value, &access);
    if (!sequence(cpu, &access.sequence)) goto done;
    INIT(&result);
    status = cpu->hooks.bus_access(cpu->hooks.context, &access, &result);
    if (status != EMUIF_OK) { fail(cpu, status); goto done; }
    if (PREFIX(&result) != EMUIF_OK || result.reserved[0] || result.reserved[1] ||
        result.reserved[2] || (direction == EMUIF_ACCESS_WRITE && result.value != value)) {
        fail(cpu, EMUIF_FAULT); goto done;
    }
    returned = result.value;
done:
    (void)emu_cpu_raw(&cpu->core, EMU_RAW_ACCESS_END, cpu->core.mMachineCycleCount,
                       observed_access, address, (EMUIF_SPACE_XDATA << 16) | observed_direction,
                       returned);
    return returned;
}

static uint8_t read_bus(struct em8051 *core, uint16_t address) {
    return bus((struct emuif_cpu *)core, address, EMUIF_ACCESS_READ, 0);
}
static void write_bus(struct em8051 *core, uint16_t address, uint8_t value) {
    (void)bus((struct emuif_cpu *)core, address, EMUIF_ACCESS_WRITE, value);
}
static void port_write(struct em8051 *core, uint8_t address) {
    struct emuif_cpu *cpu = (struct emuif_cpu *)core;
    struct emuif_port_output output;
    unsigned p;
    if (cpu->fault != EMUIF_OK)
        return;
    for (p = 0; p < 6 && port_addresses[p] != address; p++) {
    }
    if (p == 6) {
        fail(cpu, EMUIF_FAULT);
        return;
    }
    INIT(&output);
    if (!sequence(cpu, &output.sequence))
        return;
    output.cycle = core->mMachineCycleCount;
    output.port = p;
    output.previous_latch = cpu->port_latches[p];
    output.latch = core->mSFR[address - 0x80];
    output.changed_mask = (uint8_t)(output.previous_latch ^ output.latch);
    cpu->port_latches[p] = output.latch;
    fail(cpu, cpu->hooks.port_output(cpu->hooks.context, &output));
}

static void uart_trace(const struct em8051_sab_uart_trace_record *record, void *context) {
    struct emuif_cpu *cpu = context;
    struct emuif_signal_output output;
    if (cpu->fault != EMUIF_OK)
        return;
    if (record->event == EM8051_SAB_UART_TRACE_TX_START) {
        cpu->tx_start = record->machine_cycle;
        return;
    }
    if (record->event != EM8051_SAB_UART_TRACE_TX_END)
        return;
    INIT(&output);
    if (!sequence(cpu, &output.sequence))
        return;
    output.cycle = record->machine_cycle;
    output.start_cycle = cpu->tx_start;
    output.kind = EMUIF_OUTPUT_UART_FRAME;
    output.value = record->data | (record->ninth_bit ? 256u : 0u);
    output.width = 9;
    fail(cpu, cpu->hooks.signal_output(cpu->hooks.context, &output));
}

static void instruction_trace(const struct em8051_trace_record *record, void *context) {
    struct emuif_cpu *cpu = context;
    char text[EM8051_DEBUG_DECODE_TEXT_SIZE];
    if (record->type != EM8051_TRACE_INSTRUCTION)
        return;
    cpu->observed_pc = record->pc;
    cpu->observed_size = emu_decode_at(&cpu->core, record->pc, text);
    cpu->instruction_pending = true;
}
static void exception(struct em8051 *core, int code) {
    (void)code;
    fail((struct emuif_cpu *)core, EMUIF_FAULT);
}

static void reset_private(struct emuif_cpu *cpu) {
    unsigned p;
    cpu->fault = EMUIF_OK;
    cpu->input_reset_floor = cpu->input_max_order;
    cpu->input_seen = false;
    cpu->loaded = false;
    cpu->instruction_pending = false;
    cpu->tx_start = 0;
    emu_decode_clear(&cpu->decode_cache);
    for (p = 0; p < 6; p++)
        cpu->port_latches[p] = cpu->core.mSFR[port_addresses[p] - 0x80];
}

static emuif_status create_cpu(const struct emuif_cpu_config *config,
                               const struct emuif_cpu_hooks *hooks, struct emuif_cpu **out) {
    emuif_status status;
    struct emuif_cpu *cpu;
    unsigned p;
    if (!out)
        return EMUIF_INVALID;
    *out = NULL;
    if ((status = PREFIX(config)) != EMUIF_OK || (status = PREFIX(hooks)) != EMUIF_OK)
        return status;
    if (config->variant > EMUIF_VARIANT_SAB80535)
        return EMUIF_UNSUPPORTED;
    if (config->oscillator_hz < 1 || config->oscillator_hz > 100000000)
        return EMUIF_RANGE;
    if (!hooks->bus_access || !hooks->port_output || !hooks->signal_output)
        return EMUIF_INVALID;
    cpu = calloc(1, sizeof(*cpu));
    if (!cpu)
        return EMUIF_FAULT;
    cpu->hooks = *hooks;
    cpu->core.mCodeMem = cpu->code;
    cpu->core.mCodeMemMaxIdx = 65535;
    if (em8051_init_variant(&cpu->core, (enum em8051_variant)config->variant)) {
        free(cpu);
        return EMUIF_UNSUPPORTED;
    }
    cpu->core.mOscillatorHz = config->oscillator_hz;
    cpu->core.xread = read_bus;
    cpu->core.xwrite = write_bus;
    cpu->core.except = exception;
    for (p = 0; p < (config->variant == EMUIF_VARIANT_SAB80535 ? 6u : 4u); p++)
        cpu->core.sfrwrite[port_addresses[p] - 0x80] = port_write;
    em8051_set_execution_abort(&cpu->core, abort_execution, cpu);
    em8051_set_trace(&cpu->core, instruction_trace, cpu);
    em8051_set_sab_uart_trace(&cpu->core, uart_trace, cpu);
    reset_private(cpu);
    *out = cpu;
    return EMUIF_OK;
}
static void destroy_cpu(struct emuif_cpu *cpu) {
    if (!cpu)
        return;
    if (cpu->busy) {
        fail(cpu, EMUIF_FAULT);
        return;
    }
    memset(cpu, 0, sizeof(*cpu));
    free(cpu);
}
static emuif_status reset_cpu(struct emuif_cpu *cpu, const struct emuif_reset *request) {
    emuif_status status = available(cpu);
    if (status != EMUIF_OK)
        return status;
    if ((status = PREFIX(request)) != EMUIF_OK)
        return status;
    if (request->reserved)
        return EMUIF_INVALID;
    cpu->busy = true;
    obs_reset(cpu);
    cpu->fault = EMUIF_OK;
    em8051_set_reset_seed(&cpu->core, request->seed);
    reset(&cpu->core, true);
    cpu->core.mPC = request->entry;
    reset_private(cpu);
    cpu->busy = false;
    return EMUIF_OK;
}
static emuif_status load_code(struct emuif_cpu *cpu, const uint8_t *code, uint32_t bytes) {
    emuif_status status = available(cpu);
    if (status != EMUIF_OK)
        return status;
    if (!code)
        return EMUIF_INVALID;
    if (bytes != sizeof(cpu->code))
        return EMUIF_RANGE;
    if (cpu->fault || cpu->core.mMachineCycleCount || cpu->core.mTickDelay)
        return EMUIF_STATE;
    memmove(cpu->code, code, sizeof(cpu->code));
    emu_decode_clear(&cpu->decode_cache);
    cpu->loaded = true;
    return EMUIF_OK;
}

static void obs_rx_rejected(struct emuif_cpu *cpu, const struct emuif_input *input,
                             emuif_status status) {
    struct observed_uart rejected;
    struct cpu_observation_state *o = &cpu->observation;
    if (!o->bound || !obs_uart_new(cpu, &rejected, (uint16_t)input->value)) return;
    rejected.parent = o->delivery_order == input->order ? o->delivery_parent : 0;
    rejected.order = input->order; rejected.scheduled = input->cycle;
    obs_uart_event(cpu, &rejected, EMUIF_OBS_RX_DROP,
                   status == EMUIF_BUSY ? EMUIF_OBS_REASON_BUSY : EMUIF_OBS_REASON_REN,
                   cpu->core.mMachineCycleCount, 0);
    obs_uart_event(cpu, &rejected, EMUIF_OBS_RX_END,
                   status == EMUIF_BUSY ? EMUIF_OBS_REASON_BUSY : EMUIF_OBS_REASON_REN,
                   cpu->core.mMachineCycleCount, 0);
}
static emuif_status apply_input(struct emuif_cpu *cpu, const struct emuif_input *input) {
    emuif_status status = available(cpu);
    bool applied = false;
    if (status != EMUIF_OK)
        return status;
    if ((status = PREFIX(input)) != EMUIF_OK)
        return status;
    if (cpu->fault)
        return EMUIF_STATE;
    if (!input->order)
        return EMUIF_INVALID;
    if (input->order <= cpu->input_reset_floor ||
        (cpu->input_seen && input->cycle == cpu->input_cycle && input->order <= cpu->input_order) ||
        input->cycle < cpu->core.mMachineCycleCount)
        return EMUIF_STALE;
    if (input->cycle != cpu->core.mMachineCycleCount)
        return EMUIF_RANGE;
    if (input->kind > EMUIF_INPUT_RELEASE)
        return EMUIF_INVALID;
    if (cpu->core.mVariant != EM8051_VARIANT_SAB80535)
        return EMUIF_UNSUPPORTED;
    switch (input->kind) {
    case EMUIF_INPUT_PORT:
        if (input->index > 5 || input->value > 255 || input->mask > 255)
            return EMUIF_RANGE;
        if (input->index == 0 || input->index == 2)
            return EMUIF_UNSUPPORTED;
        applied = em8051_sab_port_drive(&cpu->core, port_addresses[input->index],
                                        (uint8_t)input->mask, (uint8_t)input->value);
        break;
    case EMUIF_INPUT_INTERRUPT:
        if (input->index > 6 || input->value > 1 || input->mask != 1)
            return EMUIF_RANGE;
        applied = em8051_sab_external_drive(
            &cpu->core, (enum em8051_sab_external_source)input->index, input->value != 0);
        break;
    case EMUIF_INPUT_ADC:
        if (input->index > 7 || input->value > 65535 || input->mask != 65535)
            return EMUIF_RANGE;
        applied =
            em8051_sab_adc_set_input(&cpu->core, (uint8_t)input->index, (uint16_t)input->value);
        break;
    case EMUIF_INPUT_UART_RX:
        if (input->index || input->value > 511 || input->mask != 511)
            return EMUIF_RANGE;
        if ((cpu->core.mSFR[REG_SCON] & 0xc0) != 0xc0)
            return EMUIF_UNSUPPORTED;
        if (cpu->observation.bound && cpu->observation.next_frame == UINT64_MAX)
            return EMUIF_EXHAUSTED;
        if (!(cpu->core.mSFR[REG_SCON] & SCONMASK_REN)) {
            obs_rx_rejected(cpu, input, EMUIF_STATE); return EMUIF_STATE;
        }
        if (cpu->core.mSABUartRxActive) {
            obs_rx_rejected(cpu, input, EMUIF_BUSY); return EMUIF_BUSY;
        }
        {
            struct cpu_observation_state *o = &cpu->observation;
            bool direct = o->delivery_order == 0;
            if (direct) { o->delivery_order = input->order; o->delivery_cycle = input->cycle; }
            applied = em8051_sab_uart_inject_rx_frame(&cpu->core, (uint8_t)input->value,
                                                      (input->value & 256) != 0);
            if (direct) o->delivery_order = o->delivery_cycle = 0;
        }
        break;
    case EMUIF_INPUT_RELEASE:
        if (input->value)
            return EMUIF_RANGE;
        if (input->index <= 5) {
            if (input->mask > 255)
                return EMUIF_RANGE;
            if (input->index == 0 || input->index == 2)
                return EMUIF_UNSUPPORTED;
            applied = em8051_sab_port_release(&cpu->core, port_addresses[input->index],
                                              (uint8_t)input->mask);
        } else if (input->index >= 256 && input->index <= 262 && input->mask == 1) {
            applied = em8051_sab_external_release(
                &cpu->core, (enum em8051_sab_external_source)(input->index - 256));
        } else
            return EMUIF_RANGE;
        break;
    default:
        return EMUIF_INVALID;
    }
    if (!applied)
        return EMUIF_UNSUPPORTED;
    cpu->input_order = input->order;
    cpu->input_cycle = input->cycle;
    cpu->input_seen = true;
    if (input->order > cpu->input_max_order)
        cpu->input_max_order = input->order;
    return EMUIF_OK;
}

static void cycle_result(const struct emuif_cpu *cpu, struct emuif_cycle_result *out,
                         uint32_t progressed) {
    uint32_t size = out->struct_size;
    INIT(out);
    out->struct_size = size;
    out->cycle = cpu->core.mMachineCycleCount;
    out->instructions = cpu->core.mInstructionCount;
    out->progressed = progressed;
    out->safe_boundary = !cpu->fault && cpu->core.mTickDelay == 0;
    out->idle = (cpu->core.mSFR[REG_PCON] & 1) != 0;
    out->power_down = (cpu->core.mSFR[REG_PCON] & 2) != 0;
}
static emuif_status cycle_cpu(struct emuif_cpu *cpu, struct emuif_cycle_result *out) {
    emuif_status status = available(cpu);
    uint64_t before;
    if (status != EMUIF_OK)
        return status;
    if ((status = PREFIX(out)) != EMUIF_OK)
        return status;
    if (!cpu->loaded || cpu->fault)
        return EMUIF_STATE;
    if ((cpu->core.mSFR[REG_PCON] & 2) && cpu->core.mTickDelay == 0) {
        cycle_result(cpu, out, 0);
        return EMUIF_UNSUPPORTED;
    }
    if (cpu->core.mMachineCycleCount == UINT64_MAX || cpu->core.mInstructionCount == UINT64_MAX) {
        fail(cpu, EMUIF_EXHAUSTED);
        obs_finish(cpu);
        return EMUIF_EXHAUSTED;
    }
    before = cpu->core.mMachineCycleCount;
    {
        unsigned p;
        for (p = 0; p < 6; p++)
            cpu->port_latches[p] = cpu->core.mSFR[port_addresses[p] - 0x80];
    }
    cpu->busy = true;
    (void)tick(&cpu->core);
    obs_finish(cpu);
    cpu->busy = false;
    if (cpu->fault)
        return cpu->fault;
    if (cpu->instruction_pending && cpu->core.mTickDelay == 0) {
        emu_decode_completed(&cpu->decode_cache, cpu->observed_pc, cpu->observed_size,
                             cpu->core.mPC);
        cpu->instruction_pending = false;
    }
    cycle_result(cpu, out, cpu->core.mMachineCycleCount != before);
    return EMUIF_OK;
}
static emuif_status snapshot_cpu(const struct emuif_cpu *cpu, struct emuif_snapshot *out) {
    emuif_status status = available(cpu);
    struct emuif_snapshot result;
    unsigned i, bank;
    if (status != EMUIF_OK)
        return status;
    if ((status = PREFIX(out)) != EMUIF_OK)
        return status;
    if (out->reserved)
        return EMUIF_INVALID;
    INIT(&result);
    result.struct_size = out->struct_size;
    result.cycle = cpu->core.mMachineCycleCount;
    result.instructions = cpu->core.mInstructionCount;
    result.state = cpu->fault ? EMUIF_STATE_FAULTED : EMUIF_STATE_PAUSED;
    result.variant = (uint32_t)cpu->core.mVariant;
    result.pc = cpu->core.mPC;
    result.a = cpu->core.mSFR[REG_ACC];
    result.b = cpu->core.mSFR[REG_B];
    result.psw = cpu->core.mSFR[REG_PSW];
    result.sp = cpu->core.mSFR[REG_SP];
    result.dptr = (uint16_t)(((uint16_t)cpu->core.mSFR[REG_DPH] << 8) | cpu->core.mSFR[REG_DPL]);
    bank = (result.psw >> 3) & 3;
    for (i = 0; i < 8; i++)
        result.r[i] = cpu->core.mLowerData[bank * 8 + i];
    result.idle = (cpu->core.mSFR[REG_PCON] & 1) != 0;
    result.power_down = (cpu->core.mSFR[REG_PCON] & 2) != 0;
    result.safe_boundary = !cpu->fault && cpu->core.mTickDelay == 0;
    *out = result;
    return EMUIF_OK;
}

static bool supported_sfr(enum em8051_variant variant, uint32_t address) {
    switch (address) {
    case 0x80:
    case 0x81:
    case 0x82:
    case 0x83:
    case 0x87:
    case 0x88:
    case 0x89:
    case 0x8a:
    case 0x8b:
    case 0x8c:
    case 0x8d:
    case 0x90:
    case 0x98:
    case 0x99:
    case 0xa0:
    case 0xa8:
    case 0xb0:
    case 0xb8:
    case 0xd0:
    case 0xe0:
    case 0xf0:
        return true;
    case 0xc8:
    case 0xca:
    case 0xcb:
    case 0xcc:
    case 0xcd:
        return variant != EM8051_VARIANT_8051;
    case 0xa9:
    case 0xb9:
    case 0xc0:
    case 0xc1:
    case 0xd8:
    case 0xd9:
    case 0xda:
    case 0xdb:
    case 0xe8:
    case 0xf8:
        return variant == EM8051_VARIANT_SAB80535;
    default:
        return false;
    }
}
static emuif_status peek_cpu(const struct emuif_cpu *cpu,
                             const struct emuif_memory_request *request, uint8_t *out,
                             uint32_t capacity) {
    emuif_status status = available(cpu);
    uint32_t end, i;
    uint8_t staged[EMUIF_MAX_MEMORY_BYTES];
    if (status != EMUIF_OK)
        return status;
    if ((status = PREFIX(request)) != EMUIF_OK)
        return status;
    if (request->reserved || (request->count && !out))
        return EMUIF_INVALID;
    if (request->count > EMUIF_MAX_MEMORY_BYTES || request->count > capacity)
        return EMUIF_CAPACITY;
    if (cpu->fault || cpu->core.mTickDelay)
        return EMUIF_STATE;
    if (request->space > EMUIF_SPACE_XDATA)
        return EMUIF_INVALID;
    if (request->space == EMUIF_SPACE_XDATA)
        return EMUIF_UNSUPPORTED;
    if (request->address > 65535 || request->count > 65536 - request->address)
        return EMUIF_RANGE;
    end = request->address + request->count;
    if (request->space == EMUIF_SPACE_CODE && !cpu->loaded)
        return EMUIF_STATE;
    if (request->space == EMUIF_SPACE_IRAM &&
        (request->address >= (cpu->core.mUpperData ? 256u : 128u) ||
         end > (cpu->core.mUpperData ? 256u : 128u)))
        return EMUIF_RANGE;
    if (request->space == EMUIF_SPACE_SFR &&
        (request->address < 128 || request->address > 255 || end > 256))
        return EMUIF_RANGE;
    for (i = 0; i < request->count; i++) {
        uint32_t address = request->address + i;
        if (request->space == EMUIF_SPACE_CODE)
            staged[i] = cpu->code[address];
        else if (request->space == EMUIF_SPACE_IRAM)
            staged[i] =
                address < 128 ? cpu->core.mLowerData[address] : cpu->core.mUpperData[address - 128];
        else {
            if (!supported_sfr(cpu->core.mVariant, address))
                return EMUIF_UNSUPPORTED;
            staged[i] = cpu->core.mSFR[address - 128];
        }
    }
    if (request->count)
        memcpy(out, staged, request->count);
    return EMUIF_OK;
}
static emuif_status decode_cpu(struct emuif_cpu *cpu, const struct emuif_decode_request *request,
                               struct emuif_decoded *out, uint32_t element_size,
                               uint32_t capacity) {
    emuif_status status = available(cpu);
    struct em8051_debug_decoded records[EMUIF_MAX_DECODE_RECORDS];
    enum em8051_debug_status decoded;
    uint32_t i;
    if (status != EMUIF_OK)
        return status;
    if ((status = PREFIX(request)) != EMUIF_OK)
        return status;
    if (!out || request->reserved || !request->count || request->count > EMUIF_MAX_DECODE_RECORDS)
        return EMUIF_INVALID;
    if (element_size < sizeof(*out) || capacity < request->count ||
        (uint64_t)element_size * request->count > SIZE_MAX)
        return EMUIF_CAPACITY;
    if (cpu->fault || cpu->core.mTickDelay || !cpu->loaded)
        return EMUIF_STATE;
    for (i = 0; i < request->count; i++) {
        const uint8_t *bytes = (const uint8_t *)out + (size_t)element_size * i;
        struct emuif_decoded element;
        if ((status = prefix(bytes, sizeof(element))) != EMUIF_OK)
            return status;
        memcpy(&element, bytes, sizeof(element));
        if (element.struct_size > element_size || element.reserved[0] || element.reserved[1] ||
            element.reserved[2])
            return EMUIF_INVALID;
    }
    decoded =
        emu_decode_window(&cpu->core, &cpu->decode_cache, request->address, request->byte_offset,
                          request->instruction_offset, request->count, records);
    if (decoded != EM8051_DEBUG_OK) {
        return decoded == EM8051_DEBUG_RANGE              ? EMUIF_RANGE
               : decoded == EM8051_DEBUG_INVALID_ARGUMENT ? EMUIF_INVALID
                                                          : EMUIF_FAULT;
    }
    for (i = 0; i < request->count; i++) {
        uint8_t *bytes = (uint8_t *)out + (size_t)element_size * i;
        uint32_t size;
        struct emuif_decoded element;
        memcpy(&size, bytes, sizeof(size));
        INIT(&element);
        element.struct_size = size;
        element.address = records[i].address;
        element.size = records[i].size;
        element.valid = records[i].valid;
        memcpy(element.text, records[i].text, sizeof(records[i].text));
        memcpy(bytes, &element, sizeof(element));
    }
    return EMUIF_OK;
}

emuif_status emusa80535_cpu_query_v1(struct emuif_cpu_api *out) {
    emuif_status status = PREFIX(out);
    struct emuif_cpu_api api;
    if (status != EMUIF_OK)
        return status;
    INIT(&api);
    api.struct_size = out->struct_size;
    api.capabilities = EMUIF_CPU_CAP_PORT_LATCH | EMUIF_CPU_CAP_UART_FRAME;
    api.create = create_cpu;
    api.destroy = destroy_cpu;
    api.reset = reset_cpu;
    api.load_code = load_code;
    api.apply_input = apply_input;
    api.cycle = cycle_cpu;
    api.snapshot = snapshot_cpu;
    api.peek = peek_cpu;
    api.decode = decode_cpu;
    *out = api;
    return EMUIF_OK;
}

#ifdef EMU_CPU_FACADE_TESTING
/* Deliberately absent from production objects and all SDK headers. */
void emu_cpu_test_limits(struct emuif_cpu *cpu, uint64_t cycles, uint64_t instructions,
                         uint64_t last_sequence) {
    cpu->core.mMachineCycleCount = cycles;
    cpu->core.mInstructionCount = instructions;
    cpu->sequence = last_sequence;
}
void emu_cpu_test_observation_limits(struct emuif_cpu *cpu, uint64_t frame,
                                     uint64_t access) {
    cpu->observation.next_frame = frame;
    cpu->observation.next_access = access;
}
void emu_cpu_test_context_limit(struct emuif_cpu *cpu, uint64_t context) {
    cpu->observation.next_context = context;
}
#endif

/* Sized extension records use the shared prefix; unknown tails are untouched. */
static emuif_status observation_prefix(const void *record, uint32_t size) {
    return prefix(record, size);
}
static emuif_status bind_observation(struct emuif_cpu *cpu,
                                     const struct emuif_observation_binding *binding) {
    emuif_status status = available(cpu);
    struct cpu_observation_state *o;
    if (status != EMUIF_OK) return status;
    if ((status = observation_prefix(binding, sizeof(*binding))) != EMUIF_OK ||
        (status = observation_prefix(&binding->observer, sizeof(binding->observer))) != EMUIF_OK)
        return status;
    if (!binding->generation || !binding->observer.observe) return EMUIF_INVALID;
    if (cpu->core.mVariant != EM8051_VARIANT_SAB80535) return EMUIF_UNSUPPORTED;
    o = &cpu->observation;
    if (o->bound) return EMUIF_BUSY;
    if (cpu->fault || cpu->core.mTickDelay || cpu->core.mSABUartTxActive ||
        cpu->core.mSABUartTxPending || cpu->core.mSABUartRxActive) return EMUIF_STATE;
    if (cpu->core.mMachineCycleCount != o->unbound_cycle ||
        cpu->core.mInstructionCount != o->unbound_instructions) {
        o->unknown_base = true; o->depth = 0; o->irq_depth = cpu->core.mSABIrqDepth;
    }
    o->raw.context = cpu; o->raw.fact = obs_raw_fact;
    o->generation = binding->generation; o->sink = binding->observer; o->bound = true;
    cpu->core.mObservation = &o->raw;
    return EMUIF_OK;
}
static emuif_status unbind_observation(struct emuif_cpu *cpu) {
    emuif_status status = available(cpu);
    if (status != EMUIF_OK) return status;
    if (!cpu->observation.bound) return EMUIF_OK;
    if (cpu->core.mTickDelay || cpu->observation.pending) return EMUIF_STATE;
    cpu->core.mObservation = NULL;
    cpu->observation.unbound_cycle = cpu->core.mMachineCycleCount;
    cpu->observation.unbound_instructions = cpu->core.mInstructionCount;
    cpu->observation.bound = false;
    memset(&cpu->observation.sink, 0, sizeof(cpu->observation.sink));
    return EMUIF_OK;
}
static emuif_status deliver_observation(struct emuif_cpu *cpu,
                                        const struct emuif_observed_input *envelope) {
    emuif_status status = available(cpu);
    struct cpu_observation_state *o;
    if (status != EMUIF_OK) return status;
    if ((status = observation_prefix(envelope, sizeof(*envelope))) != EMUIF_OK ||
        (status = observation_prefix(&envelope->input, sizeof(envelope->input))) != EMUIF_OK)
        return status;
    o = &cpu->observation;
    if (!o->bound) return EMUIF_STATE;
    o->delivery_parent = envelope->parent_stimulus_id;
    o->delivery_order = envelope->input.order;
    o->delivery_cycle = envelope->input.cycle;
    status = apply_input(cpu, &envelope->input);
    o->delivery_parent = o->delivery_order = o->delivery_cycle = 0;
    return status;
}
emuif_status emu_cpu_observation_lifecycle(struct emuif_cpu *cpu, uint32_t kind,
                                           emuif_status status) {
    struct emuif_observation e;
    emuif_status ready = available(cpu);
    if (ready != EMUIF_OK) return ready;
    if (!cpu->observation.bound) return EMUIF_STATE;
    if ((kind != EMUIF_OBS_RESET && kind != EMUIF_OBS_LOAD && kind != EMUIF_OBS_FAULT) ||
        status > EMUIF_NOT_FOUND || (kind == EMUIF_OBS_FAULT ? status == EMUIF_OK : status != EMUIF_OK))
        return EMUIF_INVALID;
    if (kind == EMUIF_OBS_FAULT && cpu->observation.fault_emitted) return EMUIF_OK;
    obs_init(cpu, &e, kind, EMUIF_OBS_PHASE_BOUNDARY, cpu->core.mMachineCycleCount);
    e.status = status;
    if (kind == EMUIF_OBS_FAULT) {
        e.partial_effects = 1; e.reason = EMUIF_OBS_REASON_DEVICE;
        cpu->observation.fault_emitted = true;
    }
    obs_emit(cpu, &e);
    return EMUIF_OK;
}
emuif_status emusa80535_cpu_observation_query_v1(struct emuif_observation_api *out) {
    emuif_status status = observation_prefix(out, sizeof(*out));
    struct emuif_observation_api api;
    if (status != EMUIF_OK) return status;
    INIT(&api); api.struct_size = out->struct_size;
    api.coverage = EMUIF_OBS_COVER_INSTRUCTION | EMUIF_OBS_COVER_CONTROL |
                   EMUIF_OBS_COVER_IRQ | EMUIF_OBS_COVER_LIFECYCLE |
                   EMUIF_OBS_COVER_INPUT | EMUIF_OBS_COVER_UART;
    api.bind = bind_observation; api.unbind = unbind_observation; api.deliver = deliver_observation;
    *out = api;
    return EMUIF_OK;
}

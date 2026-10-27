/* Opaque CPU API1 adapter over the preserved emu8051 engine.
 * Copyright 2006 Jari Komppa; Copyright 2026 Hans-Einar. MIT; see LICENSE. */
#include "emu_decode_internal.h"
#include <emusa80535/contracts/cpu.h>
#include <emusa80535/emu8051.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct emuif_cpu {
    struct em8051 core; /* First member: native callbacks recover this owner. */
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
    if (cpu->fault != EMUIF_OK)
        return 0;
    capture_access(cpu, address, direction, value, &access);
    if (!sequence(cpu, &access.sequence))
        return 0;
    INIT(&result);
    status = cpu->hooks.bus_access(cpu->hooks.context, &access, &result);
    if (status != EMUIF_OK) {
        fail(cpu, status);
        return 0;
    }
    if (PREFIX(&result) != EMUIF_OK || result.reserved[0] || result.reserved[1] ||
        result.reserved[2] || (direction == EMUIF_ACCESS_WRITE && result.value != value)) {
        fail(cpu, EMUIF_FAULT);
        return 0;
    }
    return result.value;
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
        if (!(cpu->core.mSFR[REG_SCON] & SCONMASK_REN))
            return EMUIF_STATE;
        if (cpu->core.mSABUartRxActive)
            return EMUIF_BUSY;
        applied = em8051_sab_uart_inject_rx_frame(&cpu->core, (uint8_t)input->value,
                                                  (input->value & 256) != 0);
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
#endif

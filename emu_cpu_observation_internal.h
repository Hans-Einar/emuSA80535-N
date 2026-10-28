/* Private same-build production seams. Copyright 2026 Hans-Einar. MIT. */
#ifndef EMU_CPU_OBSERVATION_INTERNAL_H
#define EMU_CPU_OBSERVATION_INTERNAL_H
#include <emusa80535/emu8051.h>
#include <emusa80535/contracts/observation.h>
#include "emu_cpu_lifecycle_internal.h"
/* Raw hooks contain no public ABI or controller policy. Absent hooks cost one
 * pointer check and never change architectural reads or callback order. */
enum emu_cpu_raw_fact {
    EMU_RAW_BEGIN, EMU_RAW_OPERAND, EMU_RAW_OPCODE_END, EMU_RAW_CYCLE_BEGIN,
    EMU_RAW_CONTEXT_PREFLIGHT, EMU_RAW_CALL, EMU_RAW_RET, EMU_RAW_RETI, EMU_RAW_IRQ_ENTER,
    EMU_RAW_IRQ_REQUEST, EMU_RAW_TX_HOLD, EMU_RAW_TX_START,
    EMU_RAW_TX_STOP, EMU_RAW_TX_TI, EMU_RAW_TX_END,
    EMU_RAW_RX_START, EMU_RAW_RX_ACCEPT, EMU_RAW_RX_DROP, EMU_RAW_RX_END,
    EMU_RAW_ACCESS_BEGIN, EMU_RAW_ACCESS_OLD, EMU_RAW_ACCESS_END
};
struct emu_cpu_raw_observer {
    void *context;
    uint64_t (*fact)(void *, unsigned, uint64_t, uint64_t, uint32_t, uint32_t, uint32_t);
};
static inline uint64_t emu_cpu_raw(struct em8051 *cpu, unsigned kind,
                                  uint64_t cycle, uint64_t a, uint32_t b,
                                  uint32_t c, uint32_t d) {
    struct emu_cpu_raw_observer *o = cpu->mObservation;
    return o ? o->fact(o->context, kind, cycle, a, b, c, d) : 0;
}
static inline uint8_t emu_cpu_operand(struct em8051 *cpu, unsigned offset) {
    uint8_t value = cpu->mCodeMem[(cpu->mPC + offset) & cpu->mCodeMemMaxIdx];
    if (cpu->mObservation)
        (void)emu_cpu_raw(cpu, EMU_RAW_OPERAND, cpu->mMachineCycleCount,
                          offset, value, 0, 0);
    return value;
}
/* Emit only newly asserted interrupt-request bits at the real mutation site,
 * never by polling the controller's cached pending bitmap. */
static inline void emu_cpu_irq_write(struct em8051 *cpu, uint8_t index,
                                     uint8_t value, uint64_t cycle) {
    uint8_t old = cpu->mSFR[index];
    unsigned source;
    cpu->mSFR[index] = value;
    if (!cpu->mObservation || cpu->mVariant != EM8051_VARIANT_SAB80535) return;
    for (source = 0; source < 12; ++source) {
        uint8_t reg, mask;
        if (source < 4) {
            reg = REG_TCON;
            mask = (uint8_t)(1u << (source == 0 ? 1 : source == 1 ? 5 : source == 2 ? 3 : 7));
        } else if (source == 4) { reg = REG_SCON; mask = SCONMASK_RI | SCONMASK_TI; }
        else { reg = EM8051_SAB_SFR_IRCON - 128;
            mask = source == 5 ? SAB_IRCONMASK_TF2 | SAB_IRCONMASK_EXF2 :
                   (uint8_t)(1u << (source - 6)); }
        if (index == reg && !(old & mask) && (value & mask))
            (void)emu_cpu_raw(cpu, EMU_RAW_IRQ_REQUEST, cycle, source, 0, 0, 0);
    }
}
/* Owner calls after rebinding the new generation, or once for a board fault.
 * CPU reset itself ends old UART frames before clearing architectural state. */
uint8_t em8051_decode_captured(const struct em8051 *, uint16_t pc,
                              const uint8_t bytes[3], uint8_t count, char text[97]);
#endif

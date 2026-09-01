/* 8051 emulator core
 * Copyright 2006 Jari Komppa
 *
 * Permission is hereby granted, free of charge, to any person obtaining 
 * a copy of this software and associated documentation files (the 
 * "Software"), to deal in the Software without restriction, including 
 * without limitation the rights to use, copy, modify, merge, publish, 
 * distribute, sublicense, and/or sell copies of the Software, and to 
 * permit persons to whom the Software is furnished to do so, subject 
 * to the following conditions: 
 *
 * The above copyright notice and this permission notice shall be included 
 * in all copies or substantial portions of the Software. 
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
 * IN THE SOFTWARE. 
 *
 * (i.e. the MIT License)
 *
 * core.c
 * General emulation functions
 */

#define T0_MODE3_MASK (TMODMASK_M0_0 | TMODMASK_M1_0)

#include <stdlib.h>
#include <string.h>
#include "emu8051.h"

#define SAB_SFR_INDEX(aAddress) ((uint8_t)((aAddress) - 0x80u))
#define SAB_IRQ_BIT(aSource) ((uint16_t)(1u << (unsigned)(aSource)))

static const uint16_t gSABIrqVectors[EM8051_SAB_IRQ_SOURCE_COUNT] =
{
    EM8051_SAB_VECTOR_INT0,
    EM8051_SAB_VECTOR_TIMER0,
    EM8051_SAB_VECTOR_INT1,
    EM8051_SAB_VECTOR_TIMER1,
    EM8051_SAB_VECTOR_UART,
    EM8051_SAB_VECTOR_TIMER2,
    EM8051_SAB_VECTOR_ADC,
    EM8051_SAB_VECTOR_INT2,
    EM8051_SAB_VECTOR_INT3,
    EM8051_SAB_VECTOR_INT4,
    EM8051_SAB_VECTOR_INT5,
    EM8051_SAB_VECTOR_INT6
};

static const struct em8051_variant_descriptor gVariants[] =
{
    {
        EM8051_VARIANT_8051, "8051", 12000000u, false,
        EM8051_CLASSIC_SFR_IE, EM8051_CLASSIC_SFR_IP,
        EM8051_SFR_UNAVAILABLE, EM8051_SFR_UNAVAILABLE
    },
    {
        EM8051_VARIANT_8052, "8052", 12000000u, true,
        EM8051_CLASSIC_SFR_IE, EM8051_CLASSIC_SFR_IP,
        EM8051_SFR_UNAVAILABLE, EM8051_SFR_UNAVAILABLE
    },
    {
        EM8051_VARIANT_SAB80535, "SAB80535", 11059200u, true,
        EM8051_SAB_SFR_IEN0, EM8051_SAB_SFR_IP0,
        EM8051_SAB_SFR_IEN1, EM8051_SAB_SFR_IP1
    }
};

static uint32_t reset_random(uint32_t *aState)
{
    /* A local xorshift generator avoids process-global rand() state. */
    uint32_t state = *aState;
    if (state == 0)
        state = 0x6D2B79F5u;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    *aState = state;
    return state;
}

const struct em8051_variant_descriptor *em8051_get_variant_descriptor(
    enum em8051_variant aVariant)
{
    if (aVariant < EM8051_VARIANT_8051 || aVariant > EM8051_VARIANT_SAB80535)
        return NULL;
    return &gVariants[aVariant];
}

void em8051_set_reset_seed(struct em8051 *aCPU, uint32_t aSeed)
{
    aCPU->mResetSeed = aSeed;
    aCPU->mResetSeedConfigured = true;
}

int em8051_init_variant(struct em8051 *aCPU, enum em8051_variant aVariant)
{
    const struct em8051_variant_descriptor *descriptor;
    if (!aCPU)
        return -1;
    descriptor = em8051_get_variant_descriptor(aVariant);
    if (!descriptor)
        return -1;

    aCPU->mVariant = aVariant;
    aCPU->mOscillatorHz = descriptor->default_oscillator_hz;
    if (descriptor->has_upper_iram)
        aCPU->mUpperData = aCPU->mOwnedUpperData;
    else
        aCPU->mUpperData = NULL;
    em8051_set_reset_seed(aCPU, 0x80535u);
    reset(aCPU, true);
    return 0;
}

void em8051_set_trace(struct em8051 *aCPU, em8051trace aTrace, void *aUser)
{
    aCPU->trace = aTrace;
    aCPU->trace_user = aUser;
}

void em8051_set_sab_irq_trace(struct em8051 *aCPU,
                              em8051sabirqtrace aTrace, void *aUser)
{
    if (!aCPU)
        return;
    aCPU->sab_irq_trace = aTrace;
    aCPU->sab_irq_trace_user = aUser;
}

void em8051_set_timer_overflow_callback(struct em8051 *aCPU,
                                        em8051timeroverflow aCallback,
                                        void *aUser)
{
    if (!aCPU)
        return;
    aCPU->timer_overflow = aCallback;
    aCPU->timer_overflow_user = aUser;
}

void em8051_set_sab_uart_trace(struct em8051 *aCPU,
                               em8051sabuarttrace aTrace, void *aUser)
{
    if (!aCPU)
        return;
    aCPU->sab_uart_trace = aTrace;
    aCPU->sab_uart_trace_user = aUser;
}

void em8051_trace_emit(struct em8051 *aCPU, enum em8051_trace_type aType,
                       uint16_t aAddress, uint8_t aValue)
{
    struct em8051_trace_record record;
    if (!aCPU->trace)
        return;

    record.type = aType;
    record.machine_cycle = aCPU->mMachineCycleCount;
    record.pc = aCPU->mInInstruction ? aCPU->mTracePC : aCPU->mPC;
    record.address = aAddress;
    record.value = aValue;
    aCPU->trace(&record, aCPU->trace_user);
}

static void sab_irq_sync(struct em8051 *aCPU)
{
    uint16_t pending = 0;
    uint16_t enabled = 0;
    uint8_t tcon = aCPU->mSFR[REG_TCON];
    uint8_t scon = aCPU->mSFR[REG_SCON];
    uint8_t ien0 = aCPU->mSFR[SAB_SFR_INDEX(EM8051_SAB_SFR_IEN0)];
    uint8_t ien1 = aCPU->mSFR[SAB_SFR_INDEX(EM8051_SAB_SFR_IEN1)];
    uint8_t ircon = aCPU->mSFR[SAB_SFR_INDEX(EM8051_SAB_SFR_IRCON)];
    unsigned source;

    if (tcon & TCONMASK_IE0)
        pending |= SAB_IRQ_BIT(EM8051_SAB_IRQ_INT0);
    if (tcon & TCONMASK_TF0)
        pending |= SAB_IRQ_BIT(EM8051_SAB_IRQ_TIMER0);
    if (tcon & TCONMASK_IE1)
        pending |= SAB_IRQ_BIT(EM8051_SAB_IRQ_INT1);
    if (tcon & TCONMASK_TF1)
        pending |= SAB_IRQ_BIT(EM8051_SAB_IRQ_TIMER1);
    if (scon & (SCONMASK_RI | SCONMASK_TI))
        pending |= SAB_IRQ_BIT(EM8051_SAB_IRQ_UART);
    if (ircon & (SAB_IRCONMASK_TF2 | SAB_IRCONMASK_EXF2))
        pending |= SAB_IRQ_BIT(EM8051_SAB_IRQ_TIMER2);
    if (ircon & SAB_IRCONMASK_IADC)
        pending |= SAB_IRQ_BIT(EM8051_SAB_IRQ_ADC);
    if (ircon & SAB_IRCONMASK_IEX2)
        pending |= SAB_IRQ_BIT(EM8051_SAB_IRQ_INT2);
    if (ircon & SAB_IRCONMASK_IEX3)
        pending |= SAB_IRQ_BIT(EM8051_SAB_IRQ_INT3);
    if (ircon & SAB_IRCONMASK_IEX4)
        pending |= SAB_IRQ_BIT(EM8051_SAB_IRQ_INT4);
    if (ircon & SAB_IRCONMASK_IEX5)
        pending |= SAB_IRQ_BIT(EM8051_SAB_IRQ_INT5);
    if (ircon & SAB_IRCONMASK_IEX6)
        pending |= SAB_IRQ_BIT(EM8051_SAB_IRQ_INT6);

    for (source = EM8051_SAB_IRQ_INT0;
         source <= EM8051_SAB_IRQ_UART; source++)
    {
        if (ien0 & (uint8_t)(1u << source))
            enabled |= SAB_IRQ_BIT(source);
    }
    if ((ien0 & IEMASK_ET2) || (ien1 & SAB_IEN1MASK_EXEN2))
        enabled |= SAB_IRQ_BIT(EM8051_SAB_IRQ_TIMER2);
    for (source = EM8051_SAB_IRQ_ADC;
         source < EM8051_SAB_IRQ_SOURCE_COUNT; source++)
    {
        if (ien1 & (uint8_t)(1u << (source - EM8051_SAB_IRQ_ADC)))
            enabled |= SAB_IRQ_BIT(source);
    }

    aCPU->mSABIrqPending = pending;
    aCPU->mSABIrqEnabled = enabled;
}

static void sab_irq_trace_emit(struct em8051 *aCPU,
                               enum em8051_sab_irq_trace_event aEvent,
                               enum em8051_sab_irq_source aSource,
                               uint8_t aPriority, bool aAsserted,
                               uint16_t aPendingSnapshot)
{
    struct em8051_sab_irq_trace_record record;
    if (!aCPU->sab_irq_trace)
        return;

    record.event = aEvent;
    record.machine_cycle = aCPU->mMachineCycleCount;
    record.pc = aCPU->mInInstruction ? aCPU->mTracePC : aCPU->mPC;
    record.source = aSource;
    record.priority = aPriority;
    record.asserted = aAsserted;
    record.global_enabled =
        (aCPU->mSFR[SAB_SFR_INDEX(EM8051_SAB_SFR_IEN0)] & IEMASK_EA) != 0;
    record.pending_mask = aPendingSnapshot;
    record.enabled_mask = aCPU->mSABIrqEnabled;
    record.in_service_mask = aCPU->mSABIrqInService;
    record.in_service_depth = aCPU->mSABIrqDepth;
    aCPU->sab_irq_trace(&record, aCPU->sab_irq_trace_user);
}

static void sab_irq_arm_inhibit(struct em8051 *aCPU)
{
    /* An instruction performing the write consumes the first count itself.
     * A host-side boundary write starts directly with the one instruction
     * that must execute before the next arbitration. */
    aCPU->mSABIrqInhibitInstructions = aCPU->mInInstruction ? 2u : 1u;
}

static uint8_t sab_irq_priority(const struct em8051 *aCPU,
                                enum em8051_sab_irq_source aSource)
{
    unsigned pair = (unsigned)aSource;
    uint8_t bit;
    uint8_t ip0;
    uint8_t ip1;
    if (pair >= (unsigned)EM8051_SAB_IRQ_ADC)
        pair -= (unsigned)EM8051_SAB_IRQ_ADC;
    bit = (uint8_t)(1u << pair);
    ip0 = aCPU->mSFR[SAB_SFR_INDEX(EM8051_SAB_SFR_IP0)];
    ip1 = aCPU->mSFR[SAB_SFR_INDEX(EM8051_SAB_SFR_IP1)];
    return (uint8_t)(((ip0 & bit) ? 1u : 0u) |
                     ((ip1 & bit) ? 2u : 0u));
}

static bool sab_uart_mode3(const struct em8051 *aCPU)
{
    return aCPU->mVariant == EM8051_VARIANT_SAB80535 &&
           (aCPU->mSFR[REG_SCON] & (SCONMASK_SM0 | SCONMASK_SM1)) ==
               (SCONMASK_SM0 | SCONMASK_SM1) &&
           (aCPU->mSFR[SAB_SFR_INDEX(EM8051_SAB_SFR_ADCON)] &
            SAB_ADCONMASK_BD) == 0;
}

static void sab_uart_trace_emit(struct em8051 *aCPU,
                                enum em8051_sab_uart_trace_event aEvent,
                                uint64_t aMachineCycle, uint8_t aData,
                                bool aNinthBit, uint8_t aBitIndex,
                                bool aBitValue)
{
    struct em8051_sab_uart_trace_record record;
    if (!aCPU->sab_uart_trace)
        return;

    record.event = aEvent;
    record.machine_cycle = aMachineCycle;
    record.pc = aCPU->mPC;
    record.data = aData;
    record.ninth_bit = aNinthBit;
    record.bit_index = aBitIndex;
    record.bit_value = aBitValue;
    aCPU->sab_uart_trace(&record, aCPU->sab_uart_trace_user);
}

static bool sab_uart_divider_step(uint8_t *aPhase, bool aSmod)
{
    *aPhase = (uint8_t)((*aPhase + 1u) & 0x1fu);
    return *aPhase == 0u || (aSmod && *aPhase == 16u);
}

static bool sab_uart_tx_bit_value(const struct em8051 *aCPU,
                                  uint8_t aBitIndex)
{
    if (aBitIndex == 0u)
        return false;
    if (aBitIndex <= 8u)
        return ((aCPU->mSABUartTxData >> (aBitIndex - 1u)) & 1u) != 0;
    if (aBitIndex == 9u)
        return aCPU->mSABUartTxNinth;
    return true;
}

static void sab_uart_tx_start(struct em8051 *aCPU, uint64_t aMachineCycle)
{
    aCPU->mSABUartTxData = aCPU->mSABUartTxPendingData;
    aCPU->mSABUartTxNinth = aCPU->mSABUartTxPendingNinth;
    aCPU->mSABUartTxPending = false;
    aCPU->mSABUartTxActive = true;
    aCPU->mSABUartTxBitIndex = 0;
    sab_uart_trace_emit(aCPU, EM8051_SAB_UART_TRACE_TX_START,
                        aMachineCycle, aCPU->mSABUartTxData,
                        aCPU->mSABUartTxNinth, 0, false);
}

static void sab_uart_tx_boundary(struct em8051 *aCPU,
                                 uint64_t aMachineCycle)
{
    if (!aCPU->mSABUartTxActive)
    {
        if (aCPU->mSABUartTxPending)
            sab_uart_tx_start(aCPU, aMachineCycle);
        return;
    }

    if (aCPU->mSABUartTxBitIndex < 9u)
    {
        aCPU->mSABUartTxBitIndex++;
        sab_uart_trace_emit(aCPU, EM8051_SAB_UART_TRACE_TX_BIT,
                            aMachineCycle, aCPU->mSABUartTxData,
                            aCPU->mSABUartTxNinth,
                            aCPU->mSABUartTxBitIndex,
                            sab_uart_tx_bit_value(
                                aCPU, aCPU->mSABUartTxBitIndex));
        return;
    }

    if (aCPU->mSABUartTxBitIndex == 9u)
    {
        aCPU->mSABUartTxBitIndex = 10u;
        aCPU->mSFR[REG_SCON] |= SCONMASK_TI;
        sab_uart_trace_emit(aCPU, EM8051_SAB_UART_TRACE_TX_STOP,
                            aMachineCycle, aCPU->mSABUartTxData,
                            aCPU->mSABUartTxNinth, 10u, true);
        return;
    }

    sab_uart_trace_emit(aCPU, EM8051_SAB_UART_TRACE_TX_END,
                        aMachineCycle, aCPU->mSABUartTxData,
                        aCPU->mSABUartTxNinth, 10u, true);
    aCPU->mSABUartTxActive = false;
    if (aCPU->mSABUartTxPending)
        sab_uart_tx_start(aCPU, aMachineCycle);
}

static void sab_uart_rx_boundary(struct em8051 *aCPU,
                                 uint64_t aMachineCycle)
{
    bool accepted;

    aCPU->mSABUartRxBitIndex++;
    if (aCPU->mSABUartRxBitIndex == 10u)
    {
        accepted = (aCPU->mSFR[REG_SCON] & SCONMASK_RI) == 0 &&
                   ((aCPU->mSFR[REG_SCON] & SCONMASK_SM2) == 0 ||
                    aCPU->mSABUartRxPendingNinth);
        if (accepted)
        {
            aCPU->mSABUartRxData = aCPU->mSABUartRxPendingData;
            aCPU->mSFR[REG_SBUF] = aCPU->mSABUartRxData;
            aCPU->mSFR[REG_SCON] &= (uint8_t)~SCONMASK_RB8;
            if (aCPU->mSABUartRxPendingNinth)
                aCPU->mSFR[REG_SCON] |= SCONMASK_RB8;
            aCPU->mSFR[REG_SCON] |= SCONMASK_RI;
            sab_uart_trace_emit(aCPU, EM8051_SAB_UART_TRACE_RX_ACCEPT,
                                aMachineCycle,
                                aCPU->mSABUartRxPendingData,
                                aCPU->mSABUartRxPendingNinth, 10u, true);
        }
        else
        {
            sab_uart_trace_emit(aCPU, EM8051_SAB_UART_TRACE_RX_DROP,
                                aMachineCycle,
                                aCPU->mSABUartRxPendingData,
                                aCPU->mSABUartRxPendingNinth, 10u, true);
        }
    }
    else if (aCPU->mSABUartRxBitIndex == 11u)
    {
        aCPU->mSABUartRxActive = false;
        sab_uart_trace_emit(aCPU, EM8051_SAB_UART_TRACE_RX_END,
                            aMachineCycle, aCPU->mSABUartRxPendingData,
                            aCPU->mSABUartRxPendingNinth, 10u, true);
    }
}

static void sab_uart_timer1_overflow(struct em8051 *aCPU)
{
    bool smod;
    uint64_t completed_cycle;

    if (!sab_uart_mode3(aCPU))
        return;

    smod = (aCPU->mSFR[REG_PCON] & PCONMASK_SMOD) != 0;
    completed_cycle = aCPU->mMachineCycleCount + 1u;
    if (sab_uart_divider_step(&aCPU->mSABUartDividerPhase, smod))
        sab_uart_tx_boundary(aCPU, completed_cycle);
    if (aCPU->mSABUartRxActive &&
        sab_uart_divider_step(&aCPU->mSABUartRxDividerPhase, smod))
    {
        sab_uart_rx_boundary(aCPU, completed_cycle);
    }
}

static void sab_uart_sbuf_write(struct em8051 *aCPU, uint8_t aValue)
{
    if (!sab_uart_mode3(aCPU))
        return;

    aCPU->mSABUartTxPendingData = aValue;
    aCPU->mSABUartTxPendingNinth =
        (aCPU->mSFR[REG_SCON] & SCONMASK_TB8) != 0;
    aCPU->mSABUartTxPending = true;
}

bool em8051_sab_uart_inject_rx_frame(struct em8051 *aCPU, uint8_t aData,
                                     bool aNinthBit)
{
    if (!aCPU || !sab_uart_mode3(aCPU) ||
        (aCPU->mSFR[REG_SCON] & SCONMASK_REN) == 0 ||
        aCPU->mSABUartRxActive)
    {
        return false;
    }

    aCPU->mSABUartRxPendingData = aData;
    aCPU->mSABUartRxPendingNinth = aNinthBit;
    aCPU->mSABUartRxDividerPhase = 0;
    aCPU->mSABUartRxBitIndex = 0;
    aCPU->mSABUartRxActive = true;
    sab_uart_trace_emit(aCPU, EM8051_SAB_UART_TRACE_RX_START,
                        aCPU->mMachineCycleCount, aData, aNinthBit, 0, false);
    return true;
}

uint8_t em8051_sfr_read(struct em8051 *aCPU, uint8_t aAddress)
{
    uint8_t index;
    if (!aCPU || aAddress < 0x80u)
        return 0xffu;
    index = (uint8_t)(aAddress - 0x80u);
    if (aCPU->mVariant == EM8051_VARIANT_SAB80535 &&
        aAddress == (uint8_t)(REG_SBUF + 0x80u))
    {
        if (aCPU->sfrread[index])
            return aCPU->sfrread[index](aCPU, aAddress);
        return aCPU->mSABUartRxData;
    }
    if (aCPU->sfrread[index])
        return aCPU->sfrread[index](aCPU, aAddress);
    return aCPU->mSFR[index];
}

void em8051_sfr_write(struct em8051 *aCPU, uint8_t aAddress, uint8_t aValue)
{
    uint8_t index;
    if (!aCPU || aAddress < 0x80u)
        return;
    index = (uint8_t)(aAddress - 0x80u);
    if (aCPU->mVariant == EM8051_VARIANT_SAB80535 &&
        aAddress == (uint8_t)(REG_SBUF + 0x80u))
    {
        sab_uart_sbuf_write(aCPU, aValue);
        /* Present TX through the established callback surface, then restore
         * the SBUF mirror from canonical RX state as it exists after the
         * callback (which may advance or otherwise update receive state). */
        aCPU->mSFR[index] = aValue;
        if (aCPU->sfrwrite[index])
            aCPU->sfrwrite[index](aCPU, aAddress);
        aCPU->mSFR[index] = aCPU->mSABUartRxData;
        em8051_trace_emit(aCPU, EM8051_TRACE_SFR_WRITE, aAddress, aValue);
        return;
    }
    aCPU->mSFR[index] = aValue;
    if (aCPU->sfrwrite[index])
        aCPU->sfrwrite[index](aCPU, aAddress);
    em8051_trace_emit(aCPU, EM8051_TRACE_SFR_WRITE, aAddress, aValue);
    if (aCPU->mVariant == EM8051_VARIANT_SAB80535 &&
        (aAddress == EM8051_SAB_SFR_IEN0 ||
         aAddress == EM8051_SAB_SFR_IEN1 ||
         aAddress == EM8051_SAB_SFR_IP0 ||
         aAddress == EM8051_SAB_SFR_IP1))
    {
        sab_irq_arm_inhibit(aCPU);
    }
}

bool em8051_sab_irq_set_pending(struct em8051 *aCPU,
                                enum em8051_sab_irq_source aSource,
                                bool aPending)
{
    uint8_t *request_sfr;
    uint8_t request_mask;

    if (!aCPU || aCPU->mVariant != EM8051_VARIANT_SAB80535 ||
        (unsigned)aSource >= (unsigned)EM8051_SAB_IRQ_SOURCE_COUNT)
    {
        return false;
    }

    switch (aSource)
    {
    case EM8051_SAB_IRQ_INT0:
        request_sfr = &aCPU->mSFR[REG_TCON];
        request_mask = TCONMASK_IE0;
        break;
    case EM8051_SAB_IRQ_TIMER0:
        request_sfr = &aCPU->mSFR[REG_TCON];
        request_mask = TCONMASK_TF0;
        break;
    case EM8051_SAB_IRQ_INT1:
        request_sfr = &aCPU->mSFR[REG_TCON];
        request_mask = TCONMASK_IE1;
        break;
    case EM8051_SAB_IRQ_TIMER1:
        request_sfr = &aCPU->mSFR[REG_TCON];
        request_mask = TCONMASK_TF1;
        break;
    case EM8051_SAB_IRQ_UART:
        request_sfr = &aCPU->mSFR[REG_SCON];
        request_mask = SCONMASK_RI | SCONMASK_TI;
        break;
    case EM8051_SAB_IRQ_TIMER2:
        request_sfr =
            &aCPU->mSFR[SAB_SFR_INDEX(EM8051_SAB_SFR_IRCON)];
        request_mask = SAB_IRCONMASK_TF2 | SAB_IRCONMASK_EXF2;
        break;
    case EM8051_SAB_IRQ_ADC:
        request_sfr =
            &aCPU->mSFR[SAB_SFR_INDEX(EM8051_SAB_SFR_IRCON)];
        request_mask = SAB_IRCONMASK_IADC;
        break;
    case EM8051_SAB_IRQ_INT2:
    case EM8051_SAB_IRQ_INT3:
    case EM8051_SAB_IRQ_INT4:
    case EM8051_SAB_IRQ_INT5:
    case EM8051_SAB_IRQ_INT6:
        request_sfr =
            &aCPU->mSFR[SAB_SFR_INDEX(EM8051_SAB_SFR_IRCON)];
        request_mask = (uint8_t)(1u <<
            ((unsigned)aSource - (unsigned)EM8051_SAB_IRQ_ADC));
        break;
    default:
        return false;
    }

    if (aPending)
    {
        /* Aggregate UART and Timer-2 sources use RI and TF2 respectively for
         * a synthetic assertion; clearing the source clears both members. */
        if (aSource == EM8051_SAB_IRQ_UART)
            request_mask = SCONMASK_RI;
        else if (aSource == EM8051_SAB_IRQ_TIMER2)
            request_mask = SAB_IRCONMASK_TF2;
        *request_sfr |= request_mask;
    }
    else
    {
        *request_sfr &= (uint8_t)~request_mask;
    }

    sab_irq_sync(aCPU);
    sab_irq_trace_emit(aCPU, EM8051_SAB_IRQ_TRACE_REQUEST, aSource,
                       sab_irq_priority(aCPU, aSource), aPending,
                       aCPU->mSABIrqPending);
    return true;
}

void em8051_raise_exception(struct em8051 *aCPU, int aCode)
{
    aCPU->mExceptionRaised = true;
    aCPU->mLastException = aCode;
    if (aCPU->except)
        aCPU->except(aCPU, aCode);
}

static void serial_tx(struct em8051 *aCPU) {
	// Test if still something to send
	if (! aCPU->serial_out_remaining_bits)
	       return;

	aCPU->serial_out_remaining_bits--;
	bool tx_bit = (aCPU->mSFR[REG_SBUF] >> aCPU->serial_out_remaining_bits);
	// Set P3.1 according to the currently clocked out SERIAL bit
	aCPU->mSFR[REG_P3] &= ~(1 << 1);
	if (tx_bit) aCPU->mSFR[REG_P3] |= (1 << 1);

	// If everything is sent now, add it to the visual buffer & raise interrupt
	if (aCPU->serial_out_remaining_bits == 0) {
		aCPU->serial_out[aCPU->serial_out_idx] = aCPU->mSFR[REG_SBUF];
		aCPU->serial_out_idx = (aCPU->serial_out_idx + 1) % sizeof(aCPU->serial_out);
		aCPU->mSFR[REG_SCON] |= (1<<1); // Set TI bit
		if (aCPU->mSFR[REG_IE] & IEMASK_ES) aCPU->serial_interrupt_trigger = 1; // Trigger Serial Interrupt
	}
}

static void timer_overflow_emit(struct em8051 *aCPU,
                                enum em8051_timer aTimer)
{
    struct em8051_timer_overflow_record record;

    aCPU->mTimerOverflowCount[aTimer]++;
    if (aTimer == EM8051_TIMER1 &&
        aCPU->mVariant == EM8051_VARIANT_SAB80535)
    {
        /* Consume the producer event directly. TF1 and the public observer
         * remain independent views of the same completed timer overflow. */
        sab_uart_timer1_overflow(aCPU);
    }
    if (!aCPU->timer_overflow)
        return;

    record.timer = aTimer;
    /* timer_tick() runs before the shared machine-cycle counter is advanced. */
    record.machine_cycle = aCPU->mMachineCycleCount + 1u;
    if (aTimer == EM8051_TIMER0)
    {
        record.tl = aCPU->mSFR[REG_TL0];
        record.th = aCPU->mSFR[REG_TH0];
    }
    else
    {
        record.tl = aCPU->mSFR[REG_TL1];
        record.th = aCPU->mSFR[REG_TH1];
    }
    aCPU->timer_overflow(&record, aCPU->timer_overflow_user);
}


static void timer_tick(struct em8051 *aCPU)
{
    uint8_t increment;
    uint16_t v;

    // TODO: External int 0 flag

    if ((aCPU->mSFR[REG_TMOD] & (TMODMASK_M0_0 | TMODMASK_M1_0)) == (TMODMASK_M0_0 | TMODMASK_M1_0))
    {
        // timer/counter 0 in mode 3

        increment = 0;
        
        // Check if we're run enabled
        // TODO: also run if GATE is one and INT is one (external interrupt)
        if (!(aCPU->mSFR[REG_TMOD] & TMODMASK_GATE_0) && 
            (aCPU->mSFR[REG_TCON] & TCONMASK_TR0))
        {
            // check timer / counter mode
            if (aCPU->mSFR[REG_TMOD] & TMODMASK_CT_0)
            {
                // counter op;
                // counter works if T0 pin was 1 and is now 0 (P3.4 on AT89C2051)
                increment = 0; // TODO
            }
            else
            {
                increment = 1;
            }
        }
        if (increment)
        {
            v = aCPU->mSFR[REG_TL0];
            v++;
            aCPU->mSFR[REG_TL0] = v & 0xff;
            if (v > 0xff)
            {
                // TL0 overflowed
                aCPU->mSFR[REG_TCON] |= TCONMASK_TF0;
            }
        }

        increment = 0;
        
        // Check if we're run enabled
        // TODO: also run if GATE is one and INT is one (external interrupt)
        if (!(aCPU->mSFR[REG_TMOD] & TMODMASK_GATE_1) && 
            (aCPU->mSFR[REG_TCON] & TCONMASK_TR1))
        {
            // check timer / counter mode
            if (aCPU->mSFR[REG_TMOD] & TMODMASK_CT_1)
            {
                // counter op;
                // counter works if T1 pin was 1 and is now 0
                increment = 0; // TODO
            }
            else
            {
                increment = 1;
            }
        }

        if (increment)
        {
            v = aCPU->mSFR[REG_TH0];
            v++;
            aCPU->mSFR[REG_TH0] = v & 0xff;
            if (v > 0xff)
            {
                // TH0 overflowed
                aCPU->mSFR[REG_TCON] |= TCONMASK_TF1;
            }
        }

    }

    {   // Timer/counter 0
        
        increment = 0;
        
        // Check if we're run enabled
        // TODO: also run if GATE is one and INT is one (external interrupt)
        if (!(aCPU->mSFR[REG_TMOD] & TMODMASK_GATE_0) && 
            (aCPU->mSFR[REG_TCON] & TCONMASK_TR0))
        {
            // check timer / counter mode
            if (aCPU->mSFR[REG_TMOD] & TMODMASK_CT_0)
            {
                // counter op;
                // counter works if T0 pin was 1 and is now 0 (P3.4 on AT89C2051)
                increment = 0; // TODO
            }
            else
            {
                increment = 1;
            }
        }
        
        if (increment)
        {
            switch (aCPU->mSFR[REG_TMOD] & (TMODMASK_M0_0 | TMODMASK_M1_0))
            {
            case 0: // 13-bit timer
                v = aCPU->mSFR[REG_TL0] & 0x1f; // lower 5 bits of TL0
                v++;
                aCPU->mSFR[REG_TL0] = (aCPU->mSFR[REG_TL0] & ~0x1f) | (v & 0x1f);
                if (v > 0x1f)
                {
                    // TL0 overflowed
                    v = aCPU->mSFR[REG_TH0];
                    v++;
                    aCPU->mSFR[REG_TH0] = v & 0xff;
                    if (v > 0xff)
                    {
                        // TH0 overflowed; set bit
                        aCPU->mSFR[REG_TCON] |= TCONMASK_TF0;
                    }
                }
                break;
            case TMODMASK_M0_0: // 16-bit timer/counter
                v = aCPU->mSFR[REG_TL0];
                v++;
                aCPU->mSFR[REG_TL0] = v & 0xff;
                if (v > 0xff)
                {
                    // TL0 overflowed
                    v = aCPU->mSFR[REG_TH0];
                    v++;
                    aCPU->mSFR[REG_TH0] = v & 0xff;
                    if (v > 0xff)
                    {
                        // TH0 overflowed; set bit
                        aCPU->mSFR[REG_TCON] |= TCONMASK_TF0;
                        timer_overflow_emit(aCPU, EM8051_TIMER0);
                    }
                }
                break;
            case TMODMASK_M1_0: // 8-bit auto-reload timer
                v = aCPU->mSFR[REG_TL0];
                v++;
                aCPU->mSFR[REG_TL0] = v & 0xff;
                if (v > 0xff)
                {
                    // TL0 overflowed; reload
                    aCPU->mSFR[REG_TL0] = aCPU->mSFR[REG_TH0];
                    aCPU->mSFR[REG_TCON] |= TCONMASK_TF0;
                }
                break;
            default: // two 8-bit timers
                // TODO
                break;
            }
        }
    }

    // TODO: External int 1 

    {   // Timer/counter 1 
        
        increment = 0;

        if (!(aCPU->mSFR[REG_TMOD] & TMODMASK_GATE_1) && 
            (aCPU->mSFR[REG_TCON] & TCONMASK_TR1))
        {
            if (aCPU->mSFR[REG_TMOD] & TMODMASK_CT_1)
            {
                // counter op;
                // counter works if T1 pin was 1 and is now 0
                increment = 0; // TODO
            }
            else
            {
                increment = 1;
            }
        }

        if (increment)
        {
            switch (aCPU->mSFR[REG_TMOD] & (TMODMASK_M0_1 | TMODMASK_M1_1))
            {
            case 0: // 13-bit timer
                v = aCPU->mSFR[REG_TL1] & 0x1f; // lower 5 bits of TL0
                v++;
                aCPU->mSFR[REG_TL1] = (aCPU->mSFR[REG_TL1] & ~0x1f) | (v & 0x1f);
                if (v > 0x1f)
                {
                    // TL1 overflowed
                    v = aCPU->mSFR[REG_TH1];
                    v++;
                    aCPU->mSFR[REG_TH1] = v & 0xff;
                    if (v > 0xff)
                    {
                        // TH1 overflowed; set bit
                        // Only update TF1 if timer 0 is not in "mode 3"
                    if (!((aCPU->mSFR[REG_TMOD] & T0_MODE3_MASK ) == T0_MODE3_MASK))
                        aCPU->mSFR[REG_TCON] |= TCONMASK_TF1;

                    } 
                }
                break;
            case TMODMASK_M0_1: // 16-bit timer/counter
                v = aCPU->mSFR[REG_TL1];
                v++;
                aCPU->mSFR[REG_TL1] = v & 0xff;
                if (v > 0xff)
                {
                    // TL1 overflowed
                    v = aCPU->mSFR[REG_TH1];
                    v++;
                    aCPU->mSFR[REG_TH1] = v & 0xff;
                    if (v > 0xff)
                    {
                        // TH1 overflowed; set bit
                        // Only update TF1 if timer 0 is not in "mode 3"
              
                        if (!((aCPU->mSFR[REG_TMOD] & T0_MODE3_MASK)  == T0_MODE3_MASK))
                            aCPU->mSFR[REG_TCON] |= TCONMASK_TF1;

                    }
                }
                break;
            case TMODMASK_M1_1: // 8-bit auto-reload timer
                v = aCPU->mSFR[REG_TL1];
                v++;
                aCPU->mSFR[REG_TL1] = v & 0xff;
                if (v > 0xff)
                {
                    // TL0 overflowed; reload
                    aCPU->mSFR[REG_TL1] = aCPU->mSFR[REG_TH1];
                    // Only update TF1 if timer 0 is not in "mode 3"
                    
            
                    if (!((aCPU->mSFR[REG_TMOD] & T0_MODE3_MASK ) == T0_MODE3_MASK))
                    {
                        aCPU->mSFR[REG_TCON] |= TCONMASK_TF1;
                        timer_overflow_emit(aCPU, EM8051_TIMER1);
                    }
                    
                }
                break;
            default: // disabled
                break;
            }

	    // If Timer1 overflowed, see if we need to send a serial bit.
            // SAB80535 Timer/UART scheduling is variant-owned and is not
            // provided by this inherited classic serial shortcut.
            if (aCPU->mVariant != EM8051_VARIANT_SAB80535 &&
                (aCPU->mSFR[REG_TCON] & TCONMASK_TF1)) {
                if (aCPU->mSFR[REG_SCON] & SCONMASK_SM1) {
                    serial_tx(aCPU);
		    aCPU->mSFR[REG_TCON] &= ~TCONMASK_TF1; // clear overflow flag
                }
            }
        }
    }

    // TODO: serial port, timer2, other stuff
}

static bool sab_irq_timer2_is_enabled(const struct em8051 *aCPU)
{
    uint8_t ien0 = aCPU->mSFR[SAB_SFR_INDEX(EM8051_SAB_SFR_IEN0)];
    uint8_t ien1 = aCPU->mSFR[SAB_SFR_INDEX(EM8051_SAB_SFR_IEN1)];
    uint8_t ircon = aCPU->mSFR[SAB_SFR_INDEX(EM8051_SAB_SFR_IRCON)];
    return ((ircon & SAB_IRCONMASK_TF2) && (ien0 & IEMASK_ET2)) ||
           ((ircon & SAB_IRCONMASK_EXF2) &&
            (ien1 & SAB_IEN1MASK_EXEN2));
}

static void sab_irq_auto_clear(struct em8051 *aCPU,
                               enum em8051_sab_irq_source aSource)
{
    uint8_t *ircon =
        &aCPU->mSFR[SAB_SFR_INDEX(EM8051_SAB_SFR_IRCON)];
    switch (aSource)
    {
    case EM8051_SAB_IRQ_INT0:
        if (aCPU->mSFR[REG_TCON] & TCONMASK_IT0)
            aCPU->mSFR[REG_TCON] &= (uint8_t)~TCONMASK_IE0;
        break;
    case EM8051_SAB_IRQ_TIMER0:
        aCPU->mSFR[REG_TCON] &= (uint8_t)~TCONMASK_TF0;
        break;
    case EM8051_SAB_IRQ_INT1:
        if (aCPU->mSFR[REG_TCON] & TCONMASK_IT1)
            aCPU->mSFR[REG_TCON] &= (uint8_t)~TCONMASK_IE1;
        break;
    case EM8051_SAB_IRQ_TIMER1:
        aCPU->mSFR[REG_TCON] &= (uint8_t)~TCONMASK_TF1;
        break;
    case EM8051_SAB_IRQ_INT2:
    case EM8051_SAB_IRQ_INT3:
    case EM8051_SAB_IRQ_INT4:
    case EM8051_SAB_IRQ_INT5:
    case EM8051_SAB_IRQ_INT6:
        *ircon &= (uint8_t)~(1u <<
            ((unsigned)aSource - (unsigned)EM8051_SAB_IRQ_ADC));
        break;
    case EM8051_SAB_IRQ_UART:
    case EM8051_SAB_IRQ_TIMER2:
    case EM8051_SAB_IRQ_ADC:
    default:
        /* RI/TI, TF2/EXF2 and IADC are software-clear classes. */
        break;
    }
}

static bool handle_sab_interrupts(struct em8051 *aCPU)
{
    int selected = -1;
    uint8_t selected_priority = 0;
    uint8_t current_priority = 0;
    uint16_t pending_snapshot;
    unsigned source;
    uint8_t depth;

    sab_irq_sync(aCPU);
    if (aCPU->mSABIrqInhibitInstructions != 0)
        return true;
    if (!(aCPU->mSFR[SAB_SFR_INDEX(EM8051_SAB_SFR_IEN0)] & IEMASK_EA))
        return true;

    depth = aCPU->mSABIrqDepth;
    if (depth != 0)
        current_priority = aCPU->mSABIrqPriorityStack[depth - 1u];

    for (source = 0; source < (unsigned)EM8051_SAB_IRQ_SOURCE_COUNT;
         source++)
    {
        enum em8051_sab_irq_source candidate =
            (enum em8051_sab_irq_source)source;
        uint16_t bit = SAB_IRQ_BIT(source);
        uint8_t priority;

        if (!(aCPU->mSABIrqPending & bit) ||
            !(aCPU->mSABIrqEnabled & bit) ||
            (aCPU->mSABIrqInService & bit))
        {
            continue;
        }
        if (candidate == EM8051_SAB_IRQ_TIMER2 &&
            !sab_irq_timer2_is_enabled(aCPU))
        {
            continue;
        }

        priority = sab_irq_priority(aCPU, candidate);
        if (depth != 0 && priority <= current_priority)
            continue;
        if (selected < 0 || priority > selected_priority)
        {
            selected = (int)source;
            selected_priority = priority;
        }
    }

    if (selected < 0 || depth >= 4u)
        return true;

    pending_snapshot = aCPU->mSABIrqPending;
    if (!push_to_stack(aCPU, (uint8_t)(aCPU->mPC & 0xffu)))
        return false;
    if (!push_to_stack(aCPU, (uint8_t)(aCPU->mPC >> 8)))
        return false;

    aCPU->mSFR[REG_PCON] &= (uint8_t)~0x01u;
    aCPU->mPC = gSABIrqVectors[selected];
    aCPU->mTickDelay = 1;
    aCPU->mSABIrqSourceStack[depth] = (uint8_t)selected;
    aCPU->mSABIrqPriorityStack[depth] = selected_priority;
    aCPU->mSABIrqSavedACC[depth] = aCPU->mSFR[REG_ACC];
    aCPU->mSABIrqSavedPSW[depth] = aCPU->mSFR[REG_PSW];
    aCPU->mSABIrqSavedSP[depth] = aCPU->mSFR[REG_SP];
    aCPU->mSABIrqDepth = (uint8_t)(depth + 1u);
    aCPU->mSABIrqInService |= SAB_IRQ_BIT((unsigned)selected);

    sab_irq_auto_clear(aCPU, (enum em8051_sab_irq_source)selected);
    sab_irq_sync(aCPU);
    sab_irq_trace_emit(aCPU, EM8051_SAB_IRQ_TRACE_ACCEPT,
                       (enum em8051_sab_irq_source)selected,
                       selected_priority, true, pending_snapshot);
    return true;
}

void em8051_sab_irq_reti(struct em8051 *aCPU, uint8_t aOriginalSP)
{
    uint8_t depth;
    uint8_t index;
    enum em8051_sab_irq_source source;
    uint8_t priority;

    if (!aCPU || aCPU->mVariant != EM8051_VARIANT_SAB80535)
        return;

    depth = aCPU->mSABIrqDepth;
    if (depth != 0)
    {
        index = (uint8_t)(depth - 1u);
        source = (enum em8051_sab_irq_source)
            aCPU->mSABIrqSourceStack[index];
        priority = aCPU->mSABIrqPriorityStack[index];

        if (aCPU->mSABIrqSavedACC[index] != aCPU->mSFR[REG_ACC])
            em8051_raise_exception(aCPU, EXCEPTION_IRET_ACC_MISMATCH);
        if (aCPU->mSABIrqSavedSP[index] != aOriginalSP)
            em8051_raise_exception(aCPU, EXCEPTION_IRET_SP_MISMATCH);
        if ((aCPU->mSABIrqSavedPSW[index] &
             (PSWMASK_OV | PSWMASK_RS0 | PSWMASK_RS1 |
              PSWMASK_AC | PSWMASK_C)) !=
            (aCPU->mSFR[REG_PSW] &
             (PSWMASK_OV | PSWMASK_RS0 | PSWMASK_RS1 |
              PSWMASK_AC | PSWMASK_C)))
        {
            em8051_raise_exception(aCPU, EXCEPTION_IRET_PSW_MISMATCH);
        }

        aCPU->mSABIrqDepth = index;
        aCPU->mSABIrqInService &=
            (uint16_t)~SAB_IRQ_BIT((unsigned)source);
        sab_irq_sync(aCPU);
        sab_irq_trace_emit(aCPU, EM8051_SAB_IRQ_TRACE_RELEASE, source,
                           priority, false, aCPU->mSABIrqPending);
    }

    sab_irq_arm_inhibit(aCPU);
}

static bool handle_interrupts(struct em8051 *aCPU)
{
    int16_t dest_ip = -1;
    uint8_t hi = 0;
    uint8_t lo = 0;

    if (aCPU->mVariant == EM8051_VARIANT_SAB80535)
        return handle_sab_interrupts(aCPU);

    // can't interrupt high level
    if (aCPU->mInterruptActive > 1) 
        return true;

    if (aCPU->mSFR[REG_IE] & IEMASK_EA)
    {
        // Interrupts enabled
        if (aCPU->mSFR[REG_IE] & IEMASK_EX0 && aCPU->mSFR[REG_TCON] & TCONMASK_IE0)
        {
            // External int 0 
            dest_ip = ISR_INT0;
            if (aCPU->mSFR[REG_IP] & IPMASK_PX0)
                hi = 1;
            lo = 1;
        }
        if (aCPU->mSFR[REG_IE] & IEMASK_ET0 && aCPU->mSFR[REG_TCON] & TCONMASK_TF0 && !hi)
        {
            // Timer/counter 0 
            if (!lo)
            {
                dest_ip = ISR_TF0;
                lo = 1;
            }
            if (aCPU->mSFR[REG_IP] & IPMASK_PT0)
            {
                hi = 1;
                dest_ip = ISR_TF0;
            }
        }
        if (aCPU->mSFR[REG_IE] & IEMASK_EX1 && aCPU->mSFR[REG_TCON] & TCONMASK_IE1 && !hi)
        {
            // External int 1 
            if (!lo)
            {
                dest_ip = ISR_INT1;
                lo = 1;
            }
            if (aCPU->mSFR[REG_IP] & IPMASK_PX1)
            {
                hi = 1;
                dest_ip = ISR_INT1;
            }
        }
        if (aCPU->mSFR[REG_IE] & IEMASK_ET1 && aCPU->mSFR[REG_TCON] & TCONMASK_TF1 && !hi)
        {
            // Timer/counter 1 enabled
            if (!lo)
            {
                dest_ip = ISR_TF1;
                lo = 1;
            }
            if (aCPU->mSFR[REG_IP] & IPMASK_PT1)
            {
                hi = 1;
                dest_ip = ISR_TF1;
            }
        }
        if (aCPU->mSFR[REG_IE] & IEMASK_ES && aCPU->serial_interrupt_trigger && !hi)
        {
            // Serial port interrupt 
            if (!lo)
            {
                dest_ip = ISR_SR;
                lo = 1;
            }
            if (aCPU->mSFR[REG_IP] & IPMASK_PS)
            {
                hi = 1;
                dest_ip = ISR_SR;
            }
            // TODO
        }
#ifdef __8052__
        if (aCPU->mSFR[REG_IE] & IEMASK_ET2 && !hi)
        {
            // Timer 2 (8052 only)
            if (!lo)
            {
                dest_ip = ISR_SR;
                lo = 1;
            }
            if (aCPU->mSFR[REG_IP] & IPMASK_PT2)
            {
                hi = 1;
                dest_ip = ISR_SR;
            }
            // TODO
        }
#endif // __8052__
    }
    
    // no interrupt
    if (dest_ip == -1)
        return true;

    // can't interrupt same-level
    if (aCPU->mInterruptActive == 1 && !hi)
        return true;

    // some interrupt occurs; perform LCALL
    if (!push_to_stack(aCPU, aCPU->mPC & 0xff))
        return false;
    if (!push_to_stack(aCPU, aCPU->mPC >> 8))
        return false;
    aCPU->mSFR[REG_PCON] &= ~0x01; // clear idle flag, but not Power down flag
    aCPU->mPC = dest_ip;
    /* Interrupt entry consumes two machine cycles. This tick accounts for
     * the first; one pending cycle completes before the vector opcode. */
    aCPU->mTickDelay = 1;
    switch (dest_ip)
    {
    case ISR_TF0:
        aCPU->mSFR[REG_TCON] &= ~TCONMASK_TF0; // clear overflow flag
        break;
    case ISR_TF1:
        aCPU->mSFR[REG_TCON] &= ~TCONMASK_TF1; // clear overflow flag
        break;
    case ISR_SR:
        aCPU->serial_interrupt_trigger = 0; // handled the serial interrupt trigger
        break;
    }

    if (hi)
    {
        aCPU->mInterruptActive |= 2;
    }
    else
    {
        aCPU->mInterruptActive = 1;
    }
    aCPU->int_a[hi] = aCPU->mSFR[REG_ACC];
    aCPU->int_psw[hi] = aCPU->mSFR[REG_PSW];
    aCPU->int_sp[hi] = aCPU->mSFR[REG_SP];
    return true;
}

bool tick(struct em8051 *aCPU)
{
    uint8_t v;
    bool ticked = false;

    /* A pending cycle belongs to the already-started instruction or interrupt
     * entry. It advances peripherals and virtual time, but must not also start
     * the next opcode in the same architectural cycle. */
    if (aCPU->mTickDelay)
    {
        aCPU->mTickDelay--;
        timer_tick(aCPU);
        aCPU->mMachineCycleCount++;
        return false;
    }

    // Test for Power Down
    if ((aCPU->mSFR[REG_PCON]) & 0x02) {
        return false;
    }

    // Interrupts are sent if the following cases are not true:
    // 1. interrupt of equal or higher priority is in progress (tested inside function)
    // 2. current cycle is not the final cycle of instruction (tickdelay = 0)
    // 3. Siemens arbitration is inhibited for one instruction after RETI or
    //    a write to IEN0/IEN1/IP0/IP1 (handled by the SAB controller).
    /* A failed interrupt stack push terminates entry immediately. Do not run
     * the interrupted opcode after the failed entry attempt. */
    if (!handle_interrupts(aCPU))
    {
        timer_tick(aCPU);
        aCPU->mMachineCycleCount++;
        return false;
    }

    if (aCPU->mTickDelay == 0)
    {
        // IDL activate the idle mode to save power
        bool is_idle = (aCPU->mSFR[REG_PCON]) & 0x01;
        if (is_idle) {
            /* IDLE stops opcode execution, but virtual machine cycles and
             * classic timers continue until an interrupt clears IDLE. */
        } else {
            uint8_t opcode = aCPU->mCodeMem[aCPU->mPC & aCPU->mCodeMemMaxIdx];
            aCPU->mTracePC = aCPU->mPC;
            aCPU->mInInstruction = true;
            em8051_trace_emit(aCPU, EM8051_TRACE_INSTRUCTION,
                              aCPU->mPC, opcode);
            aCPU->mTickDelay = aCPU->op[opcode](aCPU);
            aCPU->mInInstruction = false;
            aCPU->mInstructionCount++;
            if (aCPU->mVariant == EM8051_VARIANT_SAB80535 &&
                aCPU->mSABIrqInhibitInstructions != 0)
            {
                aCPU->mSABIrqInhibitInstructions--;
            }
            ticked = true;
        }
        // update parity bit
        v = aCPU->mSFR[REG_ACC];
        v ^= v >> 4;
        v &= 0xf;
        v = (0x6996 >> v) & 1;
        aCPU->mSFR[REG_PSW] = (aCPU->mSFR[REG_PSW] & ~PSWMASK_P) | (v * PSWMASK_P);
    }

    timer_tick(aCPU);
    aCPU->mMachineCycleCount++;

    return ticked;
}

static void fill_run_result(struct em8051 *aCPU,
                            struct em8051_run_result *aResult,
                            enum em8051_stop_reason aReason,
                            uint64_t aStartInstructions,
                            uint64_t aStartCycles)
{
    if (!aResult)
        return;
    aResult->reason = aReason;
    aResult->instructions = aCPU->mInstructionCount - aStartInstructions;
    aResult->machine_cycles = aCPU->mMachineCycleCount - aStartCycles;
    aResult->pc = aCPU->mPC;
    aResult->exception_code = aCPU->mExceptionRaised ? aCPU->mLastException : -1;
}

static enum em8051_stop_reason run_control(struct em8051 *aCPU,
                                            bool aHasTarget,
                                            uint16_t aTargetPC,
                                            uint64_t aMaxInstructions,
                                            struct em8051_run_result *aResult)
{
    uint64_t start_instructions = aCPU->mInstructionCount;
    uint64_t start_cycles = aCPU->mMachineCycleCount;
    enum em8051_stop_reason reason = EM8051_STOP_INSTRUCTION_LIMIT;

    aCPU->mExceptionRaised = false;
    aCPU->mLastException = -1;

    while (true)
    {
        /* Public run calls only return at a completed cycle boundary. This
         * also leaves an interrupt vector observable after its two entry
         * cycles and before execution of the vector opcode. */
        while (aCPU->mTickDelay != 0)
            (void)tick(aCPU);

        if (aCPU->mExceptionRaised)
        {
            reason = EM8051_STOP_EXCEPTION;
            break;
        }
        if (aHasTarget && aCPU->mPC == aTargetPC)
        {
            reason = EM8051_STOP_TARGET_PC;
            break;
        }
        if (aCPU->mBreakpointEnabled && aCPU->mPC == aCPU->mBreakpoint)
        {
            reason = EM8051_STOP_BREAKPOINT;
            break;
        }
        if (aCPU->mSFR[REG_PCON] & 0x03)
        {
            reason = EM8051_STOP_HALT;
            break;
        }
        if (aCPU->mInstructionCount - start_instructions >= aMaxInstructions)
        {
            reason = EM8051_STOP_INSTRUCTION_LIMIT;
            break;
        }

        (void)tick(aCPU);
    }

    fill_run_result(aCPU, aResult, reason, start_instructions, start_cycles);
    return reason;
}

enum em8051_stop_reason em8051_run(struct em8051 *aCPU,
                                      uint64_t aMaxInstructions,
                                      struct em8051_run_result *aResult)
{
    return run_control(aCPU, false, 0, aMaxInstructions, aResult);
}

enum em8051_stop_reason em8051_step_instruction(
    struct em8051 *aCPU, struct em8051_run_result *aResult)
{
    return em8051_run(aCPU, 1, aResult);
}

enum em8051_stop_reason em8051_run_until_pc(struct em8051 *aCPU,
                                               uint16_t aTargetPC,
                                               uint64_t aMaxInstructions,
                                               struct em8051_run_result *aResult)
{
    return run_control(aCPU, true, aTargetPC, aMaxInstructions, aResult);
}

void em8051_set_breakpoint(struct em8051 *aCPU, uint16_t aPC, bool aEnabled)
{
    aCPU->mBreakpoint = aPC;
    aCPU->mBreakpointEnabled = aEnabled;
}

uint8_t decode(struct em8051 *aCPU, uint16_t aPosition, char *aBuffer)
{
    bool is_idle = (aCPU->mSFR[REG_PCON]) & 0x01;
    if (is_idle) {
        strcpy(aBuffer, "IDLE");
        return 0;
    }
    bool is_powerdown = (aCPU->mSFR[REG_PCON]) & 0x02;
    if (is_powerdown) {
        strcpy(aBuffer, "POWER DOWN");
        return 0;
    }
    return aCPU->dec[aCPU->mCodeMem[aPosition & (aCPU->mCodeMemMaxIdx)]](aCPU, aPosition, aBuffer);
}

void disasm_setptrs(struct em8051 *aCPU);
void op_setptrs(struct em8051 *aCPU);

void reset(struct em8051 *aCPU, bool aWipe)
{
    uint32_t random_state = aCPU->mResetSeed;
    size_t i;
    const struct em8051_variant_descriptor *descriptor =
        em8051_get_variant_descriptor(aCPU->mVariant);

    if (!descriptor)
    {
        aCPU->mVariant = EM8051_VARIANT_8051;
        descriptor = em8051_get_variant_descriptor(aCPU->mVariant);
    }
    if (aCPU->mOscillatorHz == 0)
        aCPU->mOscillatorHz = descriptor->default_oscillator_hz;
    if (descriptor->has_upper_iram)
        aCPU->mUpperData = aCPU->mOwnedUpperData;

    // clear memory, set registers to bootup values, etc    
    if (aWipe)
    {
        if (aCPU->mCodeMem)
            memset(aCPU->mCodeMem, 0, (size_t)aCPU->mCodeMemMaxIdx + 1u);
        if (aCPU->mExtData)
            memset(aCPU->mExtData, 0, (size_t)aCPU->mExtDataMaxIdx + 1u);
        if (aCPU->mResetSeedConfigured)
        {
            for (i = 0; i < sizeof(aCPU->mLowerData); i++)
                aCPU->mLowerData[i] = (uint8_t)reset_random(&random_state);
            if (aCPU->mUpperData)
                for (i = 0; i < 128; i++)
                    aCPU->mUpperData[i] = (uint8_t)reset_random(&random_state);
        }
        else
        {
            memset(aCPU->mLowerData, 0, sizeof(aCPU->mLowerData));
            if (aCPU->mUpperData)
                memset(aCPU->mUpperData, 0, 128);
        }
    }

    memset(aCPU->mSFR, 0, 128);

    aCPU->mPC = 0;
    aCPU->mTickDelay = 0;
    aCPU->mSFR[REG_SP] = 7;
    aCPU->mSFR[REG_P0] = 0xff;
    aCPU->mSFR[REG_P1] = 0xff;
    aCPU->mSFR[REG_P2] = 0xff;
    aCPU->mSFR[REG_P3] = 0xff;
    if (aCPU->mVariant == EM8051_VARIANT_SAB80535)
    {
        /* P4/P5 high are documented SAB reset values. The zeroed IP0/IP1 and
         * ADCON fields above, and the input-only P6 value below, are stable
         * Stage-0 model choices for hardware-indeterminate/unspecified state;
         * they are not claims about physical reset values or a P6 latch. */
        aCPU->mSFR[EM8051_SAB_SFR_P4 - 0x80] = 0xff;
        aCPU->mSFR[EM8051_SAB_SFR_P5 - 0x80] = 0xff;
        aCPU->mSFR[EM8051_SAB_SFR_P6 - 0x80] = 0xff;
    }

    // Power-off flag will be 1 only after a power on (cold reset).
    // A warm reset doesn’t affect the value of this bit
    // ... Therefore, we only set it if aWipe is 1
    if (aWipe)
        aCPU->mSFR[REG_PCON] |= (1<<4);

    // Hardware leaves SBUF undefined after power-on.
    if (aWipe)
    {
        if (aCPU->mResetSeedConfigured)
            aCPU->mSFR[REG_SBUF] = (uint8_t)reset_random(&random_state);
        else
            aCPU->mSFR[REG_SBUF] = (uint8_t)rand();
    }

    // build function pointer lists

    disasm_setptrs(aCPU);
    op_setptrs(aCPU);

    // Clean internal variables
    aCPU->mInterruptActive = 0;
    aCPU->mSABIrqPending = 0;
    aCPU->mSABIrqEnabled = 0;
    aCPU->mSABIrqInService = 0;
    aCPU->mSABIrqDepth = 0;
    memset(aCPU->mSABIrqSourceStack, 0,
           sizeof(aCPU->mSABIrqSourceStack));
    memset(aCPU->mSABIrqPriorityStack, 0,
           sizeof(aCPU->mSABIrqPriorityStack));
    memset(aCPU->mSABIrqSavedACC, 0, sizeof(aCPU->mSABIrqSavedACC));
    memset(aCPU->mSABIrqSavedPSW, 0, sizeof(aCPU->mSABIrqSavedPSW));
    memset(aCPU->mSABIrqSavedSP, 0, sizeof(aCPU->mSABIrqSavedSP));
    aCPU->mSABIrqInhibitInstructions = 0;
    memset(aCPU->mTimerOverflowCount, 0,
           sizeof(aCPU->mTimerOverflowCount));
    aCPU->mSABUartDividerPhase = 0;
    aCPU->mSABUartRxDividerPhase = 0;
    aCPU->mSABUartRxData = aCPU->mSFR[REG_SBUF];
    aCPU->mSABUartTxPendingData = 0;
    aCPU->mSABUartTxData = 0;
    aCPU->mSABUartTxBitIndex = 0;
    aCPU->mSABUartRxPendingData = 0;
    aCPU->mSABUartRxBitIndex = 0;
    aCPU->mSABUartTxPendingNinth = false;
    aCPU->mSABUartTxNinth = false;
    aCPU->mSABUartTxPending = false;
    aCPU->mSABUartTxActive = false;
    aCPU->mSABUartRxPendingNinth = false;
    aCPU->mSABUartRxActive = false;
    aCPU->mInstructionCount = 0;
    aCPU->mMachineCycleCount = 0;
    aCPU->mExceptionRaised = false;
    aCPU->mLastException = -1;
    aCPU->mInInstruction = false;
    aCPU->mBreakpointEnabled = false;

    // Clean Serial
    aCPU->serial_interrupt_trigger = 0;
    aCPU->serial_out_remaining_bits = 0;
}

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
    aCPU->trace(aCPU, &record, aCPU->trace_user);
}

uint8_t em8051_sfr_read(struct em8051 *aCPU, uint8_t aAddress)
{
    uint8_t index;
    if (!aCPU || aAddress < 0x80u)
        return 0xffu;
    index = (uint8_t)(aAddress - 0x80u);
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
    aCPU->mSFR[index] = aValue;
    if (aCPU->sfrwrite[index])
        aCPU->sfrwrite[index](aCPU, aAddress);
    em8051_trace_emit(aCPU, EM8051_TRACE_SFR_WRITE, aAddress, aValue);
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
                        aCPU->mSFR[REG_TCON] |= TCONMASK_TF1;
                    
                }
                break;
            default: // disabled
                break;
            }

	    // If Timer1 overflowed, see if we need to send a serial bit
            if (aCPU->mSFR[REG_TCON] & TCONMASK_TF1) {
                if (aCPU->mSFR[REG_SCON] & SCONMASK_SM1) {
                    serial_tx(aCPU);
		    aCPU->mSFR[REG_TCON] &= ~TCONMASK_TF1; // clear overflow flag
                }
            }
        }
    }

    // TODO: serial port, timer2, other stuff
}

void handle_interrupts(struct em8051 *aCPU)
{
    int16_t dest_ip = -1;
    uint8_t hi = 0;
    uint8_t lo = 0;

    /* The SAB80535 controller has different enable/priority registers and is
     * intentionally deferred to Stage 1. Never interpret SAB IEN1 at B8 as
     * the classic IP register. */
    if (aCPU->mVariant == EM8051_VARIANT_SAB80535)
        return;

    // can't interrupt high level
    if (aCPU->mInterruptActive > 1) 
        return;    

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
        return;

    // can't interrupt same-level
    if (aCPU->mInterruptActive == 1 && !hi)
        return; 

    // some interrupt occurs; perform LCALL
    aCPU->mSFR[REG_PCON] &= ~0x01; // clear idle flag, but not Power down flag
    push_to_stack(aCPU, aCPU->mPC & 0xff);
    push_to_stack(aCPU, aCPU->mPC >> 8);
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
    // 3. the instruction in progress is RETI or any write to the IE or IP regs (TODO)
    handle_interrupts(aCPU);

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

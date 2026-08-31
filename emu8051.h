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
 * emu8051.h
 * Emulator core header file
 */

#ifndef EMU8051_H
#define EMU8051_H

#include <stdint.h>
#include <stdbool.h>

struct em8051;

// Operation: returns number of ticks the operation should take
typedef uint8_t (*em8051operation)(struct em8051 *aCPU);

// Decodes opcode at position, and fills the buffer with the assembler code. 
// Returns how many bytes the opcode takes.
typedef uint8_t (*em8051decoder)(struct em8051 *aCPU, uint16_t aPosition, char *aBuffer);

// Callback: some exceptional situation occurred. See EM8051_EXCEPTION enum, below
typedef void (*em8051exception)(struct em8051 *aCPU, int aCode);

// Callback: an SFR register is about to be read (not called for 'a' ops nor psw changes)
// Default is to return the value in the SFR register. Ports may act differently.
typedef uint8_t (*em8051sfrread)(struct em8051 *aCPU, uint8_t aRegister);

// Callback: an SFR register has changed (not called for 'a' ops)
// Default is to do nothing
typedef void (*em8051sfrwrite)(struct em8051 *aCPU, uint8_t aRegister);

// Callback: writing to external memory
// Default is to update external memory
// (can be used to control some peripherals)
typedef void (*em8051xwrite)(struct em8051 *aCPU, uint16_t aAddress, uint8_t aValue);

// Callback: reading from external memory
// Default is to return the value in external memory 
// (can be used to control some peripherals)
typedef uint8_t (*em8051xread)(struct em8051 *aCPU, uint16_t aAddress);

enum em8051_variant
{
    EM8051_VARIANT_8051 = 0,
    EM8051_VARIANT_8052,
    EM8051_VARIANT_SAB80535
};

/* Full SFR addresses. These names deliberately keep the classic and Siemens
 * meanings separate: address B8 is classic IP but SAB80535 IEN1. */
enum em8051_classic_sfr
{
    EM8051_CLASSIC_SFR_IE = 0xA8,
    EM8051_CLASSIC_SFR_IP = 0xB8
};

enum em8051_sab80535_sfr
{
    EM8051_SAB_SFR_IEN0 = 0xA8,
    EM8051_SAB_SFR_IP0 = 0xA9,
    EM8051_SAB_SFR_IEN1 = 0xB8,
    EM8051_SAB_SFR_IP1 = 0xB9,
    EM8051_SAB_SFR_IRCON = 0xC0,
    EM8051_SAB_SFR_CCEN = 0xC1,
    EM8051_SAB_SFR_T2CON = 0xC8,
    EM8051_SAB_SFR_CRCL = 0xCA,
    EM8051_SAB_SFR_CRCH = 0xCB,
    EM8051_SAB_SFR_TL2 = 0xCC,
    EM8051_SAB_SFR_TH2 = 0xCD,
    EM8051_SAB_SFR_ADCON = 0xD8,
    EM8051_SAB_SFR_ADDAT = 0xD9,
    EM8051_SAB_SFR_DAPR = 0xDA,
    EM8051_SAB_SFR_P6 = 0xDB,
    EM8051_SAB_SFR_P4 = 0xE8,
    EM8051_SAB_SFR_P5 = 0xF8
};

/* Stable Siemens interrupt identities. Their numeric order is also the
 * documented equal-priority polling order. */
enum em8051_sab_irq_source
{
    EM8051_SAB_IRQ_INT0 = 0,
    EM8051_SAB_IRQ_TIMER0,
    EM8051_SAB_IRQ_INT1,
    EM8051_SAB_IRQ_TIMER1,
    EM8051_SAB_IRQ_UART,
    EM8051_SAB_IRQ_TIMER2,
    EM8051_SAB_IRQ_ADC,
    EM8051_SAB_IRQ_INT2,
    EM8051_SAB_IRQ_INT3,
    EM8051_SAB_IRQ_INT4,
    EM8051_SAB_IRQ_INT5,
    EM8051_SAB_IRQ_INT6,
    EM8051_SAB_IRQ_SOURCE_COUNT
};

enum em8051_sab_irq_vector
{
    EM8051_SAB_VECTOR_INT0 = 0x0003,
    EM8051_SAB_VECTOR_TIMER0 = 0x000B,
    EM8051_SAB_VECTOR_INT1 = 0x0013,
    EM8051_SAB_VECTOR_TIMER1 = 0x001B,
    EM8051_SAB_VECTOR_UART = 0x0023,
    EM8051_SAB_VECTOR_TIMER2 = 0x002B,
    EM8051_SAB_VECTOR_ADC = 0x0043,
    EM8051_SAB_VECTOR_INT2 = 0x004B,
    EM8051_SAB_VECTOR_INT3 = 0x0053,
    EM8051_SAB_VECTOR_INT4 = 0x005B,
    EM8051_SAB_VECTOR_INT5 = 0x0063,
    EM8051_SAB_VECTOR_INT6 = 0x006B
};

enum em8051_sab_irq_trace_event
{
    EM8051_SAB_IRQ_TRACE_REQUEST = 0,
    EM8051_SAB_IRQ_TRACE_ACCEPT,
    EM8051_SAB_IRQ_TRACE_RELEASE
};

struct em8051_sab_irq_trace_record
{
    enum em8051_sab_irq_trace_event event;
    uint64_t machine_cycle;
    uint16_t pc;
    enum em8051_sab_irq_source source;
    uint8_t priority;
    bool asserted;
    bool global_enabled;
    uint16_t pending_mask;
    uint16_t enabled_mask;
    uint16_t in_service_mask;
    uint8_t in_service_depth;
};

/* IRQ observers receive only an immutable record and caller-owned context. */
typedef void (*em8051sabirqtrace)(
    const struct em8051_sab_irq_trace_record *aRecord, void *aUser);

#define EM8051_SFR_UNAVAILABLE 0xFFFFu

struct em8051_variant_descriptor
{
    enum em8051_variant variant;
    const char *name;
    uint32_t default_oscillator_hz;
    bool has_upper_iram;
    uint16_t interrupt_enable0_sfr;
    uint16_t interrupt_priority0_sfr;
    uint16_t interrupt_enable1_sfr;
    uint16_t interrupt_priority1_sfr;
};

enum em8051_trace_type
{
    EM8051_TRACE_INSTRUCTION = 0,
    EM8051_TRACE_SFR_WRITE,
    EM8051_TRACE_MOVX_READ,
    EM8051_TRACE_MOVX_WRITE,
    EM8051_TRACE_UNSUPPORTED_MOVX_READ,
    EM8051_TRACE_UNSUPPORTED_MOVX_WRITE
};

struct em8051_trace_record
{
    enum em8051_trace_type type;
    uint64_t machine_cycle;
    uint16_t pc;
    uint16_t address;
    uint8_t value;
};

/* Trace observers receive only an immutable record and caller-owned context.
 * CPU storage is intentionally unreachable through this callback signature. */
typedef void (*em8051trace)(const struct em8051_trace_record *aRecord,
                            void *aUser);

enum em8051_stop_reason
{
    EM8051_STOP_INSTRUCTION_LIMIT = 0,
    EM8051_STOP_BREAKPOINT,
    EM8051_STOP_TARGET_PC,
    EM8051_STOP_EXCEPTION,
    EM8051_STOP_HALT
};

struct em8051_run_result
{
    enum em8051_stop_reason reason;
    uint64_t instructions;
    uint64_t machine_cycles;
    uint16_t pc;
    int exception_code;
};

enum em8051_load_result
{
    EM8051_LOAD_OK = 0,
    EM8051_LOAD_IO_ERROR = -1,
    EM8051_LOAD_SIZE_ERROR = -2,
    EM8051_LOAD_CONFIGURATION_ERROR = -3
};


struct em8051
{
    unsigned char *mCodeMem; // 1k - 64k, must be power of 2
    uint16_t mCodeMemMaxIdx;
    unsigned char *mExtData; // 0 - 64k, must be power of 2
    uint16_t mExtDataMaxIdx;
    unsigned char mLowerData[128]; // 128 bytes
    unsigned char *mUpperData; // 0 or 128 bytes; leave to NULL if none
    unsigned char mOwnedUpperData[128]; // owned by 8052/SAB80535 variants
    unsigned char mSFR[128]; // 128 bytes; (special function registers)
    uint16_t mPC; // Program Counter; outside memory area
    uint8_t mTickDelay; // How many ticks should we delay before continuing
    em8051operation op[256]; // function pointers to opcode handlers
    em8051decoder dec[256]; // opcode-to-string decoder handlers    
    em8051exception except; // callback: exceptional situation occurred
    em8051sfrread sfrread[128]; // callback array: SFR register being read
    em8051sfrwrite sfrwrite[128]; // callback array: SFR register written
    em8051xread xread; // callback: external memory being read
    em8051xwrite xwrite; // callback: external memory being written

    enum em8051_variant mVariant;
    uint32_t mOscillatorHz;
    uint32_t mResetSeed;
    bool mResetSeedConfigured;
    uint64_t mInstructionCount;
    uint64_t mMachineCycleCount;
    em8051trace trace;
    void *trace_user;
    em8051sabirqtrace sab_irq_trace;
    void *sab_irq_trace_user;
    bool mBreakpointEnabled;
    uint16_t mBreakpoint;
    bool mExceptionRaised;
    int mLastException;
    uint16_t mTracePC;
    bool mInInstruction;

    // Internal values for interrupt services etc.
    uint8_t mInterruptActive;
    // Stored register values for interrupts (exception checking)
    uint8_t int_a[2];
    uint8_t int_psw[2];
    uint8_t int_sp[2];

    /* SAB80535 interrupt-controller state is separate from the classic
     * two-level mInterruptActive representation. */
    uint16_t mSABIrqPending;
    uint16_t mSABIrqEnabled;
    uint16_t mSABIrqInService;
    uint8_t mSABIrqDepth;
    uint8_t mSABIrqSourceStack[4];
    uint8_t mSABIrqPriorityStack[4];
    uint8_t mSABIrqSavedACC[4];
    uint8_t mSABIrqSavedPSW[4];
    uint8_t mSABIrqSavedSP[4];
    uint8_t mSABIrqInhibitInstructions;

    // Internal handling of UART
    char serial_out[18]; // The shown size is only 18 chars
    uint8_t serial_out_idx;
    uint8_t serial_out_remaining_bits;
    bool serial_interrupt_trigger;
};

/* Select a stable CPU variant and perform a deterministic cold reset. The
 * caller supplies CODE/XDATA buffers as with the upstream API before calling
 * this function. SAB80535 and 8052 upper IRAM is owned by the CPU object. */
int em8051_init_variant(struct em8051 *aCPU, enum em8051_variant aVariant);

const struct em8051_variant_descriptor *em8051_get_variant_descriptor(
    enum em8051_variant aVariant);

/* Hardware power-on RAM is undefined. A configured seed produces repeatable
 * pseudo-random IRAM and SBUF contents on every cold reset. */
void em8051_set_reset_seed(struct em8051 *aCPU, uint32_t aSeed);

// Set the emulator into reset state. Must be called before tick(), as it also
// initializes the function pointers. aWipe performs a cold reset: CODE/XDATA
// are cleared and IRAM/SBUF use the configured deterministic power-on seed
// (legacy callers without a configured seed retain the upstream zero/rand
// behavior). A warm reset preserves RAM.
void reset(struct em8051 *aCPU, bool aWipe);

// run one emulator tick, or 12 hardware clock cycles.
// returns "true" if a new operation was executed.
bool tick(struct em8051 *aCPU);

enum em8051_stop_reason em8051_step_instruction(
    struct em8051 *aCPU, struct em8051_run_result *aResult);
enum em8051_stop_reason em8051_run(struct em8051 *aCPU,
                                      uint64_t aMaxInstructions,
                                      struct em8051_run_result *aResult);
enum em8051_stop_reason em8051_run_until_pc(struct em8051 *aCPU,
                                               uint16_t aTargetPC,
                                               uint64_t aMaxInstructions,
                                               struct em8051_run_result *aResult);
void em8051_set_breakpoint(struct em8051 *aCPU, uint16_t aPC, bool aEnabled);
void em8051_set_trace(struct em8051 *aCPU, em8051trace aTrace, void *aUser);

/* Install a record-only Siemens interrupt observer. */
void em8051_set_sab_irq_trace(struct em8051 *aCPU,
                              em8051sabirqtrace aTrace, void *aUser);

/* Set or clear the canonical request flag(s) for one generic SAB80535
 * interrupt source. This API models no physical pin, board or protocol. */
bool em8051_sab_irq_set_pending(struct em8051 *aCPU,
                                enum em8051_sab_irq_source aSource,
                                bool aPending);

// decode the next operation as character string.
// buffer must be big enough (64 bytes is very safe). 
// Returns length of opcode.
uint8_t decode(struct em8051 *aCPU, uint16_t aPosition, char *aBuffer);

// Load an intel hex format object file. Returns negative for errors.
int load_obj(struct em8051 *aCPU, char *aFilename);

/* Load a raw CODE image. Exactly 65536 bytes and a 64 KiB CODE buffer are
 * required; XDATA is never used or resized. */
int em8051_load_binary(struct em8051 *aCPU, const char *aFilename);

// Alternate way to execute an opcode (switch-structure instead of function pointers)
uint8_t do_op(struct em8051 *aCPU);

// Internal: Pushes a value onto the stack and reports whether it succeeded.
bool push_to_stack(struct em8051 *aCPU, uint8_t aValue);

/* Internal access gateways shared by the opcode engine and embedders. Invalid
 * CPU pointers or addresses below the SFR range are rejected: reads return FF
 * and writes have no effect. */
uint8_t em8051_sfr_read(struct em8051 *aCPU, uint8_t aAddress);
void em8051_sfr_write(struct em8051 *aCPU, uint8_t aAddress, uint8_t aValue);
void em8051_trace_emit(struct em8051 *aCPU, enum em8051_trace_type aType,
                       uint16_t aAddress, uint8_t aValue);
void em8051_raise_exception(struct em8051 *aCPU, int aCode);
/* Internal opcode hook: release one Siemens in-service entry after RETI. */
void em8051_sab_irq_reti(struct em8051 *aCPU, uint8_t aOriginalSP);


// SFR register locations
enum SFR_REGS
{
    REG_ACC = 0xE0 - 0x80,
    REG_B   = 0xF0 - 0x80,
    REG_PSW = 0xD0 - 0x80,
    REG_SP  = 0x81 - 0x80,
    REG_DPL = 0x82 - 0x80,
    REG_DPH = 0x83 - 0x80,
    REG_P0  = 0x80 - 0x80,
    REG_P1  = 0x90 - 0x80,
    REG_P2  = 0xA0 - 0x80,
    REG_P3  = 0xB0 - 0x80,
    REG_IP  = 0xB8 - 0x80,
    REG_IE  = 0xA8 - 0x80,
    REG_TMOD = 0x89 - 0x80,
    REG_TCON = 0x88 - 0x80,
    REG_TH0 = 0x8C - 0x80,
    REG_TL0 = 0x8A - 0x80,
    REG_TH1 = 0x8D - 0x80,
    REG_TL1 = 0x8B - 0x80,
    REG_SCON = 0x98 - 0x80,
    REG_SBUF = 0x99 - 0x80,
    REG_PCON = 0x87 - 0x80
};

enum PSW_BITS
{
    PSW_P = 0,
    PSW_UNUSED = 1,
    PSW_OV = 2,
    PSW_RS0 = 3,
    PSW_RS1 = 4,
    PSW_F0 = 5,
    PSW_AC = 6,
    PSW_C = 7
};

enum PSW_MASKS
{
    PSWMASK_P = 0x01,
    PSWMASK_UNUSED = 0x02,
    PSWMASK_OV = 0x04,
    PSWMASK_RS0 = 0x08,
    PSWMASK_RS1 = 0x10,
    PSWMASK_F0 = 0x20,
    PSWMASK_AC = 0x40,
    PSWMASK_C = 0x80
};

enum IE_MASKS
{
    IEMASK_EX0 = 0x01,
    IEMASK_ET0 = 0x02,
    IEMASK_EX1 = 0x04,
    IEMASK_ET1 = 0x08,
    IEMASK_ES  = 0x10,
    IEMASK_ET2 = 0x20,
    IEMASK_UNUSED = 0x40,
    IEMASK_EA  = 0x80
};

enum PT_MASKS
{
    PTMASK_PX0 = 0x01,
    PTMASK_PT0 = 0x02,
    PTMASK_PX1 = 0x04,
    PTMASK_PT1 = 0x08,
    PTMASK_PS  = 0x10,
    PTMASK_PT2 = 0x20,
    PTMASK_UNUSED1 = 0x40,
    PTMASK_UNUSED2 = 0x80
};

enum TCON_MASKS
{
    TCONMASK_IT0 = 0x01,
    TCONMASK_IE0 = 0x02,
    TCONMASK_IT1 = 0x04,
    TCONMASK_IE1 = 0x08,
    TCONMASK_TR0 = 0x10,
    TCONMASK_TF0 = 0x20,
    TCONMASK_TR1 = 0x40,
    TCONMASK_TF1 = 0x80
};

enum TMOD_MASKS
{
    TMODMASK_M0_0 = 0x01,
    TMODMASK_M1_0 = 0x02,
    TMODMASK_CT_0 = 0x04,
    TMODMASK_GATE_0 = 0x08,
    TMODMASK_M0_1 = 0x10,
    TMODMASK_M1_1 = 0x20,
    TMODMASK_CT_1 = 0x40,
    TMODMASK_GATE_1 = 0x80
};

enum IP_MASKS
{
    IPMASK_PX0 = 0x01,
    IPMASK_PT0 = 0x02,
    IPMASK_PX1 = 0x04,
    IPMASK_PT1 = 0x08,
    IPMASK_PS  = 0x10,
    IPMASK_PT2 = 0x20
};

enum SCON_MASKS
{
    SCONMASK_RI   = 0x01,
    SCONMASK_TI   = 0x02,
    SCONMASK_RB8  = 0x04,
    SCONMASK_TB8  = 0x08,
    SCONMASK_REN  = 0x10,
    SCONMASK_SM2  = 0x20,
    SCONMASK_SM1  = 0x40,
    SCONMASK_SM0  = 0x80,
};

enum ISR_VECTORS
{
    ISR_RST  = 0x00,
    ISR_INT0 = 0x03,
    ISR_TF0  = 0x0B,
    ISR_INT1 = 0x13,
    ISR_TF1  = 0x1B,
    ISR_SR   = 0x23,
#ifdef __8052__
    ISR_TF2  = 0x2B,
#endif // __8052__
};

enum EM8051_EXCEPTION
{
    EXCEPTION_STACK,  // stack address > 127 with no upper memory, or roll over
    EXCEPTION_ACC_TO_A, // acc-to-a move operation; illegal (acc-to-acc is ok, a-to-acc is ok..)
    EXCEPTION_IRET_PSW_MISMATCH, // psw not preserved over interrupt call (doesn't care about P, F0 or UNUSED)
    EXCEPTION_IRET_SP_MISMATCH,  // sp not preserved over interrupt call
    EXCEPTION_IRET_ACC_MISMATCH, // acc not preserved over interrupt call
    EXCEPTION_ILLEGAL_OPCODE     // for the single 'reserved' opcode in the architecture
};

enum SAB_IEN1_MASKS
{
    SAB_IEN1MASK_EADC = 0x01,
    SAB_IEN1MASK_EX2 = 0x02,
    SAB_IEN1MASK_EX3 = 0x04,
    SAB_IEN1MASK_EX4 = 0x08,
    SAB_IEN1MASK_EX5 = 0x10,
    SAB_IEN1MASK_EX6 = 0x20,
    SAB_IEN1MASK_SWDT = 0x40,
    SAB_IEN1MASK_EXEN2 = 0x80
};

enum SAB_IRCON_MASKS
{
    SAB_IRCONMASK_IADC = 0x01,
    SAB_IRCONMASK_IEX2 = 0x02,
    SAB_IRCONMASK_IEX3 = 0x04,
    SAB_IRCONMASK_IEX4 = 0x08,
    SAB_IRCONMASK_IEX5 = 0x10,
    SAB_IRCONMASK_IEX6 = 0x20,
    SAB_IRCONMASK_TF2 = 0x40,
    SAB_IRCONMASK_EXF2 = 0x80
};

#endif /* EMU8051_H */


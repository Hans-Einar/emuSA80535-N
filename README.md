emu8051
=======

8051/8052 emulator with curses-based UI

Binaries and info: http://iki.fi/sol/8051.html

Note on git history - when I wrote this, I kept version backups as zip files; I created the version history by submitting each of the zips in order. Apologies for the submit messages..

What
====

This is a simulator of the 8051/8052 microcontrollers. For sake of simplicity, I'm only referring to 8051, although the emulator can emulate either one. For more information about the 8-bit chip(s), please check out www.8052.com or look up the data sheets. Intel, being the originator of the architecture, naturally has information as well.

The 8051 is a pretty easy chip to play with, in both hardware and software. Hence, it's a good chip to use as an example when teaching about computer hardware. Unfortunately, the simulators in use in my school were a bit outdated, so I decided to write a new one.

The scope of the emulator is to help test and debug 8051 assembler programs. What is particularily left out is clock-cycle exact simulation of processor pins. (For instance, MUL is a 48-clock operation on the 8051. On which clock cycle does the CPU read the operands? Or write the result?). Such simulation might help in designing some hardware, but for most uses it is unneccessary and complicated.

The emulator is designed to have two separate modules, consisting of the emulator core and separate front-end. This enables the creation of different kinds of front-ends. For instance, this lets the user use the emulator core as a DLL in a C/C++ application which can simulate other kinds of hardware (such as leds, switches, displays, audio, or whatnot).

Simulation accuracy is valued over speed. Nevertheless, already at v.0.1 the emulator could run at over-realtime speeds on a P4/2.6GHz (running the emulator at over 12MHz). Based on profiler output, over half of the processing time is wasted on pipeline trashing when branching to the opcode functions. This could possibly be helped by JITing the code, but that is considered unneccessary at this point. Also, CPUs with shorter pipelines are not harmed by this behavior as badly.

License
=======

The emulator core is written completely in ANSI C for portability, and the sources are available under the MIT license.

Copyright 2006 Jari Komppa

Permission is hereby granted, free of charge, to any person obtaining 
a copy of this software and associated documentation files (the 
"Software"), to deal in the Software without restriction, including 
without limitation the rights to use, copy, modify, merge, publish, 
distribute, sublicense, and/or sell copies of the Software, and to 
permit persons to whom the Software is furnished to do so, subject 
to the following conditions: 

The above copyright notice and this permission notice shall be included 
in all copies or substantial portions of the Software. 

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
IN THE SOFTWARE. 

Features
========

Current features include:

- Full 8051 instruction set.

- ncurses-based UI - works fine over SSH for instance.

- The main view includes:
    - Memory view.
    - Stack view.
    - Opcode and disassembly view.
    - History view of SP, P0, P1, P2, P3, IP, IE, TMOD, TCON, TH0, TL0, TH1, TL1, SCON, PCON, A, B, R0, R1, R2, R3, R4, R5, R6, R7 and DPTR, as well as all processor status bits.
    - Cycle and real-time counter.

- Other views include:
    - Logic board (leds'n'switches) view, with optional widgets such as 7-seg displays and 44780-style text output
    - Memory editor, showing all five types of memory at the same time
    - Options, where user can disable debug exceptions etc.
- Support for all sorts of 8051 memory combinations - 128 or 256B internal RAM, 0-64k of external RAM and 0-64k of ROM. External RAM and ROM may even point at the same memory, enabling self-modifying code.
- Loads Intel HEX files.
- Support for exceptions on invalid instructions, odd stack behavior, and messing up important registers in interrupts. One breakpoint is also supported.
- The emulator performs callbacks on register area or external memory read/write, which can be used to implement simulation of new special features or whatever is connected to the IO ports.
- Timer 0 and 1 modes 0, 1, 2 and 3, as well as interrupt priorities.

Core-only Stage-0 API
=====================

The repository also provides a curses-independent core test gate:

    make core-test

The gate compiles `core.c`, `opcodes.c`, `disasm.c`, and `binary_loader.c`
with focused classic and SAB80535-N tests. The normal `make` target remains the
upstream curses frontend build.

`em8051_init_variant()` selects classic 8051, classic 8052, or SAB80535. The
SAB variant owns its upper 128 bytes of IRAM and keeps indirect addresses
`80..FF` separate from direct SFR addresses. Its default oscillator is
11.0592 MHz. The public variant descriptor distinguishes classic `IP=B8` from
SAB80535 `IEN1=B8`.

The deterministic embedding surface provides an exact 65536-byte raw CODE
loader, 64-bit instruction and machine-cycle counters, bounded run and
run-until-PC calls, a breakpoint, typed stop reasons, and optional normalized
instruction/SFR-write/MOVX trace records. Missing MOVX backing is reported as
an unsupported trace record. Trace callbacks receive only an immutable trace
record and caller-owned user context; the callback signature exposes no CPU
pointer or CPU-owned memory.
Specialized accumulator/PSW opcode forms and
internal peripheral SFR changes do not yet pass through the SFR-write trace
gateway, so Stage 0 does not claim complete SFR observation.
Step and bounded-run calls return at a completed machine-cycle boundary: all
cycles credited to the last instruction have advanced virtual time and classic
timers, and no pending instruction delay remains.

Hardware power-on IRAM and SBUF contents are undefined. The original `reset()`
entry point remains available for classic callers; explicit variant
initialization configures a deterministic default seed. Call
`em8051_set_reset_seed()` before a cold `reset(cpu, true)` to reproduce a
chosen pseudo-random power-on state. A warm `reset(cpu, false)` preserves IRAM.
For SAB80535, documented P4/P5 reset-high values are modeled as hardware facts.
Zeroed IP0/IP1/ADCON fields and the high input-only P6 model byte are
deterministic Stage-0 placeholders for indeterminate or unspecified hardware
state, not claims about physical reset values or a P6 output latch.

SAB80535 interrupt-controller API
==================================

The SAB variant implements the twelve architectural interrupt sources in
fixed polling order, the canonical `IEN0/IP0/IEN1/IP1/IRCON` register map and
four priority levels formed from paired `IP1.x:IP0.x` bits. Pending, enabled
and in-service state are independent. Only a strictly higher priority may
nest, `RET` preserves in-service state, and `RETI` releases exactly the top
entry. Classic 8051/8052 interrupt handling continues to use classic `IP=B8`.

`em8051_sab_irq_set_pending(cpu, source, pending)` is the generic deterministic
producer seam. It sets or clears the canonical request flag for one CPU source
without modeling a physical pin, protocol or board. UART maps to RI for a
synthetic assertion and Timer 2 maps to TF2; clearing either aggregate source
clears both of its canonical request flags. Existing peripheral logic may also
assert TCON, SCON or IRCON flags directly and arbitration consumes them at the
next architectural boundary.

`em8051_set_sab_irq_trace()` installs an optional record-only observer. Each
immutable IRQ record contains virtual cycle, PC, event/source, selected
priority, pending/enabled/in-service masks, global-enable state and in-service
depth. Observation has no CPU pointer and does not alter execution.

Vector entry auto-clears edge-mode IE0/IE1, TF0/TF1 and IEX2..IEX6. Level-mode
IE0/IE1, UART RI/TI, ADC IADC and Timer-2 TF2/EXF2 remain asserted until their
canonical SFR flags are cleared. TF2 is gated by IEN0.ET2 and EXF2 by
IEN1.EXEN2. Siemens arbitration is held off until one further instruction has
executed after `RETI` or a write to IEN0, IEN1, IP0 or IP1.

The controller does not add UART mode-3 timing, ADC conversion, Timer-2
counting, GPIO edge sampling or live I/O.

Deterministic Timer0/Timer1 timing
=================================

The shared classic timer engine advances from virtual machine cycles, not host
wall time. Timer0 mode 1 provides a live 16-bit `TH0:TL0` counter; the SAB
`DCEF` scheduler reload reaches `TF0` after exactly 8977 eligible cycles.
Timer1 mode 2 reloads `TL1` from the current `TH1`; an `FD` reload produces an
overflow every three eligible machine cycles. Live SFR writes, interrupt-entry
cycles and ISR/software-reload latency remain visible.

Each Timer0 mode-1 and Timer1 mode-2 wrap increments a resettable 64-bit
overflow count. `em8051_set_timer_overflow_callback()` optionally observes an
immutable record containing the timer identity, completed machine cycle and
post-wrap/reload bytes. Overflow events repeat independently of sticky
`TF0`/`TF1` and of the observer callback.

SAB80535 mode-3 9-bit UART
==========================

When SAB mode 3 selects Timer1 rather than the dedicated baud generator, every
internal Timer1 mode-2 overflow advances a continuous integer serial phase.
`PCON.SMOD=1` divides the overflow stream by 16 and `SMOD=0` by 32. Thus the
11.0592 MHz, `TH1=FD`, SMOD=1 configuration produces one bit every 48 machine
cycles, exactly 19200 bits/s, without host time or floating-point scheduling.

SAB SBUF reads and writes use physically separate modeled storage. A write
captures the transmit byte and current TB8 without overwriting unread receive
data. TX begins on the next divider boundary and emits START, D0..D7, TB8 and
STOP; TI rises when STOP begins and remains software-clear. One pending write
can produce a contiguous next frame after the complete STOP interval.

`em8051_sab_uart_inject_rx_frame(cpu, data, ninth)` begins one deterministic
valid in-memory receive frame when mode 3 and REN are enabled. Its independent
receive phase updates receive SBUF, RB8 and RI at STOP start only when RI is
clear and SM2 accepts the ninth bit. No RxD/TxD electrical model or live serial
transport is opened. `em8051_set_sab_uart_trace()` optionally observes
immutable logical frame records with exact virtual-cycle boundaries. RI and TI
feed only the existing shared SAB vector `0023` and remain software-clear.

Install
=======

You need to install the ncurses lib development files, which is the only dependency.

On Debian/Ubuntu it is simply done as

    sudo apt-get install libncurses5 libncurses5-dev


Code Style
==========

Arguments to functions are prefixed with "a", such as in

    static int read_mem(struct em8051 *aCPU, int aAddress)

Local variables are using standard [snake case](https://wikipedia.org/wiki/Snake_case).

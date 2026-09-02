# EMU-SAB80535-DES-009 — Deterministic Siemens Timer2 design

Status: active target design
Design items: DES-086..DES-095
Established: 2026-09-02
Steering authority: Issue #17 SLC-015

## Authority and stop boundary

SLC-015 implements only the reached REQ-014 Timer2 timer-function path.
Primary behavioral authority is the Siemens *SAB 80515/SAB 80C515 Family
User's Manual*, Edition 08.95, Timer2 section 7.5 and interrupt section 8.
Ponsse PR #25 HEAD `19ef6ff45efa719612b70e70b8c31b9cb2ebb7e9`
and `doc/P1000/` are consumer evidence only.

External-counter/gated modes, CRC/T2EX reload, EXF2 production,
compare/capture behavior, automatic P5.4 coupling, target/board logic,
physical/live I/O and `emu-debug` protocol changes are outside this design.

## DES-086 — Canonical Siemens register layout

Timer2 uses the existing Siemens SFRs T2CON C8, CRCL CA, CRCH CB, TL2 CC,
TH2 CD, IRCON C0 and IEN0 A8. T2CON is interpreted as:

```text
bit 7 T2PS | bit 6 I3FR | bit 5 I2FR | bit 4 T2R1
bit 3 T2R0 | bit 2 T2CM | bit 1 T2I1 | bit 0 T2I0
```

SLC-015 behaviorally owns only T2PS, T2I1:T2I0, live TL2:TH2 and TF2 for the
reached reload-disabled timer path. I2FR/I3FR remain owned by accepted SLC-013.
T2CM and reload bits remain software-visible register state.

## DES-087 — Timer-function source and prescaler

T2I1:T2I0 selects the input source. SLC-015 produces counts only for `01`
(timer function). `00` stops the counter. `10` external-counter and `11` gated
modes retain register state but produce no guessed counts in this Slice.

For timer function:

- T2PS=0 increments once per CPU machine cycle (`fosc/12`);
- T2PS=1 increments once per two CPU machine cycles (`fosc/24`).

A bounded CPU-owned divider phase is used for /24. Any transition of the
clock-selection tuple `{T2PS,T2I1,T2I0}` resets that divider phase before the
new mode consumes machine cycles. Thus a newly selected /24 mode requires two
full machine cycles before its first increment; /12 increments on the first
machine cycle in the selected mode. This deterministic transition convention
makes no claim about undocumented sub-cycle prescaler phase.

## DES-088 — Reload-disabled live counter

With T2R1=0, Timer2 is a 16-bit up-counter whose canonical value is TH2:TL2.
Every eligible input tick increments the live SFR bytes. `FFFF -> 0000` is an
overflow event. There is no hardware reload and CRCL/CRCH are not copied.

Independent software writes to TL2 or TH2 immediately change that byte of the
live counter. Sequential low/high or high/low writes are not made atomic by the
emulator; a timer tick between writes observes the intermediate architectural
value naturally.

If T2R1=1, SLC-015 does not create reload behavior or a substitute timer
producer. The software-visible bits and registers remain intact so future
requirements can promote the mode without hidden approximation.

## DES-089 — Overflow and canonical TF2

Each reload-disabled wrap:

1. leaves TH2:TL2 at 0000 after the increment;
2. sets canonical IRCON.TF2;
3. increments a 64-bit Timer2 overflow-event counter;
4. emits an optional immutable overflow record;
5. synchronizes the accepted interrupt-controller pending state.

TF2 is sticky/software-clear. A later wrap is still an observable overflow
event while TF2 is already set; event production must never be inferred from a
TF2 0-to-1 transition.

## DES-090 — Existing interrupt controller is the only service path

Timer2 product code only produces TF2. The accepted controller owns ET2/EAL,
paired priority, polling, preemption, in-service state, vector 002B, inhibit and
RETI. Vector entry and RETI do not clear TF2.

Counting and TF2 production continue while ET2 or EAL is disabled. A pending
TF2 becomes serviceable through normal arbitration if enables later permit it.
No Timer2 helper may directly change PC or invoke firmware code.

## DES-091 — Machine-cycle integration

Timer2 advances from the common virtual machine-cycle progression, beside the
accepted Timer0/Timer1 and ADC producers and before the shared machine-cycle
counter is committed. An overflow record therefore names the completed cycle
as `mMachineCycleCount + 1`, matching the accepted timer/ADC time convention.

Timer2 advances through multi-cycle instructions, interrupt entry/ISR cycles
and accepted IDLE peripheral progression. Host wall time, floating point,
threads and operating-system timers are forbidden.

## DES-092 — Start, stop and live configuration changes

`T2I1:T2I0=00` stops Timer2 without changing TL2, TH2 or TF2. Selecting timer
function resumes from the current live counter value; there is no implicit
reload/reset. Clock-selection changes reset only the bounded /24 divider phase
per DES-087.

I2FR/I3FR or unrelated T2CON bit writes do not reset Timer2 clock phase because
they do not change the clock-selection tuple. SLC-013 external-edge semantics
must therefore remain unchanged while sharing T2CON.

## DES-093 — Immutable diagnostics

Expose a SAB Timer2-specific record-only observer and a monotonic 64-bit
overflow-event count. Each overflow record contains at least:

- completed machine cycle;
- post-wrap TL2 and TH2;
- T2CON snapshot;
- TF2 state;
- cumulative overflow count.

Observer presence is behavior-neutral and exposes no mutable CPU storage.
Classic variants report no Siemens Timer2 events.

## DES-094 — P5.4 and consumer boundary

Timer2 overflow has no automatic connection to P5.4 in SLC-015. P5.4 remains a
normal accepted SAB port bit. The P1000 consumer sequence is evidence that
firmware reaches vector 002B and then changes P5.4 with ordinary instructions;
that target meaning is not embedded in the CPU peripheral.

A generic ISR fixture may stop Timer2, modify any ordinary port bit, write
TL2/TH2, software-clear TF2, restart timer function and RETI. Product code and
generic tests must contain no connector/valve semantics.

## DES-095 — Verification and exclusions

Focused tests cover T2CON masks, stopped state, /12 and /24 timing, exact 5555
count-to-overflow, FFFF wrap, repeated sticky-TF2 overflows, masking and
interrupt service, live byte writes, software stop/restart/reload, deterministic
clock-mode phase, unsupported-mode non-production, SLC-013 I2FR/I3FR
regression, replay/observer neutrality, classic isolation and generic ISR
behavior.

All accepted Stage0/IRQ/Timer0/Timer1/UART/ports/edges/ADC/debug regressions
remain mandatory. No reload producer, EXF2 producer, capture/compare, automatic
P5.4 action, target policy or live/physical I/O is introduced.

## Traceability

DES-086..DES-095 refine ARCH-002..ARCH-006 and implement the authorized
reload-disabled Timer2 portion of REQ-014 through SPR-009 / ITR-015 / SLC-015.

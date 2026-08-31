# EMU-SAB80535-DES-003 — Timer0/Timer1 deterministic timing design

Status: active target design
Design items: DES-019..DES-026
Established: 2026-08-31
Steering authority: Issue #2 SLC-006 checkpoint

## Authority and boundary

SLC-006 realizes REQ-010 only. Siemens documents Timer0/Timer1 as compatible
with the classic 8051 for the used modes. The existing shared `timer_tick()` is
therefore retained and corrected narrowly; no parallel SAB timer engine is
created.

Authority is the Siemens SAB80515/80535 Timer0/Timer1 and serial-baud chapters,
Ponsse PR #25 commit `7891aceb2b713659ea9936b9743a5aee73579ae9`,
and the latest SteeringGroup checkpoint on Issue #2.

## DES-019 — One timer step per machine cycle

An eligible internal timer increments once for each virtual machine cycle
advanced by `tick()`, including instruction delay cycles, interrupt-entry
cycles, ordinary ISR cycles and IDLE cycles where the architecture continues
peripherals. Host wall time and floating-point scheduling are forbidden.

For the authorized modes eligibility requires `TRx=1`, `C/T=0`, `GATE=0`.
Counter input and gated-pin behavior remain explicit non-goals.

## DES-020 — Timer0 mode 1 live counter

Timer0 mode 1 treats TH0:TL0 as one live 16-bit up-counter. `FFFF -> 0000`
sets sticky TF0 and emits one Timer0 overflow event. A load of `DCEF` reaches
overflow after exactly `0x10000-0xDCEF = 8977` eligible timer steps.

The accepted interrupt controller consumes TF0 and clears it only on accepted
vector `000B`. Timer counting itself never clears TF0.

## DES-021 — Live SFR writes and software reload latency

TL0/TH0/TL1/TH1 remain normal SFRs. A write changes that register at the
instruction's architectural write point; the timer continues on subsequent
eligible machine cycles. Timer0 software reload is two independent byte writes,
so interrupt entry and ISR instructions naturally contribute time and the next
overflow period is not replaced with a fixed host-side interval.

## DES-022 — Timer1 mode 2 auto-reload

Timer1 mode 2 increments TL1 as an 8-bit up-counter. On `FF -> overflow`, TL1
reloads from the then-current TH1, sticky TF1 is set, and one Timer1 overflow
event is emitted. TH1 writes do not alter the current TL1 value; they affect the
next reload.

With TH1/TL1 at `FD`, overflows occur every three eligible machine cycles. At
11.0592 MHz the integer machine-cycle rate is 921600 Hz and event rate is
307200/s. No floating-point accumulator is used.

## DES-023 — Overflow event seam independent of flags

Sticky TF0/TF1 flags and overflow events are distinct. Every wrap increments a
monotonic 64-bit per-timer overflow counter and may emit a record-only generic
timer overflow callback. Events repeat even while TF1 is already set or Timer1
interrupts are disabled.

The seam contains timer identity, machine cycle and post-overflow/reload timer
state. It does not shift UART data or touch SBUF, TB8, RB8, RI or TI.

## DES-024 — Interrupt-controller integration

TF0/TF1 requests flow through the accepted SLC-004 controller. ET0/ET1 and EAL
gate acceptance without stopping timer counting or erasing flags. Accepted
vectors `000B`/`001B` clear TF0/TF1 as already verified. Timer0 continues during
entry/ISR cycles.

## DES-025 — Shared classic compatibility

Corrections should use shared helper functions where semantics are identical.
Classic 8051/8052 mode1/mode2 values, flags and legacy serial shortcut must not
regress. The SAB variant continues to exclude the inherited classic serial
shortcut; future UART consumes overflow events instead.

## DES-026 — Diagnostics and stop boundary

Timer event observation is immutable and behavior-neutral. Tests use integer
cycle counts and bounded execution. This Slice does not implement UART frame
state, divide-by-16 shifting, GPIO edges, ADC, Timer2, board behavior or any
physical/live transport.

## Traceability

DES-019..DES-026 refine ARCH-003, ARCH-004, ARCH-005 and ARCH-006, satisfy
REQ-010, and constrain SPR-003 / ITR-006 / SLC-006.


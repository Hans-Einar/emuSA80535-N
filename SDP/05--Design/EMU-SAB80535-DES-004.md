# EMU-SAB80535-DES-004 — Mode-3 9-bit UART design

Status: active target design
Design items: DES-027..DES-038
Established: 2026-09-01
Steering authority: Issue #2 SLC-007 checkpoint

## Authority and stop boundary

SLC-007 realizes REQ-011 only. Authority is the Siemens SAB80515/80535 serial
interface chapter (SBUF/SCON, Timer1 baud generation and modes 2/3), Ponsse
PR #25 commit `7891aceb2b713659ea9936b9743a5aee73579ae9`, accepted DES-002/DES-003,
and the latest Issue #2 SteeringGroup checkpoint.

No GPIO/pin electrical model, ADC, Timer2 producer, Stage 2, P1000 policy or
host/live serial transport is introduced.

## DES-027 — Internal Timer1 overflow consumer

The SAB mode-3 UART consumes every internal Timer1 mode2 overflow emitted by
the accepted SLC-006 producer. It does not infer pulses from sticky TF1, host
time, floating point or the public observer callback. Public timer observation
remains independent and behavior-neutral.

Timer1 is used when mode 3 is selected and the dedicated Siemens internal baud
generator is not selected. The dedicated generator is outside this Slice.

## DES-028 — Continuous serial divider phase

A deterministic serial divider phase advances on Timer1 overflows regardless
of whether TX/RX is active. `PCON.SMOD=1` wraps after 16 overflows; `SMOD=0`
wraps after 32. A wrap is one UART bit-time boundary.

With TH1=FD at 11.0592 MHz, overflows are 307200/s. Divide by 16 gives exactly
19200 bit boundaries/s and 48 machine cycles/bit. State is integer-only and
reset deterministically.

RX start detection owns an independent receive subphase reset, matching the
documented receive alignment; TX uses the continuously running global phase.

## DES-029 — Physically separate SBUF roles

For the SAB variant, writing address 99 captures transmit-buffer data and does
not overwrite unread receive data. Reading address 99 returns the receive-side
buffer. Existing SFR callbacks and trace records still observe architectural
reads/writes without merging the two storage roles.

Classic 8051/8052 behavior remains unchanged.

## DES-030 — TX pending and in-flight latches

An architectural SBUF write in mode 3 captures both byte and current SCON.TB8
into a pending transmit slot. Starting a frame transfers them into immutable
in-flight latches. Later TB8 or SBUF changes do not alter the current frame.

One pending next frame may be written while a frame is active. A later write to
that pending slot replaces the not-yet-started value deterministically; it
never truncates the in-flight frame.

## DES-031 — Mode-3 TX frame and TI timing

TX emits eleven logical wire bits: START=0, D0..D7 LSB first, latched TB8, and
STOP=1. A pending frame begins at the next serial-divider wrap after SBUF write.

At each subsequent bit boundary the wire-bit index advances. Hardware sets
canonical SCON.TI when STOP begins and TI remains set until software clears it.
The stop interval lasts one complete bit time. The frame becomes idle at the
next boundary.

## DES-032 — Back-to-back TX during STOP

If software writes TB8/SBUF after TI rises but before STOP ends, the next frame
remains pending. At the boundary that ends STOP, the new START begins, producing
contiguous frames with one full stop interval and no truncation.

## DES-033 — Immutable TX/frame diagnostics

A record-only callback may expose TX start, each logical bit boundary, TI/STOP
start and frame end, including completed machine cycle, byte, ninth bit,
bit index and bit value. It must not expose mutable CPU storage or open a host
serial device.

## DES-034 — Deterministic receive injection

A generic in-memory API injects one valid mode-3 framed byte+ninth-bit at a
virtual start boundary. It models no physical RxD edge or stop-bit error.

`REN=0` rejects a new start. `REN=1` starts independent timed RX progression
using Timer1 overflow cadence and a receive divide subphase reset at injection.
Only one injected frame may be active; a concurrent second start is rejected.

## DES-035 — RX final-shift acceptance

After ten complete bit intervals (START + eight data + ninth), the final shift
occurs at the beginning of the STOP interval. Receive SBUF/RB8/RI update only
when RI=0 and (`SM2=0` or received ninth bit=1). Otherwise the frame is lost
without overwriting prior receive data/RB8 or setting a new RI.

One bit interval later RX returns idle. The injected STOP value is fixed valid
but irrelevant to SBUF/RB8/RI, matching mode-3 semantics.

## DES-036 — Full-duplex independence

TX and RX state machines, storage, divider/subphase and completion are
independent. Simultaneous activity cannot mutate the other direction's latched
byte/ninth bit. Both consume the same Timer1 overflow stream deterministically.

## DES-037 — Canonical shared UART interrupt

Hardware TX sets SCON.TI; accepted RX sets SCON.RI. The accepted interrupt
controller's existing `(RI || TI)` source and vector `0023` are the only
interrupt path. RI/TI remain software-clear; clearing one while the other stays
set keeps the request pending; RETI does not clear either. Software SETB TI is
identical to hardware TI for arbitration.

## DES-038 — Reset, modes and exclusions

Reset clears UART internal state/dividers/queues deterministically while
preserving documented SBUF undefined-data handling. SLC-007 behavior is enabled
only for SAB mode 3 with Timer1 source. Unsupported/different modes do not gain
unreviewed behavior.

No transport backend, host wall-clock scheduling, RxD/TxD pin model, physical
GPIO, protocol interpretation or P1000 identity is present.

## Traceability

DES-027..DES-038 refine ARCH-002/003/004/005/006, satisfy REQ-011, and
constrain SPR-004 / ITR-007 / SLC-007.


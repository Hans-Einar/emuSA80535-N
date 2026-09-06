# EMU-SAB80535-DES-008 — Deterministic MYMOS ADC design

Status: active current design
Design items: DES-074..DES-085
Established: 2026-09-02
Steering authority: Issue #13 SLC-014

## Authority and stop boundary

SLC-014 implements only REQ-013's deterministic single-conversion ADC path.
Primary behavioral authority is the Siemens *SAB 80515/SAB 80C515 Family
User's Manual*, Edition 08.95, especially sections 2.1.4/2.1.5 and 7.4,
figures 7-26..7-32 and tables 7-6/7-7 on pages 72..81. Issue #13 fixes the
target-generation MYMOS conversion time and accepted architectural boundary.

Ponsse PR #25 branch `codex/p1000-386am-reconstruction` at exact evidence HEAD
`19ef6ff45efa719612b70e70b8c31b9cb2ebb7e9` and current `doc/P1000/` are
consumer evidence only. No target sensor, connector, voltage/calibration,
board, NEC/D71055 or machine meaning may enter product code.

Siemens Timer2, capture/compare, continuous ADC chaining, physical/live I/O and
changes to the frozen `emu-debug` protocol are outside this design.

## DES-074 — Canonical register and bit ownership

The SAB variant owns ADCON D8, ADDAT D9, DAPR DA and the existing IRCON.IADC /
IEN1.EADC request path. ADCON is:

```text
bit 7 BD   existing UART/clock selection state
bit 6 CLK  existing clock state
bit 5      reserved/non-ADC state
bit 4 BSY  ADC hardware status, software-write ignored
bit 3 ADM  conversion mode
bit 2..0   MX2..MX0 channel AN0..AN7
```

ADC hardware updates may change only BSY. ADCON writes preserve the current
hardware BSY value while committing all writable/non-ADC fields, including BD
and CLK. ADDAT remains software-readable/writable while idle and is overwritten
only by a successful conversion result.

## DES-075 — Generic normalized input API

Expose AN0..AN7 as generic channels using an unsigned 16-bit normalized sample
over the external VAGND..VAREF span: 0 is external VAGND and 65535 is external
VAREF. The CPU stores one deterministic value per channel; reset initializes
all eight values to zero. Invalid CPU/variant/channel input fails without
mutation. The API opens no device and assigns no physical voltage or target
meaning.

The selected channel and its normalized input are latched when the first
machine-cycle progression of a newly armed conversion begins. Later input or
ADCON changes do not alter that active conversion.

## DES-076 — DAPR write and callback transaction

Every architectural DAPR write commits the byte through the established SFR
gateway and arms a new conversion independently of the written value. A write
while active supersedes the old conversion; it never queues two completions.

DAPR start processing occurs after the established mutable callback returns.
Nested/reentrant DAPR writes are coalesced at the outermost gateway boundary so
the final canonical DAPR/ADCON state arms exactly one start/restart. A callback
must not produce duplicate, stale or ghost completions. The write itself does
not clear an existing software-owned IADC request.

## DES-077 — Single-conversion mode boundary

ADM=0 performs exactly one conversion and returns idle after result transfer.
SLC-014 stores ADM but does not implement continuous auto-restart. If ADM=1 is
present, a DAPR write still performs the one explicitly triggered conversion,
but no automatic next conversion is created; trace state makes the unsupported
continuous request visible. This is a bounded compatibility behavior, not a
claim of complete continuous-mode support.

## DES-078 — Exact MYMOS virtual-cycle timing

The first `advance_machine_cycle` progression after an outermost DAPR write is
conversion cycle 1: BSY asserts, and channel/DAPR/input context is latched.
The converter advances once for every CPU machine cycle, including instruction
delay cycles, interrupt entry, ISR instructions and IDLE cycles where the
accepted CPU continues peripherals. Host time, threads and floating point are
forbidden.

At the completion of conversion cycle 15, before the next interrupt arbitration
boundary, the model atomically transfers ADDAT for a valid conversion, clears
BSY and asserts IADC. No result/request is exposed earlier. This is the explicit
Issue #13 MYMOS architectural boundary. Siemens figure 7-32 describes generic
early internal IADC/BSY anticipation but supplies no numbered MYMOS offsets;
SLC-014 does not invent unquantified subphases. The independent Reviewer must
adjudicate this documented modeling choice against the primary manual.

## DES-079 — Reference programming and validity

DAPR low nibble selects internal VIntAGND and high nibble VIntAREF in sixteenth
steps of the external normalized span:

- lower `0` selects external VAGND; otherwise valid lower values are 1..12;
- upper `0` selects external VAREF (effective step 16); otherwise valid upper
  values are 4..15;
- a valid pair has at least four ladder steps between effective lower and upper
  references, matching the manual's minimum documented example constraint;
- any other combination is explicitly diagnostic/unsupported.

Even unsupported DAPR programming still starts and consumes the normal timing.
At completion it preserves the previous ADDAT, clears BSY, sets IADC and emits
an invalid-reference completion record. This avoids inventing an analog result
while preserving the documented start and request lifecycle.

## DES-080 — Deterministic conversion arithmetic

For valid references, keep all arithmetic integer/rational. Let normalized
input `S` be 0..65535, lower/upper effective ladder steps `L/U` be 0..16, and
compare `16*S` with `65535*L/U`.

- input at/below the lower endpoint returns `00`;
- input at/above the upper endpoint returns `FF`;
- in range returns
  `floor(((16*S - 65535*L) * 256) / (65535*(U-L)))`, clamped to 0..255.

This implements the manual's 256-code resolution without floating point and
defines exact cross-platform endpoint and rounding behavior.

## DES-081 — Restart and live-register interactions

A DAPR write while BSY=1 discards the old elapsed/context state and arms one
new conversion beginning on the next machine-cycle progression. BSY remains
hardware-owned; no old completion may later fire. The restarted conversion
latches the final new DAPR, current MX2..MX0 and selected input.

ADCON channel/ADM writes during an active conversion affect later starts only.
BD, CLK and reserved state remain live/preserved. Existing IADC survives a
restart and is set again at the new completion if software has not cleared it.

## DES-082 — Canonical interrupt integration

Completion asserts only IRCON.IADC. The accepted controller owns EADC/EAL
gating, paired priority, polling, preemption, in-service state, entry vector
`0043`, inhibit and RETI. ADC code never changes PC or invokes an ISR.

IADC is software-clear. DAPR writes, vector entry and RETI do not clear it.
Completion while EADC or EAL is disabled remains pending and becomes eligible
through the normal controller when enabled.

## DES-083 — Reset and variant isolation

Reset cancels active/pending ADC work and clears elapsed/latched state, BSY,
ADDAT, DAPR and all normalized inputs according to the deterministic emulator
reset model. It emits no ADC event. The existing reset handling for BD/CLK and
P6 remains authoritative.

Classic 8051/8052 variants reject the SAB ADC input API and gain no DAPR start,
cycle state, ADDAT update, BSY or IADC producer behavior.

## DES-084 — Immutable ADC diagnostics

An optional record-only observer exposes START, RESTART and COMPLETE values
with at least machine cycle, channel, DAPR, normalized latched input, ADDAT,
BSY, IADC, reference validity and ADM/continuous-request state. Records are
fully initialized value data and cannot reach mutable CPU storage. Null and
active observers produce identical final CPU state and normalized traces.

START/RESTART is emitted when the first conversion cycle begins, not merely
when DAPR is written. COMPLETE is emitted at the cycle-15 architectural update.

## DES-085 — Consumer fixture, regressions and exclusions

A bounded generic ROM-style fixture uses ADCON=00, DAPR start, AN0, exact
15-cycle completion, IADC pending, real vector `0043`, ISR-style software clear,
ADDAT read and RETI. P1000 addresses, diameter/calibration names and board
routes remain outside product code.

All accepted Stage0/IRQ/Timer/UART/ports/edges/debug-runtime/DAP regressions
remain mandatory. No Timer2 producer, capture/compare, board/NEC logic,
physical/live I/O or frozen debug-protocol change is introduced.

## Traceability

DES-074..DES-085 refine ARCH-002..ARCH-006 and implement REQ-013 through
SPR-008 / ITR-014 / SLC-014.

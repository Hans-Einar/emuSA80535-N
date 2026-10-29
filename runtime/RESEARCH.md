# Execution research and recovery

This guide describes accepted runtime 028 and the separately selected 031
extension. For a new owned research session, verify runtime/image/module/config
hashes and the selected profile; retain the original image unchanged. The frozen [observation](../interfaces/0.3.0/docs/02-observation.md),
[control](../interfaces/0.3.0/docs/03-control.md) and
[trace](../interfaces/0.3.0/docs/04-trace.md) contracts remain authoritative.

## Capturing state and one invocation

Hold the exclusive controller lease and pause at a safe boundary. Retain session
ID, current generation, status, input schedule, seed, oscillator and execution mode.

| Read | Available evidence and limits |
|---|---|
| `status` | Atomic scalar snapshot: PC, A, B, PSW, SP, DPTR, selected-bank R0..R7, cycles, legacy instruction count and stopRevision. It omits most memory and hidden peripheral state. |
| `readMemory` IRAM | SAB addresses 0..255, at most 256 bytes per request; retain all banks and stack bytes. SP plus raw bytes does not establish typed CALL/IRQ frames. |
| `readMemory` SFR | Supported architectural addresses within 128..255. A request containing an unsupported SFR fails; retain unavailable ranges explicitly. Latch values do not establish external pin values. |
| `readMemory` XDATA | Optional board pure-peek callback; the board owns mapping and availability. CPU API1 alone does not provide XDATA peek. |
| `boardInspect` | Advertised board-specific schema only. Faulted inspection additionally requires the explicit fault-inspection capability. |
| `decodeCode` / CODE reads | Current-image inspection while paused, not proof of execution. Backward predecessor knowledge can come from earlier forward inspection. |

Memory/decode reads require paused state and current generation. With the lease
retained, check status afterwards: session, generation, cycle, PC and stopRevision
must remain unchanged. This gives coherent exposed state, not a full checkpoint
or proof of hidden-state equality. Faulted state disallows ordinary memory/decode
reads; preserve pre-fault captures and trace. Reset is not pre-fault inspection.
Faulted scalar status may explicitly report an unsafe boundary.

Arm persistent capture A before the activity. For one invocation, arm B mode1
before the next successful CALL to the target, or stop at entry and use B mode2
only when its fully observed, retired CALL context is the current top frame.
Matching PC/SP alone is insufficient. B includes nested calls and IRQ work,
the successful root RET and its retirement; it excludes caller continuation.
Unknown context, mismatch, limits, reset and fault can close it incomplete.

Retain capture/configuration IDs, bounded limits, replies, canonical ordering,
cursors, loss counters and file completion/segments. Capture state again after
the stop. Register/RAM deltas show net changes, not every access or its cause:
full ACCESS coverage is absent. Correlate actual input/output/outcome facts. Restore temporary
debug configuration while still holding the lease. Reading the previous CODE
breakpoint set requires explicitly selected 3.1; 3.0 needs an already known owned
set or an explicit replacement policy. There is no automatic restore on disconnect.

## Execution evidence and synthetic entry

Begin identifies an attempted instruction; retirement confirms successful effects
and timing completion. An abort may follow partial effects and has no retirement
for that attempt. The legacy instruction counter is not a retired counter.
Captured bytes contain only actual fetches. For example, an untaken `CJNE` path
can retire without fetching its branch-offset operand, leaving a short prefix
and empty text. Preserve that evidence. If useful, display separately labeled
static image disassembly with its image identity; never fill the canonical
record from a later peek. Source sequence orders facts within a cycle; do not
reorder them by phase or equate source IDs with storage record IDs.

Public CPU API1 and persistent wire 3.0/3.1 have no preserved-state set-PC,
register/RAM-write, checkpoint/restore or function-injection operation.
`reset(entry)` performs a full reset and selects the entry PC; it does not
preserve caller state or create an observed CALL.

A derived synthetic image can initialize registers/IRAM through instructions,
execute a real CALL to retained target code, and return to a sentinel. Retain
original/derived hashes, byte changes, bootstrap disassembly, assumed arguments,
stack/register/RAM before and after, board initialization, inputs and IRQ context.
Label it synthetic: matching selected values proves neither natural reachability
nor warm-caller equivalence. A private native harness that mutates state needs
reviewed initialization/context/isolation rules; it is not the stock opaque API
or a supported checkpoint mechanism. Distinguish original and altered state
from unobserved assumptions.

## UART outcome validity

Validate the event kind, correlations, reason and independently tagged values.
Do not apply RX acceptance's value requirement to every RX lifecycle fact.

| Current accepted mode3 fact | Meaning |
|---|---|
| `rx.accept` (19) | `newValue` is known, unsigned, width16, value 0..511 including the ninth bit. |
| `rx.drop` (20) | Current producer emits `newValue` as `{"value":"0","width":8,"known":false,"isSigned":false}`. Reason 1 REN, 2 busy, 3 RI or 4 SM2 identifies the drop; zero is not a received byte. |

The frozen generic unknown-value shape requires value zero, width8 and unsigned
when `known` is false. Known values must satisfy their own width/signedness rules.
Keep those checks strict, together with per-kind requirements; do not invent a
received payload from a previously admitted input. UART register/timing known
bits are independent of `newValue.known`.

Ordinary drop/end facts have status `OK`, describing a valid observation, not
successful RX. REN-disabled or busy delivery emits a rejected frame's drop/end
without start/accept, while the separate required `input.failed` has status
`STATE`/`BUSY` and faults the session. Late RI/SM2 filtering follows a successful
start and does not itself fail delivery. Unsupported UART mode rejects before
frame allocation, so no RX frame/drop is invented.

Retain a bounded rejected raw response separately, with exact bytes/provenance
and validation error. Do not advance validated cursor/counters or treat it as
accepted evidence. Without retained raw data, the precise event/reason remains
unknown; a similar fixture does not recover it.

## Recovering an owned session

External wall-clock pacing failure differs from CPU fault state. Inspect the
session through its owner/control plane before reset. Reconnect needs fresh
hello/attach, identity/state queries and a new controller lease; never blindly
replay mutations with lost replies. Paused/faulted sessions still service control.

With the controller lease and current generation, reset is accepted from paused
or faulted state. Successful reset retains session identity and CODE breakpoints,
advances generation, sets cycle0 and returns paused at the requested entry.
Use the returned generation for subsequent operations; reset is not a lease
acquisition and its response does not supply a new leaseId. Execution resumes
only by explicit commands. Ordinary fault/reconnect/reset paths were verified in
[audit 032](../SDP/Verification/VER-SLC-032.md) with 62 validated replies;
exhaustion or failing CPU/board
callbacks can still make reset fail. Terminated sessions cannot be recovered this way.

Reset cancels admitted queued inputs and ends old UART frames before the new
generation. It retains bounded producer outcome/trace history without reusing
IDs; it does not retract sent bytes or reset a board's opaque history, external
socket, kernel buffer or host queue by implication. Those owners must retain
their own explicit disposition/provenance. Never silently replay old/cold bytes
after reset. Unknown host arrival time/generation stays unknown. See
[audit 030](../SDP/Verification/VER-SLC-030.md) for the admission and reset proof.

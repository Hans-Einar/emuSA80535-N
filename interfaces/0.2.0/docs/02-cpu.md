# CPU/controller API 1

`contracts/cpu.h` defines a query function filling caller-owned `emuif_cpu_api`.
The table is a portable C source interface (same toolchain ABI), not the board
module loader ABI. Query copies the version-1 table and is side-effect free.
All entries are mandatory; capabilities declares output coverage. CPU API1 is implemented by `libemusa80535_cpu.a` and `libemusa80535.a`.

| Operation | Exact behavior |
|---|---|
| create | Validate variant 0/1/2 and oscillator_hz 1..100000000; copy config/hooks, allocate opaque CPU. On failure set output handle NULL. |
| destroy | Release owned CPU storage; NULL is allowed; never free hook context. |
| reset | While not executing (including a fault with pending delay), cold reset to seed and uint16 entry, counters zero. CODE/XDATA may be cleared; controller restores image afterwards. |
| load_code | Exactly 65536 bytes; copy into CODE while stopped at cycle zero after reset. Reject all other sizes. No file I/O. |
| apply_input | Apply one valid already-due input at the input phase of the current boundary; no time advancement. CPU does not own a second input queue. |
| cycle | Advance exactly one machine cycle, including pending instruction/IRQ delays or IDLE. Populate result. In power-down return UNSUPPORTED, progressed=0; no fabricated ticks. |
| snapshot | Copy current scalar state; safe_boundary identifies whether pending instruction/IRQ delay remains. generation/state are set by controller when exposing externally. |
| decode | Stopped only; exact legacy decodeCode byte/instruction-offset and predecessor semantics; count1..256, element_size>=sizeof(emuif_decoded), capacity>=count, all element prefixes initialized. No private CPU access or second decoder required. |
| peek | Side-effect-free CODE/IRAM/SFR inspection; validate full range first; count 0..256, capacity>=count. No device reads, no callback execution. XDATA is controller/board-owned: UNSUPPORTED here. |

CODE/XDATA addresses are 0..65535; IRAM addresses 0..255 (upper half only on
variants that support it), SFR uses architectural 128..255. Unsupported SFRs
return UNSUPPORTED; do not fabricate registers. Peek errors write no bytes.
No memory/register mutation API is exposed in this revision other than reset,
image load and admitted external inputs.

CPU bus callback receives the same `emuif_bus_access` used by the board:
cycle is the current cycle before the instruction's first cycle is completed;
pc is the executing opcode; address is final 16-bit MOVX address; port_latches
is a value snapshot P0..P5 (unimplemented ports zero); direction READ/WRITE.
READ request value is zero; WRITE value is the byte being written. access.sequence
is a nonzero monotonically assigned identity across resets within the session.
Exactly one callback determines the final read/write result. WRITE result.value
must echo the bus write byte, not a guessed device latch. Emit immutable trace
observations only after this result. No extra device read to determine old data.

`port_output` runs synchronously after an architectural port-latch write, before
internal peripheral advancement. It reports previous/new latch and changed_mask
(their XOR), including writes with changed_mask=0. Its sequence shares the
session's access/output sequence allocator, so order is unambiguous. Reset port
levels are conveyed by the reset contract, not synthetic instruction writes.

Missing required bus/port/signal hooks reject create. The controller supplies generic
no-op/error adapters where a board has no relevant functionality. Legacy
xread/xwrite, MOVX observers and `run_control` remain supported separately;
controller adaptation must not invoke a bus access twice or reorder core timer,
UART, ADC or interrupt effects. See [time](04-time.md).

## Effective pins and UART output

CPU_CAP_PORT_LATCH is mandatory and supplies port_output above. Optional
CPU_CAP_UART_FRAME supplies signal_output(kind=UART_FRAME,index=0,value=completed
data including ninth bit,width=8 or9,start_cycle=frame start,cycle=completion).
Timing follows the accepted CPU UART producer; no host serial port is involved.
One completed frame produces one observation, even if software clears flags.
Optional CPU_CAP_EFFECTIVE_PIN supplies kind=PIN,index=port*8+bit,value0/1,width1,
start_cycle=cycle. This is the resolved effective output level, distinct from
port latch writes; alternate-function changes need this capability. Inputs must
not be reflected as output events unless actual modeled output level changes.
Both event kinds share the access/output sequence allocator. Output callbacks
occur synchronously at the source effect, before completing its boundary.

A CPU implementation advertises only coverage it implements and verifies.
Accepted baseline does not imply complete effective-pin/alternate-function
coverage. A board declaring a missing required_cpu_capabilities bit must be
rejected before create; silently substituting latch state for effective pin
state is forbidden. signal_output callback is mandatory (no-op allowed).

The CPU facade owns predecessor knowledge, updated by completed sequential
instructions and successful forward decoding. Reset/load clear it. Decode errors
leave cache/output unchanged. Valid records have size1..3, valid1 and authoritative
UTF-8 text <=96 bytes; unknown-predecessor records have size1, valid0, text
`<invalid>`. Negative offsets never scan arbitrary bytes to guess boundaries.
The wire projection adds reason `decoded` or `unknown-predecessor` respectively.

## Implemented profile and failure boundaries (bundle 0.2.0)

The query capability mask describes available implementation profiles, not a
promise that every profile works on every variant. PORT_LATCH applies to all
three variants. UART_FRAME applies only to SAB80535 mode3: completed logical
9-bit frames, timed by the accepted Timer1 producer. Classic8051/8052 and other
UART modes do not emit this frame profile. Session creation must reject a board
requiring UART_FRAME with a non-SAB80535 configuration; mode3 requirements remain
part of board configuration. EFFECTIVE_PIN is not advertised.

Virtual external input APIs cover the existing SAB80535 producer: ports P1/P3/P4/P5,
external interrupts0..6, ADC0..7 and mode3 UART RX. P0/P2 and classic variants
return UNSUPPORTED. UART RX also requires REN and returns BUSY for an active
frame. Inputs must have the current cycle and nonzero order. Within one delivery cycle,
order must strictly increase; later cycles can carry a lower order. The controller
assigns globally unique monotonic admission identities, sorts delivery by
(cycle,order), and clears its queue on reset. It may admit a far-future event
before an earlier event. The CPU tracks only the last order in the current cycle
and the maximum observed identity. Reset captures that maximum as a floor;
subsequent inputs must exceed the floor. This bounded check is partial stale
protection, not full generation or uniqueness validation: reuse across different
cycles above the floor cannot be detected here. The owner enforces those rules.
Access/output sequence identities survive reset; time counters restart.

A non-OK callback latches a fault immediately. The failing cycle returns its
status, without writing a success result or completing subsequent instruction
or peripheral phases. Effects accepted before the callback are retained;
rollback is not claimed. Snapshot remains available with FAULTED and
safe_boundary=0, even if the native delay is zero. Further cycle, input, peek
and decode calls reject STATE until reset. Reset clears the latch before core
reset and does not replay pending work. Load the image explicitly afterwards.
Unknown callback status and malformed successful bus results become FAULT.

Peek reads modeled stored registers without callbacks, not resolved pin reads.
Supported SFR addresses on all variants are 80,81,82,83,87,88,89,8A,8B,8C,8D,
90,98,99,A0,A8,B0,B8,D0,E0,F0 (hex). 8052/SAB add C8,CA,CB,CC,CD.
SAB additionally supports A9,B9,C0,C1,D8,D9,DA,DB,E8,F8. SBUF inspection is the
stored core byte, with no receive-side consumption. Full ranges are validated
before any output is copied. Decode shares the legacy authoritative decoder and
predecessor helper; there is no second CPU or independent decode cache algorithm.

Callbacks must not reenter the CPU. Status-returning operations detect this and
return BUSY; reentrant destroy latches FAULT and leaves the handle alive for
subsequent ordinary destruction. CPU operations are single-owner, not thread-safe.

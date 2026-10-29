# Virtual time, scheduling and stops

Time unit is one CPU machine cycle, equal to 12 oscillator periods for this
profile. oscillator_hz is metadata for rational conversion; floating point and
host clocks never determine state order. cycle is uint64 and starts at 0 on
reset. Generation increments on reset; input/access order identities do not
wrap or restart in the same session. The host has one serialized execution owner.

## Ordered cycle boundary t

At reset, no instruction or delay is pending. At each completed boundary t:

1. Core internal effects from the preceding cycle have completed, including
   timers/UART/ADC, count increment and accepted legacy scheduled external
   inputs. Legacy-only scheduling retains its historical ordering. The new
   controller profile routes new inputs solely through its own queue, never
   schedules the same input through both paths.
2. Apply controller CPU inputs and board stimuli due at t in increasing assigned order.
   This phase runs once when reaching t, never on repeated paused service turns. Inputs from
   before t are invalid. During reset the queue is cleared before board reset.
3. Invoke board.boundary(t) once; it can schedule only timestamps >t.
4. At a **safe** boundary, apply accepted debug/admin requests in queue order,
   evaluate applicable stops, and publish a snapshot. If paused, do not enter
   step5 until explicit resume/step. Control I/O remains alive while paused.
5. CPU cycle(t) first drains a pending delay, or arbitrates interrupts if no
   delay remains, then performs an opcode when permitted. Bus and port-output
   callbacks occur synchronously at their architectural access. Existing core
   ordering decides interrupt acceptance and instruction effects.
6. Internal peripheral advancement completes cycle t+1. Return to step1.

The board callback for boundary t is never called a second time merely because
status was requested or a paused session was resumed. No instruction boundary
is synthesized inside a multi-cycle instruction or interrupt entry.

A safe boundary has no pending instruction or IRQ-entry delay. A stop requested
mid-instruction drains that instruction/entry before applying it; supported CPU
profiles must need at most 16 additional cycles. Watch stops retain their
triggering event identity. Faults stop immediately and may expose unsafe_boundary;
inspection must label it, and resume is forbidden until reset/recreation.

IDLE continues cycle and board progression without instruction count changes.
A timer or externally admitted interrupt can leave IDLE through existing CPU
arbitration. `step` while IDLE has a finite cycle budget: stop after one completed
instruction, a higher-priority stop, or budget exhaustion at a safe boundary.
Budget exhaustion is `cycle-budget`, not a claim that an instruction executed.
Power-down freezes this clock domain and returns `power-down-unsupported` as a
halt; board time does not run independently and there is no fabricated wake.
Reset is the supported recovery. A future independent-clock policy needs a new
contract version. Existing legacy run-N returns HALT for IDLE unchanged.

Pause freezes CPU, board and input application together. Pending timestamps stay
queued. Host pacing changes only wall-clock speed. Native module callback hangs
cannot be bounded by simulated budgets; native code is trusted.

## Input admission

`emuif_input`: PORT(index 0..5, value/mask uint8), INTERRUPT(index 0..6,
value0/1, mask=1), ADC(index0..7, value0..65535, mask=65535), UART_RX(index0,
value0..511, mask=511), RELEASE(index encodes port0..5 or interrupt256..262,
value=0, mask uint8 for ports or1 for interrupt). ADC is normalized over external
VAGND..VAREF, not an ADC result register. UART_RX is a complete logical frame
(data in bits0..7, ninth bit8), admitted at RX start and progressed by the CPU's
accepted mode3 timing. It is not a pin-level receiver. Queue admission validates syntax/time/capacity, not future RX busy state. At
due-time apply_input, busy RX returns BUSY before RX mutation; a due required
board input that cannot be accepted faults rather than being silently dropped. Other UART modes return
UNSUPPORTED until a separately verified profile exists.

Variant-unsupported port/channel returns UNSUPPORTED. Current SAB virtual port
coverage is P1,P3,P4,P5; capability/source expansion requires implementation.
PORT mask selects externally driven input bits, not an SFR latch write.
INTERRUPT identifies INT0..INT6 independent of legacy IRQ enum numbering.
RELEASE removes external drive and restores normal resolution; it does not
force a high/low pin. Validate all values before queue insertion and again for
state-sensitive delivery. These are virtual logical values, never physical I/O.

Wire2.0 boardInput optionally admits module-owned stimuli. Explicit cycle must
be greater than current cycle; null resolves to current cycle+1 at the admission
boundary. Record that resolved cycle/order before acknowledging. Host-arrival
timestamp never becomes virtual time. Direct arbitrary CPU pin injection is
not exposed to external clients in this baseline. Deterministic
replay pins image hash, module/config hash, CPU build, seed, oscillator and
accepted command/input order. This bundle defines replay prerequisites, not a
capture/replay file format or implemented replay service.

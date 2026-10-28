# Canonical immutable observations

The exact record is `emuif_observation` / observation.schema.json. Every wire
field is required, including zero/unknown fields. `bytes` maps to byte_count
and the leading native bytes[3]; unused bytes are zero. `text` is optional
knowledge represented by an empty string, not a missing field. No consumer may
reread a device or decode current CODE and claim it was execution evidence.
The captured bytes are authoritative; decode text is producer rendering of
those bytes using the bundled decoder at runtimeCommit. It is not a second
read. Begin contains bytes fetched by that attempt (up to 3); if operand fetch
is deferred, terminal retire/abort contains all actually captured bytes. Missing
bytes remain missing, never filled from a later memory peek.

Fields common to all events: generation, sequence and cycle; sequence orders
facts within a cycle and is session-wide. `legacyInstructions` retains the
existing counter's meaning. It increments after opcode effects but before
peripheral/delay processing, including legacy exceptional paths, and is not a
retired count. `retiredInstructions` counts only successful instruction retire.
`instructionId` identifies an attempt from begin through its terminal event;
0 means outside any instruction. `partialEffects` means effects may already
have occurred before a failed operation; it does not imply rollback.
`status` uses shared statuses, OK for ordinary facts. `reason` is 0 none, 1 REN
clear, 2 receiver busy, 3 RI set, 4 SM2 filter, 5 holding replaced, 6 reset,
7 unsupported opcode/operation, 8 device failure, 9 lost context, 10 exhausted.
`beginCycle` is the begin cycle of instructionId (zero outside an instruction);
`nextPc` is the actual PC after opcode effects on retire/abort, valid only with
known bit32. It is independent of branch target and returnPc; a partial abort
may have unknown nextPc. Instruction elapsed timing uses matching begin cycle
and the terminal event cycle, including all peripheral/delay cycles. nextPc
is captured after opcode effects and retained until the terminal event. Unknown fields are zero (unknown values additionally known=false). `known` bits:
1 pc, 2 target/returnPc, 4 context/SP pair, 8 UART register/mode/width/ninth
snapshot, 16 scheduled/start cycles. Clear bits prohibit interpreting those
fields even if zero is a plausible hardware value. oldValue/newValue have
independent known flags, width 1/8/16/32/64 and signedness; unknown value=0,
width=8,isSigned=false. Signed payload is width-bit two's complement, value
must fit width. No callback re-read to recover old or new value.

| kind | Name | Required correlation and phase |
|---:|---|---|
|1|instruction.begin|nonzero instructionId; pre=0, before opcode effects|
|2|instruction.retire|same instructionId; retire=2, after all opcode effects and peripheral/delay cycles complete|
|3|instruction.abort|same instructionId; boundary=4, failed/unsupported attempt, status non-OK, no retire for this attempt|
|4|access.attempt|nonzero accessId/instructionId; effect=1, before device dispatch|
|5|access.result|same accessId; effect=1, status and actual observed values; failed attempt is not completed access|
|6|control.call|new contextId, parentContextId, frameKind=1, target/returnPc and SP facts; effect=1|
|7|control.ret|closing contextId, frameKind=1; effect=1; successful return only confirmed by retire|
|8|irq.request|irqSource, actual cycle, peripheral=3; no frame allocated yet|
|9|irq.enter|new contextId, parentContextId, frameKind=2, irqSource/depth, target/returnPc/SP; boundary=4|
|10|control.reti|closing IRQ contextId, frameKind=2; effect=1, distinct from RET|
|11|reset|new generation; boundary=4|
|12|load|new generation; boundary=4|
|13|fault|status non-OK, partialEffects; boundary=4|
|14|input.admitted|parentStimulusId and assigned cpuInputOrder, scheduledCycle; boundary=4|
|15|input.delivered|same IDs, actual cycle; peripheral=3; hardware acceptance not implied|
|16|input.failed|same IDs, non-OK status, reason8; peripheral=3|
|17|input.cancelled|same IDs, reason6; boundary=4|
|18|rx.start|frameId, parentStimulusId/cpuInputOrder; peripheral=3|
|19|rx.accept|same frame/parent/input, captured receive value/newValue; peripheral=3|
|20|rx.drop|same frame/parent/input, reason1/2/3/4; peripheral=3|
|21|rx.end|same frame/parent/input, actual end cycle; peripheral=3|
|22|tx.hold|frameId, newValue, instructionId/accessId for SBUF write; effect=1|
|23|tx.replace|frameId new holding frame, relatedFrameId displaced holding frame, reason5; effect=1|
|24|tx.start|frameId/startCycle; peripheral=3|
|25|tx.stop|frameId, beginning of stop bit; peripheral=3|
|26|tx.ti|frameId, actual TI set edge; peripheral=3|
|27|tx.end|frameId, full frame completion; peripheral=3|
|28|watch.match|watchId, sourceEventSequence, firedActions bit index in action array; boundary=4|

`phase` names above map to 0 pre,1 effect,2 retire,3 peripheral,4 boundary.
Ordering is actual source execution, not sort-by-phase. UART/IRQ facts during the timing tail precede the terminal retire. A
peripheral/delay fault aborts the still-pending instruction, even when the
legacy instruction counter already advanced. Breakpoints fire before begin; no begin means no attempted opcode.
An opcode failure emits abort after already-issued access results, never a fake
retire. Access space is CPU API1: CODE0/IRAM1/SFR2/XDATA3; IRAM address distinguishes
lower/upper. Access bits: fetch1/read2/write4/RMW8; event access is one tag.
Write old/new values are observed internally, never speculative device reads.
Unknown returned read data stays unknown. RMW is one semantic transaction with
one accessId, not two fabricated completed accesses.

Context/frame IDs are observational tokens, not C pointers or stack addresses.
FrameKind0 none,1 CALL,2 IRQ,3 unknown; parent 0 means no known parent. `frameId`
is a UART frame token and is distinct from `contextId`. `irqSource` is the CPU
interrupt source index (0 external0,1 timer0,2 external1,3 timer1,4 serial,
5 timer2,6 ADC,7 INT2,8 INT3,9 INT4,10 INT5,11 INT6; 12..31 reserved
and rejected in extension1. Unsupported sources for a selected variant are
not generated; these source IDs are not external input pin indexes). irqDepth is depth
before routing enter/exit; update after routing, as the existing trace model.

Observation coverage bits 1 instruction,2 access,4 control,8 IRQ,16 lifecycle,
32 input,64 UART are all-or-nothing classes across the variant's supported
opcodes/peripherals. No capabilities are inferred from legacy instruction/debug
callbacks. Query advertises only implemented and verified classes; unknown bits
are rejected. Unsupported variants/classes return UNSUPPORTED. UART outcome
service requires lifecycle+input+UART coverage; invocation B additionally
requires instruction+control+IRQ. Trace filters cannot disable context tracking.

## Admission and UART lifetime

A host allocates one nonzero parentStimulusId at accepted board stimulus and
returns it in boardInput. One board stimulus may create several CPU inputs;
Controller admission independently assigns cpuInputOrder using the existing
session scheduler's global next_order sequence. Never equate the existing
board input order with CPU order. The controller owns the sole queue; it allocates order, copies input and emits
input.admitted at successful scheduling. Capacity failure assigns no identity.
`emuif_observed_input.input.order` is that nonzero assigned CPU order; CPU
observation `deliver` applies this correlated envelope only at its due cycle,
using the existing CPU apply_input semantics. It neither schedules nor allocates
an order. The parent stimulus is the immutable board-input dispatch context:
while invoking the existing board callback, the owner tags each resulting
schedule_input call with its parent; startup/autonomous board scheduling has parentStimulusId0 (no board request).
The controller emits admission/delivery/failure/cancellation events, CPU emits
UART frame and execution facts. CPU-local sequence IDs order callback facts;
the session sequencer assigns the canonical sequence and remaps any local
source references before routing/retention. There is no second matcher or
competing sequence allocator visible to clients. Native CPU observation query
alone does not provide controller admission facts; the session supplies them.
CPU binding generation must equal session generation; reset updates binding
through unbind/bind at the same stopped boundary. The owner preserves ABI1.
An external board callback still returns only its old accepted/rejected status;
new outcomes are polled by the host, not pushed as new board signal kinds.

Admission acknowledges scheduling, delivery records applying the due input,
and RX acceptance records UART hardware acceptance. These are three distinct
facts. Required due-input failure preserves the existing fault/no-retry behavior;
RX drops are ordinary hardware outcomes, not CPU faults. Late RI/SM2 filtering
at the sampling/acceptance point produces drop then end. REN=0 or busy at start
allocates a frameId and emits drop then end at that cycle, without start or
accept. Successful start emits start, then exactly one accept/drop, then end.
Reset cancels pending input IDs in ascending CPU order before reset event;
active frames end with reason6 after cancellation and before reset. No retry.

A TX write allocates frameId even when held. Replacing the one holding slot emits
replace referencing the displaced ID; displaced ID has no start/end, and replace
is its terminal outcome. Empty hold emits hold. Starting a held or immediate
frame emits start, then actual stop and TI facts when the timing engine sets
them, then end; TI is not TX completion. Existing mode-specific timing and
Timer1/SMOD behavior are authoritative; no parity or external transport model
is introduced. Reset ends active TX with reason6 and cancels pending held TX
with a tx.end reason6 without claiming it was transmitted.

All input and UART events (14..27) are also retained in a fixed 1024-record
outcome ring (destinationId=0), independently of trace enable, gates and filters.
It is pull-only, no allocation per subscriber, unlimited readers within client
bound. Same stable cursor rules as trace pages; overwrites are explicit loss.
Reset does not clear outcome history or epoch. No time-based expiry exists;
retention expires only by bounded overwrite or new session. Observation loss is
reported as loss metadata, not fabricated hardware failure or success.

Input lifecycle14..17 includes inputStage0 board or1 CPU. Board-stage facts
have parentStimulusId>0,cpuInputOrder=0; they acknowledge admission/application/
failure/cancellation of the original board stimulus even if it never schedules
a CPU input. CPU-stage facts have cpuInputOrder>0 and parentStimulusId>0 when originated
by a board request, or parentStimulusId0 for autonomous scheduling. UART RX
facts retain inputStage1 and both IDs. Other event kinds inputStage=0 unused.
Board-input success is not CPU scheduling or RX success. Board failure emits
board-stage failed with statusFAULT, faults session and never retries. Stage
is part of identity; board-stage delivery and CPU delivery are separate facts.
Autonomous scheduling uses parentStimulusId0 and remains correlated by its
nonzero CPU input order; it does not invent a board request.

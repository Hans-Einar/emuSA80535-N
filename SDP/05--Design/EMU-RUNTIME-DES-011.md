# EMU-RUNTIME-DES-011 — Runtime integration design constraints

Design items: DES-101..DES-110
Status: accepted target constraints; refined by interfaces/0.1.0 API/ABI/wire contracts; runtime implementation pending
Authority: SLC-019; refines ARCH-011..ARCH-018 / REQ-018..REQ-024

## DES-101 — Session and CPU facade

The session owns CPU, image identity, board handle, timeline and debugger state.
New host clients use a bounded facade; compatibility wrappers preserve existing
native APIs. Expose instruction/cycle budgets, stop provenance, value snapshots
and typed stimulus/bus operations. CPU source never imports platform loaders,
transport or board headers. Current raw struct availability is not an ABI
stability promise.

## DES-102 — Board lifecycle and ABI

Define sized/versioned descriptors, capabilities, opaque context, bounded data
buffers and paired host/module function tables. Validate compatibility before
attaching a board or advancing the CPU. Open the library, resolve its entry,
validate, create a session instance, reset and connect it; unwind completed
steps on failure. Native library constructors are not isolated by validation.
Destruction precedes unloading; first delivery supports neither hot reload nor
hot unload. Retain required callbacks only for their documented module/session
lifetime. This board ABI differs from the callback-free external debug facade
in DEBUG-DES-092 and does not weaken that facade's ownership rules.

## DES-103 — Canonical accesses and observation

Perform each CPU bus access exactly once through the host-bound board callback.
Capture context at the architectural access; emit immutable observations after
the final result. Never re-read a device to invent an old value. A failed module
transaction faults/stops the session; do not silently substitute a successful
fallback or promise rollback of an already-performed side effect. Complete
generic port/output observation and cycle hooks are implementation prerequisites.

## DES-104 — Time and execution phases

Freeze a phase/order contract before scheduler coding: board input sampling,
instruction access, internal peripheral advancement, interrupt arbitration,
entry delays and completed safe boundaries. Preserve accepted CPU ordering.
Expose bounded progress during IDLE even when no instruction retires; current
`run_control` intentionally returns HALT and must retain that behavior for old
clients. New orchestration cannot obtain this by repeatedly calling run-N.

Power-down requires an explicit clock-domain/wake policy. Do not equate a
stopped CPU cycle counter with advancing external virtual time. Until those
semantics are reviewed, report an unsupported/halted configuration rather than
fabricating ticks or wake events. Virtual-time units, sleep-domain mapping and
boundary hooks remain detailed design obligations, not established hardware
claims in this document.

## DES-105 — Commands, stimuli and determinism

The owner validates bounded queued requests. External stimuli carry explicit
virtual timestamps and ordering identity; reject past/out-of-order input or
apply a documented admission rule and record the resolved timestamp/order.
Interactive "next safe boundary" input is recorded at its accepted boundary;
replay uses that record, not network arrival timing. Callbacks cannot reenter
execution or mutate unrelated state. Queue exhaustion and disconnect never
silently discard accepted mutations. Host timeouts diagnose service failure
without advancing virtual time.

## DES-106 — Attach, control and backpressure

Attach negotiates access and returns session/image/module identity plus a safe
snapshot; it never reloads/resets. One controller lease orders mutations;
observers read snapshots/pages only. Detach releases the lease and preserves
running/paused state. Connection loss does not terminate an attached session;
reconnect revalidates session/generation and cannot silently replay commands.
Termination is an explicit authorized operation. Launch-owned stdio retains
its accepted EOF/terminate cleanup through its compatibility policy.

DAP graceful disconnect from an attached runtime is a separate adapter policy.
Under the pinned `@vscode/debugprotocol` 1.68.0 contract, omitted or false
`suspendDebuggee` defaults to resume. Where required,
the authorized controller requests resume, waits for acknowledgement that the
transition was applied, then detaches and relinquishes control. Advertise
`supportSuspendDebuggee` only when supported; when negotiated and requested,
apply and acknowledge suspension before detach. A failed or unacknowledged
transition must not be reported as a successful policy application. Abrupt
connection loss follows the runtime state-preserving rule above, with no
implicit terminate or silent replay after reconnect. This mapping fixes no
wire command names and does not change low-level detach semantics.

Bound command work, response queues and trace retention. Slow readers receive
explicit expiry/loss or disconnect; they cannot block the execution owner on
unbounded writes. Maintain frozen cursor/loss semantics. Pause/step/status use
completed CPU/board boundaries, with pending requests acknowledged only after
application. A cached status must identify its boundary and staleness.

## DES-107 — Unix sockets and TCP

Default to no listener. When selected, put per-user Unix sockets under a private
runtime directory, validate ownership/permissions and authenticate peer access.
TCP is explicit, with bounded stream framing and separate capability/version
negotiation. Require authorization and a protected remote channel: authenticated
TLS or an authenticated tunnel to a restricted local endpoint. Never default to
an unauthenticated wildcard listener. Endpoint/address, credential lifecycle
and precise handshake schemas belong to the transport contract. A TCP listener
does not turn runtime NDJSON into DAP Content-Length framing.

## DES-108 — Signals and status

On Linux, route blocked administrative signals through `signalfd` in the host
event loop, or use an async-safe flag/wakeup mechanism. Do not capture state,
allocate or call stdio from an asynchronous handler. SIGUSR1 requests a bounded
status report containing session/state, image/module identities, virtual time,
instruction count, stop reason and queue/trace-loss summary, emitted at a safe
boundary to stderr or a configured diagnostic sink, never protocol stdout.
TERM/INT request orderly shutdown. SIGSTOP/SIGCONT are OS suspension, not safe
debugger pause/resume. Ordinary signals can coalesce; correlated socket status
is the reliable query interface. No prompt response is promised from a hung
native callback. These choices follow the signal references in STU-004.

## DES-109 — Reset and shutdown

Reset is a session operation: stop at a boundary, reset CPU/board and scheduled
inputs consistently, restore the selected image after destructive core reset,
and advance lifecycle identity according to the accepted debug contract.
Read-only discovery/attach does not trigger reset. Orderly shutdown rejects new
mutations, completes the permitted boundary, reports final status, destroys
board state and releases endpoints/resources. Failure disposition and partial
initialization cleanup are tested; unsupported recovery never resumes silently.

## DES-110 — Work areas and integrated gate

Deliver in reviewed steps: (1) detailed CPU/host and board ABI contracts;
(2) cycle/access scheduler and a synthetic statically registered board;
(3) dynamic loader with the same module conformance tests;
(4) reusable debug service bound to the existing session and stdio compatibility;
(5) Unix/TCP attach, control arbitration and signals; (6) DAP attach integration
and release verification. Canonical producers/safe-boundary watch stops remain
required Gate-B work; preserve the accepted unified model throughout.

Acceptance includes variant regressions, synthetic timed bus/pin/IRQ fixtures,
delay/IDLE/sleep-policy checks, replay equality, ABI rejection/cleanup, observer
neutrality, one-CPU attach without reset, common CPU/board pause, repeated
disconnect/reconnect, malformed/slow clients and signal bursts. Separate
board-process IPC is excluded; richer frontend features remain later work. Existing
RPM prototypes can be reused once the integrated milestone is satisfied.

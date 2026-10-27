# ITR-019 / SLC-019 — Establish runtime architecture

Status: active

## Worker contract

Read AGENTS and lifecycle baselines. Study upstream
`5dc681275151c4a5d7b85ec9ff4ceb1b25abd5a8` and accepted fork `c8d647a`.
Create STU-004, REQ-018..REQ-024, ARCH-011..ARCH-018 and DES-101..DES-110 in
new runtime-focused lifecycle documents. These describe target capabilities,
not implemented or fully ABI-frozen interfaces. Update mandate scope only
to reflect generic orchestration/plugin/debug transport ownership and the
current user's remote-debugging request; preserve original scope/history.

Required analysis and decisions:

- Separate original core/opcode/decoder/state API from emu.c curses runner
  and tightly coupled legacy logicboard. Explain current SAB variant additions,
  same-build public structs, callbacks, standalone trace matcher and owned CPU
  in emu_debug.c. Source-ground claims and distinguish implemented vs target.
- CPU must remain buildable without dlopen/sockets/signals/frontend/board.
  Orchestration and plugin loader are separate build targets in this repo;
  target board implementations remain in their external repositories.
- Prefer one authoritative session/CPU/timeline with startup-loaded versioned
  C board ABI; optional static registration can share the same interface.
  CPU↔host API, host↔board ABI and external serialized protocol are distinct
  contracts. Do not freeze private struct layouts or duplicate debugger logic.
- Exact scheduling at CPU cycle/access boundaries matters, including multi-
  cycle instructions, IRQ entry, IDLE and power-down. State explicitly which
  hooks are missing and require implementation; a run-N wrapper alone cannot
  guarantee correct timed board input. No host arrival time or wall clock
  becomes simulated time; externally injected stimuli need deterministic
  acceptance/order/timestamps and replay records.
- Startup/validation/failure/ownership/reset/shutdown boundaries; board callback
  is not an observer; no side-effecting read to fabricate trace values. No
  hot-unload or process-splitting implementation in the initial milestone.
- One serialized execution owner, bounded commands and trace buffers; safe-
  boundary snapshots, one mutating controller, read-only observers; define
  attach/detach/terminate/reconnect and backpressure. Preserve stdio1.0 launch
  semantics via compatibility adapter; new attach never reloads or resets.
- Support debugger connection over Unix sockets and opt-in TCP with explicit
  framing/version/capability negotiation. Distinguish IDE↔DAP framing from
  DAP↔runtime NDJSON/control transport. No unauthenticated remote listener;
  remote use must have authentication and protected transport (TLS or tunnel).
- Signals are host administration, not simulated interrupts: SIGUSR1 requests
  bounded safe-boundary status; TERM/INT orderly exit; do not use STOP/CONT
  as debugger pause. signalfd or async-safe flag/wakeup only; no snapshot or
  printf from async handler. Output diagnostics never on protocol stdout.
  Socket status is correlated/reliable; ordinary signals can coalesce.
- Compare in-process .so vs separate processes on determinism/failure isolation/
  complexity, not unsupported performance claims. A future remote board
  adapter needs sequenced cycle-stamped barriers/timeouts; retain wire/API
  separation without implementing both now.
- Plan deliverable work areas and minimum integrated acceptance: synthetic
  board module, one CPU, deterministic timer/IRQ/order, attach without reset,
  pause across CPU+board, TCP and Unix socket clients, signal status, regressions.
  Reference package prototypes as provisional; packaging release waits for
  the selected integrated milestone. Exact ABI/schema/CLI names belong to
  separately scoped implementation designs.

Worker owns new STU/REQ/ARCH/DES documents and bounded mandate addendum, plus
implementationNotes. Master owns active index, Sprint contracts, handoff and
traceability. Fresh Reviewer owns REV-SLC-019. No source move/refactor now.

## Acceptance

Cross-check claims against source/upstream and primary OS/protocol references.
Independently review timing/ownership/reentrancy, ABI/wire boundaries,
compatibility/lifecycle/security and work decomposition. Resolve findings.
Verify traceability, paths, whitespace and no product change. Architectural
direction may be accepted while exact interfaces remain explicitly unfrozen.

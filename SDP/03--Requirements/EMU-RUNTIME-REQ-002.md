# EMU-RUNTIME-REQ-002 — Modular runtime requirements

Requirement set: REQ-018..REQ-024
Status: accepted target requirements; not implemented
Authority: user's 2026-10-27 architecture request; SLC-019

- **REQ-018 — Component separation:** retain an independently buildable CPU
  engine without dynamic-module loader, socket, signal, UI or external-board
  dependencies.
  Provide separately buildable generic host/debug-service components in this
  repository; preserve existing native callers and classic variants.
- **REQ-019 — Board modules:** compose one CPU/session with a startup-selected
  external board through a versioned C ABI, supporting shared-library loading
  and equivalent static registration. Reject incompatibility explicitly.
  Board implementations and target firmware remain external.
- **REQ-020 — Deterministic orchestration:** one execution owner orders CPU,
  board, interrupts and accepted stimuli on a defined virtual timeline. Cover
  access/cycle phases, delay/IRQ/IDLE and explicit power-down policy. Preserve
  deterministic replay; host delivery time must not determine simulated time.
- **REQ-021 — Independent debugging:** the debug service operates on the
  session's existing CPU and unified point/trace model. Attach/reconnect does
  not reset, reload or create another CPU. Provide bounded safe-boundary
  pause/status, controller ownership, detach and explicit termination.
- **REQ-022 — Transports:** support Unix sockets and opt-in TCP for the
  versioned runtime control protocol, with bounded framing and capability
  negotiation. Remote access requires authentication/authorization and
  protected transport. Preserve accepted stdio1.0 launch behavior.
- **REQ-023 — Administrative signals:** Linux SIGUSR1 requests bounded status;
  TERM/INT request orderly shutdown through normal lifecycle handling. Signals
  never become simulated interrupts or perform unsafe snapshot/I/O in handlers.
- **REQ-024 — Integrated verification:** prove a synthetic board and CPU share
  one timeline/state under local and remote debugging, deterministic replay,
  disconnect/reconnect, bounded failures and signals before integrated release.

These requirements derive from MND-001's runtime addendum and STU-004; they
do not assert full peripheral coverage or reopen accepted REQ-016 semantics.
Exact ABI signatures, schemas, limits and CLI names require implementation
contracts. SLC-018 packages remain unpublished development evidence.

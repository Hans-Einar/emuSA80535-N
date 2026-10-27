# EMU-LOCAL-DES-013 — Local runtime delivery

Status: implemented and accepted for the local milestone; DES-115..DES-118
Authority: REQ-026; SLC-022; bundle0.2.0

## DES-115 — Session and deterministic scheduler

Implement immutable CPU/session/board contracts in interfaces/0.2.0. Controller
owns one opaque CPU, board instance, image copy, identity, generation and bounded
mixed input queue sorted by(cycle,assigned admission order). Service each boundary
once; pause stops all virtual progress. Preserve cycle/drain/IRQ/IDLE behavior,
callback partial-fault state and reset lifecycle. Owner assigns globally unique
admission IDs; delivery need not be globally increasing. Reset clears queue and
increments generation, never reuses admission/access identity. Reject board
required capabilities before create, including UART_FRAME on classic variants.

## DES-116 — Reusable local debug control

Implement session-bound API1 debug dispatch against wire2.1 schemas, without any
socket listener. Mandatory capabilities must be real; optional boardInput,
boardInspect/boardInspectFault and memoryRead only when their paths are verified.
Do not advertise traceRead or advanced watch functionality without implementation.
Reuse/extract the existing CODE-breakpoint primitive for old/new facades, rather
than adding a second matcher; full canonical watch/trace producer work remains a
separate gate. Shared private JSON and SHA helpers preserve old protocol1 behavior.
One owner processes commands at the declared boundary, with explicit exceptions
for nonexecuting fault inspection/reset. Lease, request identity, stale generation,
capacity, error and no-retry semantics follow the frozen contract.

## DES-117 — Loader and local script host

Host loads one native module through the exact ABI1 query entry, validates all
required callbacks/capabilities/version/platform, and unwinds partial startup in
reverse order. Hash the actual image/module artifact used, not an unrelated path
snapshot; no CPU progress precedes validation. Native code remains trusted.
Implement LocalHostPlan.md as version1 product script contract with exact output
schema and executable examples under runtime/. Stdout contains only typed NDJSON;
diagnostics use stderr. Post-fault scripts can still inspect/reset through the
permitted service. This local product protocol does not alter socket wire2.1.

## DES-118 — Reproducible integration distribution

Build CPU and runtime libraries separately and a generic emu-runtime executable.
Provide pinned source-archive/build instructions, exact implementation/capability
identity and host script docs/schema in runtime release material. Ponsse owns
full-system bootstrap, module configuration, firmware and integration tests.
No CPU or board implementation is copied across owners. A synthetic independently
compiled module proves the host before external module/ROM acceptance. Network,
DAP attach, rich trace and RPM publication are not claimed by this local gate.

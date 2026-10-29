# EMU-RUNTIME-ARCH-003 — Modular simulation runtime

Architecture items: ARCH-011..ARCH-018
Status: accepted target direction; initial interface baseline in interfaces/0.1.0; runtime implementation pending
Authority: SLC-019; REQ-018..REQ-024; informed by STU-004

## ARCH-011 — Build and ownership boundaries

Keep the CPU library as the portable instruction/peripheral engine. A generic
orchestration library owns the session and scheduling; a host executable owns
configuration, module loading and OS integration. A reusable debug-service
component operates on that session. Separate build targets enforce these
boundaries even when all execute in one process. Legacy curses and stdio
executables remain compatibility frontends. No source relocation is required
by this design slice.

## ARCH-012 — Three different interfaces

The CPU/host API controls execution and exposes generic bus/pin/event seams.
A wrapper can provide opaque handles to new callers while retaining the
upstream public struct API. The host/board ABI is a versioned function table
and explicit lifetime contract for independently compiled native modules.
The client/service wire protocol is serialized values, never native memory
layout, pointers or function tables. None substitutes for the others.

## ARCH-013 — Preferred composition

Use one runtime process containing one authoritative CPU session plus a
startup-loaded board `.so`; static board registration may use the same ABI.
The host selects a module without understanding its target semantics. The
external board project owns its implementation and package.
It builds against the board SDK, not private CPU structures, and contains no
second CPU engine. Independent compilation does not require separate processes.

| Composition | Consequence |
|---|---|
| In-process module, selected baseline | Direct ordered transactions and straightforward common pause; native module faults affect the host |

The user's subsequent confirmation excludes separate board processes from the
selected architecture and bundle0.1.0. No board IPC contract or implementation
is planned here. This choice follows complexity and deterministic ordering,
not an unmeasured throughput assumption. Loaded native code is a trusted
component, not a sandbox.

## ARCH-014 — One simulation execution owner

One owner serializes CPU execution, board callbacks and accepted mutations.
I/O workers may frame/enqueue bounded requests but never touch CPU/board state.
The scheduler controls virtual advancement at defined cycle/access phases;
wall-clock pacing affects only how quickly the host performs that work.
Pausing freezes CPU and board virtual advancement together while control I/O
continues. Instruction-only chunking is insufficient for timed peripherals.

## ARCH-015 — Debug the existing session

Extract session-independent debugger operations from today's owned-CPU facade.
Both runtime and compatibility frontend use the same semantics and existing
matcher/trace engine; no adapter or board implements another matcher. The
new service supports one mutating controller and bounded read-only observers.
The frozen DEBUG-DES-090..097 versioning/paging decisions remain authoritative.

## ARCH-016 — Transport-neutral control

Runtime service messages are independent of stdio, Unix sockets or TCP. DAP
remains an external adapter: IDE-to-DAP framing and DAP-to-runtime framing are
distinct. Existing NDJSON1.0 launch remains supported by a compatibility
adapter. Attach/autonomous-run capabilities require new explicit negotiation,
not changed meanings of old run/terminate commands.

Low-level runtime detach preserves execution state. The DAP adapter translates
graceful disconnect policy into authorized lifecycle operations: where DAP
requires resuming the debuggee, resume must be applied and acknowledged before
detaching. Negotiate and honor `supportSuspendDebuggee`; abrupt connection loss
preserves state without silently replaying operations or terminating the runtime.

## ARCH-017 — Host administration

OS signals, endpoint discovery, connection authentication and diagnostics
belong to the host layer. Neither CPU nor board needs process IDs, sockets or
signal handlers. Host signals request administrative actions through the same
serialized lifecycle as socket commands; they do not emulate CPU IRQ pins.

## ARCH-018 — Delivery boundaries

Implement interfaces/scheduler, module loading, debug-service ownership and
transports as separately reviewed work areas. The external board author can
develop against a frozen module SDK and synthetic host tests without changing
the CPU. DAP transport/lifecycle changes remain in its repository. Integrated
release follows REQ-024, not merely successful RPM construction. Exact ABI and
wire documents are prerequisites to those implementation slices.

# EMU-DEBUG-DES-006 — emu-debug protocol 1.0 runtime design

Status: active target design
Design items: DES-050..DES-063
Established: 2026-09-01
Steering authority: Issue #6 and DAP PR #4 exact HEAD

## Authority and stop boundary

The exact frozen authority is DAP commit
`36639b48ddb2ffbafa14c00da794fe1734f7483b`, especially
`protocol/EMU_DEBUG_API_REQUIREMENTS.md` and the real-adapter expectations in
that revision. The implementation must not require DAP product changes.

Only the generic headless runtime is authorized. External-edge work from
Issue #7, ADC, Timer2, P1000 board/target policy, live serial/GPIO and physical
I/O remain outside SLC-011.

## DES-050 — Portable no-curses executable

Add an explicit `emu-debug` build target/program whose product sources exclude
the curses/TUI object graph. Linux and Windows builds own stdin/stdout/stderr
in binary-safe, line-oriented child-process mode and use no shell.

## DES-051 — Bounded NDJSON framing

Read at most the negotiated hard maximum per UTF-8 line. Parse exactly one
JSON request object per line and serialize one correlated response, plus only
contract-defined events. Oversize/malformed data has bounded handling. No DAP
`Content-Length` framing is accepted or emitted.

## DES-052 — Request and error schema

Require positive integer, session-unique `id`, request type and command.
Ignore unknown fields under major 1, but reject invalid required fields,
duplicate IDs and unknown required commands with stable structured
`code/message/retryable/data` errors. stdout never carries diagnostics.

## DES-053 — Hello and negotiation

The first accepted command is `hello` for protocol 1.0. Return product/version/
commit diagnostics, supported variants, the seven frozen capabilities and
numeric limits. Reject major mismatch and missing required capabilities;
minor compatibility follows the frozen named-capability rule.

## DES-054 — Image and deterministic reset

`load` accepts only an absolute `raw-code-64k` path, verifies exactly 65,536
bytes and the expected SHA-256, returns the actual digest and changes CODE only.
`reset` accepts uint32 seed and uint16 entry, performs deterministic SAB80535
cold reset, installs the entry PC by an emulator-owned rule, invalidates
derived session state as required and returns reason `entry` at idle boundary.

## DES-055 — Public atomic snapshot

Add a stable public snapshot API for PC, A, B, PSW, SP, DPTR, selected R0..R7,
variant, 64-bit counters and result provenance. Snapshot capture occurs at one
instruction boundary and is reused by `reset`, `getState`, `run` yield/stops
and `stepInstruction`.

## DES-056 — Exact decode windows

`decodeCode` applies byte offset first without uint16 wrap, then signed
instruction offset, and returns exactly the requested positive count within
the negotiated limit. Forward records use `decode()` text/true size. Backward
traversal uses only known predecessor chains; unknown slots step back one byte
as `<invalid>`/`unknown-predecessor` placeholders. Range failure is structured
and never partial.

## DES-057 — Atomic replacement breakpoints

Own a bounded unique uint16 CODE set in the debugger facade. Replacement is
all-or-nothing; empty clears; accepted/rejected output and stable reasons
follow the frozen wire contract. A matching PC stops before execution with
reason `breakpoint`. No TUI global is shared.

## DES-058 — Bounded run and exact step

`run` executes no more than the requested/negotiated positive chunk. It returns
an architectural stop for breakpoint/exception/halt or `yield` with an atomic
idle-boundary snapshot. `stepInstruction` executes exactly one architectural
instruction and returns `step` unless a higher-priority exception/halt occurs.
There is no child pause command or wall-time execution result.

## DES-059 — Lifecycle and invalidation

Serialize commands and reject invalid lifecycle transitions without mutation.
Load/reset/execution invalidate stale boundary/decode state as specified.
`terminate`, stdin EOF, malformed fatal input and unexpected errors perform
bounded cleanup and leave no orphan process.

## DES-060 — SHA-256 and implementation dependencies

Digest and JSON support must be deterministic, portable, repository-owned or
license-compatible, and usable without network/runtime package installation.
Preserve the repository MIT/copyright notices and document any added source
provenance.

## DES-061 — Cross-platform and safety gates

Verify the same executable contract through Windows and Linux process tests:
protocol-only stdout, diagnostic stderr, exit/EOF/terminate behavior,
malformed/oversize handling and no physical endpoint access. Host wall time,
serial, GPIO, CAN and machine-control APIs are forbidden.

## DES-062 — Real DAP equivalence

After independent emulator review, run DAP PR #4 at exact frozen HEAD against
the real executable without changing DAP product sources. Run its contract,
fake-vs-real, process/package and available VS Code smoke gates. Classify any
mismatch as emulator defect, fake defect or frozen-contract defect.

## DES-063 — Regression and evidence boundary

All accepted Stage-0, Stage-1 and SLC-010 suites remain mandatory on Windows
and WSL/Linux, including strict warning and sanitizer configurations. Review
and verification record exact product HEAD separately from later SDP-only
handoff commits.

## Traceability

DES-050..DES-063 refine ARCH-007..ARCH-010, implement REQ-016 and constrain
SPR-006 / ITR-011 / SLC-011.

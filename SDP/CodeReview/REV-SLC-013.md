# REV-SLC-013 — Tracepoint debugger design review

Status: approved-with-review-corrections  
Reviewed Slice: SLC-013  
Reviewed product baseline: `d9f80eba172dd9d7281aaa9e5cfef461b6b9709b`  
Frozen DAP authority: `36639b48ddb2ffbafa14c00da794fe1734f7483b`  
Reviewer: fresh independent reviewer  
Reviewed: 2026-09-02

## Disposition

Approved as a documentation/design Slice after the corrections recorded below.
No product C or TypeScript was changed. The proposal is implementable in staged
Slices and preserves the frozen protocol 1.0 and generic emulator boundary.

## Source audit

- `core.c:1314-1384` proves that pending `mTickDelay` cycles advance timers and
  return without interrupt arbitration or opcode execution; zero-delay
  arbitration precedes opcode fetch; IDLE advances timer cycles without an
  instruction.
- `core.c:713-739` proves SAB Timer1 overflow feeds the UART producer before
  invoking the timer overflow compatibility observer.
- `core.c:1049-1167` proves accepted SAB interrupt entry mutates stack, PC and
  controller state and starts an entry delay before the vector opcode.
- `core.c:1405-1449` proves the current public runner drains delay before
  returning a boundary, while POWER DOWN and IDLE need explicit watch-stop
  treatment.
- `emu_debug.c:578-648` proves facade CODE breakpoints are pre-execution and
  that execution currently advances through one-instruction calls.
- `emu_debug_server.c:807-809` and process tests prove stdout is protocol-only;
  trace files must remain separate.
- Frozen DAP protocol 1.0 requires the seven named capabilities and permits a
  minor mismatch only through capability-compatible semantics. It has no
  watchpoint/trace/file command today.

## Review corrections

### REV-SLC-013-F001 — corrected — event order contradicted `tick()`

The original prose placed interrupt acceptance after instruction-end in one
normative pass. Actual execution arbitrates at the following zero-delay
boundary, before the next opcode, and peripheral producers run in every delay
cycle. DES-068 and the detailed design now specify this exact order, including
Timer1-to-UART order, entry delays, IDLE and POWER DOWN.

### REV-SLC-013-F002 — corrected — watchpoint could never safely stop in IDLE

“Next completed instruction” was insufficient for peripheral-only delay/IDLE
events. DES-069 now defines a safe execution boundary and forbids stranding
`mTickDelay`. A stopped debugger mutation reports synchronously.

### REV-SLC-013-F003 — corrected — core/session events were conflated

The core bus now explicitly emits raw architectural facts. The debugger adds
session/generation/sequence and synthesizes reset/load/mutation/loss/sink
envelopes. Bounded fan-out, registration-during-dispatch rejection and legacy
callback single-observation rules are explicit.

### REV-SLC-013-F004 — corrected — ring loss marker edge cases

The prior marker-before-next-record rule did not define capacity one or marker-
caused eviction. Overwrite now requires capacity at least two; the first
evicting record may precede one bounded, non-recursive coalesced marker.

### REV-SLC-013-F005 — corrected — file path/stdout policy too weak

Absolute path alone did not prevent symlink/reparse traversal, special files or
overwrite. The design now requires canonical parent validation, exclusive
creation, restrictive permissions, continuation validation, no-overwrite
rotation and optional configured trace root. Headless trace bytes never share
protocol stdout.

### REV-SLC-013-F006 — corrected — condition example addressed an object

The example now reads scalar `newValue.value`, and comparisons against explicit
unknown values have deterministic false semantics.

## Verification

- `make core-test`: passed Stage0, IRQ, timer, UART and port/MOVX suites.
- debug facade binary/test: passed.
- process test could not run in this worktree environment: Make invokes missing
  `python`; available Python is 3.6.15 and cannot parse the test's
  `from __future__ import annotations`. This is an environment/toolchain gap,
  not a product or design failure; the unchanged process suite previously
  guards stdout behavior.
- `git diff --check`: passed.
- target/live audit: only explicit non-goal/test prohibitions mention target or
  physical endpoint terms; no target address, device profile or live endpoint
  was introduced.

## Remaining implementation risks

- Golden ordering tests must be written before refactoring `tick()`; otherwise
  instrumentation could accidentally redefine architectural order.
- Callbacks need a fixed subscriber bound and a documented policy for removal
  after dispatch.
- Cross-platform secure file creation requires separate POSIX and Windows
  implementations and adversarial symlink/reparse tests.
- The legacy curses command editor is new UI work; the accepted first version
  is deliberately a bounded modal command line plus paged ring view.
- Snapshot/replay remains a later Slice and is not evidence of present support.

## Boundary

This approval covers design documentation only. It authorizes no trace product
implementation, DAP change, target-specific peripheral, firmware semantics,
physical serial/network/GPIO/CAN endpoint or machine control.

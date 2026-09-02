# REV-SLC-016 — Synthetic multi-trace router and bounded ring

Date: 2026-09-02
Reviewer role: fresh independent SDP Reviewer
Disposition: **approved with review correction**

## Scope reviewed

- the SLC-016 contract and DES-080..DES-089;
- `emu_debug_trace.h/.c` and `tests/test_debug_trace.c`;
- deterministic gate ordering, nested interrupt boundaries, suppression
  accounting and destination coalescing;
- watch-result routing, replacement neutrality, advertised bounds, ring loss
  accounting, C99 portability and isolation from CPU/peripheral work.

## Finding and correction

### REV-SLC-016-F001 — Four gate timing/action cases lacked direct evidence

Severity: low. Status: corrected.

The implementation followed the required before/after semantics, and the
existing conflict test covered ordered last-action-wins behavior. It did not,
however, assert each of `on-before`, `on-after`, `off-before` and `off-after`
independently. `test_each_gate_timing` now verifies whether the triggering
event is retained and the state visible to the following event for all four
cases. The implementation itself required no correction.

## Behavioral assessment

- Replacement validates the complete bounded configuration before mutation;
  rejected trace, destination, point, gate, metadata and reference values
  leave the live router unchanged.
- Point and gate IDs must be ascending. Routes must be ascending,
  duplicate-free and resolve to live traces. Point unions remain bounded by
  the maximum trace count.
- Before gates run in ascending gate-ID order and control the current event;
  after gates run in the same deterministic order after source routing and
  control the next event. Last matching action in a phase wins.
- Interrupt enter is routed at the old depth and increments afterward;
  interrupt exit is routed at the active depth and decrements afterward.
  Underflow and overflow are rejected before mutation.
- Suppress-during-interrupt retains only the outer enter/exit boundaries;
  interrupt-only retains those boundaries and nested activity. Only an
  enabled, explicitly routed trace accrues suppression. Each trace owns its
  own bounded interval and summary even when destinations are shared.
- A destination receives one event view containing ascending trace IDs, so
  traces sharing a destination do not clone the canonical event. Different
  destinations retain the same sequence identity.
- Explicit SLC-015 watch routes are accepted only as `watch.match` events;
  ordinary routing rejects that kind, preserving the non-recursion boundary.
- Rings have fixed capacity and overwrite the oldest record deterministically;
  `overwritten` explicitly reports capacity loss.
- No diff exists in `core.c`, `opcodes.c`, SAB80535 peripherals,
  `emu_debug.c/.h`, NDJSON protocol code or DAP code.

The future dispatcher integration must bracket derived watch events between
source before- and after-gates, as DES-082/DES-084 require. SLC-016 deliberately
accepts already-formed synthetic watch events and does not connect the router
to that dispatcher or to CPU producers.

## Independent evidence

Results from the worktree root:

- focused router suite built and passed under GCC and Clang with C99,
  `-pedantic`, conversion/sign-conversion checks and warnings as errors;
- Clang AddressSanitizer and UndefinedBehaviorSanitizer run: passed;
- Valgrind full leak check: passed with no reported errors;
- complete Stage-0, IRQ, timer, UART, port/MOVX, event/watch and trace-router
  suites under normal GCC and Clang project flags: passed;
- existing debugger C-facade test: passed;
- `git diff --check`: passed;
- product-boundary diff check: no CPU, opcode, peripheral, existing debugger
  protocol or DAP changes.

The legacy TUI executable could not be linked on this host because `-lcurses`
is unavailable. The process-level NDJSON test consequently had no
`emu-debug` executable to launch. Neither target consumes the new standalone
router in this Slice, so this is an environment note rather than a router
failure.

Strict conversion warnings were applied to the new event/router modules, not
the inherited emulator core: the inherited core has pre-existing conversion
warnings under those extra flags. Its complete regression suite passed with
the repository's normal warning profile under both compilers.

## Disposition rationale

The implementation conforms to the bounded synthetic routing Slice and the
test-evidence gap was corrected. No unresolved blocker remains in SLC-016.
CPU event production, a derived-event dispatcher, sinks and debugger/DAP
exposure remain correctly deferred and should be implemented as separate
conflict-aware slices.

# Implementation notes

SLC-015 is intentionally standalone. It establishes the debugger-side seam so
ongoing SAB80535 conversion work need only call a stable event-ingest API in a
later Slice.

## Worker implementation — SLC-015

Added `emu_debug_event.h/.c` without including or modifying the CPU core. The
new additive C99 surface provides:

- a value-only event schema carrying fixed-width value validity, width and
  signedness metadata;
- debugger-owned monotonic sequencing and an eight-observer fixed-capacity
  fan-out in registration order;
- explicit rejection of subscribe, unsubscribe and nested emit attempts while
  dispatch is active;
- an atomically replaced, ID-sorted fixed-capacity watch table;
- independently conditional stop, console, quiet and trace-route actions using
  `eq`, `ne`, `lt`, `le`, `gt` and `ge` over immutable old/new event values;
- exact-width signed or unsigned comparison, unknown-is-false behavior,
  stop coalescing, quiet-over-console precedence and sorted/deduplicated bounded
  route results.

`tests/test_debug_event.c` covers observer order and capacity, immutable source
copying, dispatch reentrancy rejection, every comparison operator, signed and
unsigned edges, unknown and mixed-type operands, action independence, stop
  coalescing, quiet precedence, stable watch-ID ordering, result bounds and
  atomic invalid replacement. It also verifies that a derived `watch.match`
  event cannot recursively match watches. The focused test is available as
`make debug-event-test` and is also part of `make core-test`.

Worker verification on 2026-09-02:

- GCC C99 warnings-as-errors focused build/test: passed;
- existing Stage-0, IRQ, timer, UART and port/MOVX suites plus the new focused
  suite: passed;
- Clang was not installed in this environment, so its required independent
  warnings-as-errors build remains for Reviewer/Master or CI;
- `core.c`, `opcodes.c`, peripheral code, `emu_debug` protocol and DAP source
  were not edited.

Intentional non-goals remain interrupt routing policy, trace sessions/gates,
derived-event queuing, sinks/rings/JSONL and core instrumentation. Those later
slices consume this foundation rather than expanding SLC-015.

## Reviewer corrections — SLC-015

The independent review corrected three boundary issues before acceptance:

- signed ordering and signed-constant validation now use only portable
  unsigned two's-complement bit operations, including at 64-bit boundaries;
- each observer receives a fresh copy of the canonical event, so a callback
  cannot alter the event subsequently delivered to another observer;
- derived `watch.match` events are explicitly excluded from watch matching,
  enforcing the non-recursive DES-084 rule at the matcher boundary.

Focused, complete regression, strict C99/pedantic/conversion and Valgrind
checks pass. Clang is unavailable on this host and remains a CI/environment
verification item. See `SDP/CodeReview/REV-SLC-015.md`.

## Worker implementation — SLC-016

Added the standalone `emu_debug_trace.h/.c` layer over already sequenced
synthetic events. It provides atomically replaceable, fixed-capacity trace,
destination, point and gate tables; stable sorted route IDs; deterministic
before/after gates; nested interrupt-depth policies; per-trace suppression
summaries; destination-coalesced canonical event views; explicit watch-result
routing; and fixed-capacity rings with overwrite accounting.

The router is deliberately not connected to the CPU or existing debugger
server. `core.c`, `opcodes.c`, SAB80535 peripherals, `emu_debug.c`, the NDJSON
server and DAP remain untouched. File/console destinations and command/protocol
exposure remain later slices.

`tests/test_debug_trace.c` exercises invalid replacement neutrality, shared
and separate destinations, sorted coalesced routes, conflicting before/after
gates, nested include/suppress/interrupt-only behavior, suppression resume and
flush records, explicit non-recursive watch routing with source metadata, ring
wrap/loss accounting, metadata/referential bounds and rejection neutrality.

Worker verification on 2026-09-02:

- GCC and Clang 17 strict C99 builds with warnings, conversion warnings,
  pedantic mode and warnings-as-errors: passed;
- Clang AddressSanitizer plus UndefinedBehaviorSanitizer: passed;
- complete Stage-0, IRQ, timer, UART, port/MOVX, event/watch and new trace
  router regression suites under Clang: passed;
- `git diff --check`: passed.

One integration boundary is intentional: this slice consumes a fully formed
`watch.match` event and its SLC-015 result. Creating that derived event in the
global debugger event queue, and bracketing source after-gates around the
whole derived queue, belongs to the future dispatcher integration slice.

## Reviewer result — SLC-016

REV-SLC-016 approved the implementation with one low-severity correction to
test evidence: all four on/off and before/after gate cases are now asserted
directly. Strict focused GCC/Clang, Clang ASan/UBSan, Valgrind, normal-profile
full GCC/Clang regressions and the existing debugger facade passed. The
unconnected legacy TUI/process executable could not be produced because this
host lacks `libcurses`; this does not affect the standalone router result.

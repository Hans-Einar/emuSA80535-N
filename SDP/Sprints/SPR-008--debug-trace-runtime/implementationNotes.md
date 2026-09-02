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

## Worker implementation — SLC-017

Added `emu_debug_runtime.h/.c`, an opaque debugger-owned in-memory facade that
subscribes one dispatcher to the SLC-015 event bus and composes the accepted
watch matcher with the SLC-016 router. Source events are observed once. The
dispatcher first computes the complete bounded watch-result set, rejects an
overflow before applying any gate, route or stop action, routes the source,
then emits newly sequenced `watch.match` events only after the source callback
has unwound. Derived events drain in ascending watch-ID order before source
after-gates run.

The router received additive `source_begin` / `source_end` entry points for
that bracketing; the existing one-call `router_event` API composes them and
retains its behavior. A selector can now explicitly identify an address space,
which allows image load to disable CODE-addressed points and gates alongside
CODE watches. No CPU producer is connected in this Slice.

The facade provides bounded atomic replacement/listing for watches, trace
sessions, destinations, points and gates; atomic trace enable/disable; source
ingest; status; deferred stop consumption; non-destructive offset-paged ring
reads;
reset/load generation transitions; and clear-session. Trace/watch references
are cross-validated before mutation. Reset and load reestablish interrupt depth
zero while preserving trace configuration, counters, rings, pending stops and
suppression state; load disables stale CODE selectors; clear-session resets
configuration, records, stops, counters, generation and sequencing.

`tests/test_debug_runtime.c` covers source/derived sequence correlation,
ascending watch order, before/source/derived/after bracketing, quiet/console
accounting, multi-watch stop aggregation and primary priority, paged reads,
atomic replacement and referential rejection, trace enable neutrality,
pending-queue overflow without partial routing/gates/stops, every facade count
limit, invalid ingest, reset/load invalidation and clear-session lifecycle.

Worker verification on 2026-09-02:

- strict C99 focused builds/tests with GCC 7 and Clang 17, including pedantic,
  conversion and sign-conversion diagnostics as errors: passed;
- Clang AddressSanitizer plus UndefinedBehaviorSanitizer: passed;
- Valgrind full leak/error check of a non-sanitized focused binary: passed;
- complete Stage-0, IRQ, timer, UART, port/MOVX, event/watch, router and runtime
  regressions with normal GCC and Clang profiles: passed;
- existing debugger C facade and `emu-debug` NDJSON process suite with
  Python 3.11: passed;
- Clang static analyzer and `git diff --check`: passed;
- `core.c`, `opcodes.c`, SAB80535 peripherals, existing `emu_debug.c/.h`,
  server/wire protocol and DAP source were not modified.

File/console sinks, CLI/protocol commands, CPU producer hooks and safe-boundary
application of the returned stop request remain later integration work.

## Reviewer corrections — SLC-017

REV-SLC-017 corrected five boundary issues before approval: ring reads are now
non-destructive offset pages; reset/load are guaranteed lifecycle records for
enabled traces even after CODE selector invalidation; an open source enforces
derived source-sequence/generation correlation; facade ingest rejects invalid
event schema flags before sequencing; and router overwrite/suppression
counters saturate. Source transaction guards, stop priority across pending
sources, atomic replacement for every collection, lifecycle records and the
new validation paths received direct focused coverage.

The separate mandated holistic audit should decide the final externally stable
sized/versioned request wrappers and after-sequence page metadata before this
internal facade is exposed through the existing debugger or wire protocol.

## Issue #14 holistic-review corrections

Corrective Worker product commit:
`d956177add44dda9efbd6d9e372a9c0a6d40f777`.

The Worker resolved the six blocking findings opened by
REV-SLC-017-HOLISTIC:

- ordinary public ingest rejects reset/load; a private lifecycle path retains
  the required generation/depth behavior;
- lifecycle records route from the enabled-state snapshot captured at entry,
  while same-marker gates determine subsequent state without hiding the
  marker from an initially enabled trace;
- address-only selectors are conservatively CODE-capable during load
  invalidation while explicit non-CODE address selectors remain enabled;
- deletion or destination/policy changes for an actively suppressed trace are
  rejected until explicit flush closes the interval;
- a bounded 64-ID session registry permits live-ID updates but rejects retired
  ID reuse until init/clear-session and rejects registry exhaustion atomically;
- tags/comments receive bounded locale-independent strict UTF-8 validation.

Direct tests cover every correction, including lifecycle gate timings,
neutral forged lifecycle rejection, CODE invalidation distinctions,
suppression replacement/flush paths, trace-ID live update/reuse/exhaustion/
clear behavior and valid/malformed UTF-8 boundaries.

Worker evidence passed strict GCC 12.2 and Clang 18.1.8 C99 focused builds,
the focused suites, Clang ASan/UBSan and `make core-test`. MinGW sanitizer
runtime libraries were unavailable; this is an environment note rather than a
skipped sanitizer gate because the Clang sanitizer build/run passed.

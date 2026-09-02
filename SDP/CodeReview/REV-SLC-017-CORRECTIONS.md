# REV-SLC-017-CORRECTIONS — Independent corrective diff review

Date: 2026-09-02
Reviewer role: fresh independent SDP Reviewer
Steering authority: Issue #14
Correction parent: `c080273f8e1d7394c92d4425a6614c40ad7fc7c1`
Correction product commit: `d956177add44dda9efbd6d9e372a9c0a6d40f777`
Takeover HEAD inspected: `8c16d42621afda98b9c9ac7407027fc7ccf10058`
Status: complete
Disposition: **approved for Master verification**

## Authority, independence and scope

This is the separate fresh correction review required by Issue #14 and the
active corrective Worker contract in ITR-017. The exact reviewed product/test
range is:

`c080273f8e1d7394c92d4425a6614c40ad7fc7c1..d956177add44dda9efbd6d9e372a9c0a6d40f777`

The current takeover HEAD adds only Master SDP handoff after the correction
commit. This Reviewer inspected Issue #14 in full, the active ITR-017 contract,
`REV-SLC-017-HOLISTIC.md`, the complete current event/trace/runtime
implementation and the exact correction diff. No product correction was made
by this Reviewer.

The correction diff contains exactly these five authorized files:

- `emu_debug_runtime.c`;
- `emu_debug_trace.c`;
- `emu_debug_trace.h`;
- `tests/test_debug_runtime.c`;
- `tests/test_debug_trace.c`.

No CPU, opcode, SAB80535 peripheral, existing `emu_debug` facade/server,
wire-protocol, CLI, DAP, sink, source-map, P1000 or physical-I/O file changed.

## Findings ordered by severity

No blocking, medium, low or informational product correctness finding remains
in the reviewed correction diff.

One verification-harness observation is recorded for reproducibility, not as
a product finding: Clang ASan/UBSan with `-O1` and default inlining overflowed
the default Windows process stack at `tests/test_debug_trace.c:633`, before a
test entered product logic. The test translation unit places several very
large router values on function stacks; inlining combines those frames.
The `-O1` sanitizer build with `-fno-inline` passed the trace/runtime suites,
and an independent `-O0 -fno-inline` sanitizer build passed all three focused
suites. Normal strict `-O2` GCC and Clang builds also passed. This does not
indicate an out-of-bounds, recursion or lifetime defect in the product, whose
opaque composed runtime is heap-owned.

## Holistic finding disposition

### REV-SLC-017-HOLISTIC-F001 — resolved

Ordinary `em8051_debug_runtime_ingest()` rejects `RESET` and `LOAD` before
sequencing, counter, generation, interrupt-depth, gate, ring or stop mutation.
The assigned-sequence output is left untouched on rejection. The private
`runtime_ingest(..., true)` path is reachable only through the reset/load
lifecycle wrappers, which retain generation increment, depth reset and
rollback behavior. Direct tests distinguish neutral forged-marker rejection
from successful private reset/load operations.

### REV-SLC-017-HOLISTIC-F002 — resolved

For both reset and load, `source_begin` snapshots every trace's enabled state
before applying same-marker before-gates. Routing uses that snapshot, while
before-gates and later after-gates still establish the state for subsequent
events. Initially enabled traces therefore retain exactly one lifecycle
marker even for `off-before` or `off-after`; initially disabled traces do not
gain that marker from `on-before` or `on-after`. The test matrix covers all
four gate action/timing combinations for both lifecycle kinds and verifies
the post-marker state.

### REV-SLC-017-HOLISTIC-F003 — resolved

Load invalidation now conservatively classifies an address-range selector
without an address-space qualifier as CODE-capable. PC selectors and explicit
CODE selectors are also invalidated, while an explicit non-CODE address-space
selector remains enabled. Direct runtime tests cover points and gates in all
three categories and retain existing CODE-watch invalidation.

### REV-SLC-017-HOLISTIC-F004 — resolved

Before any replacement commit, the router checks every active suppression
interval. Deleting its trace or changing that live trace's destination or
interrupt policy returns `BUSY`; configuration, ring, suppression state and
seen-ID history remain unchanged. An explicit router flush emits the one
pending suppression summary and clears the interval, after which policy
change, destination change and deletion all succeed. Enabled state and
metadata may still be updated for the same live trace because they neither
lose nor misattribute the interval.

### REV-SLC-017-HOLISTIC-F005 — resolved

The router keeps a fixed 64-entry session history of admitted trace IDs. A
currently live ID may be updated without consuming a new history slot. A
removed ID remains retired and reuse returns `DUPLICATE_ID`; admitting an
unseen ID after all 64 slots are used returns `LIMIT`. Both rejection paths
occur before mutation. Router initialization and opaque-runtime
`clear_session` reset the registry, after which an ID may be admitted again.
Direct tests cover live updates, deletion/reuse, exhaustion neutrality and
both reset paths.

### REV-SLC-017-HOLISTIC-F006 — resolved

Tag and comment validation is fixed-bound and locale-independent. It accepts
ASCII and well-formed two-, three- and four-byte UTF-8, rejects stray
continuations, overlong encodings, truncated sequences, UTF-16 surrogate
encodings and code points above U+10FFFF, and still requires a NUL terminator
inside each advertised byte array. Configuration validation completes before
any router mutation, so malformed metadata preserves the prior configuration.
Tests cover valid four-byte input, exact 64/256-byte payload bounds and the
malformed/truncated boundary classes.

## Cross-cutting regression audit

- **Correctness and sequencing:** lifecycle snapshots affect only marker
  visibility. Canonical source/derived ordering, monotonic sequence assignment,
  source correlation and after-gate timing are unchanged.
- **Determinism and bounds:** all new searches are fixed-bound linear scans;
  route order, trace order and destination coalescing remain stable. The ID
  history has an explicit 64-entry limit and cannot overrun its `uint8_t`
  count.
- **Atomicity:** UTF-8, suppression compatibility, retired-ID and history-
  capacity checks all precede ring/config/suppression/history mutation. Failed
  ordinary lifecycle ingest also exits before runtime mutation.
- **Lifecycle:** reset/load remain the only lifecycle entry points, generation
  and sequence semantics are unchanged, reset preserves configuration, load
  adds only the required conservative CODE invalidation, and clear-session
  resets ID history with the rest of the session.
- **Suppression:** interval attribution remains bound to stable trace ID,
  destination and policy. Lifecycle marker routing closes an active interval
  before the retained marker when applicable; replacement cannot discard it.
- **C99, ABI and lifetime:** strict GCC and Clang C99 builds pass. New arrays
  have compile-time capacities and no caller-owned pointer is retained. The
  changed raw router layout remains the documented same-build internal seam;
  no claim of stable external ABI or facade version was introduced.
- **Compatibility:** event/watch behavior, existing ring retention, point/gate
  ordering and the unmodified `emu_debug` 1.0 boundary remain regression-safe.
  No CLI, protocol, DAP or CPU-producer behavior was added.

## Independent evidence

Environment:

- GCC 12.2.0;
- Clang 18.1.8, `x86_64-pc-windows-msvc`;
- GNU Make 4.4;
- Git 2.43.0.windows.1.

Evidence run from `codex/debug-trace-runtime-takeover`:

- strict GCC C99 `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion
  -Wpedantic -Werror`: event/watch, trace-router and composed-runtime suites
  passed;
- strict Clang C99 with the same diagnostics and the required Windows CRT
  compatibility define: all three focused suites passed;
- Clang ASan/UBSan at `-O0 -fno-inline`: all three focused suites passed;
- Clang ASan/UBSan at `-O1 -fno-inline`: trace-router and composed-runtime
  suites passed;
- normal GCC `make core-test`: Stage-0, IRQ, timer, UART, port/MOVX,
  event/watch, trace-router and composed-runtime suites passed;
- `git diff --check` for the exact correction range: passed;
- exact changed-file and forbidden-scope audit: passed.

This focused correction review does not substitute for the broader Master
`VER-SLC-017` matrix, including debugger facade/process, Python, Valgrind and
traceability gates required by Issue #14.

## Disposition

**Approved for Master verification.** Every blocking finding opened by
`REV-SLC-017-HOLISTIC` is resolved in the authorized standalone debugger
runtime scope, with no correction requested from the Worker and no unresolved
Steering question. The corrections preserve SLC-015..017 determinism, bounds,
atomic replacement, lifecycle, suppression, ABI/lifetime and compatibility
contracts.

SLC-017 may proceed to Master `VER-SLC-017` and the documentation-only stable
facade/versioning/paging freeze. This review does **not** itself accept SLC-017
or authorize frontend, wire, DAP, CPU integration, PR merge or any other new
scope.

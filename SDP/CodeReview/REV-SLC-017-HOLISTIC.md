# REV-SLC-017-HOLISTIC — Independent SLC-015..017 takeover review

Date: 2026-09-02
Reviewer role: fresh independent SDP Reviewer
Steering authority: Issue #14
Reviewed product baseline: `356836637d5ff432d91fc508fd55b2f17b45cdb3`
Takeover scaffolding inspected at: `79e83c5aeb254bc496a3e48cf5d921112efcd88c`
Status: complete, corrections required
Disposition: **corrections required**

## Authority, independence and scope

This is the acceptance review required by Issue #14. The review first derived
its conclusions from Issue #14, the active SDP contracts, DES-064..DES-089,
the product sources and focused tests. `REV-SLC-017.md` was read only after
that independent pass and is treated as historical evidence, not authority.

The product review covers:

- `emu_debug_event.h/.c` and `tests/test_debug_event.c` from SLC-015;
- `emu_debug_trace.h/.c` and `tests/test_debug_trace.c` from SLC-016;
- `emu_debug_runtime.h/.c` and `tests/test_debug_runtime.c` from SLC-017;
- build integration, the existing `emu_debug` 1.0 facade/process seams and the
  forbidden-scope diff.

No product correction was made by this Reviewer.

## Blocking findings

### REV-SLC-017-HOLISTIC-F001 — Public ingest can counterfeit lifecycle events

Severity: medium. Status: open. Scope: authorized SLC-017 correction.

`source_event_valid()` accepts every kind through
`EM8051_DEBUG_EVENT_DEBUGGER_MUTATION`, including `RESET` and `LOAD`
(`emu_debug_runtime.c:121-133`). The public ingest entry point then accepts
those kinds (`emu_debug_runtime.c:377-400`). Such an event receives the current
generation and is routed as a lifecycle marker, but it does not increment the
generation, reset interrupt depth or perform load-time CODE invalidation.
Those effects exist only in `lifecycle_event()` and
`em8051_debug_runtime_load()` (`emu_debug_runtime.c:441-526`).

The same public API can therefore produce two observably different reset/load
records and bypass the lifecycle invariants required by DES-065, DES-074 and
DES-086. The focused invalid-ingest test rejects `watch.match` but does not
test direct reset/load ingestion (`tests/test_debug_runtime.c:462-479`).

Required correction: reserve reset/load kinds for the lifecycle operations,
reject them at the ordinary source-ingest boundary, and add neutral rejection
tests. If a private common emitter is needed, it must not reopen the public
bypass.

### REV-SLC-017-HOLISTIC-F002 — Before-gates can suppress required lifecycle markers

Severity: medium. Status: open. Scope: authorized SLC-016/017 correction.

The router applies matching before-gates before it constructs the special
reset/load route set (`emu_debug_trace.c:287-320`). `route_ids()` then drops
every disabled trace (`emu_debug_trace.c:209-220`). A matching `off-before`
gate can consequently disable a trace before the reset/load record is stored.
This contradicts the lifecycle-marker retention rule in DES-074
(`SDP/05--Design/EMU-DEBUG-DES-007.md:98-101`) and disproves the unconditional
retention claim in the historical review. The existing lifecycle test covers
point/filter bypass but configures no gate
(`tests/test_debug_trace.c:359-379`).

Required correction: freeze and implement lifecycle gate semantics explicitly.
The recommended rule is that reset/load markers are routed from the enabled
state captured at the lifecycle boundary and are not made invisible by a gate
on that same marker. Add direct on/off, before/after lifecycle tests.

### REV-SLC-017-HOLISTIC-F003 — Load misses CODE-capable address-only selectors

Severity: medium. Status: open. Scope: authorized SLC-017 correction.

Selectors may match an address range without selecting an address space
(`emu_debug_trace.c:20-40`). Such a selector can match CODE events. The load
invalidation predicate, however, recognizes only a PC selector or an explicit
CODE address-space selector (`emu_debug_runtime.c:485-490`), so address-only
points and gates remain enabled across an image load
(`emu_debug_runtime.c:509-519`). Existing coverage exercises only a PC point,
an explicit CODE-space gate and a CODE watch
(`tests/test_debug_runtime.c:333-384`).

Required correction: either require an explicit address space whenever an
address selector is present, or conservatively classify an address-only
selector as CODE-capable for load invalidation. The chosen rule must be
documented and covered without weakening non-CODE selectors.

### REV-SLC-017-HOLISTIC-F004 — Trace replacement can silently discard a suppression interval

Severity: medium. Status: open. Scope: authorized SLC-016/017 correction.

Router replacement preserves a pending suppression interval only when the
same trace ID keeps both destination and interrupt policy. Otherwise the
temporary suppression table remains zero and the live interval disappears at
commit (`emu_debug_trace.c:242-273`). No summary is emitted and no rejection
reports that a flush is required. DES-083 requires one summary before the
trace's next retained event or on flush/close
(`SDP/05--Design/EMU-DEBUG-DES-007.md:193-213`).

Required correction: do not silently drop active intervals. A bounded option
is to reject deletion or destination/policy replacement for an actively
suppressed trace until an explicit flush has closed the interval. Whichever
rule is chosen must preserve replacement atomicity and receive tests for
policy change, destination change and deletion.

### REV-SLC-017-HOLISTIC-F005 — Trace IDs can be reused before clear-session

Severity: medium. Status: open. Scope: authorized standalone-runtime
correction or an explicit Steering-approved design revision.

DES-080 says trace IDs are not reused until `clear session`
(`SDP/05--Design/EMU-DEBUG-DES-007.md:153-160`). The runtime stores only the
current replacement set and no allocation high-water mark or used-ID history
(`emu_debug_trace.h:84-94`, `emu_debug_runtime.c:272-299`). A caller can remove
an unreferenced trace and add a different trace with the same ID without
clearing the session. That makes retained ring records and pending counters
ambiguous.

Required correction: enforce non-reuse for the session or revise the design
under Steering authority before acceptance. Tests must distinguish updating a
live trace definition from deleting and later reusing its ID.

### REV-SLC-017-HOLISTIC-F006 — Trace metadata validation does not validate UTF-8

Severity: low. Status: open. Scope: authorized standalone-runtime correction.

Configuration validation checks only for a NUL within each fixed tag/comment
array (`emu_debug_trace.c:9-12`, `emu_debug_trace.c:88-95`). It accepts malformed
UTF-8 even though DES-080 and DES-087 require bounded UTF-8 metadata and atomic
rejection (`SDP/05--Design/EMU-DEBUG-DES-007.md:153-163`,
`SDP/05--Design/EMU-DEBUG-DES-007.md:271-277`). This becomes observable through
the facade list operations.

Required correction: validate the bytes preceding the first NUL with one
bounded, locale-independent UTF-8 validator before committing a candidate.
Add malformed, truncated and boundary-length cases and prove rejection leaves
the previous configuration intact.

## Non-blocking freeze decisions and future-scope gaps

These are not authorization to implement a frontend, CPU producer, sink or
protocol extension in Issue #14.

### Current C surface remains internal and experimental

`EM8051_DEBUG_RUNTIME_API_VERSION` is only a macro; request, response, status,
point and page structs have no `struct_size` or version fields
(`emu_debug_runtime.h:10-38`, `emu_debug_runtime.h:40-112`). The raw event and
router structs also expose compiler padding and implementation-owned fields.
They are acceptable for the current same-build, in-memory test seam only. They
must not be declared the stable DES-076/DES-087 ABI.

The future external facade should use opaque runtime ownership plus separate
request/response wrappers whose first fields are fixed-width `struct_size` and
`api_version`. Inputs are copied after validating the advertised size/version;
unknown trailing fields are ignored only under a compatible major version.
Outputs are caller-owned and report required element/byte counts. No product
struct, callback pointer, enum storage size or padding crosses that ABI.

### Paging must change before external exposure

The current offset read is bounded and non-destructive, but an offset is not a
stable cursor if retention advances. The opaque facade also does not return
the ring's cumulative `overwritten` count (`emu_debug_runtime.c:577-608`), and
the current ring implements only that cumulative count rather than the
DES-072 loss interval/marker (`emu_debug_trace.c:114-128`,
`emu_debug_trace.h:106-121`). This is acceptable only as the explicitly
internal SLC-016/017 seam; it is not sufficient for CLI/protocol paging.

Before exposure, freeze `read-after-sequence` as an exclusive cursor and
return at least: requested cursor, first/last available canonical sequence,
first/last returned sequence, next exclusive cursor, returned and remaining
counts, `more`, generation/session identity, and cumulative overwrite/loss
metadata. Loss and suppression records need an unambiguous ordered cursor; the
design must not make a synthesized summary with `event.sequence == 0`
indistinguishable or unpageable. A stale cursor older than retained history
must return the available page plus explicit lost-before-page metadata, not an
empty success and not an offset reinterpretation.

Implementing configurable capacity, DES-072 loss markers, stop-on-full, full
condition language/delta/hit accounting, file/console sinks and output
encoding remains future scope. Their absence is not reclassified as an
SLC-017 regression by this review.

### One emulator-owned model is a next-integration obligation

The existing `emu_debug` 1.0 CODE-breakpoint bitmap is unchanged and the new
runtime is intentionally unconnected. The next integration slice must compose
CODE breakpoints, safe-boundary watch stops and trace/log actions in the
emulator facade and then let CLI/protocol/DAP frontends project that one model.
Exception, halt and pre-execution-breakpoint priority over watch stops is
therefore a future integration test, not an SLC-017 standalone defect. The
runtime must not be copied into separate CLI and DAP implementations.

## Fifteen-dimension review matrix

1. **Sequencing/source-derived order:** source receives one sequence; complete
   watch matching precedes routing; derived events receive ascending watch-ID
   sequences; after-gates wait for drain. Conforms for accepted ordinary
   sources.
2. **Recursion/reentrancy:** nested bus emit and subscription mutation are
   rejected during dispatch; the opaque runtime rejects public mutation while
   busy. No recursive `watch.match` path was found.
3. **Watch/point ordering:** watches are sorted by ID during atomic replacement;
   points/gates require strict ID order; route unions are ascending and
   duplicate-free. Conforms.
4. **Gate phases:** ordinary before/source/derived/after behavior conforms;
   lifecycle behavior is blocked by F002.
5. **Interrupt policies:** outer enter is routed at depth zero, outer exit at
   depth one, nested activity uses actual depth, and underflow/overflow is
   rejected before mutation. Conforms for valid streams.
6. **Pending overflow neutrality:** all matches and derived-sequence capacity
   are preflighted. Overflow changes no gate, ring, stop or router depth. The
   rejected canonical source intentionally consumes its sequence and increments
   rejection/overflow counters; this behavior is directly tested and must be
   retained or explicitly revised with paging semantics.
7. **Stop coalescing/priority:** stop count saturates and the lowest watch ID,
   then earliest sequence for equal IDs, wins across pending sources. Full
   exception/halt/breakpoint priority is correctly deferred to CPU integration.
8. **Lifecycle generation/sequence:** wrapper reset/load advance generation,
   keep sequence monotonic and restore depth zero; clear-session restarts the
   session. F001 and F002 prevent approval of the complete lifecycle boundary.
9. **Load CODE invalidation:** explicit CODE-space watches and explicit
   CODE/PC points/gates are disabled. F003 leaves a CODE-capable selector path.
10. **Ring retention/paging/loss:** current fixed ring retention, saturated
    overwrite count and non-destructive stopped-boundary offset reads work.
    The external loss/page seam is not frozen in product and is documented
    above as a mandatory pre-exposure decision.
11. **Atomic replacement/references:** candidate validation prevents dangling
    watches, traces, destinations, points and gates and preserves state on
    rejected inputs. F004-F006 cover missing lifecycle/validation rules.
12. **Bounds/overflow/saturation:** observer, watch, route, trace, destination,
    point, gate, pending and ring capacities are fixed. Sequence/generation
    exhaustion is rejected and long-lived implemented counters saturate. No
    array overflow was found in the route-union or ring arithmetic.
13. **C99 ABI/ownership/lifetime:** the runtime owns heap state and copies
    caller inputs; page/list buffers remain caller-owned. Strict C99 builds
    pass. The raw structs are not a stable cross-version ABI and must remain
    internal until the sized/versioned wrapper is implemented.
14. **`emu_debug` 1.0/CLI/DAP compatibility:** SLC-015..017 do not edit
    `emu_debug.c/.h`, `emu_debug_server.c`, CPU/opcode/peripheral sources or any
    DAP product. The existing regression suites pass. No protocol capability,
    command or unsolicited stdout behavior changed.
15. **Facade/versioning/after-sequence decision:** current facade may remain
    internal for one corrective slice. External exposure is blocked until the
    wrapper and page contract above is documented and implemented in a
    separately authorized integration slice.

## Independent treatment of historical REV-SLC-017 findings

- **F001 non-destructive ring reads:** independently reproduced. The current
  implementation copies offset pages and the same page can be read again.
- **F002 lifecycle marker retention:** the point-selector and interrupt-policy
  correction is reproduced, but the claimed complete guarantee is **rejected**
  because F002 above shows that a before-gate can still hide the marker.
- **F003 open-source derived correlation:** independently reproduced. While a
  source is open, source sequence and generation are checked and invalid
  attempts do not advance the router.
- **F004 source schema flag validation:** independently reproduced for the
  exact enum/access/known/signedness/width flags named by that review. Its
  broader implication is incomplete because F001 still permits forged
  lifecycle kinds through ordinary ingest.
- **F005 saturating overwrite/suppression counters:** independently reproduced.
  Both increments saturate at `UINT64_MAX`.

## Independent evidence

Environment:

- GCC 12.2.0;
- Clang 18.1.8, Windows MSVC target;
- GNU Make 4.4;
- Python 3.14.0;
- Git 2.43.0.windows.1.

Evidence run from the takeover worktree:

- `make core-test`: passed Stage-0, IRQ, timer, UART, port/MOVX, event/watch,
  trace-router and composed-runtime suites;
- strict GCC C99/pedantic/shadow/conversion/sign-conversion warnings-as-errors
  build and runtime suite: passed;
- strict Clang compile of each new module: passed;
- strict Clang runtime build/run with `_CRT_SECURE_NO_WARNINGS`: passed;
- without that Windows CRT define, Clang warnings-as-errors stops on the test's
  use of `strcpy` at `tests/test_debug_runtime.c:80`; this is a test/environment
  diagnostic, not a product-module warning;
- product diff audit: only the new event/trace/runtime modules and their build/
  test/SDP integration changed; no CPU, opcode, SAB peripheral, `emu_debug`
  protocol, DAP, P1000 or physical-I/O implementation was added.

ASan/UBSan, Valgrind, debugger-facade/process regressions and traceability
parsing remain Master verification gates; this review does not substitute for
VER-SLC-017.

## Disposition

**Corrections required.** F001-F006 are within the standalone SLC-015..017
boundary and must be resolved or explicitly escalated before acceptance. A
fresh correction Reviewer must inspect the resulting product diff. The facade
and paging decisions above must be integrated into authoritative design/SDP
documentation before any CLI, `emu-debug` extension or DAP work begins.

No unauthorized CPU producer, safe-boundary CPU hook, CLI, wire-protocol, DAP,
sink, source-map, P1000 or physical-I/O change is justified by these findings.

# REV-SLC-017 — Derived-event dispatcher and in-memory facade

Date: 2026-09-02  
Reviewer role: fresh independent SDP Reviewer  
Disposition: **approved with review corrections**

## Scope reviewed

- the SLC-017 contract, DES-064..DES-089 and the accepted SLC-015/016
  foundations;
- `emu_debug_event.h/.c`, `emu_debug_trace.h/.c`,
  `emu_debug_runtime.h/.c` and their focused tests;
- exact source-before-gate, source route, newly sequenced derived-watch and
  source-after-gate ordering;
- pending overflow neutrality, stop priority, source transaction state,
  referential replacement, listing, ring paging and lifecycle behavior;
- fixed bounds, C99/compiler portability and the forbidden product boundary.

## Findings and corrections

### REV-SLC-017-F001 — Facade ring reads consumed retained records

Severity: medium. Status: corrected.

The Worker API implemented pages by popping the ring. That contradicted the
DES-072/DES-076 ownership model: retrieval is a read and clearing is an
explicit lifecycle action. `em8051_debug_runtime_read_ring` now copies an
offset page in oldest-to-newest order without mutation and reports the number
of records following the page. Tests reread an earlier page and exercise a
zero-capacity query.

### REV-SLC-017-F002 — Load could omit the required lifecycle marker

Severity: medium. Status: corrected.

Load correctly disabled stale CODE selectors before dispatch, but this also
meant no point might select the load event. Reset/load records are session
markers rather than ordinary implicit subscriptions. The router now routes
them to every currently enabled trace, coalesced per destination, regardless
of ordinary point selectors. Interrupt inclusion policies cannot suppress
these boundary markers. The lifecycle test proves consecutive interrupt,
reset and load sequences and generations, including a load after its CODE
point was disabled.

### REV-SLC-017-F003 — Open-source derived correlation was not enforced

Severity: medium. Status: corrected.

The runtime constructed correct derived records, but the additive low-level
router entry point accepted an unrelated watch result while a source was open.
It now requires the watch result's source sequence and derived generation to
match the open source. Replacement and suppression flush remain `BUSY` until
`source_end`; invalid begin/end/correlation attempts are neutral and tested.

The runtime preflights the complete watch-result count and all required
derived sequence IDs before `source_begin`. Once begin succeeds, its internal
derived calls have already-valid trace references and reserved sequence room,
so the composed path contains no expected fallible operation before
`source_end`. Pending overflow therefore consumes only the canonical source
sequence and changes no gates, rings, stops, interrupt depth or router
sequence. This is asserted directly.

### REV-SLC-017-F004 — Synthetic source schema accepted invalid enum/value flags

Severity: low. Status: corrected.

The opaque facade now rejects out-of-range event kind, address space, access
bits, value-known flag, signedness and width before entering the event bus.
Rejected caller input does not consume a sequence and does not increment the
dispatcher rejection count; the latter remains reserved for a canonical event
that reached the dispatcher but could not be accepted there.

### REV-SLC-017-F005 — Two long-lived counters could wrap

Severity: low. Status: corrected.

Ring overwrite and suppression counts now saturate at `UINT64_MAX`, matching
the design's deterministic counter rule. Tests cover both saturation paths.
A duplicate `watch.match` exclusion guard in the accepted matcher was also
removed without changing behavior.

## Behavioral assessment

- One source receives one bus sequence and one dispatcher callback. Watches
  are evaluated from the complete immutable source in ascending watch-ID
  order before routing begins.
- Before-gates affect the source and every derived watch event. The source is
  routed first; each fired watch then receives its own sequence, retains the
  source sequence, and drains in watch-ID order. After-gates run only after the
  final derived event.
- A pending-list overflow is detected from the matcher's required count before
  source routing. No prefix of matches is routed and no stop/console action is
  committed.
- Stop requests coalesce and saturate. The primary is the lowest watch ID and,
  for equal IDs, the earliest source sequence, including across multiple
  sources waiting for safe-boundary consumption.
- Each replacement operates on a bounded candidate and validates references
  before commit. Rejected watch, trace, destination, point, gate and enable
  changes preserve prior state. Listings and ring reads are bounded and
  offset-paged.
- Reset/load advance generation without resetting sequence, clear interrupt
  depth, preserve configuration/counters/rings/stops, and emit retained
  lifecycle markers. Load disables CODE-space/PC selectors. Clear-session
  creates a fresh generation/sequence/configuration state.
- No changes exist in `core.c`, `opcodes.c`, SAB peripherals,
  `emu_debug.c/.h`, `emu_debug_server.c`, the NDJSON contract or DAP code.

## Independent evidence

The final corrected tree was checked with:

- focused event, trace and composed-runtime suites under GCC 7 and Clang 17
  with C99, pedantic, conversion/sign-conversion and warnings as errors;
- Clang AddressSanitizer plus UndefinedBehaviorSanitizer;
- Valgrind full leak/error checking;
- Clang static analyzer;
- complete normal-profile Stage-0, IRQ, timer, UART, port/MOVX, event, trace
  and runtime regressions under both GCC and Clang;
- the existing debugger C-facade and Python 3.11 `emu-debug` NDJSON process
  suites;
- `git diff --check`, YAML/NDJSON parsing and a forbidden-diff audit.

All executed checks passed. The inherited CPU/opcode sources retain existing
conversion warnings under stronger-than-project flags, so strict conversion
flags were correctly scoped to the new event/trace/runtime modules; complete
core regressions were run with the repository's normal warning profile.

## Holistic follow-up for the mandated subsequent audit

SLC-017 is a bounded internal composition layer, not yet the complete stable
DES-076 facade. Before external ABI stabilization, the next audit should
resolve sized/versioned public request/response wrappers and evolve offset
paging to the final after-sequence page metadata. It should also review the
already-deferred loss-marker/configurable-ring behavior, full point condition
language, file/console sinks, CPU producers and safe-boundary stop application.
These are not regressions in this Slice and must not be folded into CPU
conversion work without their own contracts.

## Disposition rationale

All SLC-017 requirements are met after the corrections above, with explicit
coverage for the previously weak lifecycle, pagination, source-state and
validation paths. No unresolved SLC-017 blocker remains. Acceptance still
requires Master verification and the contractually required separate fresh
holistic review of SLC-015..017.

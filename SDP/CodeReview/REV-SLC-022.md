# REV-SLC-022 — Deterministic session and local composed runtime

Status: approved with resolved review findings
Date: 2026-10-27
Reviewer: fresh independent Reviewer `/root/runtime_reviewer`
Authority: SPR-013 / ITR-022 / SLC-022, REQ-026, DES-115..DES-118
Reviewed baseline: `b946533`
Approved product candidate: `a03504bd186efe3a8882bd3f1cdb5c4cf463f30d`
Earlier integrated candidate: `957dd9c` (not accepted; F008 required correction)

## Decision and scope

Approve the generic local runtime implementation and its exact source handoff.
All findings below are resolved in the approved candidate. The review covers the
complete diff from the accepted CPU handoff, including shared legacy-helper
extraction, session/debug service, scheduler, loader, local script interface,
synthetic modules, build/install changes and their evidence. It does not accept
an external board implementation, full machine boot, network/DAP/signals, RPM
publication or the deferred SDP migration.

The Reviewer read AGENTS.md, CurrentIndex, the active Sprint/Handoff/RuntimeSlice/
LocalHostPlan, applicable lifecycle documents and the immutable API1/ABI1/wire2.1
contracts. Implementation remained with the two designated fresh Workers;
this Reviewer supplied findings and independent checks, not product fixes.
The Master recorded the narrow callback-time, post-fault inspection policy and
unsafe termination clarifications before their implementation was accepted.

## Resolved findings

| Finding | Severity | Resolution and evidence |
|---|---|---|
| REV-SLC-022-F001 | high | Native config whitespace removal could repair malformed JSON or strip raw NUL. Original syntax/UTF-8 validation now precedes any projection; duplicate keys and malformed surrogate data reject before board creation. Native negative cases pass. |
| REV-SLC-022-F002 | high | Host initially used a string-only matcher for primitive `true`/`null`, and script payload nesting gained a level inside the native request. Bounded primitive checks and complete pre-execution wrapper-depth validation now reject missing/invalid fields and late invalid commands before loading/execution. Process tests pass. |
| REV-SLC-022-F003 | high | UART completion reports cycle t+1 before the session finishes cycle t; using only the completed-session cycle admitted zero-delay feedback. Each callback now supplies its own scheduling floor. Genuine UART mode3 tests reject the same event timestamp, accept the following cycle and preserve admission identity. |
| REV-SLC-022-F004 | high | Read-only callbacks could attempt reentrant destruction, and numeric equality with a retained fault cause could misclassify a later ordinary STATE error as a new FAULT. Callback violation handling and a per-request actual-fault marker now preserve fault state and report wire FAULT only for the failing request. Independent STATE-cause, port and MOVX probes pass. |
| REV-SLC-022-F005 | medium | Strict framing checks were incorrectly applied to multiline native JSON and escaped NUL inside opaque JSON strings/keys. Native JSON now retains valid opaque values with length-aware duplicate-key checking. Inspection output that exceeds the enclosing wire depth returns CAPACITY without creating an architectural fault. Independent multiline/depth probes pass. |
| REV-SLC-022-F006 | medium | Connection identity retention needed an explicit resource bound without inventing monotonically increasing connection IDs. The documented implementation accepts eight concurrent clients and 8192 lifetime IDs, rejects reuse with STALE and returns CAPACITY before mutation on lifetime exhaustion. Descending arbitrary IDs, active clients and duplicate IDs at exhaustion are tested. |
| REV-SLC-022-F007 | high | Unsafe fault termination cannot truthfully emit a terminated/safe snapshot under the frozen rules. The recorded narrow interpretation of “acknowledge ... if possible” closes all clients with native OK and zero written/required bytes; it neither fabricates a safe boundary nor permits later execution/replay. Safe termination still acknowledges normally. Native lifecycle tests pass. |
| REV-SLC-022-F008 | high | Candidate957dd9c required successful board inspection `required == written`, a condition stated for the debug facade but not the board ABI. The accepted old-SDK external module reports required4096 with a shorter valid object and was incorrectly faulted. Candidatea03504b accepts `written <= required <= capacity4096`, validates exactly the written JSON, and still rejects invalid metadata. Native regression, independent generic reproduction and the exact external module probe pass. |
| REV-SLC-022-F009 | medium | Delivery needed an explicit ABI1 host-platform guard and the installed host's product documentation. The loader now rejects unsupported compilation platforms; root installation carries runtime libraries, executable, schemas/example/docs and exact source identity. Installed C99/C++17 consumers and the install checker pass. |

## Independent verification

The Reviewer cloned the repository into `/tmp/slc022-review-957dd9c`, built the
clean committed candidate independently, then advanced that isolated checkout
to the three-file correction `a03504b`. The correction changes the inspection
metadata predicate, its regression tests and an EOF whitespace defect. Earlier
unaffected CPU/legacy checks remain applicable; runtime, sanitizer and install
checks were rerun on the correction.

- `make -j4 emu-runtime sdk` succeeded from clean sources.
- `make runtime-session-test runtime-host-test` passed on the final candidate;
  **1116 actual native responses** validate against the frozen wire2.1 schema.
  Host tests independently compile old SDK0.1 ABI1 shared modules and cover
  descriptor/callback/capability rejection, startup failure, full static script
  validation, exactly-once accesses, mixed timestamp/order delivery, IDLE wake,
  deterministic replay/reset, pause/step/breakpoints and fault inspection.
- `make cpu-facade-test cpu-link-test debug-test debug-event-test
  debug-trace-test interface-check` passed. Current and immutable0.1 C99/C++17
  headers link; legacy facade/process protocol1.0 and shared helper extraction
  regressions pass. The private generated wire validator reproduces byte for byte.
- `make core-test` passed the CPU facade, Stage0, interrupts, timers, UART,
  ports/MOVX, external edges, ADC, Timer2, event/watch, trace and debug dispatcher
  gates.
- Independent adversarial probes passed on the final candidate, including
  required4096 multiline inspection; output depth CAPACITY without state change;
  MOVX failure before any completed cycle; boundary failure after exactly one
  cycle/instruction; port-output failure without fabricated cycles; retained
  STATE cause followed by a distinct ordinary STATE rejection; and preservation
  of trailing storage in larger native output records.
- Those probes also passed AddressSanitizer and UndefinedBehaviorSanitizer with
  leak detection using all runtime/CPU source files, `-O1 -g
  -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie`.
- `make install DESTDIR=/tmp/slc022-review-install prefix=/usr`,
  `packaging/check_install.py` with the exact final commit and `/usr/lib`, and a
  separate C++17 consumer linked solely against installed runtime/CPU archives
  passed. Installed runtime/debug source identities match the final commit.
- `git diff --check b946533 a03504b` passes. The diff for both immutable bundles
  and all public contract headers is empty. CPU implementation files are
  unchanged by this Slice; the controller uses the opaque CPU API. Shared JSON,
  SHA and CODE bitmap helpers replace existing private copies; no second CPU,
  decoder or independent CODE matcher was introduced.

The independent probe source was retained at `/tmp/slc022-review-probe.c`, SHA256
`3e06bcb51f4e8ef22995916d65725b13c5a45209a0a2d0e5a0ae707e63cc7e57`.
Review run logs are `/tmp/slc022-review-{build,gates,core,correction}.log` and
`/tmp/slc022-review-install-final.log`; the Master may retain them with the
runtime artifact. Durable regression cases are in the committed test suite.

## Exact artifact and external compatibility check

Independent source export with `packaging/make_source.py` yields:

- source commit: `a03504bd186efe3a8882bd3f1cdb5c4cf463f30d`;
- archive: `emusa80535-1.0.0.tar.gz`;
- SHA256: `1157a016d23462df28b078b53ce82bd5ffa4667e55a1b46033e46f21ed2d8757`.

This matches the Master's separate clean export. The earlier candidate's two
independent exports were also byte-identical; its archive remains historical,
not the approved runtime artifact. Source archives carry SOURCE_COMMIT; builds
without Git retain exact identity. The Master additionally verifies build and
native/host gates from the final extracted archive without `.git`, using a
verified immutable0.1 include tree for the separate module fixture.

The Reviewer independently reran the previously failing exact external module/
image/config script with the final runtime: all seven operations succeeded,
including both inspections at cycle99984 and explicit reset/repeat. Evidence is
`/tmp/slc022-review-external.ndjson`. This establishes the F008 interoperability
correction; it is not acceptance of board-specific behavior. The Master's longer
external probe separately reaches a typed unsupported access, retains post-fault
inspection and demonstrates reset/repeat; the external integration Slice must
review and accept those domain results.

## Remaining delivery limits

The host is a local Linux x86-64 script process. Native modules are trusted, are
mapped from the exact verified sealed bytes, and remain resident through session
destruction. `$ORIGIN`-relative module dependencies are unsupported. Post-fault
inspection requires the explicit pinned-module promise/host policy; merely
exporting INSPECT does not enable it. There is no implicit network listener,
physical I/O or machine control. Administrative signals, remote/DAP transports,
rich trace configuration, full machine fidelity and package publication retain
their separate deferred gates. No unresolved SLC-022 review finding remains.

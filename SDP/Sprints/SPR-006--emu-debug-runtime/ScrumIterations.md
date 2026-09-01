# Scrum iterations

## ITR-011 — Complete emu-debug 1.0 runtime

Status: changes-required
Iteration ID: ITR-011
Active Slice: SLC-011

### Slice contract — SLC-011

**Goal:** implement the whole frozen Issue #6 runtime as one coherent generic
vertical slice, from public snapshot/debug seams through the real no-curses
stdio process and frozen DAP integration surface.

**Why now:** PR #8 is merged; exact master
`b8a8fe67f37af250cbe20e3ce3450edfe4ea5fdf` contains all mandatory accepted
Stage-0, Stage-1 and SLC-010 regressions.

**Expected product files/modules:** public generic API in `emu8051.h` and
core helpers where required; standalone `emu-debug` server/protocol/digest
sources; top-level and test build integration; focused unit and child-process
tests; README build/protocol documentation. New small portable source modules
are allowed when they preserve licensing and keep the TUI separate.

**Required behavior:**

1. build/run no-curses `emu-debug` on Windows and Linux;
2. bounded UTF-8 NDJSON framing, correlation, schema, stable errors and stdout
   isolation;
3. exact first-command hello/version/capability/limits behavior;
4. absolute exact-64-KiB `load` plus expected/actual SHA-256;
5. deterministic uint32-seeded SAB reset with uint16 entry snapshot;
6. atomic public snapshot including selected R bank and 64-bit counters;
7. exact `decodeCode` forward/known-backward/unknown-placeholder/range/count;
8. atomic replacement CODE breakpoint set and pre-execution stops;
9. bounded synchronous `run` yield/architectural-stop semantics;
10. exact one-instruction `stepInstruction` semantics;
11. clean terminate, EOF, malformed input, exit and no-orphan behavior;
12. no target-specific or physical I/O and no DAP product-source changes;
13. all prior accepted emulator behavior remains green;
14. real DAP exact-HEAD contract/equivalence/smoke gates pass where supplied.

**Invariants:** accepted classic/SAB instruction, interrupt, Timer0/Timer1,
UART, port/pin and MOVX context semantics remain unchanged. Execution is
deterministic and virtual-time-only. Public debug state never exposes private
layout. Protocol stdout contains NDJSON only.

**Non-goals:** Issue #7 external edges, ADC, Timer2, P1000/D71055/target logic,
live transports, DAP protocol implementation, readMemory, source mapping,
watchpoints, stack reconstruction, attach/TCP or physical I/O.

**Traceability:** MND-001, STU-002, REQ-016, ARCH-007..ARCH-010,
DES-050..DES-063, SPR-006, ITR-011, SLC-011, expected REV-SLC-011 and
VER-SLC-011. External authority: Issue #6 and DAP exact HEAD
`36639b48ddb2ffbafa14c00da794fe1734f7483b`.

**Required verification:** focused C/API tests; frozen protocol schema/error/
lifecycle/image/hash/replay/snapshot/decode/breakpoint/run/step coverage;
Windows GCC and strict Clang; Linux/WSL GCC and ASan+UBSan; Windows/Linux real
child-process tests; all accepted suites; product diff/no-target/no-live audit;
DAP exact-HEAD real contract, fake-vs-real and available package/F5 smoke.

**Expected completion signal:** a fresh Worker commits bounded product/tests
and reports exact commands/evidence. A separate fresh Reviewer audits the four
Issue #6 areas and approves or opens findings. Master accepts only after exact-
HEAD verification and real DAP integration.

### Current result

Fresh Worker commit `84358bf05a400f53daace8805c8c15c6514fd03a`
implements the full SLC-011 product/test surface from activation base
`592fe4b51e72c527cc17ef063c085db9672cf39a`. The Worker reports all accepted
emulator suites, new facade/process tests, cross-platform strict/sanitizer
gates and exact-DAP real integration passing. REV-SLC-011 must independently
review the exact product HEAD before Master acceptance.

### Review result

REV-SLC-011 requires changes. F001 finds rejected RANGE decode mutating
predecessor knowledge; F002 finds raw escaped JSON key spelling bypassing
canonical required-member and duplicate handling. All other reviewed areas and
cross-platform/DAP evidence pass. Continue only through the bounded corrective
slice below.

## ITR-012 — Transactional decode and canonical JSON correction

Status: complete
Iteration ID: ITR-012
Active Slice: SLC-012

### Slice contract — SLC-012

**Goal:** resolve exactly REV-SLC-011-F001 and REV-SLC-011-F002 without
changing the frozen protocol or any other emulator behavior.

**Expected product files:** `emu_debug.c`, `emu_debug_server.c`, focused
facade/process tests. Touch build/docs only if the corrective tests require it.

**Required behavior:**

1. a failed `decodeCode` request, including RANGE after partial traversal,
   leaves all predecessor knowledge unchanged;
2. successful decode preserves the existing known-predecessor contract;
3. JSON member names are compared after standards-compliant string unescaping;
4. escaped spellings of required members behave as their canonical names;
5. semantic duplicates, including raw+escaped equivalents, are rejected;
6. malformed surrogate/escape input remains bounded and structured;
7. all SLC-011 commands, capabilities and accepted regressions remain green.

**Invariants:** no DAP product/contract changes; atomic lifecycle/state rules;
protocol-only stdout; current core/peripheral semantics; no target/live/physical
scope.

**Non-goals:** any feature beyond F001/F002, parser redesign unrelated to key
canonicalization, Issue #7 edges, ADC, Timer2, P1000 or physical I/O.

**Traceability:** SLC-012 corrects SLC-011 and addresses
REV-SLC-011-F001/F002 under REQ-016, ARCH-007..010 and DES-051/052/056/059.
Expected review/verification IDs are REV-SLC-012 and VER-SLC-012.

**Required verification:** exact independent reproductions; escaped canonical
and duplicate raw-NDJSON matrix; all facade/process/accepted suites; Windows
strict and Linux sanitizer gates; real DAP exact-HEAD rerun.

**Completion signal:** a fresh Worker commits only corrective product/tests; a
separate fresh Reviewer confirms both findings resolved before Master reruns
the complete verification and DAP integration gates.

### Worker result

Fresh corrective Worker commit
`7a547d12deac2d533a29c36a79df48210d099967` changes only the two runtime
modules and two focused tests. Worker reports F001/F002 reproductions red before
fix, resolved after fix, and the complete Windows/WSL/sanitizer/DAP real matrix
passing. REV-SLC-012 must independently confirm resolution before acceptance.

### Review result

REV-SLC-012 approved exact corrective product/test HEAD
`7a547d12deac2d533a29c36a79df48210d099967` with no new findings. Both
REV-SLC-011 findings are resolved. All EMU-BLK items are Reviewer-satisfied;
Master verification and exact-DAP rerun remain before Slice acceptance.

### Final result

Accepted. VER-SLC-012 passed the independent Master matrix against exact
product/test HEAD `7a547d12deac2d533a29c36a79df48210d099967` and exact DAP HEAD
`36639b48ddb2ffbafa14c00da794fe1734f7483b`. SLC-012 corrects SLC-011,
resolves F001/F002 and satisfies REQ-016. No unauthorized scope entered.

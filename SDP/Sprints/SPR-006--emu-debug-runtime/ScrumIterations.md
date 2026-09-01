# Scrum iterations

## ITR-011 — Complete emu-debug 1.0 runtime

Status: active
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

No product implementation has started. Worker dispatch follows the frozen
baseline checkpoint.

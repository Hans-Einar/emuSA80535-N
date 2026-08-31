# Scrum iterations

## ITR-001 — Stage 0 deterministic core boundary

Status: active  
Iteration ID: ITR-001  
Active Slice: SLC-001

### Slice contract — SLC-001

**Goal:** produce one coherent, core-only Stage 0 foundation that preserves
classic behavior and exposes the minimum generic SAB80535 variant boundary.

**Why now:** upstream has no tests and binds interrupt SFR semantics to classic
addresses. Later peripheral work would otherwise build on an unverified and
structurally incorrect base.

**Expected files/modules:** `emu8051.h`, `core.c`, narrowly necessary opcode or
loader units, build/test files under `tests/`, and minimal README/build
documentation. Do not edit curses UI files unless required only to keep them
building against a public API change.

**Required behavior:**

- core-only compile and tests independent of curses;
- explicit classic/SAB variant identity;
- SAB-owned upper IRAM with correct indirect/direct distinction;
- canonical Stage 0 SAB SFR registration/reset foundation including the
  A8/A9/B8/B9 conflict;
- exact 64 KiB raw binary loader;
- 64-bit instruction and machine-cycle counters;
- bounded run-N, breakpoint and run-until-PC with stop reason;
- optional instruction/SFR/MOVX trace records with PC and virtual cycle;
- deterministic reset/seed behavior sufficient for repeatable tests;
- backward-compatible classic behavior covered by regression tests.

**Invariants:** preserve upstream MIT notices and ancestry; CODE and XDATA never
alias implicitly; direct `80..FF` stays SFR while indirect accesses use upper
IRAM; null diagnostics have no side effects; no host wall clock or live I/O.

**Non-goals:** no finished 12-source controller, Timer/UART/ADC/Timer2 behavior,
port electrical model, P1000 ROM fixture, D71055 or board decode.

**Traceability:** MND-001, STU-001, REQ-001..REQ-008,
ARCH-001..ARCH-006, DES-001..DES-010, SPR-001, ITR-001, SLC-001,
expected REV-SLC-001 and VER-SLC-001.

**Required verification:** clean core-only build with warnings enabled; focused
classic opcode/memory regression; raw-size rejection/acceptance; direct versus
indirect upper-memory test; A9/B8/B9 SAB mapping/reset test; counter/run/breakpoint
tests; representative instruction/SFR/MOVX trace test; two identical seeded
runs compare equal; automated grep confirms no P1000 implementation leakage.

**Completion signal:** Worker reports exact changed files and commands with all
focused tests passing. A fresh Reviewer then approves the actual diff or opens
stable findings. Master records verification before accepting the Slice.

### Baseline evidence before Worker

- fork/origin HEAD: `5dc681275151c4a5d7b85ec9ff4ceb1b25abd5a8`;
- GitHub reports true fork ancestry to `jarikomppa/emu8051`;
- upstream remote tracks the same commit;
- Windows full build: blocked at `emu.c` because `curses.h` is unavailable;
- openSUSE WSL full build: blocked at `popups.c` for the same missing header;
- the unmodified core units `core.c`, `opcodes.c` and `disasm.c` compile cleanly
  with warning flags on both Windows GCC and openSUSE WSL `cc`;
- this dependency finding authorizes a core-only harness, not a semantic UI
  rewrite or an unrecorded claim that the upstream full build passed.

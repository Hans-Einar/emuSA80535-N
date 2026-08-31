# Scrum iterations

## ITR-001 — Stage 0 deterministic core boundary

Status: changes-required  
Iteration ID: ITR-001  
Slice: SLC-001

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

### Result

Worker commit `1cedf941d09779686e142511ff21b5b75de27f16` implemented the
contract and passed its submitted Windows/WSL/sanitizer tests. Independent
REV-SLC-001 found eight reproducible issues and set disposition
`changes-required`; SLC-001 is not accepted.

Open findings: REV-SLC-001-F001..F008. Corrective work moves to ITR-002 / SLC-002.

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

## ITR-002 — Stage 0 review correction

Status: changes-required  
Iteration ID: ITR-002  
Slice: SLC-002

### Slice contract — SLC-002

**Goal:** correct all REV-SLC-001 findings without expanding beyond Stage 0 or
weakening classic compatibility.

**Why now:** bounded run/time state, vector stop behavior and public gateway
memory safety are acceptance blockers; the remaining findings are necessary to
make reset and trace claims evidence-accurate.

**Expected files/modules:** `core.c`, `opcodes.c`, `emu8051.h`,
`binary_loader.c`, focused `tests/test_stage0.c`, and README wording only where
the corrected public contract requires it.

**Required corrections:**

- F001: return only at a coherent post-instruction machine-cycle/peripheral
  boundary; virtual count, `mTickDelay`, timers and trace chronology agree;
- F002: reevaluate target/breakpoint after interrupt entry and before vector
  opcode execution;
- F003: prevent invalid public SFR accesses from indexing outside SFR state;
- F004: raise the documented stack exception as soon as classic 8051 stack
  addressing enters absent upper IRAM;
- F005: label deterministic values for unspecified/indeterminate SAB reset
  state as model choices; do not assert unproven P6 hardware latch reset;
- F006: assert exact instruction and SFR trace PC/cycle/address/value for
  representative direct, bit and RMW forms plus unsupported MOVX write;
- F007: enforce read-only trace observation through the public callback type;
- F008: reject null raw-loader filenames explicitly.

**Invariants:** retain exact variant mapping, owned SAB upper IRAM, classic
opcode behavior, raw-loader normal behavior, deterministic seed behavior,
no-target boundary and no Stage 1 semantics.

**Non-goals:** no new peripheral behavior, P1000 assets/code, frontend rewrite
or live I/O.

**Traceability:** SLC-002 corrects SLC-001 and addresses
REV-SLC-001-F001..F008; expected review REV-SLC-002 and verification
VER-SLC-002. It remains governed by MND-001, REQ-001..REQ-008,
ARCH-001..ARCH-006 and DES-001..DES-010.

**Required verification:** original Stage 0 matrix on Windows/WSL/sanitizers;
new targeted probes for every finding; a multi-cycle timer-boundary assertion;
breakpoint and target at an interrupt vector; invalid SFR read/write probes
under sanitizers; classic no-upper stack failure; exact trace records and
observer immutability; null loader input; no-target audit and diff check.

**Completion signal:** fresh Worker commits the correction with all focused
evidence green; a second fresh Reviewer approves exact HEAD; Master reruns the
verification matrix and closes the Sprint only if evidence agrees.

### Result

Corrective commit `bbd63878c8ea4a2479e531509b3644e661eb875d`
resolved six findings completely and corrected the original upper-stack push
case. REV-SLC-002 independently confirmed the new cycle/vector/SFR/reset/trace
field/loader tests, but found two residual high-severity gaps. SLC-002 remains
`changes-required` and is not accepted.

Open findings: REV-SLC-002-F001 and REV-SLC-002-F002.

## ITR-003 — Trace immutability and stack-read closure

Status: complete  
Iteration ID: ITR-003  
Slice: SLC-003

### Slice contract — SLC-003

**Goal:** close the two residual SLC-002 findings without changing the now
verified cycle/interrupt behavior or expanding Stage 0.

**Expected files:** `emu8051.h`, `core.c`, `opcodes.c`, focused
`tests/test_stage0.c`, and README callback wording if needed.

**Required corrections:**

- REV-SLC-002-F001: trace callback receives only the immutable trace record and
  user context, or another by-value/read-only surface from which mutable CPU
  storage is unreachable in ordinary C; a compile/runtime regression proves a
  callback cannot alter the current operand or CPU memory through its signature;
- REV-SLC-002-F002: classic missing-upper stack reads raise
  `EXCEPTION_STACK`; POP and RET do not silently write sentinel data or commit
  invalid control flow; interrupt entry must not continue a second stack push
  after the first fails.

**Invariants:** preserve the corrected one-cycle tick semantics, post-instruction
run boundary, exact two-cycle interrupt entry/vector stops, variant mapping,
SAB upper stack success, raw loader, deterministic reset and exact trace fields.

**Non-goals:** no generic indirect-memory API redesign beyond the minimum stack
failure contract; no Stage 1 peripherals, board logic, frontend work or live I/O.

**Traceability:** SLC-003 corrects SLC-002 and addresses REV-SLC-002-F001 and
REV-SLC-002-F002. Expected review is REV-SLC-003 and verification VER-SLC-003.

**Required verification:** complete existing Windows/WSL/sanitizer matrix;
callback signature compile check and traced/untraced equality; POP and RET at
classic SP `80/81`; interrupt entry at SP `7F` raises once and does not advance
to a corrupt vector state; SAB SP `A2` LCALL/RET remains green; all original
Stage 0 tests, diff check and no-target audit.

**Completion signal:** fresh Worker commits the two corrections, third fresh
Reviewer approves exact HEAD with no findings, and Master independently reruns
the final matrix.

### Final result

Accepted. Commit `dbcc6b74adbc34896a71401366991229c0d58922`
resolved both REV-SLC-002 findings. REV-SLC-003 approved the exact HEAD with no
new findings, and VER-SLC-003 passed the independent Master matrix.

SLC-003 closes the corrective chain SLC-001 -> SLC-002 -> SLC-003. Stage 0 is
complete; no Stage 1 behavior is claimed.

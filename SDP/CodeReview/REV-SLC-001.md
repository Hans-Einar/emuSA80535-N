# REV-SLC-001 — Independent review of Stage 0 foundation

Status: changes-required
Reviewed Slice: SLC-001
Reviewed HEAD: `1cedf941d09779686e142511ff21b5b75de27f16`
Reviewed parent: `33b9be7e67eb454b07d097867333a343622f05c7`
Reviewer: fresh independent reviewer
Reviewed: 2026-08-31

## Disposition

Not approved. The structural variant work and normal focused tests are useful,
but two blocking, two high, three medium and one low finding require a bounded
corrective Slice before Stage 0 can be accepted.

## Findings

### REV-SLC-001-F001 — Blocking — run returns before credited cycles elapse

`tick()` credits the whole instruction cycle count when the opcode starts, but
the upstream timer advances once per later tick. `run_control()` returns as soon
as the instruction counter changes and can therefore leave `mTickDelay`
undrained. A two-cycle `SJMP` probe returned with one delay remaining and Timer
0 advanced by only one while the result reported two cycles. Bounded result,
trace time and peripheral state do not share one architectural boundary.

### REV-SLC-001-F002 — Blocking — vector stop is bypassed

Breakpoint and target-PC checks are outside the inner no-instruction loop.
Interrupt entry changes PC without incrementing the instruction count, so a
target/breakpoint at vector `0003` is not observed before the first ISR opcode.
Both probes ran the vector NOP and returned at PC `0004`.

### REV-SLC-001-F003 — High — public SFR gateway can corrupt CPU state

Public SFR read/write functions subtract `0x80` without validating input. A
write through address `0x00` underflowed the index and changed PC from `1234`
to `12AB`. The gateway must validate CPU/address or be private behind a
validated public surface.

### REV-SLC-001-F004 — High — classic upper-stack failure is silent

For classic 8051, indirect writes above `0x7F` are discarded when no upper RAM
exists, while stack exception is raised only after 8-bit wrap. A push from
`SP=7F` lost the byte without the documented `EXCEPTION_STACK`.

### REV-SLC-001-F005 — Medium — deterministic reset choices are overclaimed

The implementation/test describes all SAB extension registers as hardware-zero
and P6 as hardware-high. Datasheet bits in IP0/IP1/ADCON and the input-only P6
state are indeterminate/unspecified. Deterministic model placeholders are
allowed, but code and tests must not present them as proven hardware reset
facts. P4/P5 high is supported.

### REV-SLC-001-F006 — Medium — trace tests omit exact SFR/instruction fields

Tests count instruction and SFR records without asserting exact PC, opcode,
cycle, SFR address/value for direct, bit and RMW forms. Unsupported MOVX write
is also untested. Trace contract evidence is incomplete.

### REV-SLC-001-F007 — Medium — trace callback violates read-only contract

The callback receives mutable `struct em8051 *` and is invoked before opcode
execution, allowing an observer to change PC/registers and behavior. The public
contract must provide a const CPU view or otherwise enforce the read-only
architecture.

### REV-SLC-001-F008 — Low — raw loader accepts null filename unchecked

The function passes a null filename to `fopen`. Validate it explicitly and add
a focused test.

## Evidence rerun by Reviewer

- Windows GCC: `make core-test` passed.
- openSUSE WSL GCC: `make core-test` passed.
- WSL ASan/UBSan Stage 0 suite passed.
- Windows Clang warnings-as-errors and strict Windows/WSL core compilation
  passed.
- `git diff --check` passed.
- no-target grep found no P1000/Ponsse/D71055/board/live-I/O leakage.
- targeted probes reproduced F001..F004, including both F002 stop modes.
- full curses frontend remains blocked by the previously recorded missing
  `curses.h` build dependency.

## Positive observations

Explicit A8/A9/B8/B9 separation, SAB-owned upper IRAM at `SP=A2`, direct versus
indirect memory behavior, raw loader normal size handling, deterministic seeded
RAM, normal MOVX trace and unsupported-MOVX record types were structurally
sound within the reviewed Stage 0 scope.

## Required follow-up

SLC-002 must resolve REV-SLC-001-F001..F008 with targeted regression tests and
receive a fresh independent review.

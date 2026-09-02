# VER-SLC-015 — Deterministic Siemens Timer2 verification

Status: passed
Verified Slice: SLC-015
Verified product/test HEAD: `e388a007635acb4f964326f817a3d6eb049ccf6d`
Verified product tree: `8eb677ad298a3a743adb9d51f464b90ba755b0cf`
Frozen master baseline: `bc86d2633b6057529e6fd1e666896c24d72822aa`
Verifier: Master
Verified: 2026-09-03

## Result

**PASSED.** REV-SLC-015 independently approved the exact same product/test
HEAD with no findings. Master verification confirms the authorized reached
Siemens Timer2 behavior and all accepted regression/scope gates. No corrective
Slice is required.

SLC-015 satisfies REQ-014 within the explicit reached-mode boundary defined by
Issue #17 and DES-009. Deferred external-counter, gated, hardware-reload,
T2EX/EXF2 and capture/compare modes remain intentionally unimplemented rather
than being silently approximated.

## Exact identity

Both Master verification workflows checked out the immutable commit directly:

```text
HEAD = e388a007635acb4f964326f817a3d6eb049ccf6d
tree = 8eb677ad298a3a743adb9d51f464b90ba755b0cf
```

The reviewed product identity therefore does not depend on the moving PR #18
branch head.

## Master executable evidence

### Linux exact-head gate

Corrected Master Actions run `33694685888`, Linux job `100460922336` passed:

1. exact HEAD/tree identity and `git diff --check` against the frozen master;
2. GCC strict C99 full Stage0/IRQ/Timer0+1/UART/ports/edges/ADC/Timer2 matrix;
3. frozen `emu-debug` facade and child-process protocol tests;
4. strict Clang C99 `-Wall -Wextra -Wshadow -Werror -pedantic` full core matrix;
5. Clang ASan+UBSan full core matrix with leak and UB halt options, no report;
6. frozen DAP exact HEAD `36639b48ddb2ffbafa14c00da794fe1734f7483b`:
   install/build, contract tests, fixture/hash and real-contract/equivalence/F5
   smoke against the exact-head emulator;
7. target/live scope and frozen `emu-debug`/`opcodes.c` blob audits.

The earlier Linux job `100460401919` from run `33694518622` independently
passed the same complete Linux gate before the Windows harness was corrected.

### Windows exact-head gate

Corrected Master Actions run `33694685888`, Windows job `100460922488` passed:

1. checkout of exact `e388a007...` on Windows Server 2025;
2. native MinGW64 GCC full Stage0 through Stage4 core matrix;
3. native MinGW64 strict Clang full Stage0 through Stage4 core matrix.

The first Windows attempt in run `33694518622`, job `100460402183`, is
classified **environment/harness-only**: MSYS2 was configured with a minimal
PATH and the redundant first `git rev-parse` command failed with `git: command
not found`. No product compilation or test ran in that failed step. The harness
was corrected only by removing that redundant command; product/test HEAD was
unchanged. The corrected Windows rerun then passed both compiler matrices.

## Timer2 behavioral coverage

Master verification plus the approved independent review cover the Issue #17
acceptance classes, including:

- exact Siemens T2CON masks and preservation of SLC-013 I2FR/I3FR semantics;
- stopped state and producer-inert unsupported `10`/`11` input selections;
- `/12` one-count-per-machine-cycle behavior;
- `/24` free-running even-completed-cycle behavior;
- live 16-bit TH2:TL2 counting and independent byte writes;
- exact `0x5555 -> 43691` overflow case;
- `FFFF -> 0000`, sticky software-clear TF2 and repeated overflow events;
- ET2/EAL masking, pending retention and enable-after-pending service;
- accepted four-level priority/preemption/vector `002B`/in-service/RETI path;
- software stop/reload/restart and natural ISR latency;
- T2R1=0 no CRCL/CRCH hardware reload;
- IDLE/multi-cycle virtual-time progression;
- existing immutable timer overflow observer/count neutrality;
- no automatic Timer2-to-P5.4 action;
- classic 8051/8052 isolation.

## Scope, protocol and post-review audit

At the exact reviewed product HEAD, no external-counter/gated Timer2 producer,
CRC/T2EX hardware reload, EXF2 producer, capture/compare behavior,
P1000/NEC/board/connector policy, physical/live I/O or machine-control behavior
is present.

`emu_debug.c`, `emu_debug.h`, `emu_debug_server.c` and `opcodes.c` are
byte-identical to frozen master in the Master scope audit. The frozen debug
protocol therefore remains unchanged.

Commits after reviewed product HEAD are limited to temporary CI-helper removal,
Master/reviewer SDP evidence and verification scaffolding/cleanup. Product and
focused test blobs remain unchanged and are separately rechecked before final
PR handoff.

## Acceptance disposition

REV-SLC-015: **APPROVED**, no findings.
VER-SLC-015: **PASSED**.

SLC-015 is accepted and REQ-014 is satisfied/current. SPR-009 may be closed and
the focused implementation PR may be presented to Steering for merge review.
This verification record itself is not merge authorization.

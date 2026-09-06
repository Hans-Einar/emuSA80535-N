# Implementation notes

Status: verified; SLC-015 accepted.

## Frozen baseline

- Branch: `codex/sab80535-timer2`.
- Exact master: `bc86d2633b6057529e6fd1e666896c24d72822aa`.
- Accepted SLC-014/REQ-013 ADC is merged on the baseline.
- Accepted `emu-debug` 1.0 protocol remains frozen.
- Consumer evidence: Ponsse PR #25 exact HEAD
  `19ef6ff45efa719612b70e70b8c31b9cb2ebb7e9` plus current `doc/P1000/`.

## Worker product/test identity

- Focused test file commit: `ee4acd84127f6ba6867b79b7821015d5bfa0c3d7`.
- Exact tested Worker product/test HEAD:
  `e388a007635acb4f964326f817a3d6eb049ccf6d`.
- Product commit parent: `51f192f5b9b4bb060cff60d87bf7d6d7054861c5`.
- Product commit changed only `core.c`, `emu8051.h` and `tests/Makefile`;
  `tests/test_stage4_timer2.c` was already present in the tested parent.
- Current branch after removing temporary Worker CI helpers changes no
  product/test blob relative to the exact Worker HEAD.

## Implemented behavior

- canonical Siemens T2CON masks for T2I0/T2I1/T2CM/T2R0/T2R1/I2FR/I3FR/T2PS;
- generic timer-overflow identity extended with `EM8051_TIMER2`;
- timer-function `01` counts live TH2:TL2 as a 16-bit up-counter;
- T2PS=0 consumes every machine cycle; T2PS=1 consumes the free-running even
  completed-machine-cycle boundaries from global deterministic virtual time;
- stopped `00`, external-counter `10`, gated `11` and T2R1=1 reload modes do
  not produce guessed Timer2 counts in this Slice;
- reload-disabled FFFF->0000 sets sticky canonical IRCON.TF2;
- repeated wraps increment the existing 64-bit timer overflow count and emit
  immutable generic timer-overflow records even while TF2 remains set;
- existing interrupt controller remains the sole ET2/EAL/priority/vector-002B/
  in-service/RETI path;
- TL2/TH2 are ordinary live SFR bytes, so software stop/reload/restart occurs
  naturally through existing SFR instructions;
- Timer2 overflow has no automatic P5.4 action.

## Worker verification evidence

GitHub Actions run `33688691147`, job `100442147454`, executed the exact
product changes before committing `e388a007...` and passed:

- Ubuntu 24.04 GCC strict C99 full Stage0/IRQ/Timer0+1/UART/ports/edges/ADC/
  Timer2 matrix;
- GCC frozen `emu-debug` facade and child-process protocol tests;
- strict Clang C99 `-Wall -Wextra -Wshadow -Werror -pedantic` full core matrix;
- Clang ASan+UBSan full core matrix with leak/UB halt options and no report;
- SLC-015 focused tests including masks, stopped/classic isolation, /12, /24
  free-running phase, exact 43691-cycle `5555` overflow, repeated sticky TF2,
  masked/enable-after-pending controller service, live byte writes,
  unsupported-mode non-production, CRCL/CRCH no-reload, real vector-002B
  ISR-style stop/port-write/reload/clear/restart, IDLE progression and reset;
- frozen DAP exact HEAD `36639b48ddb2ffbafa14c00da794fe1734f7483b`:
  build, 45-test contract set (44 pass + one platform skip), fixture/hash and
  real-contract/fake-equivalence/F5 smoke against the modified emulator;
- `git diff --check` and added-line no-target/no-live scope audit.

The temporary `.github` Worker workflow and patch helper were removed after the
tested Worker commit. Their deletion changes no product/test blob.

## Critical boundaries preserved

- P5.4 is not hardware-toggled by Timer2; firmware may change it normally.
- No external-counter/gated producer, CRC/T2EX reload, EXF2 producer or
  compare/capture implementation.
- No P1000/NEC/board/connector/valve semantics.
- No physical/live I/O and no frozen debug-protocol change.

## Review separation

`e388a007635acb4f964326f817a3d6eb049ccf6d` is a Worker result, not acceptance.
A fresh independent Reviewer must inspect that exact product/test tree (or the
current cleanup HEAD after proving product/test blobs identical) before Master
can create VER-SLC-015 or accept REQ-014.

## Accepted review

REV-SLC-015 independently approved exact `e388a007635acb4f964326f817a3d6eb049ccf6d` / `8eb677ad298a3a743adb9d51f464b90ba755b0cf` with no
findings. The independent issue checkpoint is
https://github.com/Hans-Einar/emuSA80535-N/issues/17#issuecomment-5517618869.

## Master verification

VER-SLC-015 passed against exact `e388a007635acb4f964326f817a3d6eb049ccf6d`. Master run `33694685888` passed
Linux GCC/strict Clang/ASan+UBSan/frozen debug/frozen DAP/scope gates and
Windows GCC + strict Clang core matrices. The earlier Windows harness-only
failure in run `33694518622` attempted no compilation and was corrected without
changing product/test state.

REQ-014 is satisfied/current. No external/gated input, hardware reload, EXF2,
capture/compare, automatic P5.4, target/live I/O or debug-protocol scope was
added.

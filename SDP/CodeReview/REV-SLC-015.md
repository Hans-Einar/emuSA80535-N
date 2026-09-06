# REV-SLC-015 — Deterministic Siemens Timer2 review

Status: approved
Reviewed Slice: SLC-015
Reviewed product/test HEAD: `e388a007635acb4f964326f817a3d6eb049ccf6d`
Reviewed tree: `8eb677ad298a3a743adb9d51f464b90ba755b0cf`
Frozen master baseline: `bc86d2633b6057529e6fd1e666896c24d72822aa`
Reviewer: fresh independent ChatGPT Work reviewer
Reviewed: 2026-09-03
Issue checkpoint: https://github.com/Hans-Einar/emuSA80535-N/issues/17#issuecomment-5517618869

> This repository record is a Master-side transcription of the independently
> published Reviewer disposition above. It does not replace or retroactively
> self-author the independent review.

## Disposition

**APPROVED.** No findings were opened. There are no `REV-SLC-015-Fxxx` IDs.
Only exact product/test HEAD `e388a007635acb4f964326f817a3d6eb049ccf6d`
and tree `8eb677ad298a3a743adb9d51f464b90ba755b0cf` are approved.
Later PR #18 commits are outside the independent review identity.

## Independent Siemens reconciliation

Primary architectural authority was the Siemens *SAB 80515/SAB 80C515
User's Manual*, Edition 08.95. The Reviewer independently checked the Timer2
and interrupt material against a 270-page manual scan with SHA-256
`9eaf3fd71619c2043d0a0ed11cede6009a433122a44a12bbcd5d2fb27e297f72`.

The independently accepted Timer2 interpretation is:

- T2CON bits 7..0 are `T2PS,I3FR,I2FR,T2R1,T2R0,T2CM,T2I1,T2I0`;
- `T2I1:T2I0=01` is the internal timer-function source;
- `00` is stopped;
- SLC-015 intentionally leaves `10` external-counter and `11` gated sources
  producer-inert;
- `T2PS=0` advances at `fosc/12`, one increment per machine cycle;
- `T2PS=1` advances at `fosc/24`, every second completed global machine cycle;
- the divide-by-two stage is placed before input selection in the Siemens
  diagram and no T2CON-write phase reset is documented, so the design's
  free-running `/2` phase is accepted as a bounded deterministic model;
- with `T2R1=0`, hardware reload is disabled and wrap does not copy CRCL/CRCH;
- TF2 is canonical IRCON.C6, sticky/software-clear, and vector entry/RETI do
  not clear it.

## Behavioral audit

The Reviewer independently confirmed:

- eligible Timer2 cycles use the current live `TH2:TL2`, increment it as a
  16-bit up-counter and write the post-increment bytes back;
- independent TL2 and TH2 writes are immediate and non-atomic, so a timer tick
  between writes observes the intermediate architectural pair;
- `FFFF -> 0000` produces a Timer2 overflow event and sets TF2;
- repeated wraps remain observable through the 64-bit timer count and immutable
  observer even while TF2 is already set;
- counting and TF2 production are independent of ET2/EAL, while service remains
  exclusively under the accepted interrupt controller;
- enable-after-pending, paired four-level priority, preemption, vector `002B`,
  in-service state and RETI release use the established controller path;
- stop/restart preserves the live count and TF2, and the free-running `/2`
  phase continues across selection changes;
- SLC-013 I2FR/I3FR behavior sharing T2CON is unchanged;
- starting at `0x5555` in `/12` mode remains `0xFFFF` after 43,690 cycles and
  wraps exactly on cycle 43,691;
- Timer2 overflow has no automatic P5.4 action;
- there is no EXF2 producer, capture/compare behavior, P1000/NEC/board policy,
  physical/live I/O or frozen debug-protocol behavior in the runtime diff;
- classic 8051/8052 variants gain no Siemens Timer2 producer behavior.

## Independent executable evidence

The Reviewer ran, independently of the Worker summary:

- GCC strict C99 with warnings-as-errors: complete Stage0, IRQ, Timer0/1, UART,
  ports/MOVX, SLC-013 edges, ADC and Timer2 matrix passed;
- `debug_facade_tests` and `test_emu_debug_process.py` against exact-head
  `emu-debug`: passed;
- GCC ASan+UBSan complete core matrix: passed; local LSan was disabled because
  the managed review environment blocks `/proc` thread inspection, while the
  Worker CI independently passed Clang ASan+UBSan with leak detection enabled;
- a separate Reviewer probe swept multiple initial phases, stop/start and
  `/12`/`/24` changes, both TL2/TH2 write orders, repeated sticky-TF2 wraps,
  CRCL/CRCH and P5 isolation, and masked/pending/priority/preemption/RETI cases;
- frozen DAP exact HEAD `36639b48ddb2ffbafa14c00da794fe1734f7483b`
  built and passed its full test suite (97 pass, 2 platform-specific skips),
  fixture hash and real-contract/equivalence/F5 smoke;
- Worker Actions run `33688691147`, job `100442147454`, was inspected only as
  corroborating evidence.

## Scope and identity

Runtime product changes are limited to `core.c` and `emu8051.h`, with focused
build/test wiring in `tests/Makefile` and `tests/test_stage4_timer2.c` already
present in the reviewed tree. `emu_debug*`, opcode execution, disassembly,
loader and main remain byte-unchanged from the accepted baseline.

The review approves no deferred Timer2 mode and is not merge authorization.
Master verification against the same exact product/test identity remains the
acceptance gate.

# REV-SLC-010 — Port latch/pin and MOVX review

Status: approved
Reviewed Slice: SLC-010
Reviewed product HEAD: `ba00ef17af57076b01c7f548e8996a7d36a5c591`
Reviewed parent: `459e731ce6a2938b80af8b40d61ef561f86a0cf6`
Reviewer: fresh independent reviewer
Reviewed: 2026-09-01

## Disposition

Approved with no findings. No REV-SLC-010-F IDs were opened.

## RMW audit

Siemens table 7-2 complete set was audited in source and literal opcode tests:
ANL/ORL/XRL direct destination, JBC, CPL, INC, DEC, DJNZ, MOV port-bit,C,
CLR and SETB. Byte handlers use the latch-aware gateway; bit handlers use raw
latch and the write gateway. Other port-read handlers use resolved-pin reads.

## Port and callback evidence

P1/P3/P4/P5 reset/formula/drive/release/invalid input/byte/bit paths passed.
Read callbacks override ordinary reads without collapsing canonical state;
write callbacks observe committed latch and reentrant writes leave coherent
final latch/pins. Classic behavior remains unchanged.

## MOVX evidence

All DPTR/@Ri read/write forms capture cycle, executing PC, address, direction,
architectural/final value and raw P1 latch before legacy callback mutation.
Forced pins do not alter snapshots; save/change/access/restore, backing,
legacy callback and immutable observer behavior pass.

## Gates and boundary

All five suites pass Windows GCC/strict Clang, openSUSE WSL GCC and WSL
ASan/UBSan. Diff/ancestry/no-target/no-edge/no-live audits pass. No external
edge producer, ADC, Timer2 or target/physical behavior was added.


# REV-SLC-011 — emu-debug protocol 1.0 runtime review

Status: changes-required
Reviewed Slice: SLC-011
Reviewed product HEAD: `84358bf05a400f53daace8805c8c15c6514fd03a`
Reviewed base: `592fe4b51e72c527cc17ef063c085db9672cf39a`
Reviewer: fresh independent reviewer
Reviewed: 2026-09-01

## Disposition

Changes required. Two findings are open. SLC-011 is not accepted.

## REV-SLC-011-F001 — rejected decode range mutates predecessor state

Severity: P1 / high

`emu_debug.c:521-536` records predecessor links while constructing a forward
window before proving that the full requested window fits CODE. A later RANGE
failure leaves those links authoritative.

Independent reproduction with an all-NOP image:

1. backward decode at FFFF reports unknown predecessor at FFFE;
2. forward decode at FFFE, count 3, is rejected with RANGE;
3. the same backward decode now reports a valid NOP at FFFE.

This violates no-partial success and invalid-command-no-state-mutation rules.
Stage predecessor updates and commit only after complete success, or preflight
the full window. Add a regression proving state is unchanged after RANGE.

## REV-SLC-011-F002 — escaped JSON names bypass canonical duplicate handling

Severity: P2 / medium

`emu_debug_server.c:500-533` compares raw JSON token spelling. A request with
both `"id"` and `"\u0069d"` is accepted as if the latter were unknown, while
an escaped required spelling such as `"t\u0079pe"` is rejected. JSON parsers,
including the frozen DAP fake's `JSON.parse`, treat those as canonical names.

Decode key strings before lookup and reject duplicate canonical required
members. Add raw NDJSON tests for escaped required names and semantic duplicate
names.

## Evidence that passed

- Windows GCC and strict Clang full accepted/facade/process matrix;
- openSUSE WSL GCC and ASan+UBSan full matrix;
- DAP exact HEAD lint, 99/99 full tests, 45/45 contract and fixture/hash;
- Windows/Linux real runtime/equivalence/session smoke;
- VSIX build/contents/policy and no-orphan gates;
- no P1000/target/external-edge/ADC/Timer2/live/physical/curses scope.

## Blocker disposition

`EMU-BLK-002` remains open through F002. `EMU-BLK-006` remains open through
F001. `EMU-BLK-001`, `003..005` and `007..010` are satisfied at the reviewed
HEAD. Both findings are emulator defects repairable without changing the
frozen contract or DAP product.

## Checkpoint

https://github.com/Hans-Einar/emuSA80535-N/issues/6#issuecomment-5493338077

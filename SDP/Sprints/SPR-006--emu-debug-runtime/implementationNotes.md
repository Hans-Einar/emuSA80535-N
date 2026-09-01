# Implementation notes

Status: review pending; no accepted SLC-011 product implementation yet.

## Frozen baseline

- Branch: `codex/emu-debug-runtime`.
- Exact master: `b8a8fe67f37af250cbe20e3ce3450edfe4ea5fdf`.
- Accepted SLC-010 product: `ba00ef17af57076b01c7f548e8996a7d36a5c591`.
- DAP PR #4 contract: `36639b48ddb2ffbafa14c00da794fe1734f7483b`.

## Verified work

None yet. Record only Worker results that survive independent review and
Master verification.

## Worker result awaiting review

- Exact product HEAD: `84358bf05a400f53daace8805c8c15c6514fd03a`.
- Adds the public debugger facade, no-curses NDJSON executable, nine frozen
  commands, seven capabilities, build/docs and facade/process/real-DAP tests.
- Worker reports Windows GCC/strict Clang, WSL GCC/ASan+UBSan, all accepted
  regressions, DAP 99/99 plus 45/45 contract, real runtime/F5 and package smoke
  passing with the DAP worktree unchanged.
- This evidence is not accepted until REV-SLC-011 and VER-SLC-011 complete.

## Review disposition

REV-SLC-011 is changes-required. F001 keeps EMU-BLK-006 open because a failed
RANGE decode mutates predecessor knowledge. F002 keeps EMU-BLK-002 open because
escaped semantic duplicate JSON keys bypass canonical handling. ITR-012 /
SLC-012 is the only authorized corrective scope.

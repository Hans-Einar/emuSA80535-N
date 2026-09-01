# Implementation notes

Status: verified; SLC-012 accepted.

## Frozen baseline

- Branch: `codex/emu-debug-runtime`.
- Exact master: `b8a8fe67f37af250cbe20e3ce3450edfe4ea5fdf`.
- Accepted SLC-010 product: `ba00ef17af57076b01c7f548e8996a7d36a5c591`.
- DAP PR #4 contract: `36639b48ddb2ffbafa14c00da794fe1734f7483b`.

## Verified work

- Exact accepted product/test HEAD:
  `7a547d12deac2d533a29c36a79df48210d099967`.
- Complete no-curses emu-debug 1.0 facade/server, nine commands, seven
  capabilities, portable build/docs and facade/process/real-DAP tests.
- REV-SLC-012 approved without findings; F001/F002 resolved.
- VER-SLC-012 passed Windows GCC/strict Clang, WSL GCC/ASan+UBSan, all accepted
  regressions, Windows/Linux process, exact DAP 99/99 and 45/45, real F5 and
  VSIX/package smoke with zero orphans.
- All EMU-BLK-001..010 are satisfied; REQ-016 is satisfied.
- Focused implementation PR #9 opened against master and intentionally left
  unmerged: https://github.com/Hans-Einar/emuSA80535-N/pull/9

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

## Corrective Worker result awaiting review

- Exact corrective product/test HEAD:
  `7a547d12deac2d533a29c36a79df48210d099967`.
- F001 fix stages predecessor metadata until complete decode success.
- F002 fix canonicalizes decoded JSON member names and rejects semantic
  duplicates, with focused escape/surrogate regressions.
- Worker reports all emulator, strict, sanitizer and Windows/Linux exact-DAP
  gates passing. REV-SLC-012 and VER-SLC-012 remain mandatory.

## Accepted review

REV-SLC-012 approved the exact corrective product/test HEAD with no findings
and independently resolved F001/F002. VER-SLC-012 now accepts the product.

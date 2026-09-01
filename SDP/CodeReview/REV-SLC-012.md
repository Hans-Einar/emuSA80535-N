# REV-SLC-012 — transactional decode and canonical JSON correction review

Status: approved
Reviewed Slice: SLC-012
Reviewed product/test HEAD: `7a547d12deac2d533a29c36a79df48210d099967`
Reviewed base: `409fefa2d5968bed141fc7966819d8a59b5c44ea`
Reviewer: fresh independent reviewer
Reviewed: 2026-09-01

## Disposition

Approved with no new findings. REV-SLC-011-F001 and F002 are resolved.

## Finding resolution

- F001: old product independently reproduces predecessor leakage after a
  rejected RANGE decode; corrective HEAD stages bounded links and commits only
  after complete success. Failure paths leave prior knowledge unchanged while
  successful forward/known-predecessor behavior remains.
- F002: old product independently rejects escaped canonical required names;
  corrective HEAD decodes bounded canonical UTF-8 keys, accepts escaped
  required spellings and rejects raw+escaped semantic duplicates. Explicit
  lengths cover U+0000; surrogate validation and all allocation/free paths were
  audited.

## Independent evidence

- Windows GCC and strict Clang complete accepted/facade/process matrix;
- openSUSE WSL GCC and ASan+UBSan complete/adversarial matrix;
- all required escaped names, semantic duplicates, unknown/NUL/surrogate keys,
  invalid escapes/UTF-8 and record limits;
- DAP exact HEAD lint, 99/99, 45/45 contract and fixture/hash;
- real DAP contract/equivalence/F5 on Windows and Linux;
- VSIX policy and VS Code 1.95 packaged smoke with zero orphans;
- cumulative no-curses/no-target/no-live/no-physical/licensing audit.

## Blocker disposition

`EMU-BLK-001..010` are satisfied. EMU-BLK-004 remains preserved by the
accepted core/load/reset. EMU-BLK-002 and EMU-BLK-006 are closed by the two
corrective finding resolutions.

## Checkpoint

https://github.com/Hans-Einar/emuSA80535-N/issues/6#issuecomment-5493673826

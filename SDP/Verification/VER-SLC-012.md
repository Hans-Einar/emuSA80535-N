# VER-SLC-012 — emu-debug protocol 1.0 runtime verification

Status: passed
Verified Slice: SLC-012 correcting SLC-011
Verified product/test HEAD: `7a547d12deac2d533a29c36a79df48210d099967`
Frozen master: `b8a8fe67f37af250cbe20e3ce3450edfe4ea5fdf`
Frozen DAP HEAD: `36639b48ddb2ffbafa14c00da794fe1734f7483b`
Verifier: Master
Verified: 2026-09-01

## Result

Passed. REQ-016 is satisfied and every `EMU-BLK-001..010` is closed. The
complete generic `emu-debug` 1.0 runtime is accepted at the exact product/test
HEAD above. Later branch commits at verification time changed only SDP files;
product/test blobs were identical.

## Emulator evidence

1. Windows GCC `make core-test`, `make emu-debug`, `make debug-test` — Stage-0,
   IRQ, Timer, UART, SLC-010, facade and process/protocol suites passed.
2. Windows Clang 18 C99 with `_CRT_SECURE_NO_WARNINGS`, `-Wpedantic`,
   `-Werror`, `-Wshadow` — the same full matrix passed.
3. openSUSE WSL GCC 7.5 — full accepted/facade/Linux process matrix passed.
4. WSL GCC ASan+UBSan, strict warnings and leak/UB halt options — full matrix
   passed without report.
5. Focused cases passed for exact/oversize/malformed/UTF-8 framing,
   image size/hash/Unicode absolute path, deterministic replay and atomic
   register-bank snapshot, decode predecessor/range/exact-count transaction,
   replacement breakpoint atomicity/limit/pre-execution, yield/step/stops and
   terminate/EOF/no-orphan lifecycle.
6. Corrective adversarial cases passed for escaped canonical required names,
   semantic duplicates, unknown/NUL/surrogate-pair keys, invalid escapes,
   unpaired surrogates, invalid UTF-8 and line limits.

## Real DAP evidence

The DAP repository remained unchanged at exact frozen HEAD.

1. `npm run lint` — passed.
2. `npm test` — 99/99 passed.
3. `npm run test:contract` — 45/45 passed.
4. `npm run fixture:check` — exact 65,536-byte fixture and SHA-256 passed.
5. `tests/test_dap_real.mjs` — real client/contract/equivalence/F5 smoke passed
   against the real runtime on Windows and Linux.
6. Launch/configuration, disassembly, instruction-breakpoint stop,
   stack/scopes/registers, exact step, breakpoint clear, continue/pause and
   disconnect/terminated paths passed.
7. VSIX build/contents/policy passed with 47 exact allowlisted entries.
8. VS Code 1.95 installed packaged smoke passed with `orphanProcesses: 0`.

The frozen fake's simplified parity and one-cycle opcode timing are test-fake
fidelity differences only. Adapter control semantics match; real core parity
and timing are asserted separately. No frozen-contract defect exists.

## Safety, dependency and traceability evidence

- Windows imports only `KERNEL32.dll` and `msvcrt.dll`; Linux imports only
  libc and its loader. No curses dependency is present in the headless graph.
- Source audit found no P1000/D71055/target policy, Issue #7 external edge,
  ADC, Timer2, socket/TCP, live serial/GPIO/CAN or physical I/O.
- Master/product/current ancestry passed. The cumulative product diff contains
  only the ten authorized product/build/doc/test files.
- `git diff --check`, unique NDJSON ledger parsing, YAML/relations parsing and
  active-ID validation passed.
- Build/test cleanup left the emulator and DAP repositories clean.

## EMU-BLK disposition

| Blocker | Disposition | Evidence |
| --- | --- | --- |
| EMU-BLK-001 | satisfied | portable no-curses executable/build/docs |
| EMU-BLK-002 | satisfied | bounded NDJSON/schema/errors/canonical keys |
| EMU-BLK-003 | satisfied | exact hello/version/capabilities/limits |
| EMU-BLK-004 | satisfied/preserved | accepted deterministic core/load/reset |
| EMU-BLK-005 | satisfied | stable atomic value snapshot |
| EMU-BLK-006 | satisfied | exact transactional decode window behavior |
| EMU-BLK-007 | satisfied | atomic replacement breakpoint set |
| EMU-BLK-008 | satisfied | bounded synchronous run/yield |
| EMU-BLK-009 | satisfied | exact wire single-instruction step |
| EMU-BLK-010 | satisfied | portable terminate/EOF/error/no-orphan lifecycle |

## Review and checkpoint

- REV-SLC-011 changes-required; F001/F002 opened.
- REV-SLC-012 approved; F001/F002 resolved; no new findings.
- Master checkpoint:
  https://github.com/Hans-Einar/emuSA80535-N/issues/6#issuecomment-5493792697

## First remaining action

Open the focused implementation PR and leave it unmerged for Steering review.

# Implementation notes

Status: verified; SLC-014 accepted.

## Frozen baseline

- Branch: `codex/sab80535-adc`.
- Exact master: `a4d24786eb86a55479adc4ef14d0f27424fb5705`.
- Accepted SLC-013/REQ-012 is merged on the baseline.
- Accepted `emu-debug` 1.0 protocol remains frozen.
- Consumer evidence: Ponsse PR #25 exact HEAD
  `19ef6ff45efa719612b70e70b8c31b9cb2ebb7e9` plus current `doc/P1000/`.

## Worker result

- Exact product/test HEAD:
  `0bd39132b2eaffbfc5190e223b54743f17fc68fa`.
- Adds generic normalized AN0..AN7 input, callback-safe DAPR start/restart,
  exact cycle-15 conversion, DAPR reference arithmetic, ADDAT/BSY/IADC,
  controller integration and immutable ADC diagnostics.
- Adds exhaustive focused ADC tests and complete test-build integration.
- Worker reports Windows GCC/strict Clang and WSL GCC/ASan+UBSan seven-suite
  core plus frozen debug gates passing.
- Frozen DAP exact HEAD `36639b4...` build, 45/45 contract, fixture and real
  contract/equivalence/F5 smoke pass from a removed detached worktree.
- Frozen debug modules, `opcodes.c`, SDP and all unauthorized scope are
  unchanged by the product commit.

## Accepted review

REV-SLC-014 approved exact product/test HEAD `0bd3913...` with no findings.
Its independent Siemens, 5.96-million-state arithmetic, adversarial callback/
restart, cross-platform sanitizer, exact-DAP, scope and product-identity audits
pass.

## Verified acceptance

- VER-SLC-014 passed Windows GCC/strict Clang, WSL GCC/ASan+UBSan, all seven
  core suites, frozen debug facade/process and exact-DAP real integration.
- Product/test HEAD `0bd3913...` is accepted; later commits are SDP-only.
- REQ-013 is satisfied/current.
- The frozen debug protocol and every stop-boundary exclusion remain intact.

## Stop boundary

No Timer2, capture/compare, P1000 board/NEC/calibration logic, full continuous
ADC mode, live/physical I/O or frozen debug-protocol change is authorized.

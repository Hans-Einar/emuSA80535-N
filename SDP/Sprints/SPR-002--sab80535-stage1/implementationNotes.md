# Implementation notes

Status: active; no verified SLC-004 product implementation recorded yet.

## Baseline

- Branch: `codex/sab80535-interrupt-controller`.
- Base: Stage 0 PR #1 HEAD
  `62f40127e1aa3b24e9d8d54c2458e847bfe86488`.
- Verified Stage 0 product commit:
  `dbcc6b74adbc34896a71401366991229c0d58922`.
- Stage 1 authority: Issue #2; only SLC-004 is authorized.

## Verified work

- Product commit `529602d800e36e87f495ef088f17e67ba3659a1a`
  passed the submitted cross-platform suites and most independent semantic
  probes, but REV-SLC-004 reproduced two findings. It is not accepted.
- Independently verified positive structure includes all source/vector/order,
  priority/nesting/RETI/inhibit/trace and classic-regression behavior outside
  REV-SLC-004-F001/F002.
- Corrective commit `85fa6e1f8318c691598b9a2ae191d1bd94b436c8`
  fixed raw C6/C7 identities/split gates and SAB TF1 persistence without
  changing classic serial behavior or adding later Timer/UART functionality.
- REV-SLC-005 approved with no findings. VER-SLC-005 passed Windows GCC,
  strict Clang, WSL GCC, WSL ASan/UBSan, product diff/no-target, NDJSON and
  ancestry checks.
- The SLC-004/SLC-005 controller increment is accepted technically and ready
  for a stacked PR.
- Stacked PR #3 was opened against the unmerged Stage 0 branch:
  `https://github.com/Hans-Einar/emuSA80535-N/pull/3`.
- Exact reviewed CPU revision was reported to Ponsse Issue #26:
  `https://github.com/Hans-Einar/ponsse/issues/26#issuecomment-5483780360`.

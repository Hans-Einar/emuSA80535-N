# Implementation notes

Status: active; no verified SLC-006 product implementation yet.

## Baseline

- Branch: `codex/sab80535-timers`.
- Base/master merge: `a20815e24778760a308130cf1f9aa6d0f55b6af3`.
- Accepted interrupt-controller product: `85fa6e1f8318c691598b9a2ae191d1bd94b436c8`.
- Only REQ-010/SLC-006 is authorized.

## Verified work

- Product commit `30cf42efa845d29a47a950eca7bbaf657490fbe6`
  adds shared Timer0 mode1/Timer1 mode2 64-bit overflow counters, immutable
  completed-cycle/post-state records and focused timer tests.
- Existing shared count/reload behavior needed no parallel engine rewrite.
- REV-SLC-006 approved without findings. VER-SLC-006 passed Windows GCC,
  strict Clang, WSL GCC, WSL ASan/UBSan, diff/no-target/no-UART/NDJSON and
  master-ancestry checks.
- REQ-010/SLC-006 is accepted technically and ready for PR. REQ-011 remains
  unauthorized.
- PR #4 opened against master:
  `https://github.com/Hans-Einar/emuSA80535-N/pull/4`.
- Exact CPU revision reported to Ponsse Issue #26:
  `https://github.com/Hans-Einar/ponsse/issues/26#issuecomment-5484439484`.

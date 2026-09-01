# Implementation notes

Status: active; no verified SLC-007 product implementation yet.

## Baseline

- Branch: `codex/sab80535-uart`.
- Base/master merge: `c0cd6f26bd8984c9fed10eb81716619cb1bb96e6`.
- Accepted Timer product: `30cf42efa845d29a47a950eca7bbaf657490fbe6`.
- Only REQ-011/SLC-007 is authorized.

## Verified work

- Product commit `d2fe5d31ac887d81f0dfd17cbbfa222abba1acf3`
  passed all submitted/cross-platform suites and most independent semantics,
  but REV-SLC-007 reproduced two blocking findings. It is not accepted.
- Independently verified positive behavior covers divider/framing/TI/back-to-
  back/RX/full-duplex/interrupt/reset/classic behavior outside F001/F002.
- Corrective commit `336c50a03da4067c22d9567de6075724261a66e9`
  resolved REV-SLC-007-F001/F002, but REV-SLC-008 opened one post-callback RX
  mirror-coherence finding. It is not accepted.
- Final corrective commit `d2d3f63b1a10a0b042d052dd6d872c708db967e0`
  synchronizes post-callback SBUF mirror from canonical RX and adds direct/
  reentrant coherence regressions.
- REV-SLC-009 approved without findings. VER-SLC-009 passed all four platform/
  sanitizer suites, three product diff checks, no-target/no-live, NDJSON and
  master-ancestry gates.
- REQ-011/SLC-007 is accepted technically and ready for PR.
- PR #5 opened against master:
  `https://github.com/Hans-Einar/emuSA80535-N/pull/5`.
- Exact reviewed CPU revision reported to Ponsse Issue #26:
  `https://github.com/Hans-Einar/ponsse/issues/26#issuecomment-5486776430`.

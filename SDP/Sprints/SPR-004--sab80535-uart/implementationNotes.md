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

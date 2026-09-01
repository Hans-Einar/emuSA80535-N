# Implementation notes

Status: active; no SLC-013 product implementation recorded.

## Frozen baseline

- Branch: `codex/sab80535-external-edges`.
- Exact master: `d9f80eba172dd9d7281aaa9e5cfef461b6b9709b`.
- Accepted SLC-010 product is merged on the baseline.
- Accepted `emu-debug` 1.0 runtime is merged and frozen for this Slice.
- Consumer evidence: Ponsse PR #25 exact HEAD
  `6a22d8713b607308c94f02df17d35ddbe8a36d6a` plus `doc/P1000/`.

## Authorized work

Only ITR-013 / SLC-013 external-edge completion of REQ-012 is active. Product
implementation and Worker evidence have not started.

## Stop boundary

No ADC, Siemens Timer2, P1000 board/NEC logic, target signal semantics,
live/physical I/O or frozen `emu-debug` protocol change is authorized.


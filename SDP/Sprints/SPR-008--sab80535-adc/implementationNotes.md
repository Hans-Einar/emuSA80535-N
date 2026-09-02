# Implementation notes

Status: active; no SLC-014 product implementation recorded.

## Frozen baseline

- Branch: `codex/sab80535-adc`.
- Exact master: `a4d24786eb86a55479adc4ef14d0f27424fb5705`.
- Accepted SLC-013/REQ-012 is merged on the baseline.
- Accepted `emu-debug` 1.0 protocol remains frozen.
- Consumer evidence: Ponsse PR #25 exact HEAD
  `19ef6ff45efa719612b70e70b8c31b9cb2ebb7e9` plus current `doc/P1000/`.

## Authorized work

Only ITR-014 / SLC-014 deterministic ADM=0 single-conversion ADC work is
active. Product implementation and Worker evidence have not started.

## Stop boundary

No Timer2, capture/compare, P1000 board/NEC/calibration logic, full continuous
ADC mode, live/physical I/O or frozen debug-protocol change is authorized.

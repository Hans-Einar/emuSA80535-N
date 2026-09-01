# Implementation notes

Status: review pending; Worker implementation recorded but not accepted.

## Frozen baseline

- Branch: `codex/sab80535-external-edges`.
- Exact master: `d9f80eba172dd9d7281aaa9e5cfef461b6b9709b`.
- Accepted SLC-010 product is merged on the baseline.
- Accepted `emu-debug` 1.0 runtime is merged and frozen for this Slice.
- Consumer evidence: Ponsse PR #25 exact HEAD
  `6a22d8713b607308c94f02df17d35ddbe8a36d6a` plus `doc/P1000/`.

## Worker result awaiting review

- Exact product/test HEAD:
  `3cfc4a8e9a5accb4a91df36b0b03119bc4d1de9b`.
- Adds generic immediate/scheduled INT0..INT6 resolved-pin stimulus, a bounded
  64-event virtual-cycle queue, canonical TCON/IRCON edge/level semantics and
  immutable edge diagnostics through the accepted controller.
- Adds focused all-source, selection, level/re-arm, masking, arbitration,
  preemption/RETI, replay/reset/observer and length/saw fixture tests.
- Updates one SLC-010 negative edge assertion while preserving latch
  immutability and proving there is no direct vector/PC service.
- Worker reports Windows GCC/strict Clang full core+debug gates and WSL GCC/
  sanitizer core/facade gates passing.
- WSL Python 3.6 cannot parse the accepted process test; the same test passes
  on Windows. The available DAP checkout is not the accepted pinned revision,
  so no identity-bypassing integration result is claimed.
- Frozen `emu-debug` protocol modules, SDP and all unauthorized scope are
  unchanged by the product commit.

REV-SLC-013 must independently audit exact product HEAD before acceptance.

## Stop boundary

No ADC, Siemens Timer2, P1000 board/NEC logic, target signal semantics,
live/physical I/O or frozen `emu-debug` protocol change is authorized.

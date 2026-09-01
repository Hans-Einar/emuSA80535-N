# Implementation notes

Status: verified; SLC-013 accepted.

## Frozen baseline

- Branch: `codex/sab80535-external-edges`.
- Exact master: `d9f80eba172dd9d7281aaa9e5cfef461b6b9709b`.
- Accepted SLC-010 product is merged on the baseline.
- Accepted `emu-debug` 1.0 runtime is merged and frozen for this Slice.
- Consumer evidence: Ponsse PR #25 exact HEAD
  `6a22d8713b607308c94f02df17d35ddbe8a36d6a` plus `doc/P1000/`.

## Worker result

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
  sanitizer core/facade gates passing. Later Reviewer/Master runs also pass the
  WSL process suite with Python 3.13.9.
- Frozen `emu-debug` protocol modules, SDP and all unauthorized scope are
  unchanged by the product commit.

## Accepted review

REV-SLC-013 approved exact product/test HEAD `3cfc4a8...` with no findings.
Its independent Siemens, adversarial queue/callback, cross-platform sanitizer,
scope and product-identity audits pass.

## Verified acceptance

- VER-SLC-013 passed the complete Windows GCC/strict Clang, WSL GCC,
  WSL ASan+UBSan, core, debug facade/process and exact-DAP real matrix.
- Product/test HEAD `3cfc4a8...` is accepted; later commits are SDP-only.
- REQ-012 is satisfied/current through accepted SLC-010 plus SLC-013.
- The frozen debug protocol and all stop-boundary exclusions remain intact.
- Focused implementation PR #10 is open and intentionally unmerged:
  https://github.com/Hans-Einar/emuSA80535-N/pull/10

## Stop boundary

No ADC, Siemens Timer2, P1000 board/NEC logic, target signal semantics,
live/physical I/O or frozen `emu-debug` protocol change is authorized.

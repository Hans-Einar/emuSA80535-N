# REV-SLC-009 — Final UART coherence review

Status: approved
Reviewed Slice: SLC-009 correcting SLC-007/SLC-008
Reviewed corrective HEAD: `d2d3f63b1a10a0b042d052dd6d872c708db967e0`
Reviewed parent: `169741fbdf84235e22e49898785439fce54caf0f`
Cumulative UART commit: `d2fe5d31ac887d81f0dfd17cbbfa222abba1acf3`
Reviewer: fresh independent reviewer
Reviewed: 2026-09-01

## Disposition

Approved with no findings. No REV-SLC-009-F IDs were opened. The full UART
corrective chain is accepted at the reviewed product HEAD.

## Corrective evidence

- Mutable write callback observed TX A5, changed canonical RX/mirror, and
  returned with mirror synchronized from canonical RX.
- Read callback override returned E7 while ordinary read remained canonical.
- Guarded callback advanced an RX frame through final shift; canonical RX,
  mirror, architectural read, RI and RB8 remained coherent.
- Prior callback observation/override and unsupported mode/BD pending isolation
  remained resolved.

## Cumulative gates

All 18 UART classes and Stage0/IRQ/timer regressions passed on Windows GCC,
strict Clang, openSUSE WSL GCC and WSL ASan/UBSan. Classic SBUF behavior,
divider/framing/TI/back-to-back/RX/full-duplex/shared interrupt/reset/observer
semantics passed. Diff, ancestry, ledger and forbidden-scope audits passed.

## Stop boundary

No Stage2, ADC, Timer2 producer, target policy or live/physical I/O is approved
or implied by this review.


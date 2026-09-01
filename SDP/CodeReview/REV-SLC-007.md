# REV-SLC-007 — Independent mode-3 UART review

Status: changes-required
Reviewed Slice: SLC-007
Reviewed product HEAD: `d2fe5d31ac887d81f0dfd17cbbfa222abba1acf3`
Reviewed parent: `7dae2c3ee405b21131d905fe99997c9f623a501e`
Reviewer: fresh independent reviewer
Reviewed: 2026-09-01

## Disposition

Not approved. Two high/blocking public-contract and mode-isolation findings
require a bounded corrective Slice.

## Findings

### REV-SLC-007-F001 — High/blocking — SBUF callbacks broken

The SAB SBUF read path invokes but discards the established SFR read callback's
return value. The write path calls its callback while `mSFR[SBUF]` still holds
RX storage, so it cannot observe the architectural TX byte. A probe with RX=11,
TX=A5 and callback-read E7 observed write=11 and returned read=11.

Separate physical storage must preserve the parent callback contract: read
override controls the returned architectural value; write callback observes the
TX write value without permanently overwriting RX storage.

### REV-SLC-007-F002 — High/blocking — unsupported mode write leaks to mode 3

SBUF write mutates pending byte/TB8 before validating mode3/Timer1 source. A
pending mode3 frame was replaced by a mode1 write; similarly a write with
ADCON.BD set replaced a pending Timer1 frame. Restoring supported mode/source
then transmitted the unsupported write.

Unsupported mode or dedicated-generator writes must not mutate mode3 pending or
in-flight state. They may still be observed through the generic SFR callback/
trace contract without gaining UART behavior.

## Positive findings

Internal Timer1 event use, continuous 16/32 phase, exact 48-cycle/19200 timing,
TX framing/TI, real-ISR back-to-back, RX timing/loss rules, full duplex, shared
0023, software TI/RETI, reset, classic regression and observer determinism
otherwise matched SLC-007. All cross-platform/sanitizer suites passed and no
target/live-I/O leakage was found.

## Required follow-up

ITR-008 / SLC-008 corrects F001/F002 with callback override/observation and
unsupported-mode/BD pending-isolation regressions, then receives fresh review.


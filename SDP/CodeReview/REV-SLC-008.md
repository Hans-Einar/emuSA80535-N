# REV-SLC-008 — Corrective SBUF/mode-isolation review

Status: changes-required
Reviewed Slice: SLC-008
Reviewed corrective HEAD: `336c50a03da4067c22d9567de6075724261a66e9`
Reviewed parent: `573d8db5682c6db595aa54262accc0bd4e06c39b`
Reviewer: fresh independent reviewer
Reviewed: 2026-09-01

## Disposition

Not approved. REV-SLC-007-F001/F002 are resolved, but one new high/blocking
callback-state coherence finding requires a narrower correction.

## Resolved prior findings

- SAB SBUF read callback override controls returned data.
- Write callback observes TX value while ordinary RX storage is preserved and
  trace records TX.
- Mode0/1/2 and ADCON.BD writes remain observable without changing pending or
  active mode3 byte/TB8; restoring support transmits original frames.

## REV-SLC-008-F001 — High/blocking — stale RX mirror restoration

The write path saves the pre-callback SBUF mirror and restores it afterward.
Because the established callback receives mutable CPU state, it may coherently
change canonical receive data/mirror or perform reentrant receive progress. The
stale restore then desynchronizes `mSABUartRxData` and `mSFR[SBUF]`.

Probe: canonical/mirror 11; callback observed TX A5 and set both RX
representations to 22; return left canonical 22 and mirror 11. Post-callback
restoration must set the physical mirror from current canonical RX storage.

## Evidence

All four platform/sanitizer suites and other framing/divider/RX/interrupt/
classic/mode-isolation/no-scope probes passed. No files were changed by review.

## Required follow-up

ITR-009 / SLC-009 fixes only post-callback RX mirror coherence, adds mutation/
reentrant regression and receives a fresh independent review.


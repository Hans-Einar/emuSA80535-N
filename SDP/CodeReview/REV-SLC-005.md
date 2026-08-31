# REV-SLC-005 — Final corrective review of Siemens interrupt controller

Status: approved
Reviewed Slice: SLC-005 correcting SLC-004
Reviewed corrective HEAD: `85fa6e1f8318c691598b9a2ae191d1bd94b436c8`
Reviewed parent: `ec5155f170839868de55fa74ffea28af76cf2ac1`
Cumulative controller commit: `529602d800e36e87f495ef088f17e67ba3659a1a`
Reviewer: fresh independent reviewer
Reviewed: 2026-08-31

## Disposition

Approved with no new findings. REV-SLC-004-F001 and F002 are resolved and the
cumulative SLC-004 controller contract is satisfied at the corrective HEAD.

## Finding resolution

- Raw IRCON uses `C6/0x40=TF2` and `C7/0x80=EXF2`.
- TF2 is eligible only through IEN0.ET2; EXF2 only through IEN1.EXEN2.
- Synthetic Timer2 assertion sets raw C6; explicit source clear clears both
  aggregate flags.
- Literal ROM instruction `C2 C6` clears only C6 and leaves C7 pending.
- Startup-like SAB Timer1 mode2/TR1/SM1 preserves TF1 across six instructions
  with EAL masked and six more with ET1 masked.
- Accepted Timer1 vector `001B` auto-clears TF1.
- Classic 8051/8052 serial shortcut still clears TF1 as before.

## Cumulative evidence

The Reviewer reconfirmed all 12 sources/vectors/order, pair priorities/four
levels, strict-higher nesting/depth, source-in-service exclusion, RET versus
RETI, inhibit timing, flag clearing matrix, two-cycle/failed entry, invalid
API/reset behavior, immutable deterministic trace and Stage 0/classic
regressions.

Windows GCC/strict Clang, openSUSE WSL GCC normal/strict and WSL ASan/UBSan
passed. Corrective diff/file-set and no-target audits passed. No Timer/UART/ADC/
GPIO scheduler or physical I/O was added.

## Open boundary

Approval is only for Issue #2 SLC-004 plus its SLC-005 corrections. Later
Timer0/Timer1 and 9-bit UART slices remain unauthorized.


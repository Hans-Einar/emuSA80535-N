# REV-SLC-004 — Independent review of Siemens interrupt controller

Status: changes-required
Reviewed Slice: SLC-004
Reviewed product HEAD: `529602d800e36e87f495ef088f17e67ba3659a1a`
Reviewed parent: `f09b1d3efd4dc5d1693443b115a1dff44145ef5e`
Reviewer: fresh independent reviewer
Reviewed: 2026-08-31

## Disposition

Not approved. The controller structure and most Issue #2 behaviors pass, but
one blocking raw register error and one high pending-state integration error
require a bounded corrective Slice.

## Findings

### REV-SLC-004-F001 — Blocking — Timer2 IRCON identities reversed

The product defines `EXF2=0x40/C6` and `TF2=0x80/C7`. Siemens figure 8-6 and
the PR #25 generated rows at ROM `4C73`/`8DA8` establish `C7=EXF2` and
`C6=TF2`. This reverses split ET2/EXEN2 gating and makes the synthetic Timer2
request set the wrong raw bit. Raw C6/C7 tests are required; enum-only tests
cannot validate the enum values themselves.

### REV-SLC-004-F002 — High — SAB Timer1 pending is erased before acceptance

The inherited serial path clears TF1 whenever Timer1 runs with `SCON.SM1=1`,
even when no transmission is active. Under startup-like SAB TR1/mode2/SM1 with
EAL disabled, a synthetic Timer1 pending request loses canonical TF1 after one
NOP and disappears at the next arbitration. DES-016 requires TF1 to clear on
accepted Timer1 vector entry, not as a UART side effect.

The fix must be variant-bounded controller integration: prevent the legacy
serial shortcut from destroying SAB TF1. It must not introduce Timer1 baud or
UART behavior.

## Positive findings

The Reviewer independently verified all 12 vectors/order, priority pairs,
four levels, strict-higher nesting/depth, source-in-service exclusion, RET
versus RETI, one-following-instruction inhibit, remaining clear classes,
two-cycle/failed entry, invalid API/reset, immutable deterministic trace,
classic/Stage 0 regression and no-target/later-slice boundaries.

## Evidence

- Windows GCC and strict Clang Stage 0/1 suites passed.
- openSUSE WSL GCC normal/strict and ASan/UBSan passed.
- Targeted raw Timer2 and startup-like Timer1 probes reproduced both findings.
- Product diff check and blob identity passed; worktree remained clean.

## Required follow-up

ITR-005 / SLC-005 must correct REV-SLC-004-F001 and F002, add raw numeric and
startup-like persistence regressions, then receive a fresh independent review.


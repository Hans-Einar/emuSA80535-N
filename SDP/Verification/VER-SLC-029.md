# VER-SLC-029 — Indirect MOV operand correction

Date:2026-10-28. Disposition:PASS.
Final source:d247e7ff82b7b3890da4154d720c94bdb86af64f.
Fresh review:6809ea1bfcc999ced1bac767b2108a0305165767 (REV-SLC-029).
Artifact:[SLC-029-ARTIFACT.json](SLC-029-ARTIFACT.json).

The two-line production correction restores indirect Ri source and encoded
direct destination for opcodes86/87. It changes no interfaces, ROM, board model
or UART policy. Worker and independent Reviewer reproduced the old failure.
480 generic matrix cases cover variants, banks, both dispatchers, lower/upper
IRAM, SFR writes, callbacks/abort and aliases. Both exact supplied synthetic
programs are retained across three variants and both dispatchers:12 additional
cases. Each executes five instructions, reaches PC000C/cycle8 and loads A=A5.
Old code fails2020 assertions with the final tests; the fixed code passes.

Full fresh core/control/runtime-host/Unix-control/legacy-debug/link tests passed
on product commit4e74190; the final successor adds only the readable regression
programs. Final exact Stage-0 and ASan/UBSan/leak checks passed. Independent extra
144 boundary/ACC/absent-upper cases and80 reverse A6/A7 cases passed. Reviewer
records the distinction between full-suite and test-only successor validation.

Master built and installed the exact final source archive without Git and ran
the installed-artifact checker. An independent real Unix probe used separately
installed old45b92ab and corrected d247e7f executables with an ABI1 synthetic
module compiled only against installed headers. Both original programs produce
A=5A on old runtime and A=A5 on fixed runtime, at identical PC000C, five
instructions and eight cycles. Recorded hello/source identities and snapshots
are retained in verification/installed-mov-repro.json alongside build/install
and Worker/Reviewer evidence. No external firmware was changed or used here.

Two exact source exports are byte-identical, SHA256
be0c84c8eec3cba9f64ff0722f54beabe5ad477e3924f707ab0667615adbd350.
All frozen headers/interface directories and old artifact pins remain unchanged.
Capability inventory is unchanged from SLC-025: local control3 works; observation,
UART outcome and tracing capabilities remain unavailable. This corrected source
supersedes45b92ab for new integration; preserve accepted V0 as historical evidence.
Master checks ledger/aliases/traceability before acceptance and hands off the
corrected artifact, then resumes planned SLC-026 under standing authorization.

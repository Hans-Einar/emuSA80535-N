# Handoff

## Current handoff — 2026-09-06

Issue #14 comment 5517725977 supersedes historical next-step/topology text
below. PR #16 targets master and must not be merged by this task. The active
pass is debugger SPR-008 / ITR-017 / SLC-017 current-master reconciliation.
Baseline: accepted PR `1e588d28fb168a7c5a42c4c7dc4b51f84d29d1ed`;
exact fetched master: `b43fe36b0965b6ac8628677bb6fcc16513d1f567`.
Next: fresh Worker merges that exact master and resolves bounded conflicts,
then fresh independent reconciliation review and complete new verification.
All accepted runtime semantics and DES-090..DES-097 must survive.
CurrentIndex, Relations and Ledger register the reconciliation pass.

SLC-015 and SLC-016 are accepted. SLC-017 is under the Issue #14 takeover
acceptance pass. The active work is a fresh holistic review, any authorized
standalone-runtime corrections, full verification and a documentation-only
facade/versioning/paging freeze. Core producer hooks and all frontend/protocol
work remain deferred.

## Pull request

- Source WIP branch: `codex/debug-trace-runtime-slc017-wip`
- Source WIP HEAD: `356836637d5ff432d91fc508fd55b2f17b45cdb3`
- Active takeover branch: `codex/debug-trace-runtime-takeover`
- Related PR #11: design PR, open against `master`; do not merge here
- Related PR #12: runtime PR, open against PR #11 branch; do not merge here
- Takeover PR #16:
  https://github.com/Hans-Einar/emuSA80535-N/pull/16
- Takeover PR base: `codex/tracepoint-debugger-spec` (PR #11 branch)
- Takeover PR head: `codex/debug-trace-runtime-takeover`
- Topology: PR #16 supersedes runtime PR #12 but does not rewrite, merge or
  close PR #11/#12; merge requires separate Steering authorization

## Exact next step

Open the takeover PR from `codex/debug-trace-runtime-takeover` against the
stacked design branch `codex/tracepoint-debugger-spec`. State explicitly that
it supersedes runtime PR #12 without merging or rewriting PR #11/#12. Do not
merge the takeover PR without separate Steering authorization.

## Traceability IDs

- `SPR-008`, `ITR-017`, `SLC-015`, `SLC-016`, `SLC-017`
- historical/provisional `REV-SLC-017`
- completed, corrections-required `REV-SLC-017-HOLISTIC`
- resolved `REV-SLC-017-HOLISTIC-F001..F006`
- approved `REV-SLC-017-CORRECTIONS`
- passed-with-environment-note `VER-SLC-017`
- active target design `DES-090..DES-097`

## Verification completed

- strict GCC/Clang focused suites on Windows and WSL: passed;
- Clang ASan/UBSan on Windows and WSL: passed;
- full Stage-0/IRQ/timer/UART/port-MOVX/event/trace/runtime regressions under
  GCC and Clang on Windows and WSL: passed;
- existing debugger facade and modern-Python process suites: passed;
- Clang static analyzer, diff, traceability and forbidden-scope gates: passed;
- Valgrind: not installed, recorded in VER-SLC-017 under “where available”.

## Design freeze

DES-090..DES-097 is authoritative for the future stable wrapper and page
cursor. The current runtime C header remains internal. No frontend/protocol/
DAP/CPU implementation is present in this takeover.

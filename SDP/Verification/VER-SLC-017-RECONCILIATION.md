# VER-SLC-017-RECONCILIATION — Current-master regression gate

Status: passed-with-environment-note
Verified: 2026-09-06
Verifier: Master, independent of reconciliation Worker
Scope: debugger SLC-017 current-master reconciliation only

## Exact authority and tested identity

- Latest Stage-4 Steering authority:
  https://github.com/Hans-Einar/emuSA80535-N/issues/14#issuecomment-5559152334.
- Original bounded merge-readiness contract:
  https://github.com/Hans-Einar/emuSA80535-N/issues/14#issuecomment-5517725977.
- Exact integrated master: `b43fe36b0965b6ac8628677bb6fcc16513d1f567`.
- Accepted prior PR #16 HEAD: `1e588d28fb168a7c5a42c4c7dc4b51f84d29d1ed`.
- Accepted correction product: `d956177add44dda9efbd6d9e372a9c0a6d40f777`.
- Reconciliation merge: `74dca67aafebdddc82ade3e5d6864a1457ea75bb`.
- Exact tested/review candidate: `c21d63f121f6305afd006a145a96c8dddae8b55e`.

The candidate contains exact master as an ancestor. It preserves all master
C/H/test blobs, including accepted external-edge, ADC and Timer2 behavior,
and preserves the accepted standalone debugger module/test blobs and frozen
DES-090..DES-097 document. Later commits are limited to SDP review,
verification, authority and handoff records; product/test/Makefile identity
is rechecked before publication. No new CLI/DAP/wire/CPU/sink feature exists.

## Executed matrix

| Gate | Windows native | WSL/Linux | Disposition |
|---|---|---|---|
| Event/watch, trace-router, composed runtime | GCC + Clang strict C99 | GCC + Clang strict C99 | PASS |
| Stage-0, IRQ, Timer0/1, UART, port/MOVX | GCC + Clang strict C99 | GCC + Clang strict C99 | PASS |
| External edges, ADC, Timer2 | GCC + Clang strict C99 | GCC + Clang strict C99 | PASS |
| Existing debugger C facade | GCC + Clang | GCC + Clang | PASS |
| Existing emu-debug 1.0 NDJSON process | GCC/Clang servers, Python 3.14.0 | GCC/Clang servers, Python 3.12.13 | PASS |
| ASan + UBSan | Clang, all 12 C suites | Clang, all 12 C suites, leak detection | PASS, no diagnostic |
| Clang static analyzer | All 3 standalone modules | All 3 standalone modules | PASS, empty diagnostic logs |
| Actual reconciled Makefile | Fresh exact-commit snapshot, all 11 test recipes | Fresh exact-commit snapshot, all 11 test recipes | PASS |
| Plain git diff --check | Exact master and prior PR ranges | Same repository evidence | PASS |
| YAML / NDJSON / provenance | Duplicate-safe YAML; ledger records and both-parent reconstruction | Same repository evidence | PASS |
| Reconciliation and forbidden-scope audit | Both-parent blob and changed-path checks | Same repository evidence | PASS |
| Valgrind | NOT_AVAILABLE | Not installed in either available WSL distribution | Environment note under where-available rule |

The accepted execution set contains 81 successful compile/run/version/analyzer
steps per environment, plus two successful actual Makefile runs. The twelve C
suites are three focused standalone debugger suites, eight Stage0..Stage4
regression suites, and the debugger C facade. No executable assertion failed.

## Exact tools and flags

Windows 11 build 26200 x86_64: GCC 12.2.0; Clang 18.1.8 targeting
`x86_64-pc-windows-msvc`; GNU Make 4.4; Python 3.14.0; Git 2.43.0.windows.1;
PyYAML 6.0.3. Clang uses installed MSVC/UCRT headers and libraries.

WSL2 openSUSE Leap 15.5, Linux
`6.18.33.2-microsoft-standard-WSL2` x86_64: GCC 7.5.0; Clang 17.0.6;
GNU Make 4.2.1; Git 2.35.3. The system Python is 3.6.15 and is not used for
the process suite. Existing portable Python 3.12.13 at
`/tmp/codex-python-3.12.13/python/bin/python3.12` ran the modern Python gates.

Focused flags: `-std=c99 -Wall -Wextra -Werror -pedantic -Wshadow
-Wconversion -Wsign-conversion -O2 -g`. Core/facade/server flags retain
`-Werror -pedantic` and use `-Wno-unused-parameter`, omitting the additional
conversion warnings. Native Clang uses `-D_CRT_SECURE_NO_WARNINGS` to compile
standard C library calls against Microsoft headers; warnings-as-errors stays
enabled. Sanitizers add `-O0 -fno-inline -fsanitize=address,undefined
-fno-sanitize-recover=all -fno-omit-frame-pointer`. Windows test executables
use an 8 MiB stack, as in accepted VER-SLC-017, and add Clang's installed ASan
runtime directory to the test-process PATH. ASAN halts on error; UBSAN halts
and prints stack traces; Linux additionally enables leak detection.

## Harness corrections and environment limitations

The first native run passed all GCC gates but its Clang compilation initially
omitted `_CRT_SECURE_NO_WARNINGS`. Microsoft deprecated declarations for
standard C `strcpy`/`fopen` therefore failed warnings-as-errors before those
test executables existed. The external harness was corrected; the complete
native Clang, sanitizer and analyzer set then passed on the same exact product
HEAD. Failed harness-attempt records are retained in the evidence manifest.
No product source or assertion was changed to obtain a pass.

Final independent SDP review required the reusable verification helper to
derive its sources from the supplied commit rather than trusting a mutable
checkout. The helper now validates a full immutable SHA and compiles a fresh
Git archive. The existing matrix remains valid by the separately verified
candidate/source equality and log audit. The helper correction was checked
on Windows and WSL: all 155 extracted files match the candidate's committed
blobs, and a symbolic `HEAD` input is rejected. Product files are unchanged.

The first WSL Makefile snapshot attempt used a Windows-created worktree's
`.git` pointer, whose drive-letter path Linux Git cannot resolve. No build or
test executed in that attempt. Using the shared main repository's Git object
store to archive the same immutable candidate fixed the harness; all eleven
Makefile suites passed. No repository Git configuration was changed.

Valgrind is absent on PATH and in both installed WSL distributions. MinGW GCC
sanitizer libraries are unavailable; native and WSL Clang provide the required
ASan/UBSan coverage. These do not constitute silent skipped gates.

## Reproduction and retained evidence

`Tools/slc017_reconciliation_verify.py` compiles and runs the existing suites
from a fresh Git archive of the exact full `--head`, independently of mutable
checkout content. Supply a Git object repository as `--repo` and a fresh
`--output` directory; use Python 3.12+ for the archive extraction helpers.
`--prepare-only` validates/extracts sources without running compilers.
On Windows, put GCC and Clang on PATH; the script finds
Clang's sanitizer runtime. `--compilers clang` reproduces only the corrected
native Clang pass. In WSL, use the main Git object repository when Windows
worktree drive-letter pointers cannot be resolved by Linux Git.

`Tools/slc017_reconciliation_make.py` archives the exact `--head` and executes
the actual merged test target in a separate output directory. In WSL, supply
the main Git repository for archive access when the worktree was made by
Windows Git.

`Tools/slc017_reconciliation_audit.py` checks both ancestry baselines, plain
whitespace gates, preserved blobs, scope, duplicate-safe YAML, registry source
paths, NDJSON and exact historical ledger record preservation. The dedicated
reconciliation audit additionally verifies ledger block hashes, namespace
mapping and the nine explicitly recorded whitespace normalizations.

`SLC-017-RECONCILIATION-EVIDENCE.json` retains exact commands, environments,
exit codes and SHA-256 hashes of logs. Raw logs remain at
`C:/Users/hanse/GIT/emuSA80535-N-reconciliation-evidence/`. The evidence binds
to the exact candidate, not a moving branch. Final source-identity and static
gates are recorded in the final SDP acceptance event.

Independent review is separately recorded in
`SDP/CodeReview/REV-SLC-017-RECONCILIATION.md`. Verification is not PR merge
authorization; Steering owns the merge of PR #16.

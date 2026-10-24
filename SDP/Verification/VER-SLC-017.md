# VER-SLC-017 — Composed trace runtime takeover verification

Status: passed-with-environment-note
Verified Slice: SLC-017
Steering authority: Issue #14
Verified: 2026-09-02
Source WIP HEAD: `356836637d5ff432d91fc508fd55b2f17b45cdb3`
Corrected product HEAD: `d956177add44dda9efbd6d9e372a9c0a6d40f777`

## Disposition

SLC-017 passes the complete available takeover matrix after
REV-SLC-017-HOLISTIC and the independently approved F001..F006 correction.
SLC-015..017 remain deterministic, bounded, atomically configured and isolated
from CPU, frontend, protocol, DAP, product and physical-I/O implementation.

Valgrind is not installed in either available WSL distribution and is recorded
as `NOT_AVAILABLE`. Issue #14 requires Valgrind where available, so this is an
environment note rather than a failed or silently skipped gate. Clang
AddressSanitizer and UndefinedBehaviorSanitizer passed on both Windows and
WSL/Linux. Historical WIP claims are not counted as takeover execution.

## Exact environment

### Windows

- Windows native x86_64;
- GCC 12.2.0;
- Clang 18.1.8, target `x86_64-pc-windows-msvc`, POSIX thread model;
- GNU Make 4.4;
- Python 3.14.0;
- Git 2.43.0.windows.1.

### WSL/Linux

- openSUSE Leap 15.5 under WSL2;
- Linux `6.18.33.2-microsoft-standard-WSL2` x86_64;
- GCC 7.5.0;
- Clang 17.0.6;
- GNU Make 4.2.1;
- Python 3.13.9;
- Git 2.35.3;
- Valgrind: `NOT_AVAILABLE` (`command not found`);
- the older openSUSE Leap 15.4 image also has no Valgrind or C compiler and
  therefore supplies no alternative execution environment.

## Verification matrix

| Gate | Windows | WSL/Linux | Result |
|---|---|---|---|
| event/watch focused suite | GCC 12.2 + Clang 18 strict | GCC 7.5 + Clang 17 strict | passed |
| trace-router focused suite | GCC 12.2 + Clang 18 strict | GCC 7.5 + Clang 17 strict | passed |
| composed-runtime focused suite | GCC 12.2 + Clang 18 strict | GCC 7.5 + Clang 17 strict | passed |
| strict C99 warnings-as-errors | pedantic, shadow, conversion, sign-conversion | same flags | passed |
| ASan + UBSan | Clang 18, `-O0 -fno-inline`, 8 MiB test stack | Clang 17, `-O0 -fno-inline` | passed |
| static analysis | Clang 18 analyzer, all three modules | not repeated | passed |
| Stage-0 regression | GCC + Clang | GCC + Clang | passed |
| IRQ regression | GCC + Clang | GCC + Clang | passed |
| timer regression | GCC + Clang | GCC + Clang | passed |
| UART regression | GCC + Clang | GCC + Clang | passed |
| port/MOVX regression | GCC + Clang | GCC + Clang | passed |
| existing debugger C facade | GCC | GCC | passed |
| `emu-debug` NDJSON process | Python 3.14 | Python 3.13.9, exact product provenance injected | passed |
| Valgrind | not available | not installed | NOT_AVAILABLE |
| diff/traceability/forbidden scope | native Git/Python | same repository evidence | passed |

The Windows sanitizer build uses a larger test-process stack because the
focused test translation unit intentionally holds several bounded but large
router objects on its stack. The independent correction review reproduced a
default-stack failure under sanitizer inlining and passed with inlining
disabled. Product runtime state remains opaque and heap-owned; no sanitizer
finding was reported.

MinGW GCC sanitizer runtimes (`libasan`/`libubsan`) are absent. This does not
remove sanitizer coverage because both native Clang 18 and WSL Clang 17
ASan/UBSan builds and executions passed.

## Behavioral evidence

Focused tests directly cover:

- single source sequencing, ascending derived watch sequences and source
  correlation;
- callback unwind before derived dispatch and non-recursive watch matching;
- before/source/derived/after gate order and all four ordinary timings;
- all four same-marker lifecycle gate timings for reset and load;
- nested interrupt depth and include/suppress/interrupt-only policies;
- pending derived overflow neutrality and sequence exhaustion checks;
- stop coalescing, saturation and lowest-watch-ID/earliest-source ordering;
- reset/load generation increments, monotonic event sequence and clear-session;
- forged public reset/load neutral rejection;
- PC, explicit CODE and address-only CODE-capable invalidation on load while
  explicit XDATA selectors remain enabled;
- non-destructive offset pages, ring wrapping and saturated overwrite count;
- atomic replacement and referential validation for all collections;
- active suppression replacement rejection until explicit flush;
- bounded trace-ID live update, retirement, exhaustion and clear-session reuse;
- bounded strict UTF-8 metadata validation and atomic rejection;
- every advertised observer/watch/action/route/trace/destination/point/gate/
  pending/ring bound.

## Compatibility and forbidden-scope audit

Relative to the stacked design base
`origin/codex/tracepoint-debugger-spec`, product changes are limited to:

- `emu_debug_event.h/.c`;
- `emu_debug_trace.h/.c`;
- `emu_debug_runtime.h/.c`;
- their focused tests and Makefile integration.

No change exists in `core.c`, `opcodes.c`, SAB80535 peripheral behavior,
`emu_debug.c/.h`, `emu_debug_server.c`, the 1.0 command/capability set or DAP
product source. No P1000 ROM address, protocol/board type, D71055 device,
machine signal, hydraulics/valve policy, physical serial/GPIO/CAN or other
physical machine control was added.

## Static repository gates

- `git diff --check origin/codex/tracepoint-debugger-spec...HEAD`: passed after
  removal of six pre-existing Markdown line-end spaces in WIP review records;
- `CurrentIndex.yaml` and `Relations.yaml`: parsed with Python/PyYAML;
- every non-empty `Ledger.ndjson` line: parsed with Python `json`;
- exact changed-file and forbidden-token audit: passed;
- worktree: clean after generated test/analyzer artifacts were removed.

## Conclusion

VER-SLC-017 is **passed-with-environment-note**. The unavailable Valgrind tool
is the only unexecuted requested environment-dependent gate and is not a
blocker under Issue #14's “where available” rule. No unresolved product,
review, verification or Steering blocker remains for SLC-017 acceptance.

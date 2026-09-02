# VER-SLC-015 — Standalone debug event/watch runtime verification

Status: passed-with-environment-note
Verified Slice: SLC-015
Verified: 2026-09-02

## Scope and result

Master verified the Worker implementation and REV-SLC-015 corrections. The
new additive C99 module owns event sequencing, bounded observer fan-out and
conditional watch matching without changing CPU execution or peripherals.

Evidence:

- `make debug-event-test` passes;
- `make core-test` passes Stage-0, IRQ, timer, UART, port/MOVX and the new
  debug-event/watch suite;
- Reviewer strict GCC C99 warnings-as-errors, ASan/UBSan and Valgrind pass;
- all six comparisons, signed/unsigned edges, unknown values, observer order,
  mutation isolation, reentrancy rejection, action composition and bounds have
  focused regression coverage;
- YAML, NDJSON and `git diff --check` pass;
- no diff exists in `core.c`, `opcodes.c`, peripheral implementations,
  `emu_debug.c`, `emu_debug_server.c` or DAP.

Clang is unavailable on this host and must be exercised by CI or a later host.
This does not block the GCC-tested additive Slice.

## Disposition

SLC-015 is accepted. Core event production, derived-event draining, trace
sessions/gates, interrupt policies, destinations and protocol/CLI exposure
remain explicitly outside this Slice.

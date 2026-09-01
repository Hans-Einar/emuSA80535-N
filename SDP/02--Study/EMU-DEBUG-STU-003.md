# EMU-DEBUG-STU-003 — Tracepoint debugger source study

Status: accepted design evidence
Study ID: STU-003
Studied: 2026-09-02
Baseline: `emuSA80535-N` `d9f80eba172dd9d7281aaa9e5cfef461b6b9709b`
Consumer authority: `emuSA80535-DAP` commit
`36639b48ddb2ffbafa14c00da794fe1734f7483b`

## Existing emulator seams

The core already exposes immutable, record-only observers:

| Seam | Source | Present record | Important gap |
|---|---|---|---|
| instruction and diagnostic trace | `emu8051.h`, `core.c` | type, machine cycle, executing PC, address, value | one callback slot; no sequence, opcode, completion or call/return classification |
| SFR write | `em8051_sfr_write()` | address and written value | no old value, access kind or SFR read record |
| MOVX transaction | `em8051_movx_context` | cycle, PC, address, direction, final value, P1 latch | no old value for write; callback and backing paths need one canonical observation point |
| Siemens interrupt | `em8051_sab_irq_trace_record` | request/accept/release and controller masks | separate callback and no common ordering sequence |
| Timer0/1 overflow | `em8051_timer_overflow_record` | completed cycle and post-wrap/reload TL/TH | no common ordering sequence; no generic Timer2 observation yet |
| SAB mode-3 UART | `em8051_sab_uart_trace_record` | frame lifecycle, bit, data and ninth bit | separate callback; no common ordering sequence |

The core breakpoint API contains one pre-execution CODE breakpoint. The stable
debugger facade instead owns an atomic, bounded 64-KiB bitmap and checks it
before each instruction. `emu_debug.c` installs the sole general trace callback
to learn completed sequential predecessor relationships. A new trace consumer
must therefore use a deterministic fan-out/event bus; replacing this callback
would silently break `decodeCode` history.

The legacy curses debugger has one global PC breakpoint, run/step controls,
editable memory views and a short instruction-boundary history of SFR/lower
IRAM snapshots. It has no generic watchpoint, tracepoint, persistent trace
sink, condition parser or loss reporting. Its execution pacing uses host wall
time, so tracing must attach below that UI layer and must not use wall time as
emulated ordering.

## Missing instrumentation

An implementation cannot meet the requested semantics until all architectural
paths pass canonical hooks for:

- lower and upper IRAM reads/writes, including indirect access and register
  banks;
- SFR reads, RMW latch reads and SFR writes with captured old and final values;
- XDATA reads/writes, whether served by backing memory or a legacy callback;
- CODE fetch and instruction begin/end;
- ACALL/LCALL, RET/RETI and interrupt entry/exit classification;
- reset, image load and debugger-originated state mutation;
- peripheral events translated into one shared order.

Unknown values must remain explicitly unknown. A write callback cannot infer a
pre-write XDATA value by performing an extra read because reads may have side
effects. Likewise a CODE fetch is not equivalent to a data read or decode
request.

## Frozen protocol and DAP findings

Protocol 1.0 exposes only `hello`, `load`, `reset`, `getState`, `decodeCode`,
`replaceCodeBreakpoints`, `run`, `stepInstruction` and `terminate`. Its seven
required capabilities are frozen. It has no memory-read, watchpoint,
subscription or file-sink command. The frozen DAP consumer explicitly lists
data access events, watchpoints, logical call/return/IRQ events and trace
backpressure as later work. Consequently trace support is an optional minor
extension negotiated by named capabilities; protocol 1.0 clients continue to
work unchanged. No DAP modification is required for the first trace slices.

## Conclusions

1. Preserve the current breakpoint behavior and protocol 1.0 contract.
2. Add one generic immutable event bus at canonical core boundaries.
3. Put matching, hit accounting, bounded buffering and sinks in the debugger
   layer, never in instruction handlers or callbacks.
4. Make the legacy CLI the first interactive consumer and the C facade the
   stable automation boundary.
5. Add optional protocol commands only after facade and sink behavior is
   independently verified.
6. Keep all time virtual and all output local, bounded and explicitly opened.

## Traceability

STU-003 informs DES-064..DES-079 and SLC-013.

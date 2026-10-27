# EMU-RUNTIME-STU-004 — Engine, host and transport source study

Study ID: STU-004
Status: source-grounded proposal; independent review pending
Date: 2026-10-27
Authority: SLC-019; user's runtime architecture clarification

## Observed implementation

Compared preserved upstream `5dc681275151c4a5d7b85ec9ff4ceb1b25abd5a8`
with accepted fork `c8d647a29d5a160637025fa387e31f15628c3ec1`.

| Layer | Upstream | Accepted fork |
|---|---|---|
| CPU state/API, `emu8051.h` | Public CPU struct, memory pointers, opcode/decoder tables, SFR and external-memory callbacks | Explicit classic/SAB variants, deterministic counters/reset, SAB peripherals and generic stimulus/observer APIs |
| Execution, `core.c` / `opcodes.c` | `tick`, reset, timers/interrupts and instruction handlers | Variant-aware timing/arbitration, bounded execution, access semantics and the accepted Stage0..Stage4 behavior |
| Decoder, `disasm.c` | Instruction text/size | Retained engine with corrections and variant-aware register names |
| Host/UI, `emu.c` | Creates CPU, allocates memory, loads Intel HEX, calls `tick`, paces display using host time | Byte-identical to upstream at the compared commits |
| Legacy UI/board | `emulator.h`, `logicboard.c`, `mainview.c`, `memeditor.c`, `options.c`, `popups.c`; shared UI state and direct CPU access | Byte-identical; not a loadable module contract |

Thus embedding callbacks predate this fork; the CPU is not inherently an OS
process. `emu.c` is one host for it. Its curses and wall-time pacing are not
the deterministic core's scheduling authority.

New `binary_loader.c` loads exact raw images. New `emu_debug.c` instead owns
a CPU inside `struct em8051_debugger`, with image storage, decode history and
breakpoints. `emu_debug_server.c:protocol_loop` creates that debugger and
executes serialized NDJSON requests from stdin. This is a second host shape,
not an attach client. The event/trace/runtime modules are accepted standalone
machinery; their presence does not connect canonical CPU producers to them.

## Integration gaps

`opcodes.c` invokes `xread`/`xwrite` for actual bus transactions; its MOVX
observer reports the completed transaction. An observer cannot supply a read
response. Current native structs remain same-build interfaces, not a frozen
module ABI. The SLC-018 static SDK packages those interfaces without changing
their ownership or semantics.

`core.c:advance_machine_cycle` advances internal peripherals and scheduled
external edges. `tick` handles instruction delays and interrupt-entry cycles;
IDLE can advance peripherals, whereas power-down returns without advancing.
`run_control` returns HALT on either PCON sleep bit. Therefore repeatedly
calling bounded `em8051_run` cannot implement the proposed board scheduler.
Generic cycle/access scheduling, complete output observation, owned-session
debug attachment and independent service lifecycle still require work.
There is no implemented SIGUSR1 status contract; the proposed signal behavior
must not be exercised against today's executable as if it already existed.

The sibling DAP contract explicitly deferred attach over a restricted local
socket. Historical board guidance preferred a board-specific executable
embedding the CPU/debug facade. The new proposal retains that component
boundary but selects a generic host with a startup-loaded external module.

## Supporting references

Source anchors: `emu.c:main`, `core.c:tick/run_control/reset`,
`opcodes.c:movx_a_indir_dptr`, `emu_debug.c:em8051_debugger_create/reset`,
`emu_debug_server.c:protocol_loop`; upstream objects are accessible with
`git show <upstream>:<path>`. `git diff --stat <upstream> c8d647a -- <paths>`
confirms the layer comparison.

OS/protocol references: [Unix sockets](https://man7.org/linux/man-pages/man7/unix.7.html),
[XDG runtime directory](https://specifications.freedesktop.org/basedir/latest/),
[signalfd](https://man7.org/linux/man-pages/man2/signalfd.2.html),
[signal safety](https://man7.org/linux/man-pages/man7/signal-safety.7.html),
[signal semantics](https://man7.org/linux/man-pages/man7/signal.7.html),
and [DAP overview](https://microsoft.github.io/debug-adapter-protocol/overview.html).

STU-004 informs REQ-018..REQ-024 and ARCH-011..ARCH-018. No performance
measurement or complete-machine correctness claim follows from this study.

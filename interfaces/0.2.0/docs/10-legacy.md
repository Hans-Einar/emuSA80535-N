# Existing APIs and compatibility

Canonical implemented headers are `lib/include/emusa80535/emu8051.h`,
`emu_debug.h`, `emu_debug_event.h`, `emu_debug_trace.h`, `emu_debug_runtime.h`.
Root headers forward to those files for source compatibility. CPU/debug sources
use the same `<emusa80535/...>` include root as consumers. Installed pkg-config
provides both the namespaced include root and legacy flat include directory.
No public declaration is independently maintained in two locations.

These APIs remain same-build interfaces: public CPU struct, enum/bool/size_t
records and current trace runtime layout are not the stable board ABI or wire.
`load_obj` is a legacy TUI symbol declared in emu8051.h; it is not supplied by
the static headless library. Use the raw loader described in that header.
CPU internal callbacks and opcodes remain private dependencies, not module SDK.

`emu-debug --headless-debug` continues to use [frozen protocol1.0](../reference/legacy-protocol-1.0.md)
with owned CPU and synchronous run/yield. Its load/reset/terminate and EOF
semantics remain unchanged. Target wire2.1 is a new independently negotiated
endpoint profile, not an incompatible change to protocol1.0. Future optional
protocol1 extensions remain governed by the [stable facade reference](../reference/debug-stable-facade.md).

CPU query and its complete API1 table are implemented. Session/debug query symbols
remain declarations only. `EMUIF_CPU_IMPLEMENTED` is 1 and
`EMUIF_TARGET_IMPLEMENTED` remains 0. Compilation alone does not prove runtime
or board compatibility.

## Checked execution added in bundle 0.2.0

The same-build legacy `em8051` struct appends `execution_abort` and its context.
Initialize the whole CPU to zero and rebuild native users together; this is not
binary compatibility with an old struct allocation. The optional predicate is
pure, read-only and non-reentrant; NULL preserves the existing execution path.
It survives reset. The opaque CPU facade uses it to stop after failed callbacks,
including multi-effect instructions and timer-driven UART completion.

`tick` performs no further phase when abort is asserted. Run, run-until and step
return the appended `EM8051_STOP_ABORTED`, including when delay remains pending;
this is an unsafe partial boundary and is not an instruction-completion report.
Existing stop enum values are unchanged. The owner must recover/reset the state
before resuming; merely clearing the predicate cannot undo partial effects.
The API1/boardABI1 records and entry symbols have unchanged layout.

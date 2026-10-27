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
semantics remain unchanged. Target wire2.0 is a new independently negotiated
endpoint profile, not an incompatible change to protocol1.0. Future optional
protocol1 extensions remain governed by the [stable facade reference](../reference/debug-stable-facade.md).

New query/session/debug symbols under contracts/ are declarations only. The
static SDK contains only existing implemented functions, not stub success
implementations of target functions. `EMUIF_TARGET_IMPLEMENTED` is 0. Header
compilation verifies type usability, not runtime/board compatibility.

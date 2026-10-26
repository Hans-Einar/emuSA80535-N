# EMU-PACKAGE-DES-010 — AlmaLinux delivery boundary

Status: active target design
Design items: DES-098..DES-100
Authority: user request in this conversation, 2026-10-26

## DES-098 — Package the accepted generic emulator

Build on AlmaLinux 10.2 x86_64 for the user's existing AlmaLinux-10.2-apps
repository. Deliver `emu-debug` in an RPM and an embedding development RPM
with headers, a static library and pkg-config metadata. The library includes
the accepted core, decoder/loader, debugger facade and standalone event/trace
runtime. Preserve the existing protocol and CPU behavior byte-for-byte in
source. No new stable ABI is claimed: static-library consumers rebuild with
matching headers. The standalone trace APIs remain same-build interfaces.

The packaging version begins at 1.0.0, matching the existing executable's
product version. Record the exact source commit separately; this version does
not imply complete SAB80535 peripheral coverage or Gate-B completion.

## DES-099 — Simulator owns composition

An external simulator owns its CPU instance, image loading, generic XDATA
callbacks, virtual stimuli and scheduling. This repository supplies the CPU
library, never a target board or firmware image. An immutable MOVX observer
cannot supply read values; board reads/writes belong in the existing
`xread`/`xwrite` callbacks. The external adapter must form the required value
context without duplicating transactions in the observer.

The current opaque debugger owns its own CPU and has no external-board attach
operation. Packaging cannot make the stock `emu-debug` debug a separate
simulator CPU. A later generic embedding/debugger composition contract must
cover that need alongside Gate B. Do not expose private pointers on the wire
or implement a second matcher in the simulator to work around this boundary.

## DES-100 — Reproducible release and verification

Build binary and source RPMs from an identified clean committed source tree.
Use distribution compiler/linker flags, preserve license notices and include
no target ROM or board code. Provide staged installation and an external
consumer link/run check against the installed library and metadata. Run all
accepted local core/debug tests in the package check gate. Verify resulting
payload, dependencies, source identity and installed executable behavior.

Publication uses the existing repository signing and publishing procedure,
preserving unrelated packages and verification settings. Record signed
artifact hashes, destination and a retrieval/install check before declaring
publication complete. Do not execute AlmaLinux binaries on the openSUSE
repository host as a compatibility test.

## Traceability

DES-098..DES-100 realize REQ-017 and refine ARCH-004/ARCH-007..ARCH-010.

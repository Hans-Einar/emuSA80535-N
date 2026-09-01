# EMU-SAB80535-DES-007 — External interrupt edge design

Status: active target design
Design items: DES-064..DES-073
Established: 2026-09-01
Steering authority: Issue #7 SLC-013 checkpoint

## Authority and stop boundary

SLC-013 implements only the external-edge remainder of REQ-012. Primary
behavioral authority is the Siemens *SAB 80515/SAB 80C515 8-Bit
Single-Chip Microcontroller Family User's Manual*, especially the port,
external-interrupt and interrupt-controller chapters. The accepted
DES-002/DES-005 controller and resolved-pin designs remain authoritative.

`doc/P1000/` at current master and Ponsse PR #25 exact evidence HEAD
`6a22d8713b607308c94f02df17d35ddbe8a36d6a` are consumer evidence only. No
P1000 address, connector, board, NEC/D71055, protocol, field-signal or physical
I/O meaning may enter the product implementation.

ADC, Siemens Timer2 behavior, board/peripheral logic and changes to the frozen
`emu-debug` protocol are outside this design.

## DES-064 — Architectural external-line map

The SAB variant maps external interrupt inputs to canonical resolved pins:

| Source | Canonical pin | Request flag | Selection |
|---|---|---|---|
| INT0 | P3.2 | TCON.IE0 | TCON.IT0 level/falling-edge |
| INT1 | P3.3 | TCON.IE1 | TCON.IT1 level/falling-edge |
| INT2 | P1.4 | IRCON.IEX2 | T2CON.I2FR falling/rising |
| INT3 | P1.0 | IRCON.IEX3 | T2CON.I3FR falling/rising |
| INT4 | P1.1 | IRCON.IEX4 | Siemens fixed edge |
| INT5 | P1.2 | IRCON.IEX5 | Siemens fixed edge |
| INT6 | P1.3 | IRCON.IEX6 | Siemens fixed edge |

For I2FR/I3FR, cleared selects falling and set selects rising. The Worker and
Reviewer must independently confirm the fixed INT4..INT6 direction and every
pin/flag mapping against the Siemens manual before acceptance. Alternate
compare/capture and Timer2 behavior is not authorized by this Slice.

Classic 8051/8052 variants keep only their existing INT0/INT1 behavior and do
not acquire SAB extended sources.

## DES-065 — Resolved-pin source of truth

Detection consumes the same resolved level used by ordinary firmware reads.
The SLC-010 latch remains authoritative CPU output state, and external drive
never rewrites it. A latch-low pin cannot be forced high by an external level.
Any relevant latch, external-drive or release change must leave the detector's
sample and the ordinary port-read result coherent.

The public line surface uses generic source/port terminology only. It drives
or releases canonical virtual pin state and never opens a host device.

## DES-066 — Deterministic machine-cycle scheduling

Hosts may schedule bounded external line-level changes at explicit monotonic
machine-cycle timestamps. Scheduled changes are applied from virtual CPU time
while `tick()` advances individual machine cycles; interrupt arbitration stays
at the accepted instruction boundary. Same seed, starting state and schedule
must produce the same flags, vectors and trace.

The API must define handling for past/current timestamps, duplicate timestamps,
queue capacity and reset. It must not use host wall time, threads or oscillator
subphases. Immediate virtual drive/release remains deterministic at the current
machine-cycle boundary and follows the same resolved-pin detector.

## DES-067 — INT0/INT1 edge and level modes

With IT0/IT1 set, a resolved high-to-low transition latches IE0/IE1. A
stationary low does not retrigger until a high level re-arms a later falling
edge. The accepted controller auto-clears the flag only when the edge-triggered
request is accepted.

With IT0/IT1 clear, a resolved low asserts the canonical request and keeps it
architecturally pending while low; a resolved high releases the hardware level
request. Interrupt entry does not convert a still-low level into a cleared
edge. Masking EAL or the source enable gates acceptance, not the underlying
line/request state.

## DES-068 — INT2/INT3 selectable edges

INT2 and INT3 latch IEX2/IEX3 only on the transition selected at observation
time by I2FR/I3FR. Cleared selects high-to-low; set selects low-to-high.
Changing I2FR/I3FR does not synthesize a transition, clear an already latched
request or reinterpret an earlier transition. A subsequent qualifying edge
uses the new selection.

The accepted controller clears IEX2/IEX3 on vector acceptance. Requests
remain latched across EAL/source masking.

## DES-069 — INT4/INT5/INT6 independent edges

INT4, INT5 and INT6 each observe their own canonical P1 pin, latch only their
Siemens-documented fixed qualifying edge into IEX4/IEX5/IEX6, and never alias
one another, INT2/INT3 or Timer2. The accepted controller clears each request
flag on its own vector acceptance.

P1.1 is still an ordinary resolved pin even when a consumer samples it as a
companion phase. Product code must not suppress INT4 or assign direction,
quadrature or board meaning to that pin.

## DES-070 — Existing controller is the only service path

Line detection only updates canonical TCON/IRCON request truth. Eligibility,
four-level priority, fixed equal-level polling, preemption, inhibit timing,
entry stack behavior, vector selection and RETI release remain owned by the
accepted DES-002 controller. No stimulus API may directly set PC, enter a
vector or bypass enable/priority/in-service state.

## DES-071 — Reset and prior sampled state

Reset clears scheduled line work and edge-detector history, releases external
drive through the SLC-010 model, and seeds prior sampled line levels from the
resulting generic released/high resolved pins without emitting an edge. Reset
must not invent target pull networks, active polarity or board defaults.

## DES-072 — Immutable edge trace and replay evidence

A record-only observer receives at least machine cycle, source, old/new
resolved level, trigger classification and resulting canonical request state.
It records qualifying and non-qualifying observed changes sufficiently to
diagnose selection/re-arm behavior. Its signature exposes no mutable CPU
storage and a null or active observer has identical execution results.

Replaying the same scheduled sequence produces byte-for-byte equivalent
normalized edge/request records.

## DES-073 — Consumer fixtures, regression and exclusions

The length fixture drives P3.2/INT0 and independently controls resolved P3.5;
service must enter vector `0003` through the real controller and firmware must
observe both P3.5 levels. The saw fixture drives P1.0/INT3 and independently
controls resolved P1.1; service must enter vector `0053`, and later edges must
follow live I3FR changes. These are generic CPU fixtures derived from consumer
evidence, not embedded P1000 policy.

All accepted Stage0, interrupt, Timer0/Timer1, UART, SLC-010 and `emu-debug`
runtime regressions remain mandatory. SLC-013 adds no ADC, Timer2 behavior,
P1000/NEC/board logic, live GPIO/serial/CAN/USB, physical control or
`emu-debug` command/capability/protocol change.

## Traceability

DES-064..DES-073 refine ARCH-002..ARCH-006 and complete the authorized
external-edge portion of REQ-012 through SPR-007 / ITR-013 / SLC-013.


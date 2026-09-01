# EMU-SAB80535-DES-005 — Port latch/pin and MOVX context design

Status: active target design
Design items: DES-039..DES-049
Established: 2026-09-01
Steering authority: Issue #7 SLC-010

## Authority and stop boundary

SLC-010 implements the port/MOVX substrate portion of REQ-012 only. Authority
is the Siemens SAB80515/80535 port-driver and read-modify-write documentation,
Ponsse PR #25 commit `7891aceb2b713659ea9936b9743a5aee73579ae9`,
accepted prior SDP, and Issue #7.

External INT0..INT6 edge production, ADC, Timer2, P1000 decoding and all live/
physical I/O remain unauthorized.

## DES-039 — Canonical output latch

For SAB ports P1 (90), P3 (B0), P4 (E8) and P5 (F8), the existing SFR byte is
the authoritative CPU output latch. Port writes update that latch through the
central SFR gateway and remain visible to established write callbacks.

Reset keeps the accepted documented-high latch values. External stimulus never
rewrites the latch.

## DES-040 — External drive and resolved pins

Each modeled port owns an external-drive mask and external level byte.
Deterministic quasi-bidirectional resolution is:

```text
resolved = latch & (~external_mask | external_levels)
```

A latch zero forces low. A latch one is released/high unless externally driven
low. External high cannot override a latch zero. Releasing the drive removes
the mask and exposes the latch/released state.

This is a digital behavioral model, not analog strength/contention/pull-up
simulation.

## DES-041 — Ordinary SFR reads

Ordinary SAB reads of P1/P3/P4/P5 return resolved pins. Bit-test and move-from-
bit forms derive their bit from the same resolved byte.

If an established SFR read callback is installed, it remains an architectural
override. The canonical latch/external state is not collapsed or overwritten
by merely reading. Reentrant callback mutations must leave the next latch/pin
query coherent.

Classic variants retain prior behavior.

## DES-042 — RMW latch reads

The documented port RMW set reads the output latch and writes the resulting
whole byte back through the SFR write gateway:

- ANL, ORL and XRL direct-destination forms;
- JBC and CPL bit forms;
- INC, DEC and DJNZ direct forms;
- MOV port-bit,C, CLR port-bit and SETB port-bit.

For modeled SAB ports, these reads do not use resolved pins or a read callback
override. Writes still invoke the established write callback with coherent
latch state. Non-port SFR behavior is unchanged.

## DES-043 — Host-facing virtual port API

A generic API identifies a modeled port by full SFR address and provides:

- drive selected external bits with levels;
- release selected bits;
- get latch byte;
- get resolved pin byte.

Invalid CPU/variant/port inputs fail without mutation. The API contains no
target signal names, device files, USB/GPIO or physical authority.

## DES-044 — Callback compatibility

Write callbacks see the new canonical latch already committed. Ordinary read
callbacks retain their return override. External state remains separately
queryable. If a mutable/reentrant callback changes the latch through the public
SFR gateway, resolved state recomputes from the resulting canonical latch and
the existing external drive.

## DES-045 — Immutable MOVX context

A new record-only MOVX observer receives:

- completed/current virtual machine-cycle convention documented by the API;
- executing PC;
- address;
- direction;
- read/write value;
- full P1 output-latch snapshot.

P1.7:P1.6 may be derived by consumers. The CPU assigns no meaning to them.

## DES-046 — Capture ordering

MOVX captures PC and P1 latch before invoking legacy `xread`/`xwrite`, so a
mutable legacy callback cannot retroactively change the transaction's bank
snapshot. A read record uses the final value returned by callback/backing; a
write record uses the architectural accumulator value.

The immutable observer runs after the legacy access and cannot mutate CPU
state through its signature.

## DES-047 — MOVX form coverage

Both `MOVX A,@DPTR` / `MOVX @DPTR,A` and `MOVX A,@Ri` /
`MOVX @Ri,A` use one context-emission helper. Save/change/access/restore P1
sequences therefore report the latch present at each individual access.

## DES-048 — Backing and legacy callbacks

Existing `xread`/`xwrite` callback signatures and behavior remain available.
Without them, generic XDATA backing remains unchanged. The new context surface
does not decode banks, alias addresses or replace the host's return value.

## DES-049 — Reset and exclusions

Reset releases all external-drive masks/levels deterministically while
preserving reset-high latches. No pin transition produces an interrupt request
in SLC-010. No external-edge queue, GPIO device, ADC, Timer2 or target board
logic is present.

## Traceability

DES-039..DES-049 refine ARCH-002/003/004/005/006 and implement the authorized
SLC-010 portion of REQ-012 through SPR-005 / ITR-010 / SLC-010.


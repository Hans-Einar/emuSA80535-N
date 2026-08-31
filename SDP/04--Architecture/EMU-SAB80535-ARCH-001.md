# EMU-SAB80535-ARCH-001 — Variant-aware emulator architecture

Status: active target architecture  
Architecture items: ARCH-001..ARCH-006  
Established: 2026-08-31

## ARCH-001 — Preserved instruction engine

The upstream decoder/opcode engine remains the execution foundation. Changes
must be narrow, tested and compatible with classic variants unless an explicit
verified correction says otherwise.

## ARCH-002 — Variant descriptor boundary

Variant-owned facts include memory capacity, oscillator defaults, SFR
addresses/reset values and the interrupt/peripheral controller implementation.
Classic `IP=B8` and SAB `IEN1=B8` are never inferred from one shared constant.

## ARCH-003 — CPU-owned deterministic state

The CPU owns CODE, internal RAM, SFRs, PC, virtual time, counters and generic
peripheral state. CODE and XDATA remain separate address spaces. A host may
supply XDATA callbacks, but target decoding is not CPU behavior.

## ARCH-004 — Generic embedding seam

Hosts interact through generic callbacks for XDATA, trace, pin/edge stimulus,
UART frames and analog samples. Callback records contain enough CPU context
(cycle, PC, address/value and P1 latch where relevant) to reproduce behavior
without embedding P1000 knowledge.

## ARCH-005 — Deterministic scheduling

Instructions consume machine cycles. Peripheral events and interrupt
arbitration advance from virtual CPU time at architectural boundaries, never
from host wall time. Repeating a scenario with the same seed and stimuli must
produce the same normalized trace.

## ARCH-006 — Layered diagnostics and stop control

Core execution exposes bounded run operations and typed stop reasons.
Observability is callback-based and read-only; it must not change execution.
Unsupported-resource diagnostics are explicit so bounded failures reveal PC,
time and recent operations.

## Dependency direction

```text
embedding host / board model
        -> public generic CPU API
        -> selected variant + peripherals
        -> preserved instruction engine
```

The CPU repository never depends on Ponsse/P1000 source.

## Traceability

ARCH-001..ARCH-006 realize REQ-001..REQ-015 and are refined by
DES-001..DES-010.


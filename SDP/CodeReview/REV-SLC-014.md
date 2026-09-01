# REV-SLC-014 — Multi-trace routing refinement

Status: approved with review corrections  
Slice: SLC-014  
Scope: documentation and SDP records only

## Review performed

The Reviewer read the active Slice contract, AGENTS.md, the complete detailed
tracepoint specification and DES-064..DES-089. The review checked stable trace
identity, route coalescing, gate timing, nested interrupt filtering, watch
derivation, destination isolation, bounds and lifecycle behavior.

## Findings corrected during review

### REV-SLC-014-F001 — After-gate table ignored the existing enabled state

The worker wording said that `on-after` always excluded and `off-after` always
included the current event. That is false when the trace was already enabled
or disabled. The design now defines current routing from the state remaining
after ordered before-gates; ordered after-gates affect only the next event.
Golden tests must cover all four gate operations from both initial states and
same-phase/cross-phase conflicts.

### REV-SLC-014-F002 — Interrupt boundary and suppression counting needed an
operational definition

The review made outer enter/exit exceptions explicit, distinguished nested
boundaries, and defined suppressed events as otherwise-route-eligible events.
Unrouted and disabled activity does not inflate a suppression summary. Pending
summaries are per trace even when destinations are shared.

### REV-SLC-014-F003 — Derived watch and lifecycle edges were ambiguous

The corrected design specifies a new canonical sequence per `watch.match`,
ascending watch-ID drain order, explicit-route-only delivery, no matching of
ordinary points/gates and no recursion. `quiet` wins over `console` without
cancelling stop or route. Deletion covers tracepoint/gate/watch and destination
references; reset/image-load/clear-session behavior and relevant bounds are
explicit.

### REV-SLC-014-F004 — Raw stdout needed a format boundary

Dedicated raw CLI stdout is now explicitly bounded human-oriented text, never
the `emu-debug` protocol NDJSON stream. NDJSON server mode rejects this
destination.

## Result

Approved with the corrections above. No product or DAP code was changed. The
design now expresses the steering intent without relying on implicit state or
recursive observer behavior.

## Checks

- `git diff --check`: passed;
- no product source was added or modified;
- target/product vocabulary audit of the design delta: passed;
- traceability and required-document presence audit: passed.

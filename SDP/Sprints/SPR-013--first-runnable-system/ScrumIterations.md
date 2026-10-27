# ITR-021 / SLC-021 — Implement CPU contract facade

Status: active

## Worker contract

Read AGENTS and lifecycle mandate/study/requirements/architecture/design,
accepted bundle0.1.0 docs01..04, frozen headers and SPR-012 handoff. Implement
emusa80535_cpu_query_v1 and its full mandatory CPU function table against the
existing core, as a separately buildable generic component. No session, loader,
network, DAP, Ponsse semantics or physical I/O in this Slice.

Exact bundle0.1.0 semantics govern sized/versioned records, ownership/errors,
cycle/delay/IRQ/IDLE/power-down handling, access/output ordering, reset/image
loading, side-effect-free memory inspection and transactional decoder state.
One opaque facade owns one CPU. Reuse existing engine and authoritative decoder;
extract shared private decode/cache helpers from emu_debug.c if necessary rather
than duplicating decoder logic or creating a second CPU. Preserve legacy debug
facade/protocol1.0 and classic variant regressions.

Implement all required entries create/destroy/reset/load_code/apply_input/cycle/
snapshot/peek/decode. Provide synchronous exactly-once bus hook and port-latch
output, including unchanged writes; sequence identities survive reset and never
wrap. Preserve legacy instruction/timer/IRQ ordering and actual access context.
Validate requests before mutation; errors/faults/reentrancy must be explicit.
Handle callback failures and partial instruction state honestly; no substituted
successful bus values, implicit retry, fabricated safe boundary or silent time.
UART frame output is optional but desirable using the accepted producer; never
advertise effective-pin support without complete tested coverage. Virtual input
supports agreed variant coverage, INT0..6, normalized ADC16, UART9 frame and
release operations with due-time/state validation. Power-down remains explicitly
unsupported. Query must expose only implemented and verified capabilities.

Do not modify any frozen interfaces/0.1.0 payload or canonical exported header
in place. New implementation/private helper files may live outside that payload.
If a header/contract change is truly needed, report a concrete proposed versioned
correction to Master before dependent code; do not add ABI-breaking shortcuts.
Availability is recorded in separate implementation evidence, not by rewriting
EMUIF_TARGET_IMPLEMENTED or immutable SDK documentation.

Worker owns new CPU facade/private helpers, necessary bounded core/debug wiring,
build/test changes and implementationNotes. Master owns sprint/index/ledger/
relations/handoff. Tests must cover all API entries, invalid-size/version/output
buffers with no mutation, callbacks once/order, fault/reentrancy, multiple CPU
instances, deterministic reset/load, signed decode range/cache atomicity,
cycle/delay/IDLE wake/power-down, supported stimulus types, optional UART and
legacy regression gates. Use synthetic firmware/data only. Provide an external
C/C++ linked consumer smoke and sanitizer checks where useful. Commit candidate
and report exact evidence and capability limitations. Fresh Reviewer required.

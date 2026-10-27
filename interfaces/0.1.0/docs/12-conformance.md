# Conformance and implementation gates

Bundle checks: compile every target and legacy header as C99 and C++17 from an
isolated include root; validate example messages against strict command schemas;
reject wrong uint64 encoding, unknown keys, incompatible hello and oversized
arrays. Verify checksums, source references, exact archive/copy inventory,
modified/missing/extra files, receipt identity and deterministic export. Run old
CPU, trace, facade and protocol1.0 regressions after canonical include migration.

Implementation acceptance is separate and requires all of:

1. CPU-cycle adapter preserves core ordering, delay/IRQ drain, timer/ADC/UART
   and variant regressions. IDLE advances without instructions; power-down is
   explicitly unsupported. Bus transaction occurs once; observation is neutral.
2. Synthetic native board passes query rejection, create/reset/destroy cleanup,
   timed inputs, port changes, peek noninterference, callback errors and bounded
   queue overflow. No physical I/O. Same tests for static registration and .so.
3. CPU/board pause and reset remain coherent. Same image/config/build/seed and
   admitted input order produce identical state/trace. Input queue identity and
   time never wrap; out-of-order/past input is rejected.
4. Debug service targets the existing CPU. Attach does not reset/load. One
   controller and observers obey generation/lease rules; dropped response and
   reconnect do not replay or kill. Rich trace semantics preserve reference
   cursor/loss identities; only verified capabilities are advertised.
5. Unix/TCP framing/authentication, fragmented/oversized/malformed input, slow
   clients, queue limits and abrupt disconnect are tested. No implicit listener.
6. DAP maps actual runtime state, reference lifetimes, graceful disconnect and
   abrupt loss correctly. Legacy launch1.0 remains separately regression-tested.
7. Signals report safe labeled status and orderly exit; no async-unsafe handler
   work; diagnostic output cannot corrupt protocol framing.

Header/schema examples alone do not satisfy these runtime gates. Release of
integrated packages remains blocked until REQ-024 verification and a delivery
contract. Board-specific whole-machine boot acceptance belongs to its owner.

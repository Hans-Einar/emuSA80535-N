# Native board-module ABI 1.0

The startup-selected `.so` exports `emusa80535_board_query_v1` with C linkage and
default visibility. Host resolves exactly that symbol after loading the absolute,
explicitly selected module path with local symbol scope. No directory scanning,
implicit plugin fallback, hot reload, hot unload or separate board process.

ABI 1.0 supports Linux ELF64 little-endian x86-64, System V AMD64 ABI, standard
C alignment (no packing). Host checks `platform==1`, ABI major==1, minor>=0,
record size/version, capabilities and mandatory non-NULL callbacks. Other
platforms need a separately identified ABI profile; source portability alone is
not binary compatibility. Query writes only declared fields into a caller-owned
zeroed table with size/API prefix initialized. It has no simulation side effects.
A module's constructors may run before query; modules are trusted native code.
Unknown capability bits are ignored unless the host requires them. ID<=63,
version<=31, build_id<=64 characters excluding NUL; module_id/version nonempty,
build_id is its source commit or SHA256 artifact identity. Host separately hashes
the loaded artifact and records that hash in session identity.

Base callbacks including signal_output are mandatory; no-op boundary/output is allowed.
Stimulus and inspection callbacks are optional as described below.
`peek` may be NULL iff CAP_PEEK is absent. Table strings/function pointers are
copied by host, while the library remains loaded for the whole session.

## Lifetime

1. Validate query and configuration before CPU advancement.
2. `create(host,config,&instance)` copies JSON config bytes (0..16384; empty means
   `{}`), variant and oscillator. It copies host callbacks/context if retained.
   On error output instance=NULL and module cleans its partial allocations.
3. Host creates CPU, establishes generation=1, resets CPU and calls board reset;
   restores the validated image and entry. Board reset gets seed and generation,
   clears board RAM/peripherals and diagnostic retention deterministically.
   Reset must not schedule inputs or call host callbacks.
4. At initial boundary 0, and once after each completed cycle, host calls boundary.
5. `destroy(instance)` releases module allocations before CPU/session teardown
   and unloading. Host callbacks/context stay valid until destroy returns.

Configuration bytes and create/reset/boundary/access records are borrowed for
that call. The module owns its instance, never the CPU or host context. No module
callback receives CPU storage. Configuration meaning belongs to the external
board project; generic runtime interprets only length/UTF-8/JSON-object validity.

## Call behavior

`boundary` may update timed board state and call host.schedule_input for the
**next** or a later cycle (strictly greater than the supplied cycle). `access`
performs the transaction exactly once and fills result. `port_output` observes
an actual CPU latch write. Either may schedule input only at cycle+1 or later;
zero-delay feedback is excluded in ABI1.0. This avoids reentrant CPU mutation.
For initial inputs, board stores reset levels and schedules them at boundary0
for cycle1. The session API's initial CPU pin state is the
accepted CPU reset state; module configurations requiring a different cycle0
state are unsupported by this revision and must be rejected during create.

Host.schedule_input copies a valid record into a 1024-element queue. Module
sets order=0; owner assigns nonzero monotonic order. Queue-full returns CAPACITY
without insertion. A callback must propagate failed required scheduling so the
session faults; dropping a required stimulus silently is forbidden. The host
records assigned timestamp/order in replay evidence. Module internal deterministic
state changes need not be separately replayed because they are regenerated.

`peek` only supports XDATA in this revision, at stopped safe boundaries. It must
read existing stored values without bus side effects, trace insertion, clock
advancement or changing diagnostic counts. If a device lacks a valid peek,
return UNSUPPORTED for the entire request. Never implement peek by access(READ).
On error no output bytes are valid. Result.diagnostics is module-local uint32
metadata, not a CPU status or generic machine-control code. Diagnostic trace
saturation must not turn a successful bus access into FAULT; module defines
bounded drain/overwrite/loss policy in its own documentation.

## Runtime/scenario → board stimuli

CAP_STIMULUS requires both validate_stimulus and apply_stimulus; otherwise both
are NULL. A stimulus has generation, future cycle, owner-assigned order, a
module-owned schema ID/version string (<=63 ASCII characters) and <=4096 bytes
of UTF-8 JSON object. A module may reject unknown schema/version with UNSUPPORTED.
The host knows no device names or payload fields. `validate_stimulus` must be
pure: no mutation, tracing, queueing, callbacks or time advancement. Validate
payload syntax/value constraints independent of transient device state.

The host validates identity/time/size/capacity, invokes module validation, then
copies and records the accepted payload and assigned order atomically. All
board and CPU input envelopes share the 1024-entry queue/order allocator.
At its due boundary, call apply_stimulus once in input order; it applies the
module-defined input changes atomically. A failure here faults the session and
is never retried; no partial successful admission is reported. Stale generation,
past/equal timestamp and full queue are rejected before admission/mutation.
Internal module configuration may also define a deterministic scenario driven
by boundary callbacks, but externally admitted inputs always use this channel.
Only callbacks running for a due stimulus may schedule CPU input at cycle+1 or
later; no immediate recursive CPU execution. Reset clears all admitted inputs.

## Read-only inspection and bank context

For XDATA peek, the board uses its **current bank-latch state at the stopped
boundary**, maintained by port_output callbacks and reset defaults. A debugger
cannot select another bank by a hidden port write. A board unable to maintain
that context or produce a side-effect-free value must decline CAP_PEEK.

CAP_INSPECT requires inspect; absent capability requires NULL. Input is a
module-owned schema/version ID plus <=4096-byte JSON object and generation.
Output is <=4096-byte JSON object in caller storage. Capacity must be4096 before
call; smaller returns CAPACITY with required4096 and no mutation. inspect is
pure/non-consuming and never calls access or advances time, including errors.
Module-specific schema documentation defines status fields and trace payloads.
Any trace paging schema must include generation, opaque cursor/nextCursor,
returned count, loss/overflow indication and explicit stale-cursor behavior.
Repeated reads of unchanged state/cursor return identical content. Existing
finite append-only traces may expose sticky saturation; no pretend drain or
reset-on-read is allowed. CPU/debug canonical trace paging remains a separate
contract. Host passes opaque inspection responses without board interpretation.

Wire2.1 may advertise boardInspectFault only together with boardInspect and an
explicit module promise to provide pure bounded post-fault status/last-access
inspection. This permits the inspect callback while nonexecuting and faulted,
even at an unsafe CPU boundary. It does not permit peek, access, input, execution
or replay; all other fault restrictions remain. Board ABI1 layout is unchanged.

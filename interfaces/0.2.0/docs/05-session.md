# Controller session and host startup

`contracts/runtime.h` is the controller's same-process API. One session owns its
CPU, board instance, code copy, generation, input queue and debug state. Creating
a session validates all fields before CPU execution. Config contains variant,
oscillator_hz, seed, entry, exactly 65536 CODE bytes, a UTF-8 JSON object for board
configuration (empty allowed), and queried CPU/board tables. module_sha256 is the host-verified lowercase
64-hex artifact digest plus NUL; reserved_identity is zero. The session computes
CODE and config SHA256 from copied bytes (empty config normalizes to `{}`). The API copies code,
configuration and table values. Caller owns the loaded library and keeps it
resident until session destruction. The controller does not close that library.

Session creation performs the startup/reset ordering in [board](03-board.md),
starts paused at boundary0 with generation1 and a new unpredictable 128-bit
session ID (32 lowercase hex characters). Session IDs are identity, not secrets.
Creation failure returns NULL and unwinds initialized resources in reverse.
The caller must not reuse an ID for a different session.

The host accepts explicit absolute image/module paths, expected SHA256 hashes,
variant, oscillator, seed, entry and board JSON. It reads the image once, enforces
exact size/hash, hashes the selected module/config and records canonical config
bytes before creating the session. Required hash mismatch fails startup, never
runs another file. Paths are host-local, not client-local; remote clients cannot
load arbitrary files in this revision. Firmware image loading belongs to the
host, and board decode/configuration semantics remain external.

`session_pump(cycle_budget)` is valid only when state is running. budget is
1..100000. Execute at most that many cycles plus <=16 safe-drain cycles; return
at a safe boundary unless an execution fault prevents it. Pump returning due to
budget does not pause the session or create a debugger stop. progress.stop_reason
is 0 for no stop, 1 entry, 2 pause, 3 step, 4 breakpoint, 5 watchpoint,
6 exception, 7 cycle-budget, 8 power-down-unsupported, 9 terminated. Faulted
state records reason6 and cannot resume. Snapshot state values are in types.h.
Pump must service queued control requests between bounded turns; host I/O worker
threads only frame/authenticate/enqueue and never call session APIs concurrently.

Reset through the debug facade is valid paused or faulted, requires controller
ownership and matching generation. Clear inputs and pending stops, reset CPU and
board using the original or requested seed/entry, restore the original image,
increment generation and pause at entry. Ring record identities retain the
rules in [debug facade reference](../reference/debug-stable-facade.md); reset is
not ring clear. Any reset failure leaves the session faulted. Generation UINT64_MAX
rejects reset with EXHAUSTED before mutation. Destroy rejects further commands,
destroys board then CPU and frees copied inputs/code/config. Host closes sockets
and unloads module only after destruction.

Wire2.1 may advertise boardInspectFault only together with boardInspect and an
explicit module promise to provide pure bounded post-fault status/last-access
inspection. This permits the inspect callback while nonexecuting and faulted,
even at an unsafe CPU boundary. It does not permit peek, access, input, execution
or replay; all other fault restrictions remain. Board ABI1 layout is unchanged.

# Compatibility and native rules

Bundle 0.3.0 freezes control wire **3.0**, observation extension **1**, and trace
facade **1**. CPU API1, board ABI1.0 and all existing layouts remain byte-for-byte
unchanged. `version.h` macros still mean compatibility profile 0.2.0/wire2.1;
`target_version.h` uses distinct `EMUIF_TARGET_*` macros and zero implementation
macros. Header presence is not capability discovery. A host optionally resolves
`emusa80535_cpu_observation_query_v1`, `emusa80535_trace_query_v1`, and
`emusa80535_control_query_v3`; missing symbols mean unavailable. Returned sized
tables contain only nonnull functions on success. Query validates the caller's
prefix before writing. No fallback cast of opaque CPU storage is legal.

All new records including nested records/array elements use the existing prefix:
uint32 struct_size, uint16 api_version=1, uint16 flags=0. The minimum size is
`sizeof` the named v1 type on the native platform. Too small => INVALID, version
mismatch => VERSION, before mutation. Larger stride/size is accepted; unknown
trailing bytes are neither read nor overwritten. Padding is not compared,
serialized or initialized by the callee. Reserved fields and unused fixed-array
slots must be zero; unused nested prefixes remain initialized. Caller output
pointers/capacities/element sizes are inputs and are preserved. CAPACITY reports
required count without mutation; page metadata remains valid on CAPACITY, and
no partial elements are written. Native booleans are uint32 0/1 except the
explicit uint8 hardware fields; tags/statuses are fixed-width macros, never C
enums. Native enum fields with no named macro use the exact numeric mappings
in header comments and schemas. Wire booleans map to native 0/1. Strings are
NUL-terminated UTF-8, zero after terminator; bounds exclude NUL and are bytes.

The owner serializes every API at safe boundaries. Callback exceptions, native
crashes and hangs are host failures. CPU observer context/callback are retained
until unbind, CPU destroy, or replacement binding; binding requires a quiescent
safe CPU and nonzero generation, and replacement while bound returns BUSY.
Unbind is idempotent. Callback records are borrowed for the callback only; copy
bounded facts to a queue, never retain pointers. No allocation, I/O, reentry,
execution, reset or mutation from callbacks. Observers return void: observation
failure must not invent an architectural CPU fault. Debug/control facades retain
no caller buffers or callbacks. Trace config is deep-copied atomically.

All IDs/counters are uint64 unless explicitly uint32. IDs are nonzero and never
wrap. Exhaustion rejects allocation or stops execution at a safe boundary with
EXHAUSTED before reuse. Saturation is permitted only cumulative loss counters.
Session ID is 32 lowercase hexadecimal characters. Native char[33] and wire
string carry identical spelling, no binary reinterpretation. Each session has
its own generation >=1; reset/load increments it. Source sequence, instruction,
access, context, frame and stimulus IDs never reset within a session. CPU-local
observations become session canonical events through one sequencer; session
must preserve correlation while assigning source sequence. Generation is never
truncated to existing internal uint32 debug records: implementation must widen
or adapt that internal seam before advertising coverage.

The native trace table supplies replace/page/outcomes/capture-start/sink-open.
All other commands, including configuration listing, clear, capture stop/status
and sink flush/close/status, use the callback-free control3 `request` JSON bridge.
There are deliberately no promised C symbols for those convenience operations.
Native trace functions are trusted owner operations; the control3 projection
performs session/generation/lease checks before calling them. CPU API1 and board
ABI1 hooks do not gain observation callbacks or new signal kinds.

Baseline 0.2 export/install receipts retain original source pin
0a77c2d0da22ecb0bd37781b430e30444f779e39 and original payload; they do not claim
current runtime source. New0.3 receipts identify the exact reviewed clean commit.
`make install` installs both doc bundles (interface-bundles/0.2.0 and /0.3.0)
alongside headers; runtime --version and manifest hash still identify0.2.0.
Frozen baseline snapshots are checked against the old manifest before use.

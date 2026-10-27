# Roles and common rules

One host owns one session, one CPU, one board instance and one virtual timeline.
The CPU library has no loader, sockets, UI or OS signals. The controller owns
scheduling, image identity, common reset and stops. The host owns files, `dlopen`,
endpoint authentication and diagnostics. Board callbacks implement external
hardware only. A debug service operates on that existing session and is the only
projection of the emulator-owned matching/trace semantics.

A debugger process can be a CLI, or the DAP adapter can include a debugger-client
library directly. There is no mandatory third debugger process between DAP and
the host. If a debugger process itself implements a DAP server, the IDE uses the
same standard DAP interface in [08-dap](08-dap.md). We define no additional
private debugger-to-DAP protocol. CPU and board never communicate over IPC.

## Native records

All target declarations are under `emusa80535/contracts/`. Each public record
starts with uint32 struct_size, uint16 api_version, uint16 flags. API 1 requires
flags=0, reserved fields=0 and size >= sizeof(the declared version-1 type).
Callers zero-initialize records and set those three fields; sizes include native
padding. Receivers read only the version-1 prefix, ignore larger trailing input,
and never overwrite trailing output. On invalid version/size return VERSION or
INVALID without mutation. Output fields are undefined on failure unless a
specific operation specifies metadata; caller-owned storage is never freed.
Do not pack these structures or serialize native bytes. Stable status and tag
values are uint32 macros, not C enum or bool storage. Counts use explicit widths.

Strings in fixed arrays are NUL-terminated UTF-8, no embedded NUL, zero-filled
after terminator. IDs/version strings use ASCII letters/digits plus `._-`.
All counters are unsigned and never wrap; exhaustion faults the session before
identity reuse. Statistics may saturate only where documented. Zero is invalid
for connection IDs, access/output sequences, assigned input order and generation.
Cycle and instruction counters start at zero. API handles belong to one owner
thread and must never be used concurrently. Native callbacks cannot reenter API
execution, reset, mutation or debugger calls, allocate unbounded work, or perform
physical I/O. They return within bounded simulated work; native crashes/hangs
remain host failures, not recoverable sandbox errors.

## Status values

The numeric codes in `types.h` are shared by CPU/board/session facades:
OK(0), INVALID(1), VERSION(2), STATE(3), RANGE(4), CAPACITY(5), UNSUPPORTED(6),
BUSY(7), STALE(8), DENIED(9), FAULT(10), EXHAUSTED(11), IO(12), HASH(13),
NOT_FOUND(14). Validation failures do not mutate state. Device transaction
failure can already have side effects: enter faulted state, preserve diagnostic
context, never retry or fall back to RAM. No rollback is promised for device
faults, allocation failure during initialization or execution exceptions.

Every native input/output pointer is borrowed for the call unless a specific
CPU/board callback lifecycle explicitly permits retention. The callback-free
debug facade always copies accepted data and retains no caller pointer.

# SLC-021 implementation notes

Worker candidate, 2026-10-27. Implements the CPU slice authorized from `df3d273`,
with the checked-execution/bundle0.2 amendment (`73e02b6`) and optional post-fault
board inspection contract amendment (`314130f`). Acceptance remains the Master's
responsibility after independent review and verification.

## Delivered behavior

- `emu_cpu_facade.c` implements the entire mandatory CPU API1 table around one
  owned original `em8051` engine. It owns code, deterministic reset state, copied
  callbacks and bounded decoder history. It does not use the legacy debugger as
  a second execution engine. No process global mutable state, loader, socket,
  board-specific logic or physical I/O was introduced.
- MOVX accesses invoke one board callback. Port writes and completed SAB mode3
  UART frames share a nonwrapping sequence allocator with MOVX. Reset retains
  access/output identity, clears faults and decode history, and requires code
  reload; it does not replay partial work. Callback errors and malformed results
  stop the native instruction/peripheral path immediately. Accepted pre-fault
  effects remain visible; no rollback or completed cycle is fabricated.
- `emu_decode_internal.c/.h` extracts the existing authoritative decoder and
  predecessor algorithm for both facades. Decode validates/stages output and
  history before committing; errors leave both unchanged. Private-header edits
  rebuild all dependent SDK objects and debugger binaries.
- The optional pure legacy `execution_abort` predicate/context is NULL by default
  and retained by reset. The same-build CPU struct grows; consumers must rebuild.
  Instruction helpers check after bus/port callbacks and exceptions, including
  XCH, DJNZ, JBC and RETI. UART completion stops before starting a pending next
  frame or advancing later peripheral phases. Run/step/run-until return the new
  appended `EM8051_STOP_ABORTED` when a pending delay cannot drain, without hangs.
  Existing enum values and NULL-hook execution behavior are preserved.
- Snapshot labels faults unsafe even with native delay zero. Safe peek accesses
  modeled stored CODE/IRAM/SFR only, with no callbacks; XDATA remains board-owned.
  Typed input validation preserves the native SAB port/external IRQ/ADC/UART
  producer semantics. IDLE advances time and can wake on virtual interrupt;
  power-down returns UNSUPPORTED without invented progression.

## Clarifications approved during implementation

The root approved these bounded clarifications for the new, not-yet-frozen0.2
bundle; API1/boardABI1 layouts and entry points remain unchanged:

1. Query capability PORT_LATCH applies to all three variants. UART_FRAME is the
   available SAB80535 mode3 logical 9-bit frame profile only; the session must
   reject a board requiring it for a classic variant. Effective-pin coverage is
   not advertised. This does not promise unsupported serial modes.
2. Input order is an admission identity/tie-breaker, not chronological delivery
   order. CPU comparison is per current cycle. It tracks maximum observed order
   and captures that maximum as a floor at reset. Later cycles may carry lower
   orders above the floor; same-cycle descending/repeated orders are stale.
   The controller owns global uniqueness, generation, queue clearing and its
   monotonic admission allocator. The bounded CPU check cannot detect unseen or
   cross-cycle reused identities above its reset floor. A test admits order2 at
   cycle50 then order1 at cycle100 and proves reset retains floor2, not last1.
3. Target wire2.1 adds optional `boardInspectFault`, dependent on `boardInspect`,
   for pure bounded module inspection in a nonexecuting faulted session, even
   when the CPU boundary is unsafe. It grants no peek/decode/execution/replay.
   Session/debug/board/wire docs, schema, examples, version header and fixture
   validator agree. No wire/session/loader implementation is claimed here.

## Version and build disposition

Canonical `lib/include` and distribution tools now select bundle0.2.0, native
API1, boardABI1.0 and target wire2.1. `EMUIF_CPU_IMPLEMENTED=1` while
`EMUIF_TARGET_IMPLEMENTED=0`. Manifest availability identifies only implemented
CPU functions; runtime/debug attach remain targets. Historical vendored references
retain their provenance and hashes.

`interfaces/0.1.0` is byte-identical to accepted source
`7d924cc27e48c599d5775904543070d92e413c26`. Its accepted archive still has SHA256
`4771155cf60148a3cd0bc6eb31c26b13bee913f3507c9fc174706e06f4c407bb`.
The old exported header/tool payload must be retrieved from that immutable
archive/revision, not reconstructed from current canonical headers.

`make cpu-sdk` builds `libemusa80535_cpu.a` without debugger or curses. The existing
`libemusa80535.a` also exports API1. Install includes both archives, canonical
headers and the selected bundle. The narrowly authorized packaging consistency
fix adds the CPU archive to `%files devel`; existing `%check core-test` now includes
the CPU suite and the staged install checker links the CPU-only archive. No new
RPM build requirements were needed; no RPM was built or published in this slice.

## Worker verification

All completed successfully on the candidate worktree:

- `make cpu-facade-test`: sized records, tiny prefixes, invalid versions/flags,
  tail preservation, unaligned decode stride, callbacks and reentrancy including
  destroy, all variant port profiles, all MOVX forms, multi-effect callback and
  exception faults, RETI partial-state preservation, reset recovery/identities,
  independent CPUs, decode/predecessor behavior and output atomicity, inspection,
  typed virtual input, same-cycle and out-of-order admission tests, IDLE interrupt
  entry timing, power-down, UART completion/fault and pending-frame stop, sequence
  and counter exhaustion, bounded legacy run abort.
- `make core-test debug-test PYTHON=python3`: existing eight CPU stage suites,
  event/trace/runtime suites, legacy debug facade and process protocol regressions.
- `make cpu-link-test`: isolated C99/C++17 consumers compile and link with current
  and immutable0.1 CPU/board headers; sizes/offsets match. An actual `.so` compiled
  against0.1 board headers is loaded by a current-header test host, which queries
  its table and exercises a callback across the ABI boundary. This fixture is not
  the runtime module loader or a claim that a real external board is integrated.
- CPU suite compiled with `-fsanitize=address,undefined -fno-omit-frame-pointer`
  and run successfully. Normal test compilation uses C99 and `-Werror`.
- `make interface-check`: isolated headers, wire2.1 positive/negative fixtures,
  boardInspectFault dependency, references and local links (including exported
  root README), reproducible export, archive/copy integrity and optimized-Python
  negative integrity checks.
- Staged `make install DESTDIR=/tmp/slc021-install prefix=/usr libdir=/usr/lib64`
  followed by `packaging/check_install.py`: installed-only legacy/CPU libraries,
  executable protocol1.0 and source identity pass. This is a temporary staging
  validation, not a distributed release artifact.
- `git diff --exit-code 7d924cc27e48c599d5775904543070d92e413c26 -- interfaces/0.1.0`
  and the historical archive SHA256 check pass.

Independent reviewer evidence and the exact committed candidate/export identities
belong in the Master's review/verification records. Subsequent slices must build
session ownership/scheduling, module loading and the local/script/debug host on
this facade before claiming a runnable composed system.

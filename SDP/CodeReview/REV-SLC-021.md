# REV-SLC-021 — CPU API1 and checked execution

Date: 2026-10-27
Reviewer: fresh independent `cpu_facade_reviewer`
Disposition: **APPROVED** for SLC-021
Reviewed candidate: `0a77c2d0da22ecb0bd37781b430e30444f779e39`
Product baseline: `82cdfb2`
Bundle: `0.2.0`; native API1; board ABI1.0; target wire2.1
Manifest SHA-256: `7be6be79dac99753c051b9b1a257e0774a14bf1115d70553e1371a513e583e72`

## Scope and conclusion

Reviewed the active contract and amendments, applicable lifecycle documents,
frozen0.1 contracts, complete candidate diff and implementation evidence. The
facade implements every mandatory CPU API1 operation over one owned CPU. The
original engine and authoritative decoder remain shared with the legacy debugger;
there is no second CPU or independent decoder implementation in the new facade.

The optional pure execution-abort predicate propagates callback failures through
instruction and peripheral phases without substituted successful reads, later
instruction effects or fabricated cycle completion. Pre-fault effects remain;
snapshot explicitly reports an unsafe fault even when native delay is zero.
Reset is recovery. NULL-hook legacy behavior and existing enum values remain
covered by the regression gates.

Sized/versioned records, reserved fields, output tails, callback reentrancy,
identity exhaustion, reset/load, side-effect-free inspection, transactional
decode history, IDLE/IRQ entry and unsupported power-down match the contract.
The CPU-only archive builds independently and is installed alongside the existing
SDK. No remaining blocking findings were identified.

## Findings resolved before approval

| Finding | Correction verified in candidate | Status |
|---|---|---|
| REV-SLC-021-F001 | Checking abort only outside an opcode permitted post-callback ACC/PC and other effects. MOVX and all port-write opcode paths now check immediately, including XCH, DJNZ, JBC and bit operations. Peripheral checks stop UART completion before pending-frame start and subsequent phases. | Resolved |
| REV-SLC-021-F002 | Legacy run control could loop forever draining an aborted pending delay. Run/step/run-until now return appended STOP_ABORTED without attempting further progress. | Resolved |
| REV-SLC-021-F003 | Illegal opcode, ACC-to-A and RETI exception paths continued effects after the facade faulted. Immediate checks preserve the fault-point state; independent A5 reproduction now retains PC0 and zero completed cycles/instructions. | Resolved |
| REV-SLC-021-F004 | A global last-delivered input-order check rejected valid future admission permutations, as identified by Master. Comparison is now per delivery cycle, with maximum observed identity retained as the reset floor. Controller retains global identity/generation/queue authority. | Resolved |
| REV-SLC-021-F005 | Private decoder header edits did not invalidate every dependent build target; new library/test cleanup and RPM inventory also needed consistency updates. Dependencies, cleanup/ignore, default CPU test coverage and installed archive inventory now match. | Resolved |
| REV-SLC-021-F006 | New bundle root documentation linked to an old-version directory absent from an export, and fault-inspection wording needed consistent exceptions across channels. Exported-copy links now resolve; board/session/debug/wire docs consistently define the optional pure post-fault inspection contract. | Resolved |

The approved UART clarification is explicit: PORT_LATCH covers all three CPU
variants; UART_FRAME describes the SAB80535 mode3 logical nine-bit producer.
Future session creation must reject a board requiring UART_FRAME with a classic
variant. Effective-pin output is not advertised. The new input-order rule allows
order2 at cycle50 followed by order1 at cycle100; reset retains floor2. It does
not pretend a bounded CPU adapter can enforce global duplicate detection.

## Independent verification

All checks below passed against the committed candidate:

- `make interface-check`, including isolated C99/C++17 headers, wire2.1 fixtures,
  boardInspectFault dependency, manifest/reference/link integrity and negative
  archive/copy cases.
- `python3 tests/test_cpu_link.py`: current and immutable0.1 C99/C++17 consumers
  link to the CPU archive with matching API/board sizes and offsets. An actual
  old-header shared module is queried and called through current board headers.
- A clean `git archive` of the exact candidate, with SOURCE_COMMIT, built
  `cpu-sdk` and `emu-debug`; `make -C tests test` and `make debug-test` passed.
  This includes the CPU facade suite, all eight core stage suites, event/trace/
  runtime suites and legacy debugger facade/process tests. Staged installation
  and `packaging/check_install.py` passed using installed libraries/headers only.
- The final facade suite passed ASan and UBSan. Coverage includes oversized and
  unaligned decode output strides, output tails and invalid prefixes, callbacks
  once/in order, reentrant reset/cycle/destroy, malformed bus results, fault/reset,
  independent CPUs, all variant port profiles, IDLE IRQ wake, UART pending-frame
  fault, RETI exceptions and input order/reset-floor behavior.
- An independent raw-core sweep tried all256 opcode slots;32 bus/port-callback
  cases fired, with zero observed post-fault PC/ACC/SFR/IRAM/counter mutations.
  The independent A5 facade fault case also executed successfully when compiled
  against headers extracted from the immutable0.1 export and linked to the new
  CPU-only library.
- Clean-source export produced archive SHA-256
  `5abd71f52925c8fcf299b8b8ed417279a1bc20f2a5f004425bc083f87e9cf91d`.
  Its extracted copy contains no0.1 directory or sibling project. The exported
  bundle verifier and full contract checker both passed from that copy.
- `interfaces/0.1.0` is byte-identical to accepted `7d924cc`; its existing export
  still hashes to `4771155cf60148a3cd0bc6eb31c26b13bee913f3507c9fc174706e06f4c407bb`.
  Native API1/boardABI1 records and symbols retain their layouts. Only the legacy
  same-build CPU struct grows, explicitly requiring native users to rebuild.

Clean-source verification log and temporary artifacts are under
`/tmp/slc021-review-final-ch_wb7mx/`. These are review evidence, not published
release artifacts. The full-range whitespace check reports only the preserved
extra EOF newline in `interfaces/0.2.0/reference/dap-LICENSE`, copied byte-for-byte
from its licensed reference. No blanket whitespace pass is claimed.

## Acceptance boundary

Approval covers CPU API1, shared decode extraction, checked execution, the new
immutable contract payload and associated build/verification changes. The
availability markers truthfully expose CPU implementation while leaving the
overall target-runtime marker false. Wire2.1 boardInspectFault is contract-only:
it grants no CPU peek/decode/execution/mutation after a fault and requires an
explicit module promise of pure bounded inspection.

Session scheduling/ownership, runtime module loading, local/script/socket host,
debugger attach, DAP attach and complete P1000 execution remain subsequent work.
This review neither accepts those implementations nor authorizes RPM publication
or SDP migration. Master owns Slice acceptance, traceability and consumer handoff.

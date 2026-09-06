# REV-SLC-014 — Deterministic MYMOS ADC review

Status: approved
Reviewed Slice: SLC-014
Reviewed product/test HEAD: `0bd39132b2eaffbfc5190e223b54743f17fc68fa`
Reviewed tree: `d6e6ee7f93d545f61f3b33abe08070cc0e0964c2`
Reviewed parent: `8906274ff1a5096d1403b3021569bbae37920e97`
Reviewer: fresh independent reviewer
Reviewed: 2026-09-02

## Disposition

Approved with no findings. No `REV-SLC-014-Fxxx` IDs were opened. The exact
product/test commit satisfies REQ-013, DES-074..DES-085 and all 28 Issue #13
verification classes.

## Independent Siemens reconciliation

Primary authority was the Siemens *SAB 80515/SAB 80C515 User's Manual*,
Edition 08.95, independently downloaded with SHA-256
`860C88C8E180E6D0B415B5BC5BE77563C7EEBF08F6A3F9737C4D7889A4921B29`.
Pages 33 and 72..81 were rendered and visually inspected.

- Page 33: ADCON reset `00X0 0000B`; DAPR/ADDAT reset `00H`.
- Pages 72..75: eight channels, 15 MYMOS machine cycles, ADCON layout,
  read-only BSY, IADC, AN0..AN7, ADDAT persistence and every-write DAPR start.
- Pages 76..78: lower/upper DAPR nibbles, zero external endpoints, 1/16 ladder,
  documented ranges and minimum-span/clipping semantics.
- Page 80: DAPR restarts active conversion; conversion and BSY begin on the
  next machine cycle.
- Page 81: result transfer occurs in the final cycle. BSY/IADC anticipation is
  qualitative and no numbered MYMOS offsets are provided.

DES-078's atomic cycle-15 ADDAT/BSY/IADC architectural boundary is approved.
Issue #13 forbids early ADDAT/IADC completion and defines successful completion
as the result/BSY/IADC update. Atomic exposure is a conservative documented
abstraction rather than an invented subphase. Any future early-anticipation
requirement needs new timing authority.

## Implementation audit

- Normalized AN0..AN7 API, reset, invalid-input handling and classic isolation
  are correct.
- Direct and opcode-driven DAPR writes start at the next progression as cycle 1
  and complete exactly at cycle 15.
- Busy restart and mutable/direct/nested callback cases latch final canonical
  state once and produce no ghost completion.
- ADCON byte/bit writes protect hardware BSY while preserving MX/ADM/reserved/
  BD/CLK; UART interaction remains green.
- ADDAT changes only for valid completion. Existing IADC survives starts/
  restarts and remains software-clear across vector entry/RETI.
- Every DAPR byte starts. The 91 valid reference pairs convert; 165 unsupported
  pairs preserve ADDAT while completing timing/BSY/IADC diagnostically.
- Integer arithmetic is overflow-safe and matches an independent uint64 oracle.
- ADM=1 records the request and performs only the explicit conversion without
  continuous auto-chaining.
- The accepted controller remains the only EADC/EAL/priority/preemption/
  vector/in-service/RETI path.
- Multi-cycle, interrupt entry/ISR, IDLE, run/step/breakpoint and trace/replay
  behavior is deterministic and observer-neutral.
- The generic ROM-style fixture uses literal SFR instructions and real vector
  `0043`, not a direct PC hook.

## Independent evidence

- Windows GCC 12.2: all seven core suites and debug facade/process pass.
- Windows Clang 18.1.8 strict C99/`-Werror`/`-pedantic`: all seven core suites
  and debug facade/process pass.
- openSUSE WSL GCC 7.5: all seven core suites and debug facade/process pass.
- WSL strict ASan+UBSan: all seven core suites, debug suites and independent
  reviewer probe pass without report.
- Independent probe checked 5,963,941 reference/input states, all 256 DAPR
  bytes, opcode timing, cycle 14/15, nested callbacks, restart, BSY and reset.
- Frozen DAP exact HEAD `36639b48ddb2ffbafa14c00da794fe1734f7483b`
  passed build, 45/45 contract, fixture/hash and real contract/equivalence/F5
  smoke on Windows.

A WSL attempt to reuse the Windows-managed DAP worktree stopped before tests
because its `.git` file used a Windows path. No repository was changed to
bypass this environment-only limitation.

## Identity, traceability and scope

The product diff is exactly `README.md`, `core.c`, `emu8051.h`,
`tests/Makefile` and `tests/test_stage3_adc.c`. Later commits through
`932499d073d174d9031bde31e2545a6072aa134d` are SDP-only and all product/test
blobs match the reviewed commit.

`opcodes.c`, frozen `emu-debug` modules/tests and the DAP integration test are
unchanged. No Timer2 producer, capture/compare, P1000/Ponsse/NEC/D71055/board/
calibration policy, full continuous chaining, live/physical I/O, machine
control, protocol change or direct PC hook was added.

## First remaining blocker

Master must complete VER-SLC-014 against exact product HEAD and integrate
acceptance before opening the focused unmerged PR and Ponsse handoffs.

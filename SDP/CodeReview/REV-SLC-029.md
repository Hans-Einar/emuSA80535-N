# REV-SLC-029 — MOV direct,@Ri operand correction

Date: 2026-10-28
Reviewer: fresh independent `indirect_mov_reviewer`
Disposition: **APPROVED** for bounded SLC-029
Reviewed final candidate: `d247e7ff82b7b3890da4154d720c94bdb86af64f`
Initial product candidate: `4e74190aed7d76af4483a4e63fe47a018df53e7a`
Activation baseline: `66f3cbbafc5853cf1dbe8a7daf57a8e95c2964af`

## Scope and decision

Read AGENTS, CurrentIndex, the active IndirectMovFixSlice, Sprint and handoff,
and applicable mandate, CPU/runtime study, requirements, architecture and design.
Reviewed both exact candidates in an independent detached worktree. The
Reviewer changed no product code. No blocking findings remain.

The two-line handler correction captures the active bank's R0/R1 indirect
address as the source and the encoded direct operand as the destination. The
source byte is read before any destination write, including destinations that
overwrite Ri or PSW. Existing indirect RAM selection, direct SFR gateway,
callback/abort checks and timing return remain intact. Both the opcode table
and alternate switch dispatcher call this same corrected handler. A6/A7 and
all other handlers are unchanged.

The [Philips/Handshake 80C51 manual, page 113](https://www.keil.com/dd/docs/datashts/handshake/ht80c51_um.pdf)
independently confirms the 86/87 encoding, indirect source, direct destination,
two-byte length and two machine cycles. Source review also checked the decoder,
SFR gateway, parity update and public step/run timing boundaries.

The product/test diff touches only `opcodes.c` and `tests/test_stage0.c`.
The final successor adds the user's short synthetic programs and this review;
its production source is byte-identical to the initial product candidate.
Public headers,
immutable 0.1/0.2/0.3 bundles, decoder, licensing and upstream ancestry are
unchanged. No external firmware, application address, board behavior or physical
I/O enters this correction. Approval establishes this instruction correction;
it does not establish whole-firmware behavior or deferred SLC-026 capabilities.

## Finding and reproduction

**REV-SLC-029-F001 — inherited operand reversal, resolved.** An independently
written tiny probe executed both opcodes on the activation baseline with Ri
pointing at RAM 62, RAM[62]=A9 and direct destination RAM[35]=17. Both executions
left destination 17 and incorrectly changed source to 17, while still reporting
PC=2, one instruction and two cycles. The probe exited 1. On the exact candidate,
both destination and preserved source are A9 with the same correct timing; the
same probe exits 0.

Separately compiled the committed new Stage-0 tests against baseline source:
they report 1984 failed assertions for the initial matrix and 2020 after adding
the requested short programs. This independently reproduces the Worker's
negative results and demonstrates that the regressions distinguish the defect.

## Independent verification

Checks used pinned detached revisions in `/tmp/slc029-review-candidate`,
without modifying tracked files. The full suite ran on the initial product
candidate; Stage-0 and its sanitizer build were rerun on the final candidate
after reviewing the test-only successor and confirming unchanged product code:

- `make core-test control3-test runtime-session-test runtime-control-test
  runtime-host-test debug-test cpu-link-test PYTHON=python3` passed, exit 0.
  This covers CPU facade and Stage 0 through Stage 4, debug event/trace/runtime,
  wire3 native/C++ and 141 schema-checked responses, wire2.1 session responses,
  real Unix controls, script1 host behavior, legacy debugger process behavior,
  and current/immutable old-header C99/C++17 linking and module loading.
- The committed 480-case matrix passes both opcodes, four banks, three variants
  and both dispatch paths. It checks lower/upper RAM, direct RAM/SFR, distinct
  address spaces sharing a numeric address, self-alias, destination Ri/PSW
  aliases, source/pointer preservation, absent-upper 8051 convention, unchanged
  flags except intentional PSW writes, exactly one SFR write with no SFR reads,
  PC, instruction/cycle counters and two timer ticks.
- The final candidate additionally passes the user's exact zero-padded 64 KiB
  programs with prefixes `78307530a575205a8620e52080fe` and
  `79307530a575205a8720e52080fe`. All 12 variant/opcode/dispatcher combinations
  complete five instructions in eight cycles at PC 000C, with A, destination
  and preserved source equal to A5. The old handler instead leaves all three
  at 5A. The terminating loop remains unexecuted under the five-instruction bound.
- Callback abort cases preserve the applied destination write, leave PC and
  instruction/cycle counters unchanged, perform no timer ticks, and retain
  pre-parity-update PSW state. These match the existing partial-effect contract.
- Fresh GCC AddressSanitizer plus UndefinedBehaviorSanitizer Stage-0 build with
  `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer` passed with
  `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
  `UBSAN_OPTIONS=halt_on_error=1`; no diagnostics.
- A separate temporary probe passed 144 cases spanning variants, banks, both
  opcodes and dispatchers with PC=FFFF and the direct operand at CODE 0000.
  It additionally verifies ACC destination/parity and absent-upper behavior.
- A separate reverse-direction probe passed 80 A6/A7 cases covering all banks,
  variants and dispatchers, direct B source and lower/upper indirect destination.
- `git diff --check 66f3cbb d247e7` passed.

Transient logs are `/tmp/slc029-review-suite.log`,
`/tmp/slc029-review-sanitizer.log`, `/tmp/slc029-review-sanitizer-final.log`,
`/tmp/slc029-review-old-matrix.log` and
`/tmp/slc029-review-old-matrix-final.log`. Independent probe sources are
`/tmp/slc029-review-old/review_mov_probe.c`, `/tmp/slc029-review-extra.c` and
`/tmp/slc029-review-reverse.c`. Master retains exact-source artifact/install,
verification, traceability and acceptance duties.

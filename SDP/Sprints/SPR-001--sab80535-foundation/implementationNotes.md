# Implementation notes

Status: active; verified implementation has not yet been recorded.

## Baseline

- Upstream/fork baseline: `5dc681275151c4a5d7b85ec9ff4ceb1b25abd5a8`.
- Branch: `codex/sab80535-foundation`.
- Full curses frontend build requires a development header absent from both
  available build environments. No source change has been made to hide that
  result.
- The unmodified `core.c`, `opcodes.c` and `disasm.c` units compile with
  `-std=c11 -Wall -Wextra -Wno-unused-parameter -Wshadow` on both Windows GCC
  and openSUSE WSL `cc`.

## Verified work

- Worker commit `1cedf941d09779686e142511ff21b5b75de27f16`
  introduced the Stage 0 foundation and passed Windows/WSL/sanitizer core
  tests when independently rerun by REV-SLC-001.
- The same independent review reproduced eight findings; the implementation is
  not accepted and must be corrected by SLC-002.
- Structurally accepted observations from the review: explicit A8/A9/B8/B9
  variant separation, CPU-owned SAB upper IRAM, direct/indirect separation,
  normal exact-size raw loading and generic no-target boundary.
- Corrective commit `bbd63878c8ea4a2479e531509b3644e661eb875d`
  resolved the cycle boundary, vector stop, invalid SFR access, reset wording,
  trace-field evidence and null-loader findings. REV-SLC-002 verified those
  corrections but left two high findings for trace reachability and stack
  reads; Stage 0 is still not accepted.
- Final corrective commit `dbcc6b74adbc34896a71401366991229c0d58922`
  replaced the shallow CPU trace view with a record-only callback and closed
  classic missing-upper stack read/control-flow behavior.
- REV-SLC-003 approved exact HEAD with no findings. VER-SLC-003 independently
  passed Windows GCC, strict Clang, WSL GCC, WSL ASan/UBSan, three product diff
  checks, no-target audit, NDJSON parsing and ancestry checks.
- Stage 0 is accepted. Implemented behavior is limited to the explicit variant
  foundation, 256-byte SAB indirect IRAM, exact raw loader, deterministic
  counters/run control/reset, bounded diagnostics/trace and tests. Stage 1
  peripherals remain unimplemented.
- Branch `codex/sab80535-foundation` was pushed and opened as PR #1:
  `https://github.com/Hans-Einar/emuSA80535-N/pull/1`.

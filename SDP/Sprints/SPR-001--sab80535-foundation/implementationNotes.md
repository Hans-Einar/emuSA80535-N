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

None yet. Add only evidence that the Master has rerun or directly inspected.

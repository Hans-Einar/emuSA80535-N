# REV-SLC-015 — Standalone debug event/watch runtime

Date: 2026-09-02  
Reviewer role: fresh independent SDP Reviewer  
Disposition: **approved with review corrections**

## Scope reviewed

- the SLC-015 contract and DES-064..DES-089;
- `emu_debug_event.h/.c` and `tests/test_debug_event.c`;
- build integration and the complete product-code diff;
- C99 portability, bounded behavior, signed comparisons, unknown values,
  observer neutrality and isolation from CPU/peripheral work.

## Findings and corrections

### REV-SLC-015-F001 — Signed comparison was implementation-dependent

Severity: medium. Status: corrected.

The original signed comparator converted out-of-range `uint64_t` values to
`int64_t`. C99 makes that conversion implementation-defined, including the
important 64-bit negative range. The comparator now orders equal-width
two's-complement bit patterns by sign bit and unsigned order, without an
out-of-range signed conversion. Signed constant validation was changed to
check canonical sign extension using unsigned operations. A 64-bit
`INT64_MIN < -1` regression case was added.

### REV-SLC-015-F002 — One observer could alter another observer's event

Severity: medium. Status: corrected.

The public callback type is `const`, but a callback can cast the qualifier
away because the dispatch object's underlying storage was mutable. All later
observers then saw the mutation. Dispatch now gives every observer a fresh
copy of the same canonical event. A hostile-observer regression test proves
that the following observer and assigned sequence remain unchanged.

This correction preserves observer neutrality without exposing CPU state or
changing producer behavior.

### REV-SLC-015-F003 — Derived watch events could re-enter the matcher

Severity: medium. Status: corrected.

DES-084 forbids a `watch.match` from recursively producing another
`watch.match`. The standalone matcher now returns no matches for that event
kind, and a regression test covers the boundary. The later router therefore
has a defensive guarantee in addition to its own non-recursive queue rule.

## API and boundary assessment

- The API is additive C99 and does not include or modify CPU-core structures.
- Capacity is compile-time bounded: 8 observers, 64 watches, 8 actions per
  watch and 16 deduplicated trace routes per result.
- Replacement validates and sorts into temporary storage before changing the
  live table; invalid and duplicate configurations are atomic failures.
- Registration mutation and nested dispatch are rejected while dispatching.
- Width and signedness must match explicitly. Every comparison, including
  `ne`, evaluates false for an unknown operand.
- Stop actions coalesce; quiet suppresses console but does not suppress stop
  or trace routing. Results retain ascending watch-ID and trace-ID order.
- The product diff does not touch `core.c`, `opcodes.c`, peripherals,
  `emu_debug`, DAP, instruction timing or architectural state.

## Independent evidence

Commands run from the worktree root:

```text
make debug-event-test
make core-test
valgrind --quiet --error-exitcode=99 --leak-check=full ./tests/debug_event_tests
gcc -std=c99 -pedantic -Wall -Wextra -Wshadow -Wconversion \
    -Wsign-conversion -Werror -I. tests/test_debug_event.c \
    emu_debug_event.c -o /tmp/em8051-debug-review
/tmp/em8051-debug-review
git diff --check
```

Results:

- focused debug event/watch suite: passed;
- Stage-0, IRQ, timer, UART and port/MOVX regression suites: passed;
- Valgrind full leak check: passed with no reported errors;
- strict C99 pedantic/conversion build and run: passed;
- whitespace/error check: passed;
- Clang is not installed on this host, so independent Clang verification is
  deferred to CI or another environment.

## Disposition rationale

The three review findings were corrected in the Slice worktree and covered by
focused regression tests. No unresolved blocker remains within SLC-015. The
runtime is suitable as the standalone debugger-owned foundation; CPU producer
instrumentation, derived-event scheduling, interrupt routing, trace gates and
sinks remain correctly deferred to later slices.

# REV-SLC-006 — Independent Timer0/Timer1 review

Status: approved
Reviewed Slice: SLC-006
Reviewed product HEAD: `30cf42efa845d29a47a950eca7bbaf657490fbe6`
Reviewed parent: `b2d3491dab1d156eb8d57657d7d90d62515025ec`
Reviewer: fresh independent reviewer
Reviewed: 2026-08-31

## Disposition

Approved with no findings. No REV-SLC-006-F IDs were opened.

## Independent timing evidence

- Timer0 DCEF stayed below overflow through 8976 eligible cycles and wrapped/
  emitted/set TF0 at completed cycle 8977.
- Live opcode TL0/TH0 writes, FFFF wrap, controller 000B clearing and counting
  through entry passed.
- Synthetic ISR events occurred at 8977, then 8985/8985/8985 gaps and an 8986
  gap after the every-fourth extra instruction, preserving software latency.
- Timer1 FD events occurred at 3/6/9/12 while TF1 stayed sticky and interrupts
  were disabled; 001B auto-clear did not stop continued counting.
- Live TH1 changed only the next reload. 3,000,003 cycles produced exactly
  1,000,001 events.
- Overflow records use completed-cycle numbering and post-wrap state; counters
  reset, remain monotonic and are observer-neutral.
- IDLE advances eligible timers; GATE/C/T ineligible configurations do not.
- Classic 8051/8052 event delivery occurs before the preserved serial shortcut
  clears TF1.

## Gates

Stage0, IRQ and timer suites passed on Windows GCC/strict Clang, openSUSE WSL
GCC normal/strict and WSL ASan/UBSan. Product diff, ancestry, NDJSON,
no-target/no-UART/no-wall-time audits passed. Event emission exists only for
Timer0 mode1 and Timer1 mode2.

## Scope boundary

No UART frame/divider behavior, target policy or physical/live I/O was added.
REQ-011 remains unauthorized.


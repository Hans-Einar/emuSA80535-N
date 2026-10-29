# VER-SLC-033 — Execution evidence and recovery documentation

Status: passed and independently approved through REV-SLC-033.
Authority: SPR-014 / ITR-033 / SLC-033 ProbeDocumentationSlice.md.
Baseline Master acceptance/activation: 78b5445.

This change corrects the live README's claim that every retired instruction has
complete captured bytes/disassembly and adds runtime/RESEARCH.md. It documents
existing inspection, invocation, synthetic-entry, UART-value and recovery limits.
No product code, frozen interface artifact, consumer source/pin or process changes.

Source/contract checks performed:

- Frozen interfaces/0.3.0/docs/02-observation.md preserves only actually fetched
  bytes, independent value-known flags, RX acceptance versus drop, source order
  and distinct admission/delivery/acceptance facts. `obs_bytes`/`obs_uart_event`
  in emu_cpu_facade.c match this; `em8051_decode_captured` in disasm.c leaves
  incomplete text empty. `cjne_a_imm_offset` in opcodes.c omits OPERAND2 on its
  untaken path. The correction changes documentation, not decoding semantics.
- CPU API1 table in lib/include/emusa80535/contracts/cpu.h has no write-state or
  checkpoint operations. `reset_cpu` resets before assigning entry. Snapshot,
  memory and decode implementations in emu_cpu_facade.c and emu_session.c confirm
  selected-bank state, paused/current-generation reads and optional board peek.
- Frozen control/trace docs and runtime README confirm exclusive lease,
  reconnect/no-replay, explicit profile selection, actual CALL binding for B,
  root RET retirement, retained A/history and incomplete scope outcomes.
- emu_session.c dispatch permits reset from paused/faulted state after ownership
  checks; reset_session performs queue cancellation, CPU/board reset, generation
  transition and paused stop. It does not implement external host-queue recovery.
  Existing VER-SLC-030 and independently approved VER-SLC-032 supply actual
  behavioral evidence; this slice claims no new runtime tests.
- Relative Markdown links and diff whitespace checked. Owned-file diff is limited
  to runtime/README.md, runtime/RESEARCH.md and this verification record.

No runtime, test process, lab, emulator or external guest was started or attached.
Documentation was checked against source and retained accepted evidence; no
performance improvement, full checkpoint support or natural execution of a
synthetic entry is claimed. Fresh review432f5188c61840f6c53c6705f121ff4bf1f8e493 approved exact
Worker c6a2a8707403a11800091ba2aba644d4a43896ca. Master verified relative links,
whitespace and audit/documentation-only changed paths; SLC-033 accepted.

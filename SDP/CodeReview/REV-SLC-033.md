# REV-SLC-033 — Execution evidence and recovery documentation

Status: **APPROVED** for the bounded documentation-only SLC-033.
Reviewed candidate: `c6a2a8707403a11800091ba2aba644d4a43896ca`.
Baseline: `78b5445` (SLC-032 acceptance / SLC-033 activation).
Reviewer: fresh `probe_docs_reviewer_033`, separate from Documentation Worker.
Date: 2026-10-29. Authority: SPR-014 / ITR-033 / ProbeDocumentationSlice.md.

## Decision

The README correction and new research guide accurately describe the accepted
runtime and frozen contracts. No blocking finding remains. This approval covers
documentation, not a new runtime capability, performance improvement or public
state-mutation API. Master acceptance and traceability integration remain separate.

Read AGENTS, CurrentIndex, the active Slice, Sprint/Handoff and relevant mandate,
study, requirements, architecture and design before review. Independently checked
the exact three-file candidate diff: runtime/README.md, runtime/RESEARCH.md and
VER-SLC-033.md. No implementation, public header, frozen bundle, consumer source,
pin or existing process changed.

## Independent source and contract checks

- Captured instruction evidence: frozen0.3 observation contract requires actual
  fetches only. `opcodes.c:cjne_a_imm_offset` does not fetch OPERAND2 on the untaken
  path; `emu_cpu_facade.c:obs_bytes` retains the captured prefix and
  `disasm.c:em8051_decode_captured` leaves text empty when that prefix is short.
  The guide correctly separates begin, retirement and abort, and permits static
  image enrichment only as separately labeled evidence.
- State inspection: CPU API1's table has no general PC/register/RAM-write or
  checkpoint operation. `reset_cpu` performs cold reset before assigning entry.
  `snapshot_cpu` exposes the selected register bank and scalar state, not hidden
  peripherals. Session memory/decode dispatch requires paused state; generation
  checks precede dispatch. SFR support and optional board pure-peek boundaries
  agree with the guide. Coherent multi-read state is not called a full checkpoint.
- Invocation semantics: frozen0.3 trace contract requires actual observed CALL
  context for B, successful root RET retirement and typed IRQ/RETI tracking.
  PC/SP coincidence and synthetic entry do not establish natural reachability.
  The guide preserves those distinctions, bounded loss reporting and the absence
  of full ACCESS coverage. Synthetic bootstrap guidance does not promise a
  supported warm-state mutation or checkpoint facility.
- UART values: `obs_uart_event` marks RX_ACCEPT19's newValue known16, whereas
  current RX_DROP20 retains canonical unknown value0/width8/unsigned. The guide
  correctly treats UART-state known bits independently from value-known flags
  and ordinary fact statusOK independently from input.failed STATE/BUSY.
  Its validation guidance does not weaken generic unknown-value or per-kind
  correlation checks and does not infer a missing rejected raw response.
- Recovery: session reset dispatch accepts paused/faulted states after identity,
  generation and controller checks. Successful reset preserves session identity
  and CODE breakpoints, advances generation and returns paused; reset does not
  acquire a lease or replay the old admitted queue. Failure/exhaustion and
  externally owned queue/history limits remain explicit. Accepted030/032 supply
  behavioral evidence; the guide does not claim every reset must succeed.

## Documentation verification and limits

Independently checked all seven relative Markdown links across the three changed
documents; all resolve. `git diff --check 78b5445 c6a2a8707403a11800091ba2aba644d4a43896ca`
passes. Exact candidate diff contains only the three authorized Markdown files;
the source/header/interface diff is empty.

No runtime, test executable, emulator, lab or external guest was launched or
attached for this review. Source/contract review and link/diff checks are sufficient
for this low-impact documentation correction. Prior030/032 evidence is cited as
prior evidence, not presented as newly executed tests. No new archive or consumer
pin is needed for this Slice.

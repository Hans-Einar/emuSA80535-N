# ITR-029 / SLC-029 — Correct MOV direct,@Ri operand direction

Status: active urgent CPU correction; SLC-026 remains planned
Authority: explicit coordinating user handoff2026-10-28 reports a blocking CPU
instruction defect and requests fresh bounded fix/review before consumer UART
integration. Baseline accepted SLC-025 source45b92ab; Master acceptance5e52f36.

Independent Master source inspection confirms mov_mem_indir_rx currently reads
indirectly from the encoded direct operand and writes directly to the value in
Ri. Opcode86/87 decoding identifies MOV direct,@R0/@R1, which instead copies the
indirect RAM byte selected by the active bank's Ri to the encoded direct address.
The related A6/A7 handlers implement the opposite direction and must not change.

Primary ISA reference: [Philips/Handshake 80C51 instruction definitions, page113]
(https://www.keil.com/dd/docs/datashts/handshake/ht80c51_um.pdf): opcode1000011i,
two-byte instruction, two classic machine cycles, indirect source and direct
destination. Existing decoder and dispatch tables independently agree on the
encoding. Preserve this repository's established timing and exception behavior.

Fresh Worker owns only the minimal opcodes.c correction and meaningful generic
regression coverage (existing stage0 or dedicated instruction tests plus necessary
build metadata). No ROM patches, application addresses, board behaviors, UART
policy change, interface/header/schema modification or tracing implementation.
The Worker must demonstrate failure on the uncorrected implementation first.
Capture the active-bank R0/R1 pointer/source value before the destination write;
retain correct SFR write path and callback/abort behavior, PC+2, one instruction
and two completed machine cycles. Preserve classic variants and dispatcher paths.

Coverage: both opcodes, all four register banks; lower indirect RAM and upper
IRAM on8052/SAB80535; direct lower RAM and direct SFR destinations; direct SFR
write callback exactly once without source SFR read; source/destination same
numeric address but distinct spaces; exact self-alias and destination aliasing
Ri/PSW where relevant; unaffected source/pointer locations; PC/cycle/flags and
side-effect/abort semantics. Use synthetic discriminating byte values, not the
external firmware. Exercise table and alternate switch dispatcher if supported.

A separate fresh Reviewer reproduces the old failure, inspects the small diff,
and runs focused matrix plus appropriate CPU/core/control/legacy regressions
and sanitizer coverage. Master owns review/verification/traceability/artifact
handoff. Export a new exact runtime source archive preserving all accepted pins;
notify coordinator that this supersedes the control-only candidate for UART
integration while V0 remains historical evidence. Then resume plannedSLC-026.

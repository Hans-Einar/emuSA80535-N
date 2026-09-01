# P1000 target evidence reference

This directory mirrors only the **processor-relevant conclusions** from the P1000 reverse-engineering work in `Hans-Einar/ponsse`. It is a target-evidence reference for emulator development and regression fixtures; it is **not** authority to put P1000 board policy into the generic SAB80535 emulator.

## Source snapshot

Primary source at this sync:

- repository: `Hans-Einar/ponsse`
- PR: `#25 — P1000: reconstruct 386AM firmware and I/O architecture`
- branch: `codex/p1000-386am-reconstruction`
- reviewed snapshot HEAD used here: `6a22d8713b607308c94f02df17d35ddbe8a36d6a`
- reference ROM: `p1000/MAI386AM-19.03.2004.bin`
- ROM SHA-256: `d1b5702e931330269c75ff8f08c32e9f107bb01c5ee89d852eda4bf14bfa073e`

Important source documents include:

- `doc/P1000_ORIGINAL_ARCHITECTURE.md`
- `doc/P1000_OUTPUT_DRIVER_MAP.md`
- `doc/P1000_DIRECT_OUTPUT_CODE_CORRELATION.md`
- `doc/P1000_INPUT_MAPPING_WORKSHEET.md`
- `doc/P1000_EXTERNAL_CONNECTOR_MAP.md`
- `doc/P1000_NEC_IO_MEASUREMENT_MAP.md`
- `doc/P1000_NEC3_KEYPAD_INTERFACE.md`
- `doc/P1000_DIAMETER_ADC_MAP.md`
- `p1000/disassemble/emulation/SAB80535_INTERRUPTS_TIMERS.md`
- `p1000/disassemble/emulation/SAB80535_EMULATOR_REQUIREMENTS.md`
- `p1000/disassemble/emulation/EMU8051_SAB80535_REQUIREMENTS.md`
- generated SFR access ledger and reachability artifacts under `p1000/disassemble/generated/`

## Files in this directory

- `P1000_CPU_TARGET_REFERENCE.md` — clock, timing, interrupt, port, ADC, Timer2 and external-bus facts that matter when validating the CPU/peripheral emulator against 386AM.
- `P1000_SIMULATOR_BOUNDARY.md` — ownership contract between the generic processor emulator and the P1000 board/peripheral simulator.

## Evidence rule

A P1000-specific fact may be used to create a fixture or acceptance test in this repository, but generic emulator behavior must still be justified by Siemens/SAB80535 architecture and the repository SDP. Examples:

- `P5.4` being physically connected to rear-knife extra pressure is useful target evidence, but the CPU implementation must still call it `P5.4`/Timer2 output rather than a valve.
- `P1.7:P1.6` plus `A15` selecting NEC devices belongs to the P1000 board decoder, not to the SAB80535 core.
- AN0/P6.0 being the 386AM diameter channel is a target fixture fact; ADC conversion timing and register semantics are CPU-peripheral behavior.

When PR #25 or later hardware work changes a processor-relevant conclusion, update this directory with the exact new Ponsse commit and preserve the evidence grade (`confirmed`, `probable`, `hypothesis`, `unknown`).

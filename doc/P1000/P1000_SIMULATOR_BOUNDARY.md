# P1000 processor / peripheral simulator boundary

Status: architecture guidance synchronized with `Hans-Einar/ponsse` PR #25 source HEAD `6a22d8713b607308c94f02df17d35ddbe8a36d6a` and current `emuSA80535-N` master.

The goal is to keep the processor emulator generic while allowing the P1000 simulator to reproduce the real 386AM board environment deterministically.

## 1. Ownership rule

### `emuSA80535-N` owns CPU-internal architecture

The processor repository owns behavior that is part of the Siemens SAB80535/80C535 device itself:

- instruction execution and memory-space rules;
- CODE / IRAM / SFR semantics;
- interrupt controller and RETI/in-service state;
- Timer0 and Timer1;
- mode-3 UART and its internal timing;
- P1/P3/P4/P5 latches, resolved pins and read-modify-write semantics;
- generic external interrupt line/edge semantics (REQ-012 remainder);
- ADC registers, conversion timing and CPU interrupt behavior (REQ-013);
- Siemens Timer2, TF2 and P5.4 alternate-function behavior (REQ-014);
- deterministic execution, trace/debug facade and `emu-debug` protocol.

The CPU may expose generic callbacks/injection APIs, but it must not know connector names, valve functions, NEC device identities, P1000 bank meaning or protocol policy.

### `Hans-Einar/ponsse` owns the P1000 board/peripheral model

The P1000 simulator should own everything external to the CPU package:

- P1000 XDATA RAM topology and address decode;
- the measured 74HC138 decode driven by `{A15, P1.6, P1.7}`;
- physical/logical NEC μPD71055 devices (NEC1, NEC2, NEC3);
- NEC1 output latches and NEC2 input images;
- NEC3 keypad/palette handshake wiring and interrupt-producing board path;
- connector/PCB3/C1/C2/C3 routing;
- ULN and later MOSFET/power-stage models;
- head sensors and switch models;
- quadrature signal generation;
- diameter sensor voltage/scaling model;
- recorded serial/panel/sensor capture replay;
- machine-domain names and confidence grades;
- safe simulator defaults and output observation.

No board model should bypass the CPU by directly changing ROM counters or internal firmware variables.

## 2. Current external bus contract

The current CPU-side MOVX observer/context provides the facts the P1000 decoder needs:

```text
machine cycle
executing PC
16-bit XDATA address
read/write direction
transaction value
P1 output-latch snapshot
```

For P1000, the board model derives:

```text
A15 = address bit 15
bank = P1.7:P1.6
```

Measured selections currently include:

```text
bank 01 + A15=0 -> 74HC138 /Y2 -> NEC1 output PPI
bank 01 + A15=1 -> 74HC138 /Y3 -> NEC2 input PPI
bank 10 + A15=0 -> predicted /Y4 -> NEC3 operator interface
```

The P1000 simulator should implement those selections. `emuSA80535-N` should continue to treat the address and P1 latch only as generic CPU facts.

## 3. Pin and interrupt integration

The P1000 simulator generates virtual physical signals; the processor emulator resolves and samples CPU pins.

Examples:

### Length encoder

```text
P1000 sensor model
  -> two virtual phases
  -> CPU P3.2/INT0 + P3.5
  -> SAB interrupt controller
  -> real 386AM ISR 8676
```

The board simulator supplies phase timing/polarity; the CPU owns interrupt edge/level semantics, pending state, masking and ISR timing.

### Saw quadrature

```text
P1000 saw-disc model
  -> two virtual phases
  -> CPU P1.0/INT3 + P1.1
  -> optionally same physical pair reflected into NEC2 PA6/PA7
  -> real 386AM ISR 917D
```

The board model may fan one simulated physical source out to multiple electrical endpoints when the measured board topology establishes that fan-out. The CPU must still see only its own generic pins.

### NEC3 / INT6

NEC3 mode/handshake behavior belongs to the P1000 simulator. If the board handshake asserts the CPU's INT6 line, it should do so through the generic external-line API and normal interrupt arbitration.

## 4. ADC integration

Split the model at the CPU analog pin:

```text
P1000 diameter sensor / potentiometer / conditioning
  -> target-specific voltage or normalized sample
  -> CPU AN0/P6.0 injection
  -> SAB ADC conversion timing/register behavior
  -> ADDAT/IADC/vector 0043
```

`emuSA80535-N` owns DAPR start semantics, 15-machine-cycle conversion timing, ADCON/ADDAT/BSY/IADC and arbitration.

The P1000 simulator owns:

- diameter geometry/sensor model;
- excitation/reference assumptions;
- engineering-unit to voltage/sample conversion;
- connector routing and calibration.

## 5. Timer2 / P5.4 integration

Timer2 is CPU-internal and stays in `emuSA80535-N`.

```text
386AM Timer2 configuration
 -> SAB Timer2 counting / overflow / ISR
 -> P5.4 latch/pin transition
 -> P1000 board observes P5.4
 -> ULN/MOSFET/load simulator
```

The target mapping `P5.4 -> PCB3-35 -> C3-19 -> rear knives extra pressure` belongs in Ponsse simulator configuration/tests, not in the CPU source.

## 6. Direct output integration

The board simulator should observe CPU port latch/output transitions and map them to board nets. Current physically closed P5 routes are P5.7..P5.0 to PCB3 32..39.

Likewise, NEC1 is an external PPI. Firmware writes XDATA; the CPU invokes the board callback; the P1000 decoder selects NEC1; NEC1 state then drives the modeled board outputs.

This distinction is important:

```text
CPU P5 write -> CPU port peripheral -> board observer
MOVX write   -> CPU bus transaction -> board decoder -> NEC1 -> board outputs
```

Do not merge both paths into a single synthetic firmware output image inside the CPU.

## 7. Recommended simulator executable architecture

The preferred runtime is a P1000-specific executable in the `ponsse` repository that links the generic CPU core as a dependency:

```text
VS Code / emuSA80535-DAP
        |
        | emu-debug 1.x stdio
        v
p1000-sim-debug  (ponsse repo)
        |
        +-- emuSA80535-N core/debug facade
        +-- P1000 bus decoder
        +-- XDATA RAM
        +-- NEC1 / NEC2 / NEC3
        +-- panel/head sensor models
        +-- capture/replay
```

A non-debug `p1000-sim` executable can use the same composition without the stdio debug-server layer.

The DAP should remain a frontend, not the owner of the board model.

## 8. Dependency strategy

### Processor emulator

`emuSA80535-N` is a real build/runtime dependency of the P1000 simulator. Pin an exact reviewed commit.

Good options, in preference order for this project:

1. Git submodule under a simulator-specific external/dependency directory;
2. a bootstrap script plus a lock/manifest file containing repository URL and exact commit;
3. build-system fetch of an exact commit.

The key requirement is reproducibility: a simulator verification must identify the exact CPU commit it used.

### DAP repository

`emuSA80535-DAP` is normally **not** a compile-time dependency of the peripheral simulator. It is a development/debug frontend.

Recommended arrangement:

- pin an expected DAP commit in an integration-test manifest;
- allow a bootstrap/workspace script to clone/check out that exact revision as a sibling or optional external directory;
- run DAP real-runtime contract/smoke tests against `p1000-sim-debug` when that combined runtime is ready.

A DAP submodule is acceptable for a one-command developer workspace, but it should remain optional and should not be required to build the simulator engine.

## 9. Branching strategy in `ponsse`

Because PR #25 is the current evidence-rich reconstruction baseline, a new peripheral-simulator branch should start from its current branch rather than from stale `main`:

```text
base: codex/p1000-386am-reconstruction
new:  codex/p1000-peripheral-simulator
```

Treat the simulator PR as a **stacked PR on PR #25** while reconstruction remains unmerged. This gives the simulator worker direct access to the current ROM evidence, mapping documents and generated artifacts without modifying the reconstruction branch itself.

When PR #25 is merged, retarget/rebase the simulator PR onto `main` through an explicit integration step.

Do not let the simulator branch rewrite evidence documents merely to fit its implementation. If implementation reveals a contradiction, report it back to the reconstruction/Steering thread and correct the evidence separately.

## 10. Initial peripheral-simulator milestones

A practical first sequence is:

1. deterministic P1000 board shell + pinned CPU dependency;
2. XDATA RAM and 74HC138 bank decoder;
3. NEC1 output PPI with current confirmed PA/PB routing;
4. NEC2 input PPI with current confirmed PA/PB routing;
5. physical-signal abstraction feeding direct CPU pins and NEC2 where appropriate;
6. NEC3 operator/keypad PPI/handshake after chip-select/address wiring is sufficiently confirmed;
7. capture/replay harness using the real 386AM image;
8. combined `p1000-sim-debug` runtime compatible with the generic DAP protocol.

ADC sensor scaling, final output power polarity and unresolved connector semantics should remain configurable/unknown until evidence closes them.

## 11. Safety/default behavior

The simulator is in-memory/offline by default. It must not open physical serial, GPIO, CAN, USB I/O or machine-control endpoints as a side effect of normal execution.

Target outputs are state observations only. Unknown or unresolved field polarity must stay explicit rather than being guessed from source bits or ULN inversion.

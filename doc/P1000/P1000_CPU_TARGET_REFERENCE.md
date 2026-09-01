# P1000 / 386AM CPU target reference

Status: processor-facing target evidence synchronized from `Hans-Einar/ponsse` PR #25 branch `codex/p1000-386am-reconstruction` at source HEAD `6a22d8713b607308c94f02df17d35ddbe8a36d6a`.

This document records P1000 facts that are useful for validating the generic SAB80535 emulator. It does not authorize P1000-specific behavior in the CPU implementation.

## 1. Target identity and timing baseline

Reference firmware:

```text
MAI386AM-19.03.2004.bin
size       = 65536 bytes
SHA-256    = d1b5702e931330269c75ff8f08c32e9f107bb01c5ee89d852eda4bf14bfa073e
oscillator = 11.0592 MHz
```

At standard 12-clock 8051 timing:

```text
machine-cycle rate = 11,059,200 / 12 = 921,600 cycles/s
machine-cycle time = 1.085069444... us
```

The oscillator value is consistent with the recovered Timer1/UART setup and is the working P1000 hardware value. Preserve it as a target configuration, not as a universal SAB80535 constant.

## 2. Recovered startup SFR state

386AM startup writes the following processor state around ROM `A96B..A998`:

| SFR | Value | Target meaning |
|---|---:|---|
| IEN0 `A8` | `36` | initial interrupt enables, EA still clear |
| IEN1 `B8` | `35` | ADC / extended-interrupt enables |
| TCON `88` | `55` | INT0/INT1 edge mode; T0/T1 running |
| TMOD `89` | `21` | Timer0 mode1; Timer1 mode2; internal clocks |
| SCON `98` | `D0` | mode-3 9-bit UART; receiver enabled |
| IP0 `A9` | `00` | priority bank low bits |
| IP1 `B9` | `10` | UART/INT5 pair at level 2 in the recovered setup |
| IRCON `C0` | `00` | extended request flags initially clear |
| T2CON `C8` | `60` | I2FR/I3FR set; Timer2 otherwise not running yet |
| CCEN `C1` | `00` | capture/compare disabled initially |
| ADCON `D8` | `00` | AN0/P6.0, single-conversion configuration used by 386AM |
| TL0 / TH0 | `EF / DC` | Timer0 live count `DCEF` |
| TH1 | `FD` | Timer1 mode2 reload |
| PCON `87` | `80` | SMOD=1 |
| P5 | `FB` | target reset/startup latch state; do not infer valve polarity from bit values |

Later initialization changes individual interrupt enables. Emulator tests should exercise live enable changes and pending requests rather than freezing the startup mask.

## 3. Timer0 scheduler timing

Timer0 is standard mode 1 and is software-reloaded to `DCEF`.

```text
counts to overflow = 65536 - 0xDCEF = 8977
period to overflow = 8977 / 921600 = 9.7406684 ms
nominal frequency  = 102.662... Hz
```

The observed overflow-to-overflow interval is longer than 8977 cycles because the timer continues while interrupt entry and ISR instructions execute before the two software reload writes. The emulator must retain that natural latency; a host-side fixed-period scheduler is not equivalent.

Relevant target path:

```text
vector 000B -> B7E3 -> 8A1C
TL0=EF / TH0=DC reload at 8A2A..8A2F
```

## 4. Timer1 and UART timing

Timer1 is mode 2 with `TH1=FD`. Each overflow occurs every three eligible machine cycles. With `SMOD=1`, the mode-3 UART divides the Timer1 overflow stream by 16:

```text
Timer1 overflow rate = 921600 / 3 = 307200 /s
UART bit rate        = 307200 / 16 = 19200 bit/s
UART bit time        = 48 machine cycles
```

The 386AM serial configuration is therefore an exact deterministic 19200-baud target fixture. Timer1 overflow events must remain distinct from sticky TF1 state.

## 5. Interrupt and direct-pin target paths

### INT0 / length quadrature — confirmed topology

The firmware interrupt phase is `P3.2 / INT0`; ISR body `8676` samples `P3.5` for direction.

Current physical evidence:

| Signal | CPU signal | PLCC-68 pin | External route | Evidence |
|---|---|---:|---|---|
| length interrupt phase | P3.2 / INT0 | 23 | C3-13 / PCB3-30 or C3-14 / PCB3-29 | CPU topology + field pair confirmed; A/B order unresolved |
| length direction phase | P3.5 | 26 | other member of C3-13/14 pair | CPU topology + field pair confirmed; A/B order unresolved |

Do not model this as a direct counter update. External pin state, interrupt request latching, EA masking, ISR latency and resolved P3.5 sampling are architectural behavior.

### Saw quadrature — function now confirmed

Two inductive sensors observing an offset perforated disc form the saw movement quadrature pair. The firmware path is:

```text
P1.0 / INT3 -> vector 0053 -> body 917D
P1.1        -> sampled for direction
XDATA D02F  -> increment/decrement target index/state
```

Physical evidence:

| CPU signal | PLCC-68 pin | External route | Evidence |
|---|---:|---|---|
| P1.0 / INT3 | 36 | one of C3-26 / PCB3-13 and C3-28 / PCB3-14 | function/pair confirmed; phase order unresolved |
| P1.1 | 35 | other member of C3-26/28 | function/pair confirmed; phase order unresolved |

The same external pair is continuity-confirmed to NEC2 PA6/PA7 as well. The P1000 board simulator should therefore be able to fan one physical simulated signal pair out to both direct CPU pins and the NEC2 input image when the final electrical route is treated as confirmed.

`T2CON.I3FR` is read/rewritten by the INT3 path and must retain Siemens edge-selection semantics.

### Other extended interrupts

- INT2 (`004B`, body `8DD6`) is a separate Siemens source and must not be aliased to Timer2.
- INT4 (`005B`) and INT5 (`0063`) set a shared work flag in the recovered ROM.
- INT6 (`006B`, body `922D`) participates in the external NEC3/operator-interface path; it must enter through ordinary interrupt arbitration rather than direct PC manipulation.

Physical functions for every extended line are not all closed. Preserve independent generic lines and evidence grades.

## 6. ADC / diameter target

The 386AM ADC channel is now confirmed from firmware configuration:

```text
ADCON = 00
channel = AN0 / P6.0
PLCC-68 pin = 20
```

Every reached DAPR write starts a conversion (`A989`, `9DE7`, `9DF2`). Normal conversion completion is 15 machine cycles and flows through:

```text
DAPR write
 -> AN0/P6.0 conversion
 -> ADDAT
 -> IADC
 -> vector 0043
 -> ISR 8DB7..8DD5
 -> XDATA 7329 raw sample
```

P1000 connector candidates are C3-10/15/16 (PCB3-28/26/27). Exact voltage/reference scaling remains unresolved. The generic CPU should model ADC register/timing semantics and accept an injectable analog/sample value; the P1000 board simulator owns sensor voltage/scaling.

## 7. Siemens Timer2 target path

386AM uses the Siemens Timer2, not generic 8052 Timer2.

Recovered operating mode:

```text
T2I1 = 0
T2PS = 0
T2I0 = 1
clock = fosc/12 = 921600 ticks/s
hardware reload disabled for the reached mode
software reload in ISR
```

Initial/fallback value `0x5555` overflows after:

```text
0x10000 - 0x5555 = 0xAAAB = 43691 ticks
43691 / 921600 = about 47.407 ms
```

ISR `8D5F..8DB6` reloads from either dynamic XDATA pairs or `5555`, manipulates TF2/ET2/T2I0 and changes `P5.4`.

New physical mapping closes the target endpoint:

```text
P5.4 -> PLCC pin 63 -> ULN -> PCB3-35 -> C3-19
wiring-sheet function: rear knives extra pressure
```

The two dynamic reload pairs provide separate phase durations. Their exact engineering-time derivation is still under reverse engineering. In `emuSA80535-N`, this remains generic Timer2/P5.4 behavior; the valve/function name belongs only to target fixtures and the Ponsse board simulator.

## 8. Direct CPU P5 output mapping

The CPU-to-board continuity for all P5 bits is now closed. This table is target evidence only; the CPU must expose P5 bits without embedding these names.

| P5 bit | PLCC pin | PCB3 | External | Wiring-sheet function |
|---:|---:|---:|---|---|
| 7 | 60 | 32 | C3-22 | Urea valve |
| 6 | 61 | 33 | C3-21 | Color 2 red |
| 5 | 62 | 34 | C3-20 | Color 1 blue |
| 4 | 63 | 35 | C3-19 | Rear knives extra pressure / Timer2 path |
| 3 | 64 | 36 | C3-27 | Front knives extra pressure |
| 2 | 65 | 37 | C3-11 | Sensor supply/common |
| 1 | 66 | 38 | C2-6 | Auxiliary output |
| 0 | 67 | 39 | C3-17 | Tilt up |

The known ULN stage is inverting/sinking. Final downstream MOSFET/field active polarity is **not confirmed**. Tests must not equate a P5 latch `1` or `0` with a valve being energized.

## 9. P1000 external-bus decode relevant to CPU integration

Measured decoder inputs:

```text
74HC138 A = A15 = CPU P2.7, PLCC pin 48
74HC138 B = P1.6, PLCC pin 30
74HC138 C = P1.7, PLCC pin 29
```

Confirmed selections:

| P1.7:P1.6 | A15 | Decoder | Logical device/window | Status |
|---|---:|---|---|---|
| `01` | 0 | `/Y2` | physical NEC1, low XDATA `0000..0003`, output PPI | electrically confirmed |
| `01` | 1 | `/Y3` | physical NEC2, high XDATA `8000..8003`, input PPI | electrically confirmed |
| `10` | 0 | predicted `/Y4` | NEC3 operator/keypad interface, low numeric XDATA | ROM/decode prediction; `/CS` continuity still pending |

The emulator already exposes the full MOVX address and a pre-callback P1-latch snapshot. That is the correct generic boundary: the **Ponsse simulator**, not the CPU, evaluates `{P1.7:P1.6, A15, address}` and selects NEC/RAM devices.

### NEC1 output image

Working standard-compatible mapping:

```text
IRAM 86      -> XDATA 0000 -> NEC1 Port A
IRAM 87 & EC -> XDATA 0001 -> NEC1 Port B
IRAM 88      -> XDATA 0002 -> NEC1 Port C
XDATA 0003   -> control (80 = mode0 outputs)
```

Physical NEC1 Port A/B routes are continuity-closed. Independent A0/A1 electrical confirmation of the conventional register order remains desirable.

### NEC2 input image

Working standard-compatible mapping:

```text
XDATA 8000 -> IRAM 83 -> NEC2 Port A
XDATA 8001 -> IRAM 84 -> NEC2 Port B
XDATA 8002 -> IRAM 85 -> NEC2 Port C
XDATA 8003 -> control (9B = mode0 inputs)
```

NEC2 device/window identity and all sixteen PCB3-to-Port-A/Port-B physical routes are continuity-confirmed. Port-C field mapping is less complete.

### NEC3

386AM uses bank `10` with a non-standard-looking low-address Port-A/control access pattern and INT6 handshake. This belongs in the P1000 peripheral simulator. Do not implement NEC3 address wiring inside the SAB core.

## 10. Regression fixtures suggested by the current evidence

Processor-side fixtures should remain generic but can use these target configurations:

1. 11.0592 MHz boot SFR snapshot through `A998`.
2. Timer0 `DCEF` exact 8977-cycle overflow plus real ISR/software reload latency.
3. Timer1 `FD` + SMOD=1 exact 19200 mode-3 UART timing.
4. Timestamped P3.2/P3.5 length quadrature through the real INT0 ISR.
5. Timestamped P1.0/P1.1 saw quadrature through INT3 with I3FR behavior.
6. DAPR-started AN0 conversion completed after 15 machine cycles.
7. Siemens Timer2 dynamic reload and P5.4 waveform using all reached reload branches.
8. MOVX context checks showing bank `01` low/high accesses and bank `10` accesses without adding board decode to the CPU.

Target semantics (valve names, connector pins, NEC chip identities) should be asserted in the Ponsse peripheral simulator tests, not in generic CPU unit tests.

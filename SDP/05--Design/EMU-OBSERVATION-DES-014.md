# EMU-OBSERVATION-DES-014 — Observation/control extension constraints

Status: active target constraints, DES-119..124; REQ-027..029

DES-119: keep raw external byte transport/pacing outside CPU/host hardware
callbacks; producer owns persistent structured control and the sole execution
owner, with explicit advance/step, lease and generation checks.
DES-120: add separately negotiated sized immutable CPU observations; preserve
opaque CPU API and ABI1 board tables, accepted timing and old trace/decoder
observers. New generations/identities are uint64 and never silently truncate.
DES-121: distinguish instruction attempt, effects and retired completion;
correlate access/input/UART lifecycle from real execution, including drops and
holding-slot replacement. A separate outcome subscription remains active when
user instruction tracing is off, through the same canonical event infrastructure.
DES-122: integrate the existing matcher/router/gates and stop arbiter; evolve
bounded storage to stable record/epoch cursors without exposing private structs
or a second matcher. Capability coverage is explicit and tested by class.
DES-123: persistent trace enable survives ordinary calls/returns; invocation
capture uses typed dynamic call/IRQ frames and validated return context, with
explicit incomplete/overflow/reset/fault outcomes. Scope state differs from
trace enabled state and output filtering never disables context tracking.
DES-124: host-owned bounded file sinks consume canonical routed records outside
CPU/board callbacks; preserve metadata and explicit loss/error/completion.
Contract artifact version differs from implemented runtime profile; freeze new
headers/schemas/limits without changing historical bundles or availability.

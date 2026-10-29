# SLC-020 Worker implementation notes

Fresh Worker: interface_worker. Baseline contract activation: 6f1ff9f.

Implemented a shared contract bundle under interfaces/0.1.0 and one canonical
public header tree lib/include/emusa80535. All production includes use that
root; legacy root headers forward. Existing declarations and copyright notices
are preserved, with C++ linkage guards. No runtime behavior or protocol1.0
semantics changed. New contracts/* functions are target declarations only.

The bundle specifies CPU cycle/access/decoder and output interfaces, board ABI
lifecycle/stimuli/inspection, shared session, callback-free debug dispatch,
strict wire2.0 command schemas/examples, DAP and host administration. Consumer
feedback informed current-bank peek, normalized ADC/INT identities, logical UART
frame input/output, optional effective pin output, opaque board stimuli and pure
inspection. Unsupported effective-pin/power-down profiles fail explicitly.

Version/provenance tools provide a complete hashed payload, pinned vendored
references/licenses, deterministic source-archive-capable export, copy locks and
negative verification. No mutable latest pointer, copied private CPU layout,
second matcher or board-specific device definitions are introduced.

Verification performed before candidate commit:

- make interface-check: passed isolated C99/C++17 all-header and consumer
  compilation; strict schema examples; incompatible version, numeric/overflow
  uint64, unknown field, bounds, UTF-8 byte and nonfinite JSON negatives;
  reference hashes/links; deterministic export; tampered/missing/extra payload,
  wrong commit/archive lock and optimized-Python rejection.
- make -j2 emu-debug sdk: passed.
- make core-test debug-test PYTHON=python3: all eleven core/event/trace/runtime
  suites plus debugger facade and protocol process suite passed. Log:
  /tmp/slc020-regressions.log (ephemeral execution evidence).

Fresh Reviewer is independently examining contracts/tooling. Candidate needs
final staged install/export evidence and Master integration; the contract-only
runtime symbols are not implemented, external consumers are not adopted and
package publication remains deferred.

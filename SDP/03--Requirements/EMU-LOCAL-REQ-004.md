# EMU-LOCAL-REQ-004 — First local composed runtime

Status: active target; REQ-026
Authority: user's direct instruction to collaborate until a system builds/runs

REQ-026: provide a generic local executable/library that owns one CPU session,
loads an independently built board module, validates exact image/module/config
identity, and supports bounded run, pause, step, state, pure board inspection,
ordered board stimuli and reset/repeat through a deterministic scriptable host.
Malformed input, incompatible modules and execution faults have explicit bounded
outcomes. Preserve CPU/debug legacy behavior and permit external integration to
pin exact source/SDK/module artifacts. No full machine boot equivalence follows
from this local milestone. This is a subset milestone toward REQ-018..024;
network/DAP/signal completion and integrated publication gates remain separate.

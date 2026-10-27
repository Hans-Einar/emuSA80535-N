# Host administration and diagnostics

Configuration selects one CPU variant/oscillator, one image/hash, one board
module/hash/config and zero or explicitly enabled debug endpoints. Host startup
uses [session configuration](05-session.md). Binary names/CLI syntax are not
part of the library ABI; applications can expose flags or config files while
providing these exact values and recording them in identity.

No listener by default. No physical serial/GPIO/CAN or machine-control channel
exists. Unix/TCP are debug-control transports only. Host process signals are not
simulated CPU interrupts or board pins.

| Signal | Contract |
|---|---|
| SIGUSR1 | Request one status diagnostic at next safe boundary: sessionId,state,generation,cycle,instructions,pc,stop reason, image/module/config hashes and queue/loss counts. |
| SIGINT / SIGTERM | Request bounded orderly shutdown through owner, finish permitted safe drain, destroy board/CPU, close endpoints; native callback hangs may prevent completion. |
| SIGSTOP / SIGCONT | OS suspension/resume only; not coherent debugger pause. |

Use blocked signals via Linux signalfd, or an async-signal-safe flag/wakeup.
Never snapshot, allocate or printf inside an async handler. Standard signals may
coalesce; repeated SIGUSR1 is not a reliable request counter. Correlated socket
status is the reliable query API. Signals received while paused must not advance
simulation; the existing safe snapshot is reported. In faulted state report the
last boundary and label unsafe/current fault detail explicitly.

Diagnostics go to stderr or a configured sink, never protocol stdout or the
client socket. Include contract version/manifest hash and source/module identity
at startup. Diagnostic logging cannot advance time, block indefinitely, or drop
accepted mutations. Native traces and logs have independent bounded loss policies.

References: [signalfd](https://man7.org/linux/man-pages/man2/signalfd.2.html),
[signal safety](https://man7.org/linux/man-pages/man7/signal-safety.7.html),
[signals](https://man7.org/linux/man-pages/man7/signal.7.html).
This page is a target contract; old binaries have no SIGUSR1 handler and may
terminate on that signal.

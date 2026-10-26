# AlmaLinux packages and embedding SDK

The initial supported package target is AlmaLinux 10.2 x86_64. The package
version 1.0.0 matches the existing executable product version; it does not
claim complete SAB80535 peripheral coverage or advanced debugger integration.

- `emusa80535`: `/usr/bin/emu-debug`, protocol 1.0, license and documentation.
- `emusa80535-devel`: static `libemusa80535.a`, five matching headers,
  `emusa80535.pc` and a small installed-consumer example.
- The source RPM contains the exact committed sources and `SOURCE_COMMIT`.

No firmware image, board model, curses UI, VS Code extension or DAP adapter is
included. The executable owns its own CPU. It cannot attach to a simulator's
CPU or accept a board plugin through the current protocol.

## Build from a clean committed checkout

Install `rpm-build redhat-rpm-config gcc make binutils python3 pkgconf-pkg-config`
on the AlmaLinux builder. The build does not require ncurses or network access.
Create a new output directory outside the checkout, then run:

```sh
rpmroot=$(mktemp -d /tmp/emusa80535-rpm.XXXXXX)
mkdir -p "$rpmroot/SOURCES" "$rpmroot/SPECS"
python3 packaging/make_source.py "$rpmroot/SOURCES"
cp packaging/emusa80535.spec "$rpmroot/SPECS/"
rpmbuild -ba --define "_topdir $rpmroot" --define 'dist .el10_2' \
    "$rpmroot/SPECS/emusa80535.spec"
```

`make_source.py` rejects tracked changes and untracked nonignored files. It
archives committed HEAD (including licensing and upstream notices) and adds
its exact SHA. Archive timestamps and gzip metadata are deterministic. Source
archive reproducibility does not imply byte-identical RPM rebuilds across
different compiler/RPM versions or build roots. Signing and repository
publication are separate operations owned by the release integrator.

The RPM `%check` runs all eleven default core/event/trace/runtime suites, the
C debug facade and NDJSON process suite, and an external consumer compiled
using only installed headers/library/pkg-config metadata. Generated debug
symbol RPMs may also be produced by the distribution RPM macros.

## Install or stage without RPM

```sh
make emu-debug sdk
make install DESTDIR=/tmp/emusa80535-stage prefix=/usr libdir=/usr/lib64
python3 packaging/check_install.py /tmp/emusa80535-stage \
    --commit "$(git rev-parse HEAD)"
```

Installation is POSIX-only; existing Windows executable/test builds remain
available. Rebuild the executable when changing `DEBUG_COMMIT`; release
archives always start without build products. RPM builds explicitly pass
`SOURCE_COMMIT` even though the build tree has no `.git` directory.

After installing the RPMs, the same consumer check accepts `/` as its root.
The installed example can also be compiled directly:

```sh
cc -std=c99 /usr/share/doc/emusa80535/sdk_smoke.c \
    $(pkg-config --cflags --libs emusa80535) -o /tmp/emusa80535-smoke
/tmp/emusa80535-smoke
```

## Embedding boundary

A simulator owns its `struct em8051`, exact 65,536-byte CODE image, virtual
stimuli and execution budget. Initialize the selected CPU variant with owned
CODE backing; use `em8051_load_binary()` to load an image. Install `xread` and
`xwrite` to implement external memory/device access, and use generic UART,
analog and external-edge APIs to supply virtual input. The MOVX observer is
read-only: it observes the completed transaction and cannot supply read data.
Do not perform a second device transaction from that observer.

The static library also contains the accepted debugger facade and standalone
event/trace runtime. Their availability is not an attached debug service:
the facade currently owns a separate CPU, and canonical CPU trace producers,
safe-boundary watch stops, a stable versioned external trace facade and the
optional wire extension are future work. External simulator/debug composition
needs its own generic integration contract.

Use the exact same headers and static library build together and rebuild
consumers when updating. The native structs are not a stable shared-library
ABI or a wire format. The SDK needs neither host threads nor physical I/O.

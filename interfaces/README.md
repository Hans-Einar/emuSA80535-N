# Shared emulator interfaces

The engineering baseline is [bundle 0.1.0](0.1.0/README.md). It specifies one
runtime process hosting CPU and a startup-loaded native board module, plus
independent debugger clients and DAP projection. Separate board processes are
not part of this contract.

The canonical public include root is `lib/include` in this repository and
`include` after installation. Include `<emusa80535/...>` everywhere. Root-level
legacy headers are forwarding shims, not second copies of the declarations.

Bundle version, native ABI version, control-wire version and product version
are distinct. A contract being available does not mean its runtime is implemented.
Use the availability table, immutable manifest and consumer lock workflow in
the versioned bundle. Never silently copy an unversioned header into another repo.

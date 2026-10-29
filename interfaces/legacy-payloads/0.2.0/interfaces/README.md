# Shared emulator interfaces

The engineering baseline is [bundle 0.2.0](0.2.0/README.md). It specifies one
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

Bundle 0.2.0 implements CPU API1 and retains board ABI1.0. The accepted
[0.1.0 payload](https://github.com/Hans-Einar/emuSA80535-N/blob/7d924cc27e48c599d5775904543070d92e413c26/interfaces/0.1.0/README.md) and its exported archive remain immutable;
retrieve its canonical headers/tools from that archive or its recorded Git revision.

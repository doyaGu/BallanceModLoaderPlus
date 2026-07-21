# Superseded Script Mod v1 Contract

This filename is retained so old links do not silently lead to a false API
description. It no longer describes a usable BML 0.3.13 contract.

The old experimental export registry (`[bml.export]`, `ExportRef`,
`ExportResolver`, and `CallFrame`) and `Observe` facade were removed. They had
no released compatibility promise. Do not compile or package code that refers
to them.

The current experimental cross-mod model is documented in
[Interop v2](interop-v2.md) and [the migration guide](interop-v2-migration.md):

- public native ABI is fixed-layout C values and `BML_Interop_*` C functions;
- generated C++ facades are header-only conveniences, never exported C++ ABI;
- generated AngelScript facades provide ordinary consumers with shallow
  `BML::Runtime`, `BML::Scene`, `BML::Gameplay`, `BML::Events`, and `BML::UI`
  APIs;
- authoring a provider uses a versioned `.bmlapi` contract and
  `BML::Interop` during `OnLoad` only;
- provider/source lifetime is tied to the mod runtime and all stale handles are
  diagnosed rather than dereferenced.

The stable script-mod basics remain metadata (`bml.mod`, `bml.require`, and
`bml.optional`), lifecycle callbacks, commands, timers, DataShare, and the
CKAngelScript-owned API surface. See
[the current author guide](bml-script-mod-author-guide.md) for those APIs.

# BML+ Mod Templates

This directory contains author-facing starting points. For a normal native Mod,
use the default template and ignore the specialized templates.

## Default native Mod

- `native-mod-template`: basic CMake-based native `.bmodp` Mod template.

Create it with `scripts/bml.py new native yourname.my-mod`. Generated projects contain
`bml.mod.json`; their own `bml run` configures Win32, builds, deploys, starts
Player, and selects that Mod's log lines.

## Specialized native Mods

Use these only when two Mods need to communicate:

- `native-interface-provider-template`: publishes a typed, versioned native
  interface and installs its independent header package.
- `native-interface-consumer-template`: consumes that header package without
  linking the provider DLL.
- `native-imc-provider-template`: defines and implements a generated IMC RPC API.

Select one with `scripts/bml.py new native <id> --profile ...`.

## Script Mod

- `script-mod-template`: AngelScript `*.mod.as` script mod template.
  Instantiate it with `scripts/bml.py new script yourname.my-mod`. The generated
  project uses the same `bml build`, `bml run`, and `bml pack` commands as a
  Native Mod Project.

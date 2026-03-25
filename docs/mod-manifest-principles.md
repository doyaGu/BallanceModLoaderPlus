# Mod Manifest Principles

This document defines the conceptual rules behind `mod.toml`. Read this first
if you are unsure when to use module dependencies versus interface
requirements.

## Two Relationship Types

`mod.toml` models two different kinds of relationships:

- Module relationships: loading and version relationships between concrete
  modules
- Interface relationships: runtime contracts that modules provide and consume

These relationships must remain distinct. A module id is not an interface id,
and an interface id is not a module id.

## Identity Model

Each module has one module identity:

- `[package].id` is the `module id`
- `module id` is used for discovery, deduplication, load order, and
  module-level dependency resolution

Each interface has one interface identity:

- Keys in `[interfaces]` and `[requires]` are `interface id`s
- `interface id` is used for runtime contract resolution

Hard rules:

- `module id` and `interface id` are separate namespaces
- An `interface id` must not equal any `module id`
- A single `interface id` may have only one exporting module

## Field Responsibilities

`[package]`
Describes the module itself: identity, metadata, version, and entry file.

`[dependencies]`
Declares module-level dependencies. Keys must be `module id`s.

`[requires]`
Declares interface-level requirements. Keys must be `interface id`s.

`[interfaces]`
Declares interface-level exports. Keys must be `interface id`s.

`[conflicts]`
Declares module-level incompatibilities. Keys must be `module id`s.

`[assets]`
Declares resource mount metadata.

`[package].capabilities`
Declares lightweight, non-versioned capability tags. Capability tags are part
of package metadata, not independent relationship sections. They are not
interface contracts and do not participate in dependency resolution.

## The Four Dependency Modes

### 1. Required Module Dependency

Use this when the current module depends on a specific module implementation.

```toml
[dependencies]
"com.bml.ui" = ">=0.4.0"
```

Behavior:

- Present and version-satisfying: success
- Missing: failure
- Present but version-mismatched: failure

### 2. Optional Module Dependency

Use this when the current module can cooperate with a specific module, but can
still run without it.

```toml
[dependencies]
"com.bml.console" = { version = ">=0.4.0", optional = true }
```

Behavior:

- Present and version-satisfying: success
- Missing: warning, not failure
- Present but version-mismatched: failure

### 3. Required Interface Requirement

Use this when the current module needs a runtime capability, but does not care
which module exports it.

```toml
[requires]
"bml.ui.draw_registry" = ">=1.0.0"
```

Behavior:

- Provider exists and version satisfies: success
- No provider exists: failure
- Provider exists but version mismatches: failure

### 4. Optional Interface Requirement

Use this when the current module can use a runtime capability if present, but
can still run without it.

```toml
[requires]
"bml.console.command_registry" = { version = ">=1.0.0", optional = true }
```

Behavior:

- Provider exists and version satisfies: success
- No provider exists: warning, not failure
- Provider exists but version mismatches: failure

## How To Choose

Use `[dependencies]` when you depend on a concrete implementation.

Use `[requires]` when you depend on a capability contract.

Practical rule:

- "It must be this module" -> `[dependencies]`
- "Any provider of this interface is acceptable" -> `[requires]`

Recommended rule:

- Prefer `[requires]` when you only need a service contract
- Use `[dependencies]` when you depend on a specific module's implementation,
  lifecycle, side effects, or unique ownership of a feature

## Constraints

- Do not write `module id`s in `[requires]`
- Do not write `interface id`s in `[dependencies]`
- `optional` is a property of a dependency edge, not a property of the target
  module
- Do not use `[package].capabilities` to model versioned APIs, unique providers, or
  required runtime contracts
- Do not declare the same logical relationship as both a required module
  dependency and a required interface requirement unless you intentionally need
  both

## Capabilities

`[package].capabilities` is a lightweight tagging mechanism, not a second
interface system.

Use `[package].capabilities` for:

- coarse-grained feature tags
- soft runtime probes
- diagnostics, discoverability, or UI categorization

Do not use `[package].capabilities` for:

- versioned APIs
- unique provider contracts
- dependency or load-order rules
- permission or security models
- alternate names for interfaces already declared in `[interfaces]`

If another module must rely on a contract, use `[interfaces]` and `[requires]`
instead.

Naming guidance for capability tags:

- Prefer short tag-style names such as `ui.overlay` or
  `console.command-surface`
- Avoid names that differ from an interface id only by a prefix or minor
  wording change
- Treat `bml.internal.runtime` as a runtime-reserved internal capability, not a
  public contract

## Naming Guidance

Use visually distinct naming patterns:

- `module id`: `com.bml.ui`
- `interface id`: `bml.ui.draw_registry`

The goal is that authors can infer the category of an id from its shape before
they even read the surrounding field name.

## Diagnostic Rules

Diagnostics must always name the object category explicitly:

- `missing module dependency`
- `requires module 'com.bml.ui'`
- `requires interface 'bml.ui.draw_registry'`
- `interface id collides with module id`

Ambiguous wording such as `missing dependency` or `id conflict` should be
avoided in user-facing diagnostics.

## Example

```toml
[package]
id = "com.example.overlay"
name = "Example Overlay"
version = "1.0.0"

[dependencies]
"com.bml.ui" = ">=0.4.0"
"com.bml.console" = { version = ">=0.4.0", optional = true }

[requires]
"bml.ui.draw_registry" = ">=1.0.0"
"bml.console.command_registry" = { version = ">=1.0.0", optional = true }

[interfaces]
"com.example.overlay.api" = "1.0.0"
```

In this example:

- The module must load after `com.bml.ui`
- It can integrate with `com.bml.console` when present
- It requires a UI drawing contract
- It can optionally integrate with a console command contract
- It exports its own interface for other modules to consume

# `bml.runtime` API

Compatibility: `1.0`

Descriptor hash: `0x7467626056A20F28`

## Schemas

### `runtime_state` (1)

| Field ID | Name | Type | Optional |
| ---: | --- | --- | :---: |
| 1 | `in_game` | `bool` | no |
| 2 | `in_level` | `bool` | no |
| 3 | `paused` | `bool` | no |
| 4 | `playing` | `bool` | no |
| 5 | `cheat_enabled` | `bool` | no |

### `clock_state` (2)

| Field ID | Name | Type | Optional |
| ---: | --- | --- | :---: |
| 1 | `time_ms` | `float` | no |
| 2 | `absolute_ms` | `float` | no |
| 3 | `delta_ms` | `float` | no |
| 4 | `frame` | `int` | no |

### `score_state` (3)

| Field ID | Name | Type | Optional |
| ---: | --- | --- | :---: |
| 1 | `sr` | `float` | no |
| 2 | `hs` | `int` | no |

## Endpoints

| Name | Kind | Input schema | Output schema | Probe |
| --- | --- | ---: | ---: | :---: |
| `clock` | `resource` | - | 2 | no |
| `score` | `resource` | - | 3 | no |
| `state` | `resource` | - | 1 | no |

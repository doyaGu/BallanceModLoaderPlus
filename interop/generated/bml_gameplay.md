# `bml.gameplay` API

Compatibility: `1.0`

Descriptor hash: `0xF6A68405E0438B70`

## Schemas

### `level_state` (1)

| Field ID | Name | Type | Optional |
| ---: | --- | --- | :---: |
| 1 | `id` | `int` | no |
| 2 | `active_ball` | `object` | no |
| 3 | `reset_matrix` | `mat4` | no |
| 4 | `points` | `int` | no |

### `energy_state` (2)

| Field ID | Name | Type | Optional |
| ---: | --- | --- | :---: |
| 1 | `points` | `int` | no |
| 2 | `lives` | `int` | no |
| 3 | `start_points` | `int` | no |
| 4 | `start_lives` | `int` | no |
| 5 | `time_factor` | `float` | no |
| 6 | `life_bonus` | `int` | no |

### `catalog_entry` (3)

| Field ID | Name | Type | Optional |
| ---: | --- | --- | :---: |
| 1 | `file` | `string` | no |
| 2 | `start_ball` | `string` | no |
| 3 | `sky` | `string` | no |
| 4 | `bonus` | `int` | no |
| 5 | `music` | `int` | no |

### `checkpoint` (4)

| Field ID | Name | Type | Optional |
| ---: | --- | --- | :---: |
| 1 | `matrix` | `mat4` | no |
| 2 | `object` | `object` | no |

### `resetpoint` (5)

| Field ID | Name | Type | Optional |
| ---: | --- | --- | :---: |
| 1 | `object` | `object` | no |

## Endpoints

| Name | Kind | Input schema | Output schema | Probe |
| --- | --- | ---: | ---: | :---: |
| `catalog` | `collection` | - | 3 | yes |
| `checkpoints` | `collection` | - | 4 | yes |
| `energy` | `resource` | - | 2 | yes |
| `level` | `resource` | - | 1 | yes |
| `resetpoints` | `collection` | - | 5 | yes |

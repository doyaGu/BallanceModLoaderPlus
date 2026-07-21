# `bml.scene` API

Compatibility: `1.0`

Descriptor hash: `0x4E6FC19797065751`

## Schemas

### `object_info` (1)

| Field ID | Name | Type | Optional |
| ---: | --- | --- | :---: |
| 1 | `id` | `int` | no |
| 2 | `name` | `string` | no |
| 3 | `class_id` | `int` | no |
| 4 | `visible` | `bool` | no |
| 5 | `dynamic` | `bool` | no |

### `entity_transform` (2)

| Field ID | Name | Type | Optional |
| ---: | --- | --- | :---: |
| 1 | `position` | `vec3` | no |
| 2 | `scale` | `vec3` | no |
| 3 | `parent` | `object` | no |
| 4 | `child_count` | `int` | no |

### `find_name_request` (3)

| Field ID | Name | Type | Optional |
| ---: | --- | --- | :---: |
| 1 | `name` | `string` | no |

### `find_name_class_request` (4)

| Field ID | Name | Type | Optional |
| ---: | --- | --- | :---: |
| 1 | `name` | `string` | no |
| 2 | `class_id` | `int` | no |

### `find_result` (5)

| Field ID | Name | Type | Optional |
| ---: | --- | --- | :---: |
| 1 | `object` | `object` | no |

## Endpoints

| Name | Kind | Input schema | Output schema | Probe |
| --- | --- | ---: | ---: | :---: |
| `entity` | `component` | - | 2 | no |
| `find_name` | `query` | 3 | 5 | no |
| `find_name_class` | `query` | 4 | 5 | no |
| `object` | `component` | - | 1 | no |

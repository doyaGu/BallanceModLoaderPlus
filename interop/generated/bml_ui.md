# `bml.ui` API

Compatibility: `1.0`

Descriptor hash: `0xD21CC17B2334E884`

IMC wire hash: `0xD21CC17B2334E884`

## Schemas

### `command_result` (1)

| Field ID | Name | Type | Optional |
| ---: | --- | --- | :---: |

### `message_input` (2)

| Field ID | Name | Type | Optional |
| ---: | --- | --- | :---: |
| 1 | `message` | `string` | no |

### `hud_mode_input` (3)

| Field ID | Name | Type | Optional |
| ---: | --- | --- | :---: |
| 1 | `mode` | `int` | no |

### `visible_input` (4)

| Field ID | Name | Type | Optional |
| ---: | --- | --- | :---: |
| 1 | `visible` | `bool` | no |

### `empty_input` (5)

| Field ID | Name | Type | Optional |
| ---: | --- | --- | :---: |

### `hud_state` (6)

| Field ID | Name | Type | Optional |
| ---: | --- | --- | :---: |
| 1 | `mode` | `int` | no |
| 2 | `sr_time` | `float` | no |

## Endpoints

| Name | Kind | Input schema | Output schema | Probe |
| --- | --- | ---: | ---: | :---: |
| `hud_fps_show` | `command` | 4 | 1 | no |
| `hud_set` | `command` | 3 | 1 | no |
| `hud_sr_pause` | `command` | 5 | 1 | no |
| `hud_sr_reset` | `command` | 5 | 1 | no |
| `hud_sr_show` | `command` | 4 | 1 | no |
| `hud_sr_start` | `command` | 5 | 1 | no |
| `hud_title_show` | `command` | 4 | 1 | no |
| `map_menu_close` | `command` | 5 | 1 | no |
| `map_menu_open` | `command` | 5 | 1 | no |
| `message_add` | `command` | 2 | 1 | no |
| `message_clear` | `command` | 5 | 1 | no |
| `mods_menu_close` | `command` | 5 | 1 | no |
| `mods_menu_open` | `command` | 5 | 1 | no |
| `state` | `resource` | - | 6 | no |

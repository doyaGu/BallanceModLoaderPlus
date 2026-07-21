# `bml.events` API

Compatibility: `1.0`

Descriptor hash: `0x671750F9EA1C375C`

## Schemas

### `event` (1)

| Field ID | Name | Type | Optional |
| ---: | --- | --- | :---: |
| 1 | `kind` | `int` | no |
| 2 | `filename` | `string` | yes |
| 3 | `is_map` | `bool` | yes |
| 4 | `master_name` | `string` | yes |
| 5 | `filter_class` | `int` | yes |
| 6 | `add_to_scene` | `bool` | yes |
| 7 | `reuse_meshes` | `bool` | yes |
| 8 | `reuse_materials` | `bool` | yes |
| 9 | `dynamic` | `bool` | yes |
| 10 | `object_ids` | `array<object>` | yes |
| 11 | `master_object` | `object` | yes |
| 12 | `script` | `object` | yes |
| 13 | `target` | `object` | yes |
| 14 | `fixed` | `bool` | yes |
| 15 | `friction` | `float` | yes |
| 16 | `elasticity` | `float` | yes |
| 17 | `mass` | `float` | yes |
| 18 | `collision_group` | `string` | yes |
| 19 | `start_frozen` | `bool` | yes |
| 20 | `enable_collision` | `bool` | yes |
| 21 | `auto_calculate_mass_center` | `bool` | yes |
| 22 | `linear_damp` | `float` | yes |
| 23 | `rot_damp` | `float` | yes |
| 24 | `collision_surface` | `string` | yes |
| 25 | `mass_center` | `vec3` | yes |
| 26 | `convex_meshes` | `array<object>` | yes |
| 27 | `ball_centers` | `array<vec3>` | yes |
| 28 | `ball_radii` | `array<float>` | yes |
| 29 | `concave_meshes` | `array<object>` | yes |
| 30 | `command` | `string` | yes |
| 31 | `command_args` | `array<string>` | yes |
| 32 | `config_category` | `string` | yes |
| 33 | `config_key` | `string` | yes |
| 34 | `config_type` | `int` | yes |
| 35 | `config_value` | `string` | yes |
| 36 | `cheat_enabled` | `bool` | yes |

## Endpoints

| Name | Kind | Input schema | Output schema | Probe |
| --- | --- | ---: | ---: | :---: |
| `all` | `stream` | - | 1 | no |

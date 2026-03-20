#!/usr/bin/env python3
"""
Behavior wrapper code generator for BallanceModLoaderPlus.

Reads behavior metadata from JSON files extracted by BuildingBlockExporter
and generates type-safe C++ fluent builder classes for each behavior.

Usage:
    python tools/generate_behaviors.py [--behaviors data/behaviors.json]
                                       [--types data/parameter_types.json]
                                       [--mapping data/type_mapping.json]
                                       [--outdir include/BML/Behaviors]
"""

import argparse
import json
import re
import sys
from pathlib import Path
from collections import defaultdict


# -- Name sanitization --------------------------------------------------------

def to_identifier(name: str) -> str:
    """Strip non-alphanumeric characters except spaces/underscores."""
    return re.sub(r'[^a-zA-Z0-9_ ]', '', name)


def to_pascal_case(name: str) -> str:
    """Convert a display name to PascalCase identifier.

    'Set Position' -> 'SetPosition'
    '3D Sprite Text' -> '_3DSpriteText'
    'Op: Add' -> 'OpAdd'
    '' -> ''
    """
    cleaned = to_identifier(name)
    words = cleaned.split()
    result = ''.join(w[0].upper() + w[1:] if w else '' for w in words)
    if result and result[0].isdigit():
        result = '_' + result
    return result


def to_field_name(name: str) -> str:
    """Convert a parameter name to a PascalCase field name.

    'Fixed' -> 'Fixed'
    'Linear Damping' -> 'LinearDamping'
    '' -> 'Param'
    """
    pc = to_pascal_case(name)
    return pc if pc else 'Param'


def to_member_name(pascal: str) -> str:
    """Convert PascalCase field name to m_ prefixed member name.

    'Fixed' -> 'm_Fixed'
    'LinearDamping' -> 'm_LinearDamping'
    '_3DEntity' -> 'm_3DEntity'  (avoids reserved double-underscore m__)
    """
    if pascal.startswith('_'):
        return f'm{pascal}'
    return f'm_{pascal}'


def to_namespace(dll_name: str) -> str:
    """Convert a DLL filename to a namespace name.

    'physics_RT.dll' -> 'Physics'
    'Narratives.dll' -> 'Narratives'
    'TT_Toolbox_RT.dll' -> 'TTToolbox'
    'TT_InterfaceManager_RT.dll' -> 'TTInterfaceManager'
    '3DTransfo.dll' -> '_3DTransfo'
    """
    stem = Path(dll_name).stem
    stem = re.sub(r'_RT$', '', stem)
    parts = stem.split('_')
    result = ''.join(p[0].upper() + p[1:] if p else '' for p in parts)
    if result and result[0].isdigit():
        result = '_' + result
    return result


# -- CK_CLASSID to C++ target type -------------------------------------------

CLASSID_TO_CPP = {
    'CKCID_3DENTITY': 'CK3dEntity',
    'CKCID_2DENTITY': 'CK2dEntity',
    'CKCID_BEOBJECT': 'CKBeObject',
    'CKCID_OBJECT': 'CKObject',
    'CKCID_3DOBJECT': 'CK3dObject',
    'CKCID_LIGHT': 'CKLight',
    'CKCID_TARGETLIGHT': 'CKTargetLight',
    'CKCID_CAMERA': 'CKCamera',
    'CKCID_TARGETCAMERA': 'CKTargetCamera',
    'CKCID_CHARACTER': 'CKCharacter',
    'CKCID_BODYPART': 'CKBodyPart',
    'CKCID_SPRITE': 'CKSprite',
    'CKCID_SPRITETEXT': 'CKSpriteText',
    'CKCID_SPRITE3D': 'CKSprite3D',
    'CKCID_CURVE': 'CKCurve',
    'CKCID_MESH': 'CKMesh',
    'CKCID_MATERIAL': 'CKMaterial',
    'CKCID_TEXTURE': 'CKTexture',
    'CKCID_SOUND': 'CKSound',
    'CKCID_WAVESOUND': 'CKWaveSound',
    'CKCID_MIDISOUND': 'CKMidiSound',
    'CKCID_SCENE': 'CKScene',
    'CKCID_LEVEL': 'CKLevel',
    'CKCID_PLACE': 'CKPlace',
    'CKCID_GROUP': 'CKGroup',
    'CKCID_DATAARRAY': 'CKDataArray',
    'CKCID_GRID': 'CKGrid',
    'CKCID_RENDEROBJECT': 'CKRenderObject',
    'CKCID_KINEMATICCHAIN': 'CKKinematicChain',
}

VARIABLE_FLAGS = {
    'CKBEHAVIOR_VARIABLEPARAMETERINPUTS',
    'CKBEHAVIOR_VARIABLEPARAMETEROUTPUTS',
    'CKBEHAVIOR_INTERNALLYCREATEDINPUTPARAMS',
    'CKBEHAVIOR_INTERNALLYCREATEDOUTPUTPARAMS',
}


# -- Type resolution ----------------------------------------------------------

class TypeResolver:
    def __init__(self, type_mapping: dict, param_types: dict | None = None):
        self.mapping = type_mapping
        self.param_types = param_types or {}
        self.enum_types = {}
        if param_types:
            for t in param_types.get('types', []):
                if t.get('category') == 'enum' and t.get('enum_values'):
                    key = tuple(t['guid'])
                    self.enum_types[key] = t['enum_values']

    def resolve(self, type_name: str, type_guid: list) -> dict:
        """Resolve a CKPGUID type name to C++ type info."""
        if type_name in self.mapping:
            info = dict(self.mapping[type_name])
            guid_key = tuple(type_guid) if type_guid else None
            if guid_key and guid_key in self.enum_types:
                info['enum_values'] = self.enum_types[guid_key]
            return info

        return {
            'cpp_type': 'int',
            'setter': 'value',
            'default': '0',
            'unknown': True,
            'raw_guid': type_guid,
            'raw_name': type_name,
        }


# -- Code generation helpers --------------------------------------------------

def is_variable_params(behavior: dict) -> bool:
    """Check if a behavior has variable/dynamic parameters."""
    flags = set(behavior.get('behavior_flags', []))
    return bool(flags & VARIABLE_FLAGS)


def get_target_type(behavior: dict) -> str | None:
    """Get the C++ target type if the behavior is targetable."""
    flags = set(behavior.get('behavior_flags', []))
    if 'CKBEHAVIOR_TARGETABLE' not in flags:
        return None
    cid = behavior.get('compatible_class_id', 'CKCID_BEOBJECT')
    cpp_class = CLASSID_TO_CPP.get(cid, 'CKBeObject')
    return cpp_class + ' *'


def make_unique_field_names(params: list[dict]) -> list[str]:
    """Generate unique PascalCase field names for a parameter list.

    Handles duplicates by appending a numeric suffix to all instances.
    """
    if not params:
        return []

    names = []
    seen = defaultdict(int)
    for p in params:
        base = to_field_name(p.get('name', ''))
        seen[base] += 1
        if seen[base] > 1:
            names.append(f'{base}{seen[base]}')
        else:
            names.append(base)

    # Retroactively fix first occurrence when there are duplicates
    final_counts = defaultdict(int)
    for n in names:
        base = re.sub(r'\d+$', '', n)
        final_counts[base] += 1
    result = []
    idx_tracker = defaultdict(int)
    for n in names:
        base = re.sub(r'\d+$', '', n)
        if final_counts[base] > 1:
            idx_tracker[base] += 1
            result.append(f'{base}{idx_tracker[base]}')
        else:
            result.append(n)
    return result


def match_settings_to_locals(settings: list[dict], local_params: list[dict]) -> list[tuple[dict, int]]:
    """Map each setting to its local parameter index by name.

    Skips settings that don't match any local (with a stderr warning).
    """
    name_to_idx = {lp['name']: i for i, lp in enumerate(local_params)}
    result = []
    for s in settings:
        idx = name_to_idx.get(s['name'])
        if idx is not None:
            result.append((s, idx))
        else:
            print(f'  Warning: setting "{s["name"]}" has no matching local parameter',
                  file=sys.stderr)
    return result


def safe_setter_name(field_name: str, class_name: str) -> str:
    """Return PascalCase setter name, avoiding constructor name collision.

    If the field name matches the enclosing class name, prefix with 'Set'
    to prevent the method from being interpreted as a constructor.
    """
    if field_name == class_name:
        return f'Set{field_name}'
    return field_name


def generate_enum(name: str, values: list[str], indent: str = '    ') -> str:
    """Generate a scoped enum class nested in the behavior class."""
    lines = [f'{indent}enum class {name} : int {{']
    for i, val in enumerate(values):
        safe_val = to_pascal_case(val) if val else f'Value{i}'
        # Ensure enum value is a valid identifier even after PascalCase
        if not safe_val:
            safe_val = f'Value{i}'
        sep = ',' if i < len(values) - 1 else ''
        lines.append(f'{indent}    {safe_val} = {i}{sep}')
    lines.append(f'{indent}}};')
    return '\n'.join(lines)


def generate_apply_call(setter: str, index: int, member_expr: str, info: dict) -> str:
    """Generate a single .Input() call for the Apply() chain."""
    if info.get('enum_values') and setter == 'value':
        return f'.Input({index}, static_cast<int>({member_expr}))'
    return f'.Input({index}, {member_expr})'


def is_valid_guid(guid: list) -> bool:
    """Check if a GUID is non-zero (not CKGUID_NONE)."""
    return isinstance(guid, list) and len(guid) == 2 and (guid[0] != 0 or guid[1] != 0)


# -- Code generation ----------------------------------------------------------

def generate_behavior(behavior: dict, resolver: TypeResolver) -> str:
    """Generate a fluent builder class for one behavior.

    Returns empty string if the behavior cannot be generated (missing name/GUID).
    """
    name = to_pascal_case(behavior.get('name', ''))
    if not name:
        return ''

    guid = behavior.get('guid', [0, 0])
    if not is_valid_guid(guid):
        print(f'  Warning: skipping "{behavior.get("name", "?")}" with zero GUID',
              file=sys.stderr)
        return ''

    target_type = get_target_type(behavior)
    variable = is_variable_params(behavior)

    lines = []

    # Banner
    lines.append(f'// {"=" * 76}')
    if variable:
        lines.append(f'// {name} (variable parameters)')
    else:
        lines.append(f'// {name}')

    desc_lines = []
    if behavior.get('description'):
        desc = behavior['description'].replace('\n', ' ').strip()
        if desc:
            desc_lines.append(f'// {desc}')
    if variable:
        desc_lines.append('// Use BehaviorRunner / BehaviorFactory directly.')
    if desc_lines:
        lines.append('//')
        lines.extend(desc_lines)

    lines.append(f'// {"=" * 76}')
    lines.append('')

    # Variable-param behaviors: GUID constant only, no class
    if variable:
        lines.append(f'inline constexpr CKGUID {name}Guid = CKGUID(0x{guid[0]:X}, 0x{guid[1]:X});')
        return '\n'.join(lines)

    # Collect parameter info
    input_params = behavior.get('input_params', [])
    output_params = behavior.get('output_params', [])
    local_params = behavior.get('local_params', [])
    settings = behavior.get('settings', [])

    in_fields = make_unique_field_names(input_params)
    out_fields = make_unique_field_names(output_params)
    in_resolved = [resolver.resolve(p.get('type_name', ''), p.get('type_guid', []))
                   for p in input_params]
    out_resolved = [resolver.resolve(p.get('type_name', ''), p.get('type_guid', []))
                    for p in output_params]

    setting_matches = match_settings_to_locals(settings, local_params) if settings else []
    setting_fields = make_unique_field_names([s for s, _ in setting_matches])
    setting_resolved = [resolver.resolve(s.get('type_name', ''), s.get('type_guid', []))
                        for s, _ in setting_matches]

    use_target = target_type is not None
    use_target_str = 'true' if use_target else 'false'

    # ---- Class declaration ----
    lines.append(f'class {name} {{')
    lines.append('public:')
    lines.append(f'    static constexpr CKGUID Guid = CKGUID(0x{guid[0]:X}, 0x{guid[1]:X});')
    lines.append('')

    # Nested enum classes for enum-type input params
    for p, fname, info in zip(input_params, in_fields, in_resolved):
        if info.get('enum_values') and info['setter'] == 'value':
            lines.append(generate_enum(fname, info['enum_values']))
            lines.append('')

    # Nested Result struct for output params
    if output_params:
        lines.append('    struct Result {')
        for p, fname, info in zip(output_params, out_fields, out_resolved):
            cpp_type = info['cpp_type']
            default = info['default']
            comment = ''
            if info.get('unknown'):
                comment = f'  // Unknown type: {info.get("raw_name", "?")}'
            lines.append(f'        {cpp_type} {fname} = {default};{comment}')
        lines.append('    };')
        lines.append('')

    # ---- Fluent setters (PascalCase) ----
    if input_params:
        for p, fname, info in zip(input_params, in_fields, in_resolved):
            setter = safe_setter_name(fname, name)
            member = to_member_name(fname)
            if info.get('enum_values') and info['setter'] == 'value':
                param_type = fname
            else:
                param_type = info['cpp_type']
            lines.append(f'    {name} &{setter}({param_type} v) {{ {member} = v; return *this; }}')
        lines.append('')

    # ---- Settings setters (PascalCase) ----
    if setting_matches:
        for (s, local_idx), sfname, sinfo in zip(setting_matches, setting_fields, setting_resolved):
            setter = f'Setting{sfname}'
            member = f'm_Sett{sfname}'
            cpp_type = sinfo['cpp_type']
            lines.append(f'    {name} &{setter}({cpp_type} v) {{ {member} = v; m_HasSettings = true; return *this; }}')
        lines.append('')

    # ---- Terminal: Run(BehaviorCache&, ...) ----
    target_param = f'{target_type}target, ' if use_target else ''
    target_fwd = 'target, ' if use_target else ''

    lines.append(f'    void Run(BehaviorCache &cache, {target_param}int slot = 0) const {{')
    lines.append(f'        auto runner = cache.Get(Guid, {use_target_str});')
    lines.append(f'        Run(runner, {target_fwd}slot);')
    lines.append('    }')
    lines.append('')

    # ---- Terminal: Run(BehaviorRunner&, ...) ----
    lines.append(f'    void Run(BehaviorRunner &runner, {target_param}int slot = 0) const {{')
    if use_target:
        lines.append('        runner.Target(target);')
    if setting_matches:
        lines.append('        if (m_HasSettings)')
        lines.append('            ApplySettings(runner);')
    lines.append('        Apply(runner);')
    lines.append('        runner.Execute(slot);')
    lines.append('    }')
    lines.append('')

    # ---- Terminal: Build(CKBehavior*, ...) ----
    build_target = f', {target_type}target' if use_target else ''
    lines.append(f'    CKBehavior *Build(CKBehavior *script{build_target}) const {{')
    lines.append(f'        BehaviorFactory factory(script, Guid, {use_target_str});')
    if use_target:
        lines.append('        factory.Target(target);')
    if setting_matches:
        lines.append('        if (m_HasSettings)')
        lines.append('            ApplySettings(factory);')
    lines.append('        Apply(factory);')
    lines.append('        return factory.Build();')
    lines.append('    }')

    # ---- Terminal: RunWithResult(BehaviorCache&, ...) ----
    if output_params:
        lines.append('')
        lines.append(f'    Result RunWithResult(BehaviorCache &cache, {target_param}int slot = 0) const {{')
        lines.append(f'        auto runner = cache.Get(Guid, {use_target_str});')
        if use_target:
            lines.append('        runner.Target(target);')
        if setting_matches:
            lines.append('        if (m_HasSettings)')
            lines.append('            ApplySettings(runner);')
        lines.append('        Apply(runner);')
        lines.append('        runner.Execute(slot);')
        lines.append('        Result r;')
        for i, (p, fname, info) in enumerate(zip(output_params, out_fields, out_resolved)):
            cpp_type = info['cpp_type']
            if info['setter'] == 'object':
                lines.append(f'        r.{fname} = static_cast<{cpp_type}>(runner.ReadOutputObj({i}));')
            else:
                lines.append(f'        r.{fname} = runner.ReadOutput<{cpp_type}>({i});')
        lines.append('        return r;')
        lines.append('    }')

    # ---- Private section ----
    lines.append('')
    lines.append('private:')

    # Apply<Builder>() template
    lines.append('    template <typename Builder>')
    lines.append('    void Apply(Builder &bb) const {')
    if not input_params:
        lines.append('        (void)bb;')
    elif len(input_params) == 1:
        call = generate_apply_call(in_resolved[0]['setter'], 0,
                                   to_member_name(in_fields[0]), in_resolved[0])
        lines.append(f'        bb{call};')
    else:
        for i in range(len(input_params)):
            call = generate_apply_call(in_resolved[i]['setter'], i,
                                       to_member_name(in_fields[i]), in_resolved[i])
            if i == 0:
                lines.append(f'        bb{call}')
            elif i == len(input_params) - 1:
                lines.append(f'          {call};')
            else:
                lines.append(f'          {call}')
    lines.append('    }')

    # ApplySettings() for behaviors with settings
    if setting_matches:
        lines.append('')
        lines.append('    void ApplySettings(BehaviorFactory &factory) const {')
        for (s, local_idx), sfname, sinfo in zip(setting_matches, setting_fields, setting_resolved):
            member = f'm_Sett{sfname}'
            lines.append(f'        factory.Local({local_idx}, {member});')
        lines.append('        factory.ApplySettings();')
        lines.append('    }')
        lines.append('')
        lines.append('    void ApplySettings(BehaviorRunner &runner) const {')
        for (s, local_idx), sfname, sinfo in zip(setting_matches, setting_fields, setting_resolved):
            member = f'm_Sett{sfname}'
            lines.append(f'        runner.Local({local_idx}, {member});')
        lines.append('        runner.ApplySettings();')
        lines.append('    }')

    # Member variables for input params
    if input_params:
        lines.append('')
        for p, fname, info in zip(input_params, in_fields, in_resolved):
            member = to_member_name(fname)
            if info.get('enum_values') and info['setter'] == 'value':
                cpp_type = fname
                default = f'{fname}(0)'
            else:
                cpp_type = info['cpp_type']
                default = info['default']
            comment = ''
            if info.get('unknown'):
                comment = f'  // Unknown type: {info.get("raw_name", "?")}'
            lines.append(f'    {cpp_type} {member} = {default};{comment}')

    # Settings members
    if setting_matches:
        lines.append('')
        lines.append('    bool m_HasSettings = false;')
        for (s, local_idx), sfname, sinfo in zip(setting_matches, setting_fields, setting_resolved):
            member = f'm_Sett{sfname}'
            cpp_type = sinfo['cpp_type']
            default = sinfo['default']
            lines.append(f'    {cpp_type} {member} = {default};')

    lines.append('};')
    return '\n'.join(lines)


def generate_dll_header(dll: dict, resolver: TypeResolver) -> str:
    """Generate a complete header file for one DLL's behaviors."""
    dll_name = dll.get('name', 'unknown.dll')
    ns_name = to_namespace(dll_name)
    guard = f'BML_BEHAVIORS_{ns_name.upper()}_H'

    lines = []
    lines.append(f'// Auto-generated from {dll_name} -- do not edit manually.')
    lines.append(f'#ifndef {guard}')
    lines.append(f'#define {guard}')
    lines.append('')
    lines.append('#include "BML/Behavior.h"')
    lines.append('#include "CKAll.h"')
    lines.append('')
    lines.append(f'namespace bml::{ns_name} {{')
    lines.append('')

    for beh in dll.get('behaviors', []):
        code = generate_behavior(beh, resolver)
        if code:
            lines.append(code)
            lines.append('')

    lines.append(f'}} // namespace bml::{ns_name}')
    lines.append('')
    lines.append(f'#endif // {guard}')
    lines.append('')

    return '\n'.join(lines)


def generate_master_include(dll_headers: list[tuple[str, str]]) -> str:
    """Generate the All.h master include header."""
    lines = []
    lines.append('// Auto-generated master include -- do not edit manually.')
    lines.append('#ifndef BML_BEHAVIORS_ALL_H')
    lines.append('#define BML_BEHAVIORS_ALL_H')
    lines.append('')

    for filename, _ in sorted(dll_headers):
        lines.append(f'#include "BML/Behaviors/{filename}"')

    lines.append('')
    lines.append('#endif // BML_BEHAVIORS_ALL_H')
    lines.append('')

    return '\n'.join(lines)


# -- Main ---------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description='Generate behavior wrapper headers')
    parser.add_argument('--behaviors', default='data/behaviors.json',
                        help='Path to behaviors.json')
    parser.add_argument('--types', default='data/parameter_types.json',
                        help='Path to parameter_types.json')
    parser.add_argument('--mapping', default='data/type_mapping.json',
                        help='Path to type_mapping.json')
    parser.add_argument('--outdir', default='include/BML/Behaviors',
                        help='Output directory for generated headers')
    args = parser.parse_args()

    behaviors_path = Path(args.behaviors)
    mapping_path = Path(args.mapping)
    types_path = Path(args.types)
    outdir = Path(args.outdir)

    if not behaviors_path.exists():
        print(f'Error: {behaviors_path} not found', file=sys.stderr)
        print('Run BuildingBlockExporter with --json first to extract behavior metadata.',
              file=sys.stderr)
        return 1

    if not mapping_path.exists():
        print(f'Error: {mapping_path} not found', file=sys.stderr)
        return 1

    with open(behaviors_path, encoding='utf-8') as f:
        catalog = json.load(f)

    with open(mapping_path, encoding='utf-8') as f:
        type_mapping = json.load(f)

    param_types = None
    if types_path.exists():
        with open(types_path, encoding='utf-8') as f:
            param_types = json.load(f)

    resolver = TypeResolver(type_mapping, param_types)

    outdir.mkdir(parents=True, exist_ok=True)

    dll_headers = []
    total_behaviors = 0
    variable_behaviors = 0
    skipped_behaviors = 0

    for dll in catalog.get('dlls', []):
        ns_name = to_namespace(dll.get('name', 'unknown.dll'))
        filename = f'{ns_name}.h'

        header_content = generate_dll_header(dll, resolver)

        out_path = outdir / filename
        with open(out_path, 'w', encoding='utf-8', newline='\n') as f:
            f.write(header_content)

        behaviors = dll.get('behaviors', [])
        beh_count = len(behaviors)
        var_count = sum(1 for b in behaviors if is_variable_params(b))
        skip_count = sum(1 for b in behaviors
                         if not to_pascal_case(b.get('name', ''))
                         or not is_valid_guid(b.get('guid', [0, 0])))
        total_behaviors += beh_count
        variable_behaviors += var_count
        skipped_behaviors += skip_count

        dll_headers.append((filename, ns_name))
        print(f'  {filename}: {beh_count} behaviors ({var_count} variable-param'
              f'{f", {skip_count} skipped" if skip_count else ""})')

    all_content = generate_master_include(dll_headers)
    all_path = outdir / 'All.h'
    with open(all_path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(all_content)

    wrapped = total_behaviors - variable_behaviors - skipped_behaviors
    pct = (wrapped / total_behaviors * 100) if total_behaviors > 0 else 0
    print(f'\nGenerated {len(dll_headers)} headers + All.h')
    print(f'Total behaviors: {total_behaviors}')
    print(f'Fully wrapped: {wrapped} ({pct:.0f}%)')
    print(f'Variable-param (GUID only): {variable_behaviors}')
    if skipped_behaviors:
        print(f'Skipped (bad name/GUID): {skipped_behaviors}')

    return 0


if __name__ == '__main__':
    sys.exit(main())

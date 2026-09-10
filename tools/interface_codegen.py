#!/usr/bin/env python3
"""Generate a typed native BML interface header from a compact definition."""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path


ID_RE = re.compile(r"^[a-z][a-z0-9_-]*(?:\.[a-z0-9][a-z0-9_-]*)+$")
NAME_RE = re.compile(r"^[a-z][a-z0-9_]*$")
CPP_NAME_RE = re.compile(r"^[A-Za-z][A-Za-z0-9]*$")
VERSION_RE = re.compile(r"^(\d+)\.(\d+)$")
PROVIDER_VERSION_RE = re.compile(r"^(\d+)\.(\d+)\.(\d+)(?:[-+].*)?$")
TYPE_MAP = {
    "bool": "bool",
    "int": "int",
    "float": "float",
    "double": "double",
    "int64": "int64_t",
    "uint64": "uint64_t",
    "string": "const char *",
}


class InterfaceDefinitionError(ValueError):
    pass


@dataclass(frozen=True)
class Parameter:
    type: str
    name: str


@dataclass(frozen=True)
class Function:
    name: str
    parameters: tuple[Parameter, ...]
    output: Parameter | None


@dataclass(frozen=True)
class Interface:
    interface_id: str
    major: int
    minor: int
    functions: tuple[Function, ...]


def camel(value: str) -> str:
    return "".join(part[:1].upper() + part[1:] for part in value.split("_") if part)


def strip_comment(line: str) -> str:
    positions = [position for marker in ("#", "//")
                 if (position := line.find(marker)) >= 0]
    if positions:
        line = line[:min(positions)]
    return line.strip().rstrip(";").strip()


def parse_parameter(text: str, context: str) -> Parameter:
    parts = text.split()
    if len(parts) != 2:
        raise InterfaceDefinitionError(
            f"{context} must be written as '<type> <name>'")
    type_name, name = parts
    if type_name not in TYPE_MAP:
        supported = ", ".join(TYPE_MAP)
        raise InterfaceDefinitionError(
            f"{context} uses unsupported type {type_name!r}; choose {supported}")
    if not NAME_RE.fullmatch(name):
        raise InterfaceDefinitionError(
            f"{context} name {name!r} must use lowercase snake_case")
    return Parameter(type_name, name)


def parse_interface(text: str) -> Interface:
    lines = [(number, strip_comment(line))
             for number, line in enumerate(text.splitlines(), 1)]
    lines = [(number, line) for number, line in lines if line]
    if not lines:
        raise InterfaceDefinitionError("definition is empty")

    header_number, header = lines[0]
    header_match = re.fullmatch(r"interface\s+(\S+)\s+(\d+\.\d+)", header)
    if not header_match:
        raise InterfaceDefinitionError(
            f"line {header_number}: expected 'interface <id> <major>.<minor>'")
    interface_id, version = header_match.groups()
    if not ID_RE.fullmatch(interface_id):
        raise InterfaceDefinitionError(
            f"line {header_number}: interface id must be lowercase and owner-prefixed")
    version_match = VERSION_RE.fullmatch(version)
    assert version_match
    major, minor = (int(value) for value in version_match.groups())
    if major < 1:
        raise InterfaceDefinitionError("interface major version must be at least 1")

    functions: list[Function] = []
    generated_names: set[str] = set()
    for line_number, line in lines[1:]:
        function_match = re.fullmatch(
            r"fn\s+([a-z][a-z0-9_]*)\s*\((.*?)\)"
            r"(?:\s*->\s*([a-z0-9]+)\s+([a-z][a-z0-9_]*))?",
            line,
        )
        if not function_match:
            raise InterfaceDefinitionError(
                f"line {line_number}: expected "
                "'fn name(<type> <name>, ...) [-> <type> <name>]'"
            )
        name, inputs, output_type, output_name = function_match.groups()
        parameters = tuple(
            parse_parameter(item.strip(), f"line {line_number} parameter")
            for item in inputs.split(",") if item.strip()
        )
        parameter_names = [parameter.name for parameter in parameters]
        if len(parameter_names) != len(set(parameter_names)):
            raise InterfaceDefinitionError(
                f"line {line_number}: parameter names must be unique")
        output = None
        if output_type is not None:
            output = parse_parameter(
                f"{output_type} {output_name}", f"line {line_number} output")
            if output.type == "string":
                raise InterfaceDefinitionError(
                    f"line {line_number}: string outputs need explicit storage; "
                    "use an integer/status result or IMC instead")
        generated_name = camel(name)
        if generated_name in generated_names:
            raise InterfaceDefinitionError(
                f"line {line_number}: generated member {generated_name} is duplicated")
        generated_names.add(generated_name)
        functions.append(Function(name, parameters, output))

    if not functions:
        raise InterfaceDefinitionError("an interface must declare at least one function")
    return Interface(interface_id, major, minor, tuple(functions))


def snapshot(interface: Interface) -> dict[str, object]:
    return {
        "format": 1,
        "interface": interface.interface_id,
        "version": {"major": interface.major, "minor": interface.minor},
        "functions": [
            {
                "name": function.name,
                "parameters": [
                    {"type": parameter.type, "name": parameter.name}
                    for parameter in function.parameters
                ],
                "output": None if function.output is None else {
                    "type": function.output.type,
                    "name": function.output.name,
                },
            }
            for function in interface.functions
        ],
    }


def canonical_json(value: dict[str, object]) -> str:
    return json.dumps(value, indent=2, ensure_ascii=False) + "\n"


def read_lock(path: Path) -> dict[str, object]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as error:
        raise InterfaceDefinitionError(
            f"interface lock {path} does not exist; run 'bml interface update'") from error
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise InterfaceDefinitionError(f"interface lock {path}: {error}") from error
    if not isinstance(value, dict) or value.get("format") != 1:
        raise InterfaceDefinitionError(f"interface lock {path} has an unsupported format")
    return value


def check_evolution(previous: dict[str, object], current: dict[str, object]) -> None:
    previous_version = previous.get("version")
    current_version = current["version"]
    if not isinstance(previous_version, dict) or not isinstance(current_version, dict):
        raise InterfaceDefinitionError("interface lock has malformed version data")
    old_major = previous_version.get("major")
    old_minor = previous_version.get("minor")
    new_major = current_version.get("major")
    new_minor = current_version.get("minor")
    if previous.get("interface") != current.get("interface"):
        raise InterfaceDefinitionError(
            "interface id changed; create a new definition file instead")
    if not all(isinstance(value, int)
               for value in (old_major, old_minor, new_major, new_minor)):
        raise InterfaceDefinitionError("interface lock has malformed version numbers")
    if new_major < old_major or (new_major == old_major and new_minor < old_minor):
        raise InterfaceDefinitionError("interface version cannot move backwards")
    if new_major != old_major:
        return

    old_functions = previous.get("functions")
    new_functions = current.get("functions")
    if not isinstance(old_functions, list) or not isinstance(new_functions, list):
        raise InterfaceDefinitionError("interface lock has malformed function data")
    if new_functions[:len(old_functions)] != old_functions:
        raise InterfaceDefinitionError(
            "major version is unchanged, so existing functions must stay in the same order "
            "with the same names and types; bump the major version for a breaking change")
    if len(new_functions) > len(old_functions) and new_minor <= old_minor:
        raise InterfaceDefinitionError(
            "appending a function requires a higher minor version")


def c_parameter(parameter: Parameter) -> str:
    return f"{TYPE_MAP[parameter.type]} {parameter.name}"


def emit_header(interface: Interface, provider_id: str, provider_version: str,
                namespace: str) -> str:
    version_match = PROVIDER_VERSION_RE.fullmatch(provider_version)
    if not version_match:
        raise InterfaceDefinitionError("provider version must be semantic x.y.z")
    if not CPP_NAME_RE.fullmatch(namespace):
        raise InterfaceDefinitionError(
            "generated namespace must begin with a letter and contain only letters and digits")
    if not ID_RE.fullmatch(provider_id):
        raise InterfaceDefinitionError("provider id must be lowercase and owner-prefixed")

    prefix = namespace.upper()
    interface_type = f"{namespace}Interface"
    traits_type = f"{namespace}Traits"
    lines = [
        "// Generated by tools/interface_codegen.py. Do not edit by hand.",
        "#pragma once",
        "",
        "#include <BML/Interface.h>",
        "#include <stdbool.h>",
        "#include <stdint.h>",
        "",
        f'#define BML_{prefix}_PROVIDER_ID "{provider_id}"',
        f"#define BML_{prefix}_PROVIDER_VERSION_MAJOR {version_match.group(1)}",
        f"#define BML_{prefix}_PROVIDER_VERSION_MINOR {version_match.group(2)}",
        f"#define BML_{prefix}_PROVIDER_VERSION_PATCH {version_match.group(3)}",
        "",
        f'#define BML_{prefix}_INTERFACE_ID "{interface.interface_id}"',
        f"#define BML_{prefix}_INTERFACE_MAJOR {interface.major}u",
        f"#define BML_{prefix}_INTERFACE_MINOR {interface.minor}u",
        "",
        f"typedef struct {interface_type} {{",
        "    BML_InterfaceHeader Header;",
    ]
    for function in interface.functions:
        parameters = [c_parameter(parameter) for parameter in function.parameters]
        if function.output is not None:
            output_name = "out" + camel(function.output.name)
            parameters.append(f"{TYPE_MAP[function.output.type]} *{output_name}")
        parameter_text = ", ".join(parameters) if parameters else "void"
        lines.append(
            f"    int(BML_CDECL *{camel(function.name)})({parameter_text});")
    lines += [
        f"}} {interface_type};",
        "",
        "#ifdef __cplusplus",
        "#include <BML/Interface.hpp>",
        "",
        f"BML_DECLARE_INTERFACE_TRAITS({traits_type}, {interface_type},",
        f"                             BML_{prefix}_INTERFACE_ID,",
        f"                             BML_{prefix}_INTERFACE_MAJOR,",
        f"                             {camel(interface.functions[-1].name)});",
        "#endif",
        "",
    ]
    return "\n".join(lines)


def write_if_changed(path: Path, text: str, check: bool) -> None:
    current = path.read_text(encoding="utf-8") if path.exists() else None
    if current == text:
        return
    if check:
        raise InterfaceDefinitionError(f"generated output is stale: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--provider-id", required=True)
    parser.add_argument("--provider-version", required=True)
    parser.add_argument("--namespace", required=True)
    parser.add_argument("--update-lock", action="store_true")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.update_lock and args.check:
            raise InterfaceDefinitionError("--update-lock and --check cannot be combined")
        interface = parse_interface(args.input.read_text(encoding="utf-8"))
        current = snapshot(interface)
        lock_path = args.input.with_suffix(args.input.suffix + ".lock")
        if args.update_lock:
            if lock_path.exists():
                check_evolution(read_lock(lock_path), current)
            write_if_changed(lock_path, canonical_json(current), False)
        else:
            previous = read_lock(lock_path)
            check_evolution(previous, current)
            if canonical_json(previous) != canonical_json(current):
                raise InterfaceDefinitionError(
                    f"interface lock is stale: {lock_path}; review the change and run "
                    "'bml interface update'")
        header = emit_header(
            interface, args.provider_id, args.provider_version, args.namespace)
        write_if_changed(args.output, header, args.check)
    except (OSError, UnicodeError, InterfaceDefinitionError) as error:
        print(f"interface_codegen: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

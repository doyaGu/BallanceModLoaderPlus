#!/usr/bin/env python3
"""The single Developer Workflow interface for Native and Script BML+ Mods."""

from __future__ import annotations

import argparse
import filecmp
import json
import locale
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import time
from typing import Any
import zipfile


PROFILES = ("basic", "interface-provider", "interface-consumer", "imc-provider")
MOD_KINDS = ("native", "script")
CONFIGURATIONS = ("Debug", "Release", "RelWithDebInfo")
MOD_ID_PATTERN = re.compile(r"^[a-z][a-z0-9_-]*(?:\.[a-z0-9][a-z0-9_-]*)+$")
TARGET_PATTERN = re.compile(r"^[A-Za-z_][A-Za-z0-9_.+-]*$")
VERSION_PATTERN = re.compile(
    r"^(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)$"
)
TOKEN_PATTERN = re.compile(r"__[A-Z][A-Z0-9_]*__")
TEXT_EXTENSIONS = {
    ".cpp",
    ".c",
    ".h",
    ".hpp",
    ".txt",
    ".md",
    ".imc",
    ".bml-interface",
    ".lock",
}
SCRIPT_EXCLUDED_DIRECTORIES = {
    ".bml",
    ".git",
    ".hg",
    ".idea",
    ".svn",
    ".vscode",
    "__pycache__",
    "dist",
}
SCRIPT_EXCLUDED_FILES = {
    ".DS_Store",
    ".gitattributes",
    ".gitignore",
    "Thumbs.db",
    "as.predefined",
    "bml",
    "bml.cmd",
    "bml.mod.json",
    "bml.py",
}
SCRIPT_EXCLUDED_EXTENSIONS = {".code-workspace", ".pyc", ".pyo"}
SCRIPT_DEPLOYMENT_MARKER = ".bml-managed.json"


class BmlError(RuntimeError):
    pass


def print_help(verbose: bool = False) -> None:
    print(
        """BML+ Developer Workflow

  bml new script <mod-id>   Create the recommended beginner project.
  bml new native <mod-id>   Create a C++ project.
  bml run                   Prepare, start Ballance, and show this Mod's log.
  bml build                 Prepare without changing or starting Ballance.
  bml pack                  Produce the publishable artifact in dist.

First use:
  Windows: <BML-SDK>\\scripts\\bml.cmd new script yourname.my-mod
  POSIX:   <BML-SDK>/scripts/bml new script yourname.my-mod
  cd MyMod
  Windows: .\\bml run
  POSIX:   ./bml run

Native and Script Mods use the same commands. Their different build and reload
behavior stays behind the workflow. Missing folders are requested once and
remembered locally.
Run 'bml help --verbose' to see advanced commands."""
    )
    if verbose:
        print(
            """

Advanced:
  bml init [id] [--project <folder>] [--kind native|script]
  bml new native <id> --profile interface-provider
  bml new native <id> --profile interface-consumer --provider-id <id>
  bml new native <id> --profile imc-provider
  bml interface update
  bml interface check"""
        )


def cpp_string(value: str) -> str:
    return (
        value.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\r", "\\r")
        .replace("\n", "\\n")
        .replace("\t", "\\t")
    )


def class_name(mod_id: str) -> str:
    leaf = mod_id.rsplit(".", 1)[-1]
    result = "".join(part[:1].upper() + part[1:] for part in re.split(r"[^A-Za-z0-9]+", leaf) if part)
    if not result or not re.match(r"^[A-Za-z_]", result):
        result = f"Mod{result}"
    if not result.endswith("Mod"):
        result += "Mod"
    return result


def default_name(mod_id: str) -> str:
    leaf = mod_id.rsplit(".", 1)[-1]
    words = [word for word in re.split(r"[-_]", leaf) if word]
    return " ".join(word[:1].upper() + word[1:] for word in words)


def default_author() -> str:
    git = shutil.which("git")
    if git:
        result = subprocess.run(
            [git, "config", "--get", "user.name"],
            capture_output=True,
            check=False,
        )
        output = decode_output(result.stdout).strip()
        if result.returncode == 0 and output:
            return output
    return os.environ.get("USERNAME") or os.environ.get("USER") or "Mod Author"


def imc_api_id(mod_id: str) -> str:
    segments = []
    for segment in mod_id.split("."):
        clean = re.sub(r"[^a-z0-9]", "", segment)
        if not clean:
            raise BmlError(f"Cannot derive a message API id from Mod id '{mod_id}'.")
        segments.append(clean)
    return ".".join([*segments, "api"])


def read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise BmlError(f"Could not read {path}: {error}") from error
    if not isinstance(value, dict):
        raise BmlError(f"Expected a JSON object in {path}.")
    return value


def decode_output(value: bytes | None) -> str:
    if not value:
        return ""
    try:
        return value.decode("utf-8")
    except UnicodeDecodeError:
        return value.decode(locale.getpreferredencoding(False), errors="replace")


def write_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def require_directory(value: str | Path | None, label: str) -> Path | None:
    if value is None or not str(value).strip():
        return None
    path = Path(value).expanduser().resolve()
    if not path.is_dir():
        raise BmlError(f"{label} does not exist: {path}")
    return path


def require_file(path: Path, label: str) -> None:
    if not path.is_file():
        raise BmlError(f"{label} is missing: {path}")


def find_sdk_config(sdk: Path) -> Path | None:
    candidates = [sdk / directory / "cmake/BML/BMLConfig.cmake" for directory in ("lib", "lib64")]
    candidates.extend((sdk / "lib").glob("*/cmake/BML/BMLConfig.cmake"))
    configs = [path for path in candidates if path.is_file()]
    if len(configs) > 1:
        raise BmlError(f"Multiple BML packages in {sdk}; use one SDK per runtime and architecture.")
    return configs[0] if configs else None


def is_inside(path: Path, directory: Path) -> bool:
    try:
        common = Path(os.path.commonpath([path, directory]))
    except ValueError:
        return False
    return os.path.normcase(str(common)) == os.path.normcase(str(directory))


def validate_identity(mod_id: str, version: str, name: str, author: str) -> None:
    if not MOD_ID_PATTERN.fullmatch(mod_id):
        raise BmlError("Mod id must look like 'yourname.my-mod' and use lowercase letters.")
    if not VERSION_PATTERN.fullmatch(version):
        raise BmlError("Version must look like '1.0.0'.")
    for label, value in (("Name", name), ("Author", author)):
        if not value.strip() or "\n" in value or "\r" in value:
            raise BmlError(f"{label} must be one non-empty line.")


def copy_workflow_files(script_path: Path, destination: Path) -> None:
    for filename in ("bml.py", "bml.cmd", "bml"):
        source = script_path.with_name(filename)
        require_file(source, "BML tool")
        shutil.copy2(source, destination / filename)


def create_native_mod(args: argparse.Namespace, script_path: Path) -> None:
    mod_id = args.mod_id or input("Mod id (example: yourname.my-mod): ").strip()
    name = args.name or default_name(mod_id)
    author = args.author or default_author()
    validate_identity(mod_id, args.version, name, author)

    provider_id = args.provider_id
    if args.profile == "interface-consumer":
        if not provider_id:
            raise BmlError("The interface-consumer profile needs --provider-id.")
        if not MOD_ID_PATTERN.fullmatch(provider_id):
            raise BmlError("Provider id must look like 'yourname.provider'.")
    elif provider_id:
        raise BmlError("--provider-id is only valid with --profile interface-consumer.")

    target = class_name(mod_id)
    symbol = target.upper()
    provider_target = class_name(provider_id) if provider_id else ""
    provider_symbol = provider_target.upper()
    provider_package = f"{provider_target}Interface" if provider_target else ""
    interface_id = f"{mod_id}.value"
    api_id = imc_api_id(mod_id)
    api_namespace = "::".join(part[:1].upper() + part[1:] for part in api_id.split("."))
    api_header = f"{api_id.replace('.', '_')}_imc.hpp"
    description = args.description or f"{name} native mod"

    sdk_root = script_path.parent.parent
    templates_root = sdk_root / "templates"
    template_names = {
        "basic": "native-mod-template",
        "interface-provider": "native-interface-provider-template",
        "interface-consumer": "native-interface-consumer-template",
        "imc-provider": "native-imc-provider-template",
    }
    template = (templates_root / template_names[args.profile]).resolve()
    destination = Path(args.destination or (Path.cwd() / target)).expanduser().resolve()
    if is_inside(destination, templates_root.resolve()):
        raise BmlError("Destination must not be inside the SDK templates directory.")
    if destination.exists():
        raise BmlError(f"Destination already exists: {destination}")
    for relative in ("CMakeLists.txt", "src/HelloMod.cpp", "README.md"):
        require_file(template / relative, "Template file")

    shutil.copytree(template, destination)
    copy_workflow_files(script_path, destination)

    version_core = args.version.split("-", 1)[0].split("+", 1)[0]
    version_parts = version_core.split(".")
    tokens = {
        "__MOD_ID_CPP__": cpp_string(mod_id),
        "__MOD_VERSION_CPP__": cpp_string(args.version),
        "__MOD_NAME_CPP__": cpp_string(name),
        "__MOD_AUTHOR_CPP__": cpp_string(author),
        "__MOD_DESCRIPTION_CPP__": cpp_string(description),
        "__INTERFACE_ID_CPP__": cpp_string(interface_id),
        "__MOD_ID__": mod_id,
        "__MOD_NAME__": name,
        "__MOD_VERSION_MAJOR__": version_parts[0],
        "__MOD_VERSION_MINOR__": version_parts[1],
        "__MOD_VERSION_PATCH__": version_parts[2],
        "__INTERFACE_ID__": interface_id,
        "__PROVIDER_ID__": provider_id or "",
        "__PROVIDER_CLASS__": provider_target,
        "__PROVIDER_SYMBOL__": provider_symbol,
        "__PROVIDER_PACKAGE__": provider_package,
        "__IMC_API_ID__": api_id,
        "__IMC_NAMESPACE__": api_namespace,
        "__IMC_HEADER__": api_header,
    }
    for path in destination.rglob("*"):
        if not path.is_file() or (path.suffix not in TEXT_EXTENSIONS and path.name != "CMakeLists.txt"):
            continue
        text = path.read_text(encoding="utf-8")
        for token, value in tokens.items():
            text = text.replace(token, value)
        text = text.replace("HELLOMOD", symbol).replace("HelloMod", target)
        path.write_text(text, encoding="utf-8")

    cmake_path = destination / "CMakeLists.txt"
    source_path = destination / "src" / "HelloMod.cpp"
    renamed_source = destination / "src" / f"{target}.cpp"
    readme_path = destination / "README.md"
    cmake_text = cmake_path.read_text(encoding="utf-8").replace("1.0.0", version_core)
    source_text = source_path.read_text(encoding="utf-8")
    source_text = source_text.replace(f'return "{target}";', f'return "{cpp_string(mod_id)}";')
    source_text = source_text.replace(f'definition.Id = "{target}";', f'definition.Id = "{cpp_string(mod_id)}";')
    source_text = source_text.replace(f'.Id = "{target}",', f'.Id = "{cpp_string(mod_id)}",')
    source_text = source_text.replace('"Hello Mod"', f'"{cpp_string(name)}"')
    source_text = source_text.replace('"Template"', f'"{cpp_string(author)}"')
    source_text = source_text.replace('"1.0.0"', f'"{cpp_string(args.version)}"')
    source_text = source_text.replace(
        '"Minimal example mod for BML+"', f'"{cpp_string(description)}"'
    )
    readme_text = readme_path.read_text(encoding="utf-8").replace(
        "# BML+ Native Mod Template", f"# {name}"
    )
    cmake_path.write_text(cmake_text, encoding="utf-8")
    source_path.write_text(source_text, encoding="utf-8")
    readme_path.write_text(readme_text, encoding="utf-8")
    source_path.rename(renamed_source)

    unresolved = []
    for path in destination.rglob("*"):
        if (
            path.is_file()
            and (path.suffix in TEXT_EXTENSIONS or path.name == "CMakeLists.txt")
            and TOKEN_PATTERN.search(path.read_text(encoding="utf-8", errors="ignore"))
        ):
            unresolved.append(str(path))
    if unresolved:
        raise BmlError(f"Generated project contains unresolved template tokens: {', '.join(unresolved)}")

    manifest: dict[str, Any] = {
        "format": 1,
        "kind": "native",
        "id": mod_id,
        "name": name,
        "version": args.version,
        "profile": args.profile,
        "target": target,
    }
    if args.profile == "interface-provider":
        manifest["interfaces"] = ["api/value.bml-interface"]
    elif args.profile == "interface-consumer":
        manifest["providerId"] = provider_id
        manifest["interfacePackage"] = provider_package
    elif args.profile == "imc-provider":
        manifest["imc"] = ["api/service.imc"]
    write_json(destination / "bml.mod.json", manifest)
    (destination / ".gitignore").write_text(".bml/\nbuild/\n", encoding="utf-8")
    local_sdk = str(sdk_root.resolve()) if find_sdk_config(sdk_root) else ""
    write_json(
        destination / ".bml/settings.json",
        {
            "format": 1,
            "bmlSdk": local_sdk,
            "ballanceRoot": "",
            "generator": "",
        },
    )

    print(f"Created {name}")
    print(f"  {destination}")
    print("\nEdit:")
    print(f"  src/{target}.cpp")
    print("Run:")
    print(f"  cd \"{destination}\"")
    print("  .\\bml run" if os.name == "nt" else "  ./bml run")


def find_script_entries(project: Path) -> list[Path]:
    return sorted(
        path
        for path in project.glob("*.mod.as")
        if path.is_file()
    )


def read_script_metadata(entry: Path) -> dict[str, str]:
    text = entry.read_text(encoding="utf-8")
    declaration = re.search(r"\[bml\.mod\b(?P<body>.*?)\]", text, re.DOTALL)
    if not declaration:
        raise BmlError(f"Script entry has no [bml.mod] declaration: {entry}")
    return {
        name: value
        for name, value in re.findall(
            r"([A-Za-z][A-Za-z0-9_]*)\s*=\s*\"([^\"\r\n]*)\"",
            declaration.group("body"),
        )
    }


def create_script_mod(args: argparse.Namespace, script_path: Path) -> None:
    mod_id = args.mod_id or input("Mod id (example: yourname.my-mod): ").strip()
    name = args.name or default_name(mod_id)
    author = args.author or default_author()
    validate_identity(mod_id, args.version, name, author)
    description = args.description or f"{name} script mod"

    target = class_name(mod_id)
    sdk_root = script_path.parent.parent
    templates_root = sdk_root / "templates"
    template = (templates_root / "script-mod-template").resolve()
    if not template.is_dir():
        raise BmlError("This SDK was built without Script Mod authoring.")
    destination = Path(args.destination or (Path.cwd() / target)).expanduser().resolve()
    if is_inside(destination, templates_root.resolve()):
        raise BmlError("Destination must not be inside the SDK templates directory.")
    if destination.exists():
        raise BmlError(f"Destination already exists: {destination}")
    for relative in ("HelloScript.mod.as", "README.md"):
        require_file(template / relative, "Template file")

    shutil.copytree(template, destination)
    copy_workflow_files(script_path, destination)
    source_path = destination / "HelloScript.mod.as"
    entry_path = destination / f"{target}.mod.as"
    readme_path = destination / "README.md"
    replacements = {
        "example.hello.script": cpp_string(mod_id),
        "Hello Script": cpp_string(name),
        "Your Name": cpp_string(author),
        "1.0.0": cpp_string(args.version),
        "Minimal BML+ script mod": cpp_string(description),
        "HelloScript": target,
    }
    source = source_path.read_text(encoding="utf-8")
    for old, new in replacements.items():
        source = source.replace(old, new)
    readme = readme_path.read_text(encoding="utf-8")
    readme = readme.replace("# BML+ Script Mod Template", f"# {name}")
    readme = readme.replace("example.hello.script", mod_id)
    readme = readme.replace("Hello Script", name)
    readme = readme.replace("HelloScript", target)
    source_path.write_text(source, encoding="utf-8")
    readme_path.write_text(readme, encoding="utf-8")
    source_path.rename(entry_path)

    write_json(
        destination / "bml.mod.json",
        {
            "format": 1,
            "kind": "script",
            "entry": entry_path.name,
            "deployName": target,
        },
    )
    (destination / ".gitignore").write_text(".bml/\ndist/\n", encoding="utf-8")
    local_sdk = str(sdk_root.resolve()) if (sdk_root / "scripts/bml.py").is_file() else ""
    write_json(
        destination / ".bml/settings.json",
        {"format": 1, "bmlSdk": local_sdk, "ballanceRoot": ""},
    )

    print(f"Created {name}")
    print(f"  {destination}")
    print("\nEdit:")
    print(f"  {entry_path.name}")
    print("Run:")
    print(f"  cd \"{destination}\"")
    print("  .\\bml run" if os.name == "nt" else "  ./bml run")


def find_local_sdk(script_path: Path, explicit: str | None = None) -> Path:
    candidates = [explicit, script_path.parent.parent]
    for candidate in candidates:
        if not candidate:
            continue
        path = Path(candidate).expanduser().resolve()
        if find_sdk_config(path):
            return path
    raise BmlError("Run init from an extracted BML+ SDK, or pass --bml-sdk.")


def infer_cmake_target(cmake_text: str) -> str | None:
    uncommented = "\n".join(line.split("#", 1)[0] for line in cmake_text.splitlines())
    matches = re.findall(
        r"\bbml_add_mod\s*\(\s*([A-Za-z_][A-Za-z0-9_.+-]*)",
        uncommented,
        flags=re.IGNORECASE,
    )
    unique = list(dict.fromkeys(matches))
    if len(unique) == 1:
        return unique[0]
    if len(unique) > 1:
        raise BmlError("More than one bml_add_mod target was found; pass --target.")
    return None


def infer_cmake_version(cmake_text: str) -> str:
    match = re.search(
        r"\bproject\s*\([^)]*?\bVERSION\s+(\d+\.\d+\.\d+)",
        cmake_text,
        flags=re.IGNORECASE | re.DOTALL,
    )
    return match.group(1) if match else "1.0.0"


def append_local_ignore(project: Path, entries: tuple[str, ...] = (".bml/",)) -> None:
    path = project / ".gitignore"
    text = path.read_text(encoding="utf-8") if path.is_file() else ""
    lines = text.splitlines()
    changed = False
    for entry in entries:
        if entry not in lines:
            if text and not text.endswith("\n"):
                text += "\n"
            text += f"{entry}\n"
            lines.append(entry)
            changed = True
    if changed:
        path.write_text(text, encoding="utf-8")


def init_existing_mod(args: argparse.Namespace, script_path: Path) -> None:
    requested_project = Path(args.project or Path.cwd()).expanduser().resolve()
    if requested_project.is_file():
        raise BmlError(
            "Move a single-file Script Mod into its own folder before running init; "
            "Player loads the folder in the same way."
        )
    project = requested_project
    if not project.is_dir():
        raise BmlError(f"Project directory does not exist: {project}")
    manifest_path = project / "bml.mod.json"
    if manifest_path.exists():
        raise BmlError(f"This project is already initialized: {manifest_path}")
    cmake_path = project / "CMakeLists.txt"
    script_entries = find_script_entries(project)
    detected = []
    if cmake_path.is_file():
        detected.append("native")
    if script_entries:
        detected.append("script")
    kind = args.kind
    if not kind:
        if len(detected) != 1:
            detail = "both Native and Script inputs" if detected else "no CMakeLists.txt or *.mod.as entry"
            raise BmlError(f"Could not identify this Mod Project: {detail}. Pass --kind.")
        kind = detected[0]
    if kind == "script":
        if len(script_entries) != 1:
            raise BmlError(
                f"A Script Mod Project needs exactly one top-level *.mod.as entry; found {len(script_entries)}."
            )
        entry = script_entries[0]
        metadata = read_script_metadata(entry)
        source_mod_id = metadata.get("id", "")
        if args.mod_id and args.mod_id != source_mod_id:
            raise BmlError(
                f"Requested Mod id '{args.mod_id}' does not match the script entry id '{source_mod_id}'."
            )
        mod_id = source_mod_id
        version = args.version or metadata.get("version", "1.0.0")
        name = args.name or metadata.get("name") or default_name(mod_id)
        validate_identity(mod_id, version, name, "Existing author")
        sdk = find_local_sdk(script_path, args.bml_sdk)
        install_workflow_files(project, script_path)
        manifest = {
            "format": 1,
            "kind": "script",
            "entry": entry.name,
            "deployName": class_name(mod_id),
        }
        write_json(manifest_path, manifest)
        append_local_ignore(project, (".bml/", "dist/"))
        settings_path = project / ".bml/settings.json"
        settings = read_json(settings_path) if settings_path.is_file() else {}
        settings.update({"format": 1, "bmlSdk": str(sdk)})
        settings.setdefault("ballanceRoot", "")
        write_json(settings_path, settings)
        print(f"Initialized {name}")
        print(f"  Script entry: {entry.name}")
        print("  Existing script files were not changed.")
        print("\nRun:")
        print(f"  cd \"{project}\"")
        print("  .\\bml run" if os.name == "nt" else "  ./bml run")
        return

    require_file(cmake_path, "CMake project")
    cmake_text = cmake_path.read_text(encoding="utf-8")

    mod_id = args.mod_id or input("Existing Mod id (example: yourname.my-mod): ").strip()
    if not MOD_ID_PATTERN.fullmatch(mod_id):
        raise BmlError("Mod id must look like 'yourname.my-mod' and use lowercase letters.")
    target = args.target or infer_cmake_target(cmake_text)
    if not target:
        raise BmlError("No bml_add_mod target was found; pass its CMake target with --target.")
    if not TARGET_PATTERN.fullmatch(target):
        raise BmlError(f"Invalid CMake target: {target}")
    artifact = args.artifact or target
    if not TARGET_PATTERN.fullmatch(artifact):
        raise BmlError(f"Invalid artifact name: {artifact}")
    version = args.version or infer_cmake_version(cmake_text)
    if not VERSION_PATTERN.fullmatch(version):
        raise BmlError("Version must look like '1.0.0'.")
    sdk = find_local_sdk(script_path, args.bml_sdk)

    install_workflow_files(project, script_path)

    manifest: dict[str, Any] = {
        "format": 1,
        "kind": "native",
        "id": mod_id,
        "name": args.name or default_name(mod_id),
        "version": version,
        "profile": "existing",
        "target": target,
    }
    if artifact != target:
        manifest["artifact"] = artifact
    write_json(manifest_path, manifest)
    append_local_ignore(project)
    settings_path = project / ".bml/settings.json"
    settings = read_json(settings_path) if settings_path.is_file() else {}
    settings.update({"format": 1, "bmlSdk": str(sdk)})
    settings.setdefault("ballanceRoot", "")
    settings.setdefault("generator", "")
    write_json(settings_path, settings)

    print(f"Initialized {manifest['name']}")
    print(f"  CMake target: {target}")
    print("  Existing CMake and source files were not changed.")
    print("\nRun:")
    print(f"  cd \"{project}\"")
    print("  .\\bml run" if os.name == "nt" else "  ./bml run")


def install_workflow_files(project: Path, script_path: Path) -> None:
    for filename in ("bml.py", "bml.cmd", "bml"):
        source = script_path.parent / filename
        destination = project / filename
        require_file(source, "BML tool")
        if destination.exists() and source.resolve() != destination.resolve():
            raise BmlError(f"Refusing to overwrite existing file: {destination}")
        if source.resolve() != destination.resolve():
            shutil.copy2(source, destination)


def project_state(project_arg: str | None, script_path: Path) -> tuple[Path, dict[str, Any], dict[str, Any]]:
    if project_arg:
        root = Path(project_arg).expanduser().resolve()
    elif (Path.cwd() / "bml.mod.json").is_file():
        root = Path.cwd().resolve()
    elif (script_path.parent / "bml.mod.json").is_file():
        root = script_path.parent.resolve()
    else:
        raise BmlError("Run this command inside a generated Mod folder.")
    if not root.is_dir():
        raise BmlError(f"Project directory does not exist: {root}")
    manifest_path = root / "bml.mod.json"
    require_file(manifest_path, "Project file")
    manifest = read_json(manifest_path)
    kind = manifest.get("kind")
    valid_shape = (
        kind == "native" and bool(manifest.get("id")) and bool(manifest.get("target"))
    ) or (
        kind == "script" and bool(manifest.get("entry")) and bool(manifest.get("deployName"))
    )
    if manifest.get("format") != 1 or not valid_shape:
        raise BmlError(f"Unsupported project file: {manifest_path}")
    if kind == "script":
        entry_name = str(manifest["entry"])
        deploy_name = str(manifest["deployName"])
        if Path(entry_name).name != entry_name or not entry_name.lower().endswith(".mod.as"):
            raise BmlError(f"Unsafe Script entry in {manifest_path}: {entry_name}")
        if not TARGET_PATTERN.fullmatch(deploy_name):
            raise BmlError(f"Unsafe Script deployment name in {manifest_path}: {deploy_name}")
        entry = root / entry_name
        require_file(entry, "Script entry")
        metadata = read_script_metadata(entry)
        mod_id = metadata.get("id", "")
        version = metadata.get("version", "1.0.0")
        name = metadata.get("name") or default_name(mod_id)
        validate_identity(mod_id, version, name, metadata.get("author") or "Script author")
        manifest = {**manifest, "id": mod_id, "name": name, "version": version}
    settings_path = root / ".bml/settings.json"
    settings = read_json(settings_path) if settings_path.is_file() else {}
    return root, manifest, settings


def resolve_sdk(args: argparse.Namespace, script_path: Path, settings: dict[str, Any]) -> Path:
    candidates = [
        getattr(args, "bml_sdk", None),
        settings.get("bmlSdk"),
        os.environ.get("BML_SDK_ROOT"),
        script_path.parent.parent,
    ]
    for candidate in candidates:
        if not candidate:
            continue
        path = Path(candidate).expanduser().resolve()
        if find_sdk_config(path):
            return path
    entered = input("BML+ SDK folder: ").strip()
    path = Path(entered).expanduser().resolve() if entered else Path()
    if entered and find_sdk_config(path):
        return path
    raise BmlError("That folder is not a BML+ SDK.")


def run_checked(command: list[str], failure: str, verbose: bool = False) -> str:
    result = subprocess.run(command, capture_output=True, check=False)
    stdout = decode_output(result.stdout)
    stderr = decode_output(result.stderr)
    output = stdout + stderr
    if result.returncode != 0:
        if output:
            print(output.rstrip())
        raise BmlError(f"{failure} (exit code {result.returncode}).")
    if verbose and output:
        print(output.rstrip())
    return stdout


def find_visual_studio_generator(cmake: str) -> str:
    output = run_checked([cmake, "-E", "capabilities"], "CMake could not list its generators")
    try:
        capabilities = json.loads(output)
    except json.JSONDecodeError as error:
        raise BmlError("CMake returned invalid generator information.") from error
    generators = []
    for item in capabilities.get("generators", []):
        match = re.match(r"^Visual Studio (\d+) ", item.get("name", ""))
        if match and item.get("platformSupport"):
            generators.append((int(match.group(1)), item["name"]))
    if not generators:
        raise BmlError("No Visual Studio C++ tools were found.")
    return max(generators)[1]


def read_abi(path: Path) -> dict[str, Any]:
    abi = read_json(path)
    if (
        abi.get("schema") not in (1, 2)
        or abi.get("runtime") != "sdl"
        or abi.get("platform") not in ("windows", "linux", "macos")
        or abi.get("architecture") not in ("x86", "x64", "arm64")
        or abi.get("host_linkage") not in ("dynamic", "static")
        or not isinstance(abi.get("sdk_abi"), str)
        or not re.fullmatch(r"[0-9a-f]{64}", abi["sdk_abi"])
        or abi.get("configuration") not in CONFIGURATIONS
    ):
        raise BmlError(f"Invalid or unsupported BML ABI descriptor: {path}")
    if abi["schema"] == 2:
        if abi.get("mod_api") != "bml.mod" or abi.get("mod_api_major") != 1:
            raise BmlError(f"Unsupported native Mod C protocol: {path}")
        for name in ("extensions", "extension_abis"):
            values = abi.get(name, {})
            if not isinstance(values, dict) or any(key not in ("engine", "imgui") or not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{64}-(Debug|Release|RelWithDebInfo|MinSizeRel)", value) for key, value in values.items()):
                raise BmlError(f"Invalid native Mod extension ABI: {path}")
        if path.name.endswith(".bmodp.abi.json") and "extensions" not in abi:
            raise BmlError(f"Native Mod extension requirements are missing: {path}")
    return abi


def sdk_abi(sdk: Path) -> dict[str, Any]:
    descriptor = sdk / "share/BML/BML.abi.json"
    if descriptor.is_file():
        return read_abi(descriptor)
    raise BmlError(f"SDL SDK ABI descriptor is missing: {descriptor}")


def check_host(abi: dict[str, Any]) -> None:
    host_platform = {"win32": "windows", "linux": "linux", "darwin": "macos"}.get(sys.platform)
    host_arch = {"amd64": "x64", "x86_64": "x64", "aarch64": "arm64", "arm64": "arm64",
                 "x86": "x86", "i386": "x86", "i686": "x86"}.get(platform.machine().lower())
    compatible_architectures = {host_arch}
    if host_platform == "windows" and host_arch in ("x64", "arm64"):
        compatible_architectures.add("x86")
    if abi["platform"] != host_platform or abi["architecture"] not in compatible_architectures:
        raise BmlError(
            f"This SDK/runtime targets {abi['platform']}/{abi['architecture']}; "
            f"this workflow runs on {host_platform}/{host_arch}. Use a matching SDK/runtime."
        )


def find_player(game: Path) -> Path:
    name = "Player.exe" if os.name == "nt" else "Player"
    candidates = [game / "Bin" / name, game / name]
    players = [path for path in candidates if path.is_file()]
    if len(players) != 1:
        raise BmlError(f"Expected one SDL Player in {game}; found {len(players)}: "
                       + ", ".join(str(path) for path in players))
    player = players[0]
    if os.name != "nt" and not os.access(player, os.X_OK):
        raise BmlError(f"Player is not executable: {player}")
    if player.parent == game and not (game / "base.cmo").is_file():
        raise BmlError(f"A flat SDL Player layout requires base.cmo alongside Player: {game}")
    return player


def game_abi(game: Path) -> dict[str, Any]:
    descriptor = game / "ModLoader/BML.abi.json"
    if not descriptor.is_file():
        raise BmlError(f"SDL runtime ABI descriptor is missing: {descriptor}")
    abi = read_abi(descriptor)
    check_host(abi)
    return abi


def check_abi_match(expected: dict[str, Any], actual: dict[str, Any], label: str) -> None:
    fields = expected if expected.get("schema") == 1 else (
        "runtime", "platform", "architecture", "host_linkage", "mod_api", "mod_api_major")
    for field in fields:
        if actual.get(field) != expected[field]:
            raise BmlError(f"{label} ABI mismatch for {field}: expected {expected[field]!r}, got {actual.get(field)!r}.")
    if expected.get("schema") == 2:
        available = expected.get("extension_abis", {})
        for extension, required in actual.get("extensions", {}).items():
            if required and required != available.get(extension):
                raise BmlError(f"{label} ABI mismatch for {extension}: expected {available.get(extension)!r}, got {required!r}.")


def player_is_running() -> bool:
    if os.name != "nt":
        ps = shutil.which("ps")
        if not ps:
            raise BmlError("Cannot check running Players: ps was not found.")
        try:
            output = run_checked([ps, "-A", "-o", "comm="], "Could not check running Players")
        except OSError as error:
            raise BmlError(f"Could not check running Players: {error}") from error
        return any(Path(line.strip()).name in ("Player", "Player.exe") for line in output.splitlines())
    tasklist = shutil.which("tasklist")
    if not tasklist:
        return False
    result = subprocess.run(
        [tasklist, "/FI", "IMAGENAME eq Player.exe", "/NH"],
        capture_output=True,
        check=False,
    )
    return result.returncode == 0 and "Player.exe" in decode_output(result.stdout)


def save_settings(
    root: Path,
    sdk: Path,
    game: Path | None,
    generator: str,
) -> None:
    write_json(
        root / ".bml/settings.json",
        {
            "format": 1,
            "bmlSdk": str(sdk),
            "ballanceRoot": str(game) if game else "",
            "generator": generator,
        },
    )


def native_build_or_run(args: argparse.Namespace, script_path: Path, install: bool, launch: bool) -> Path:
    root, manifest, settings = project_state(args.project, script_path)
    sdk = resolve_sdk(args, script_path, settings)
    abi = sdk_abi(sdk)
    check_host(abi)
    if abi["host_linkage"] != "dynamic":
        raise BmlError("The SDL static-host SDK workflow is not implemented.")
    configuration = args.configuration or abi["configuration"]
    if abi["schema"] == 1 and configuration != abi["configuration"]:
        raise BmlError(f"This SDK requires configuration {abi['configuration']}; got {configuration}.")

    game_value = getattr(args, "ballance_root", None) or settings.get("ballanceRoot") or os.environ.get("BML_BALLANCE_ROOT")
    if install and not game_value:
        game_value = input("Ballance folder: ").strip()
    game = require_directory(game_value, "Ballance folder")
    if install and game is None:
        raise BmlError("A Ballance folder is required to install and run the Mod.")

    cmake = shutil.which("cmake")
    if not cmake:
        raise BmlError("CMake was not found. Install CMake and reopen this terminal.")
    generator = args.generator or settings.get("generator")
    if not generator:
        if os.name == "nt":
            generator = os.environ.get("CMAKE_GENERATOR") or find_visual_studio_generator(cmake)
        else:
            generator = os.environ.get("CMAKE_GENERATOR") or ("Ninja" if shutil.which("ninja") else "Unix Makefiles")
    player = None
    if install:
        check_abi_match(abi, game_abi(game) or {}, "Game")
        player = find_player(game)
    if install and player_is_running():
        raise BmlError("Ballance is running. Close it before replacing this native Mod.")

    state_dir = root / ".bml"
    # Keep incompatible runtimes/SDKs/configurations out of the same CMake cache.
    build_dir = state_dir / "build" / f"sdl-{abi['sdk_abi']}-{configuration}"
    install_dir = state_dir / "stage"
    prefixes = [sdk]
    if game:
        prefixes.append(game / "ModLoader")
    for value in args.package_root or []:
        path = require_directory(value, "Package folder")
        if path:
            prefixes.append(path)

    print(f"Preparing {manifest['id']}...")
    configure = [
        cmake,
        "-S",
        str(root),
        "-B",
        str(build_dir),
        "-G",
        generator,
        f"-DBML_DIR={find_sdk_config(sdk).parent}",
        f"-DCMAKE_PREFIX_PATH={';'.join(str(path) for path in prefixes)}",
        f"-DCMAKE_INSTALL_PREFIX={install_dir}",
    ]
    configure.append(f"-DCMAKE_BUILD_TYPE={configuration}")
    if generator.startswith("Visual Studio"):
        configure.extend(["-A", {"x86": "Win32", "x64": "x64", "arm64": "ARM64"}[abi["architecture"]]])
    if abi["platform"] == "macos":
        configure.append(f"-DCMAKE_OSX_ARCHITECTURES={abi['architecture'] if abi['architecture'] != 'x64' else 'x86_64'}")
    run_checked(configure, "CMake setup failed", args.verbose)
    print(f"Building {manifest['target']}...")
    run_checked(
        [
            cmake,
            "--build",
            str(build_dir),
            "--config",
            configuration,
            "--target",
            "install",
        ],
        "Build failed",
        args.verbose,
    )
    save_settings(root, sdk, game, generator)
    artifact = str(manifest.get("artifact") or manifest["target"])
    mod_path = install_dir / "Mods" / f"{artifact}.bmodp"
    require_file(mod_path, "Built Mod")
    built_abi = read_abi(mod_path.with_suffix(".bmodp.abi.json"))
    check_abi_match(abi, built_abi, "Built Mod")
    if install:
        check_abi_match(game_abi(game), built_abi, "Game")
        if player_is_running():
            raise BmlError("Ballance is running. Close it before replacing this native Mod.")
        run_checked([cmake, "--install", str(build_dir), "--config", configuration,
                     "--prefix", str(game / "ModLoader")], "Mod installation failed", args.verbose)
        mod_path = game / "ModLoader/Mods" / mod_path.name
    print("Ready:")
    print(f"  {mod_path}")
    if not launch:
        return mod_path

    if game is None:
        raise BmlError("A Ballance folder is required to start the game.")
    player = player or find_player(game)
    log_path = game / "ModLoader/ModLoader.log"
    previous_lines = len(log_path.read_text(encoding="utf-8", errors="replace").splitlines()) if log_path.is_file() else 0
    print("Starting Ballance. Exit the game to return here...")
    result = subprocess.run([str(player)], cwd=player.parent, check=False)
    lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines() if log_path.is_file() else []
    marker = f"[{manifest['id']}/"
    mod_lines = [line for line in lines[previous_lines:] if marker in line]
    print(f"\n{manifest['id']} log:")
    if mod_lines:
        print("\n".join(mod_lines))
    else:
        print(f"No new log lines were found. Full log: {log_path}")
    if result.returncode != 0:
        raise BmlError(f"Ballance exited with code {result.returncode}.")
    return mod_path


def output_path(value: str | None, default: Path, suffix: str) -> Path:
    path = Path(value).expanduser().resolve() if value else default.resolve()
    if path.suffix.lower() != suffix:
        raise BmlError(f"Output must use the {suffix} extension: {path}")
    return path


def replace_output(source: Path, destination: Path, force: bool) -> None:
    if destination.exists() and not force:
        raise BmlError(f"Output already exists: {destination}. Pass --force to replace it.")
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(f".{destination.name}.bml-write")
    try:
        shutil.copy2(source, temporary)
        os.replace(temporary, destination)
    finally:
        if temporary.exists():
            temporary.unlink()


def native_pack(args: argparse.Namespace, script_path: Path) -> None:
    root, manifest, _ = project_state(args.project, script_path)
    artifact = native_build_or_run(args, script_path, install=False, launch=False)
    _, _, settings = project_state(args.project, script_path)
    abi = sdk_abi(resolve_sdk(args, script_path, settings))
    name = str(manifest.get("artifact") or manifest["target"])
    descriptor = artifact.with_suffix(".bmodp.abi.json")
    check_abi_match(abi, read_abi(descriptor), "Packaged Mod")
    destination = output_path(args.output, root / "dist" / f"{name}.zip", ".zip")
    if destination.exists() and not args.force:
        raise BmlError(f"Output already exists: {destination}. Pass --force to replace it.")
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(f".{destination.name}.bml-write")
    try:
        with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            archive.write(artifact, artifact.name)
            archive.write(descriptor, descriptor.name)
        os.replace(temporary, destination)
    finally:
        temporary.unlink(missing_ok=True)
    print("Published:")
    print(f"  {destination}")


def collect_script_files(root: Path) -> tuple[list[tuple[Path, str]], Path]:
    files: list[tuple[Path, str]] = []
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        if not is_inside(path.resolve(), root.resolve()):
            raise BmlError(f"Refusing to read a Script Mod file outside its project: {path}")
        relative = path.relative_to(root)
        directory_names = {part.lower() for part in relative.parts[:-1]}
        if directory_names & SCRIPT_EXCLUDED_DIRECTORIES:
            continue
        if path.name.lower() in {name.lower() for name in SCRIPT_EXCLUDED_FILES}:
            continue
        if path.name == SCRIPT_DEPLOYMENT_MARKER:
            continue
        if path.suffix.lower() in SCRIPT_EXCLUDED_EXTENSIONS:
            continue
        files.append((path, relative.as_posix()))
    entries = [
        path
        for path, relative in files
        if "/" not in relative and path.name.lower().endswith(".mod.as")
    ]
    if len(entries) != 1:
        raise BmlError(
            f"A Script Mod Project needs exactly one top-level *.mod.as entry; found {len(entries)}."
        )
    return files, entries[0]


def validate_script_files(
    root: Path, manifest: dict[str, Any]
) -> list[tuple[Path, str]]:
    files, entry = collect_script_files(root)
    if entry.name != manifest["entry"]:
        raise BmlError(
            f"Script entry changed from '{manifest['entry']}' to '{entry.name}'. "
            "Update bml.mod.json deliberately before continuing."
        )
    metadata = read_script_metadata(entry)
    if metadata.get("id") != manifest["id"]:
        raise BmlError("The Script Mod id changed while the project was being prepared.")
    return files


def read_deployment_marker(destination: Path) -> dict[str, Any] | None:
    marker_path = destination / SCRIPT_DEPLOYMENT_MARKER
    if not marker_path.is_file():
        return None
    marker = read_json(marker_path)
    if marker.get("format") != 1 or not isinstance(marker.get("files"), list):
        raise BmlError(f"Unsupported managed deployment marker: {marker_path}")
    return marker


def mirror_script_project(
    root: Path, manifest: dict[str, Any], destination: Path
) -> int:
    files = validate_script_files(root, manifest)
    if destination.exists() and not destination.is_dir():
        raise BmlError(f"Script Mod deployment path is not a directory: {destination}")
    marker = read_deployment_marker(destination) if destination.exists() else None
    if destination.exists() and marker is None and any(destination.iterdir()):
        raise BmlError(
            f"Refusing to replace unmanaged Script Mod directory: {destination}"
        )
    source_identity = os.path.normcase(str(root.resolve()))
    if marker is not None:
        if marker.get("modId") != manifest["id"]:
            raise BmlError(f"Managed deployment belongs to another Mod: {destination}")
        if os.path.normcase(str(marker.get("source", ""))) != source_identity:
            raise BmlError(f"Managed deployment belongs to another source tree: {destination}")

    destination.mkdir(parents=True, exist_ok=True)
    current = {relative for _, relative in files}
    previous = set(str(value) for value in (marker or {}).get("files", []))
    changed = 0
    for source, relative in files:
        target = destination / Path(relative)
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.is_file() and filecmp.cmp(source, target, shallow=False):
            continue
        temporary = target.with_name(f".{target.name}.bml-sync")
        try:
            shutil.copy2(source, temporary)
            os.replace(temporary, target)
        finally:
            if temporary.exists():
                temporary.unlink()
        changed += 1
    for relative in sorted(previous - current, reverse=True):
        target = (destination / Path(relative)).resolve()
        if not is_inside(target, destination.resolve()):
            raise BmlError(f"Unsafe managed path in {destination / SCRIPT_DEPLOYMENT_MARKER}: {relative}")
        if target.is_file():
            target.unlink()
            changed += 1
    write_json(
        destination / SCRIPT_DEPLOYMENT_MARKER,
        {
            "format": 1,
            "modId": manifest["id"],
            "source": source_identity,
            "files": sorted(current),
        },
    )
    return changed


def script_fingerprint(root: Path, manifest: dict[str, Any]) -> tuple[tuple[str, int, int], ...]:
    files, entry = collect_script_files(root)
    if entry.name != manifest["entry"]:
        raise BmlError(f"Expected Script entry '{manifest['entry']}', found '{entry.name}'.")
    return tuple(
        (relative, path.stat().st_mtime_ns, path.stat().st_size)
        for path, relative in files
    )


def script_build(args: argparse.Namespace, script_path: Path) -> Path:
    root, manifest, _ = project_state(args.project, script_path)
    destination = root / ".bml/stage/Mods" / str(manifest["deployName"])
    changed = mirror_script_project(root, manifest, destination)
    print(f"Prepared {manifest['id']} ({changed} changed files)")
    print("Ready:")
    print(f"  {destination}")
    return destination


def resolve_game(args: argparse.Namespace, settings: dict[str, Any]) -> Path:
    value = args.ballance_root or settings.get("ballanceRoot") or os.environ.get("BML_BALLANCE_ROOT")
    if not value:
        value = input("Ballance folder: ").strip()
    game = require_directory(value, "Ballance folder")
    if game is None:
        raise BmlError("A Ballance folder is required to run the Mod.")
    abi = game_abi(game)
    if abi["host_linkage"] != "dynamic":
        raise BmlError("The SDL static-host script workflow is not implemented.")
    find_player(game)
    prefix, suffix = {"windows": ("", ".dll"), "linux": ("lib", ".so"),
                      "macos": ("lib", ".dylib")}[abi["platform"]]
    require_file(game / "Managers" / f"{prefix}BMLPlus{suffix}", "BML+ loader")
    require_file(game / "BuildingBlocks" / f"{prefix}AngelScript{suffix}", "Script Mod runtime")
    return game


def script_run(args: argparse.Namespace, script_path: Path) -> None:
    root, manifest, settings = project_state(args.project, script_path)
    game = resolve_game(args, settings)
    if player_is_running():
        raise BmlError("Ballance is already running. Close it before starting a managed Script Mod run.")
    destination = game / "ModLoader/Mods" / str(manifest["deployName"])
    direct = root.resolve() == destination.resolve()
    changed = 0 if direct else mirror_script_project(root, manifest, destination)
    settings.update({"format": 1, "ballanceRoot": str(game)})
    write_json(root / ".bml/settings.json", settings)
    print(f"Prepared {manifest['id']} ({changed} changed files)")
    print(f"Watching {root}")

    player = find_player(game)
    log_path = game / "ModLoader/ModLoader.log"
    previous_lines = len(log_path.read_text(encoding="utf-8", errors="replace").splitlines()) if log_path.is_file() else 0
    fingerprint = script_fingerprint(root, manifest)
    print("Starting Ballance. Save a source file to hot reload; exit the game to return here...")
    process = subprocess.Popen([str(player)], cwd=player.parent)
    last_sync_error = ""
    try:
        while process.poll() is None:
            time.sleep(0.35)
            try:
                updated = script_fingerprint(root, manifest)
                if updated == fingerprint:
                    continue
                changed = 0 if direct else mirror_script_project(root, manifest, destination)
                fingerprint = updated
                last_sync_error = ""
                if direct:
                    print("Source change detected; Player will hot reload it.")
                elif changed:
                    print(f"Synced {changed} changed file(s).")
            except (BmlError, OSError) as error:
                message = str(error)
                if message != last_sync_error:
                    print(f"Sync paused: {message}")
                    last_sync_error = message
    except KeyboardInterrupt:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                if os.name != "nt":
                    process.wait()
        raise
    lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines() if log_path.is_file() else []
    marker = f"[{manifest['id']}/"
    mod_lines = [line for line in lines[previous_lines:] if marker in line]
    print(f"\n{manifest['id']} log:")
    if mod_lines:
        print("\n".join(mod_lines))
    else:
        print(f"No new log lines were found. Full log: {log_path}")
    if process.returncode != 0:
        raise BmlError(f"Ballance exited with code {process.returncode}.")


def script_pack(args: argparse.Namespace, script_path: Path) -> None:
    root, manifest, _ = project_state(args.project, script_path)
    files = validate_script_files(root, manifest)
    destination = output_path(
        args.output,
        root / "dist" / f"{manifest['deployName']}.zip",
        ".zip",
    )
    files = [
        (source, relative)
        for source, relative in files
        if source.resolve() != destination
    ]
    if destination.exists() and not args.force:
        raise BmlError(f"Output already exists: {destination}. Pass --force to replace it.")
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(f".{destination.name}.bml-write")
    try:
        with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for source, relative in files:
                archive.write(source, relative)
        os.replace(temporary, destination)
    finally:
        if temporary.exists():
            temporary.unlink()
    print("Published:")
    print(f"  {destination}")


class NativeWorkflowAdapter:
    def create(self, args: argparse.Namespace, script_path: Path) -> None:
        create_native_mod(args, script_path)

    def build(self, args: argparse.Namespace, script_path: Path) -> None:
        native_build_or_run(args, script_path, install=False, launch=False)

    def run(self, args: argparse.Namespace, script_path: Path) -> None:
        native_build_or_run(args, script_path, install=True, launch=True)

    def pack(self, args: argparse.Namespace, script_path: Path) -> None:
        native_pack(args, script_path)


class ScriptWorkflowAdapter:
    def create(self, args: argparse.Namespace, script_path: Path) -> None:
        create_script_mod(args, script_path)

    def build(self, args: argparse.Namespace, script_path: Path) -> None:
        script_build(args, script_path)

    def run(self, args: argparse.Namespace, script_path: Path) -> None:
        script_run(args, script_path)

    def pack(self, args: argparse.Namespace, script_path: Path) -> None:
        script_pack(args, script_path)


WORKFLOW_ADAPTERS = {
    "native": NativeWorkflowAdapter(),
    "script": ScriptWorkflowAdapter(),
}


def interface_action(args: argparse.Namespace, script_path: Path) -> None:
    root, manifest, settings = project_state(args.project, script_path)
    definitions = manifest.get("interfaces")
    if not isinstance(definitions, list) or not definitions:
        raise BmlError(f"{manifest['id']} does not define a generated native interface.")
    sdk = resolve_sdk(args, script_path, settings)
    generator = sdk / "share/BML/tools/interface_codegen.py"
    require_file(generator, "Interface generator")
    for definition in definitions:
        input_path = root / str(definition)
        require_file(input_path, "Interface definition")
        preview = root / ".bml/interface-preview" / str(manifest["target"]) / "ValueInterface.h"
        command = [
            sys.executable,
            str(generator),
            "--input",
            str(input_path),
            "--output",
            str(preview),
            "--provider-id",
            str(manifest["id"]),
            "--provider-version",
            str(manifest["version"]),
            "--namespace",
            str(manifest["target"]),
        ]
        if args.action == "update":
            command.append("--update-lock")
        run_checked(command, f"Interface {args.action} failed", args.verbose)
        print(f"{args.action.capitalize()} complete: {definition}")


def add_common_build_options(
    parser: argparse.ArgumentParser,
    include_game: bool = True,
) -> None:
    parser.add_argument("--project")
    parser.add_argument("--bml-sdk", dest="bml_sdk")
    if include_game:
        parser.add_argument("--ballance-root", dest="ballance_root")
    parser.add_argument("--package-root", dest="package_root", action="append")
    parser.add_argument("--generator")
    parser.add_argument(
        "--configuration",
        choices=CONFIGURATIONS,
    )
    parser.add_argument("--verbose", action="store_true")


def parse_command(argv: list[str]) -> tuple[str, argparse.Namespace]:
    command = argv[0]
    parser = argparse.ArgumentParser(prog=f"bml {command}", add_help=True)
    if command == "new":
        kinds = parser.add_subparsers(dest="kind", required=True)
        native = kinds.add_parser("native", help="Create a C++ Mod Project.")
        script = kinds.add_parser("script", help="Create an AngelScript Mod Project.")
        for route in (native, script):
            route.add_argument("mod_id", nargs="?")
            route.add_argument("--name")
            route.add_argument("--author")
            route.add_argument("--version", default="1.0.0")
            route.add_argument("--description")
            route.add_argument("--destination")
        native.add_argument("--profile", choices=PROFILES, default="basic")
        native.add_argument("--provider-id", dest="provider_id")
    elif command == "init":
        parser.add_argument("mod_id", nargs="?")
        parser.add_argument("--project")
        parser.add_argument("--kind", choices=MOD_KINDS)
        parser.add_argument("--target")
        parser.add_argument("--artifact")
        parser.add_argument("--name")
        parser.add_argument("--version")
        parser.add_argument("--bml-sdk", dest="bml_sdk")
    elif command in ("run", "build"):
        add_common_build_options(parser)
    elif command == "pack":
        add_common_build_options(parser, include_game=False)
        parser.add_argument("--output")
        parser.add_argument("--force", action="store_true")
    elif command == "interface":
        parser.add_argument("action", choices=("update", "check"))
        parser.add_argument("--project")
        parser.add_argument("--bml-sdk", dest="bml_sdk")
        parser.add_argument("--verbose", action="store_true")
    else:
        raise BmlError(f"Unknown command '{command}'. Run 'bml help'.")
    return command, parser.parse_args(argv[1:])


def main(argv: list[str] | None = None) -> int:
    # A redirected Windows console can use a code page that cannot represent
    # some valid paths. Preserve the stream encoding and escape those characters.
    reconfigure = getattr(sys.stdout, "reconfigure", None)
    if reconfigure is not None:
        reconfigure(errors="backslashreplace")

    arguments = list(sys.argv[1:] if argv is None else argv)
    if not arguments or arguments[0] in ("help", "-h", "--help"):
        print_help(any(value in ("--verbose", "-v") for value in arguments[1:]))
        return 0
    try:
        command, args = parse_command(arguments)
        script_path = Path(__file__).resolve()
        if command == "new":
            WORKFLOW_ADAPTERS[args.kind].create(args, script_path)
        elif command == "init":
            init_existing_mod(args, script_path)
        elif command in ("build", "run", "pack"):
            _, manifest, _ = project_state(args.project, script_path)
            adapter = WORKFLOW_ADAPTERS[str(manifest["kind"])]
            getattr(adapter, command)(args, script_path)
        else:
            interface_action(args, script_path)
        return 0
    except BmlError as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("\nCancelled.", file=sys.stderr)
        return 130


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Archive a staged BML runtime, installed SDK, and in-tree Mods separately."""

import argparse
import json
from pathlib import Path
import re
import tempfile
import zipfile

from package_sdl_update import (allowed_path, installed_files, loader_path, package_source,
                                valid_update_target, validate_pe)


def archive(path: Path, files: dict[str, Path]) -> None:
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as package:
        for name, source in sorted(files.items()):
            package.write(source, name)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", type=Path, required=True)
    parser.add_argument("--install-manifest", type=Path, required=True)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    stage, sdk, output = (path.resolve() for path in (args.stage, args.sdk, args.output))
    if any(output == root or root in output.parents for root in (stage, sdk)):
        parser.error("Output must be outside the runtime and SDK stages")

    try:
        abi_name = "ModLoader/BML.abi.json"
        abi = json.loads((stage / abi_name).read_bytes())
        sdk_abi = json.loads((sdk / "share/BML/BML.abi.json").read_bytes())
        if any(abi.get(key) != value for key, value in sdk_abi.items()):
            raise ValueError("Runtime stage and installed SDK target do not match")
        platform = abi["platform"]
        prefix, suffix = {"windows": ("", ".dll"), "linux": ("lib", ".so"),
                          "macos": ("lib", ".dylib")}[platform]
        version = abi["version"]
        architecture = abi["architecture"]
        if (not valid_update_target(abi) or abi["host_linkage"] != "dynamic"
                or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9.+_-]{0,127}", version)):
            raise ValueError("Expected a versioned dynamic BML runtime stage")
        runtime = {path.relative_to(stage).as_posix(): path
                   for path in installed_files(stage, args.install_manifest)}
        required = {abi_name, loader_path(abi)}
        if abi["scripts"]:
            required.add(f"BuildingBlocks/{prefix}AngelScript{suffix}")
        if required - runtime.keys():
            raise ValueError(f"Runtime manifest is missing {', '.join(sorted(required - runtime.keys()))}")
        updater = "Bin/Updater.exe" if platform == "windows" else "Bin/Updater"
        for name in runtime:
            if name != updater and not allowed_path(name, abi):
                raise ValueError(f"Runtime manifest includes an unowned path: {name}")
        runtime = {name: package_source(path, stage, runtime, abi) for name, path in runtime.items()}

        development = {}
        mods = {}
        for path in sdk.rglob("*"):
            if not path.is_file() or path.name == ".DS_Store":
                continue
            relative = path.relative_to(sdk)
            source = path.resolve(strict=True)
            if sdk not in source.parents:
                raise ValueError(f"SDK entry points outside the installation: {relative}")
            if relative.parts[0] == "Mods":
                mods[f"ModLoader/{relative.as_posix()}"] = source
            else:
                development[relative.as_posix()] = source
        packages_list = sdk / "share/BML/mods.txt"
        modules = packages_list.read_text(encoding="utf-8").splitlines() if packages_list.is_file() else []
        if not modules:
            raise ValueError("Build and install the in-tree Mods before archiving")
        for module in modules:
            path = f"ModLoader/Mods/{module}"
            if Path(module).name != module or path not in mods:
                raise ValueError(f"Mod package is missing from the SDK installation: {module}")
            if module.endswith(".zip"):
                name = f"{Path(module).stem}.bmodp"
                with zipfile.ZipFile(mods[path]) as package:
                    if not {name, f"{name}.abi.json"}.issubset(package.namelist()):
                        raise ValueError(f"Mod archive is missing its module or ABI sidecar: {module}")
                    if platform == "windows":
                        with package.open(name) as binary:
                            validate_pe(binary, architecture, False, f"{module}/{name}")
            elif not module.endswith(".bmodp") or f"{path}.abi.json" not in mods:
                raise ValueError(f"Mod is missing its ABI sidecar: {module}")
            elif platform == "windows":
                with mods[path].open("rb") as binary:
                    validate_pe(binary, architecture, False, module)

        label = f"{version}-{abi['runtime']}-{platform}-{architecture}-dynamic"
        packages = {
            f"BMLPlus-Runtime-{label}.zip": runtime,
            f"BMLPlus-SDK-{label}.zip": development,
            f"BMLPlus-Mods-{label}.zip": mods,
        }
        output.mkdir(parents=True, exist_ok=True)
        if any((output / name).exists() for name in packages):
            raise ValueError("Release archive already exists")
        with tempfile.TemporaryDirectory(prefix=".bml-release-", dir=output) as temporary:
            for name, contents in packages.items():
                archive(Path(temporary) / name, contents)
            for name in packages:
                (Path(temporary) / name).rename(output / name)
                print(output / name)
    except (OSError, KeyError, ValueError, TypeError, zipfile.BadZipFile) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Package a freshly staged BML runtime; sign manifests with P-256/P1363."""

import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import struct
import subprocess
import tempfile
import sys
import zipfile


def json_bytes(value: dict) -> bytes:
    return (json.dumps(value, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=".bml-write-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        Path(temporary).unlink(missing_ok=True)


def sign(payload: bytes, key: Path, signer: str) -> bytes:
    result = subprocess.run([str(signer), "sign", str(key)], input=payload,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    if len(result.stdout) != 64:
        raise ValueError("BMLUpdateSigner returned an invalid P-256 signature")
    return base64.b64encode(result.stdout) + b"\n"


def valid_update_target(target: dict) -> bool:
    runtime = target["runtime"]
    platform = target["platform"]
    architecture = target["architecture"]
    linkage = target["host_linkage"]
    if runtime == "virtools":
        return (platform, architecture, linkage) == ("windows", "x86", "dynamic")
    return (runtime == "sdl" and platform in {"windows", "linux", "macos"}
            and architecture in {"x86", "x64", "arm64"} and linkage in {"dynamic", "static"}
            and (architecture != "x86" or platform == "windows"))


def loader_path(target: dict) -> str:
    if target["runtime"] == "virtools":
        return "BuildingBlocks/BMLPlus.dll"
    return {"windows": "Managers/BMLPlus.dll", "linux": "Managers/libBMLPlus.so",
            "macos": "Managers/libBMLPlus.dylib"}[target["platform"]]


def allowed_path(path: str, target: dict) -> bool:
    lower = path.lower()
    if lower in {"updater", "updater.exe", "bin/updater", "bin/updater.exe", "player.ini", "bin/player.ini", "player.log", "bin/player.log"}:
        return False
    if any(lower == p or lower.startswith(p + "/") for p in ("modloader/updater", "modloader/mods", "modloader/configs")):
        return False
    if path == "ModLoader/BML.abi.json" or path.startswith(("ModLoader/Fonts/", "ModLoader/Licenses/")):
        return True
    if target["host_linkage"] == "static":
        return path in {"Player", "Player.exe"} or path.startswith(("Bin/", "Managers/", "BuildingBlocks/", "Plugins/", "RenderEngines/"))
    prefix, suffix = {"windows": ("", ".dll"), "linux": ("lib", ".so"), "macos": ("lib", ".dylib")}[target["platform"]]
    if path in {loader_path(target), f"BuildingBlocks/{prefix}AngelScript{suffix}"}:
        return True
    if not path.startswith("Bin/") or "/" in path[4:]:
        return False
    name = path[4:].lower()
    return not name.startswith(("ck2", "libck2", "vxmath", "libvxmath", "sdl3", "libsdl3", "player")) and (
        name.endswith((".dll", ".dylib", ".so")) or ".so." in name)


def validate_pe(stream, architecture: str, executable: bool, name: str) -> None:
    dos = stream.read(64)
    if len(dos) != 64 or dos[:2] != b"MZ":
        raise ValueError(f"Invalid PE binary: {name}")
    offset, = struct.unpack_from("<I", dos, 0x3c)
    if offset < len(dos):
        raise ValueError(f"Invalid PE binary: {name}")
    stream.seek(offset)
    header = stream.read(26)
    if len(header) != 26 or header[:4] != b"PE\0\0":
        raise ValueError(f"Invalid PE binary: {name}")
    machine, = struct.unpack_from("<H", header, 4)
    flags, magic = struct.unpack_from("<HH", header, 22)
    expected = {"x86": (0x14c, 0x10b), "x64": (0x8664, 0x20b), "arm64": (0xaa64, 0x20b)}
    if (machine, magic) != expected[architecture] or bool(flags & 0x2000) == executable:
        raise ValueError(f"PE binary does not match {architecture} target: {name}")


def package_source(path: Path, stage: Path, selected: dict, target: dict) -> Path:
    if not path.is_symlink():
        suffix = path.suffix.lower()
        if target["platform"] == "windows" and suffix in {".dll", ".exe"}:
            with path.open("rb") as binary:
                validate_pe(binary, target["architecture"], suffix == ".exe", path.name)
        return path
    if (target["platform"] != "linux" or path.parent != stage / "Bin"
            or not re.fullmatch(r"[^/]+\.so(?:\.[0-9]+)*", path.name)):
        raise ValueError(f"Stage contains an unsupported symlink: {path}")
    try:
        source = path.resolve(strict=True)
    except (OSError, RuntimeError) as error:
        raise ValueError(f"SONAME link does not resolve to a packaged library: {path}") from error
    if (source.parent != path.parent or selected.get(f"Bin/{source.name}") != source
            or not re.fullmatch(r"[^/]+\.so(?:\.[0-9]+)*", source.name)):
        raise ValueError(f"SONAME link does not resolve to a packaged library: {path}")
    return source


def installed_files(stage: Path, manifest: Path) -> list[Path]:
    paths = set()
    for line in manifest.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        entry = Path(line)
        path = entry.parent.resolve() / entry.name
        if stage not in path.parents or not (path.is_symlink() or path.is_file()):
            raise ValueError(f"Install manifest references a missing or external file: {line}")
        paths.add(path)
    return sorted(paths)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", required=True, type=Path, help="Fresh BMLRuntime stage, or matching full static Player runtime stage")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--install-manifest", type=Path, help="CMake component install manifest; selects only files owned by that component")
    parser.add_argument("--version", required=True)
    parser.add_argument("--signing-key", required=True, type=Path)
    parser.add_argument("--signer", default="BMLUpdateSigner", help="Native P-256 publisher tool")
    parser.add_argument("--base-url", help="HTTPS directory where versioned artifacts will be published; emits a signed channel")
    parser.add_argument("--channel", choices=("stable", "beta"), default="stable")
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9.+_-]{0,127}", args.version):
        parser.error("Invalid version")
    stage, output = args.stage.resolve(), args.output.resolve()
    if output == stage or stage in output.parents:
        parser.error("Output must be outside the input stage")
    abi_name = "ModLoader/BML.abi.json"
    abi_path = stage / abi_name
    if not abi_path.is_file() or abi_path.resolve() != abi_path:
        parser.error("Stage is missing its regular ABI descriptor")
    try:
        abi_data = abi_path.read_bytes()
        abi = json.loads(abi_data.decode("utf-8"))
        if not isinstance(abi, dict):
            raise ValueError("expected a JSON object")
    except (OSError, ValueError) as error:
        parser.error(f"Unable to read the staged ABI descriptor: {error}")
    target = {name: abi.get(name) for name in ("runtime", "platform", "architecture", "host_linkage")}
    if (abi.get("schema") not in (1, 2) or not valid_update_target(target)
            or not re.fullmatch(r"[0-9a-f]{64}", abi.get("sdk_abi", ""))):
        parser.error("Stage has no valid BML ABI descriptor")
    if abi["schema"] == 2 and (abi.get("mod_api") != "bml.mod" or abi.get("mod_api_major") != 1):
        parser.error("Stage has an unsupported native Mod C protocol")
    if abi.get("version") != args.version:
        parser.error(f"Requested version {args.version} does not match the stage version {abi.get('version')!r}")
    if not isinstance(abi.get("scripts"), bool):
        parser.error("Stage ABI descriptor must declare whether scripts are enabled")
    selected = {}
    seen = set()
    if args.install_manifest:
        try:
            candidates = installed_files(stage, args.install_manifest)
        except ValueError as error:
            parser.error(str(error))
    else:
        candidates = sorted(stage.rglob("*"))
    for path in candidates:
        if path.name == ".DS_Store" or not (path.is_symlink() or path.is_file()):
            continue
        relative = path.relative_to(stage).as_posix()
        if relative.lower() in {"updater", "updater.exe", "bin/updater", "bin/updater.exe", "bin/updater-readme.txt"}:
            continue  # Updater is bootstrapped by the manual runtime archive.
        if (not allowed_path(relative, target) or any(ord(c) < 32 or ord(c) == 127 for c in relative)
                or "\\" in relative or ":" in relative):
            parser.error(f"Stage contains a path outside this update's ownership: {relative}")
        if target["platform"] == "windows":
            for segment in relative.split("/"):
                device = segment.split(".", 1)[0].lower()
                if (segment.endswith((".", " ")) or any(c in segment for c in '<>"|?*') or
                        device in {"con", "prn", "aux", "nul"} or re.fullmatch(r"(?:com|lpt)[1-9]", device)):
                    parser.error(f"Ambiguous Windows target path: {relative}")
        identity = relative.lower() if target["platform"] == "windows" else relative
        if identity in seen:
            parser.error(f"Duplicate target path: {relative}")
        seen.add(identity)
        selected[relative] = path
    prefix, suffix = {"windows": ("", ".dll"), "linux": ("lib", ".so"), "macos": ("lib", ".dylib")}[target["platform"]]
    runtime = {"Player.exe", "Bin/Player.exe"} if target["platform"] == "windows" else {"Player", "Bin/Player"}
    if target["host_linkage"] == "dynamic":
        runtime = {loader_path(target)}
    if abi_name not in selected:
        parser.error("Package inventory is missing its ABI descriptor")
    if not runtime.intersection(selected):
        parser.error("Stage is missing its Loader or static Player")
    if (target["host_linkage"] == "dynamic" and abi["scripts"]
            and f"BuildingBlocks/{prefix}AngelScript{suffix}" not in selected):
        parser.error("Stage declares scripts enabled but its script runtime is missing from the package inventory")
    files = []
    for relative, path in selected.items():
        try:
            source = package_source(path, stage, selected, target)
        except ValueError as error:
            parser.error(str(error))
        # Store one regular file per load name. The updater treats ZIP symlinks
        # as invalid, and the ELF loader still needs the original SONAME path.
        data = abi_data if relative == abi_name else source.read_bytes()
        mode = 0o755 if source.stat().st_mode & 0o111 else 0o644
        files.append((relative, data, mode))
    label = f"{target['runtime']}-{target['platform']}-{target['architecture']}-{target['host_linkage']}"
    stem = f"BMLPlus-Update-{args.version}-{label}"
    package = output / f"{stem}.zip"
    manifest_path = output / f"{stem}.manifest.json"
    signature_path = manifest_path.with_suffix(".json.sig")
    channel_path = output / target["runtime"] / target["platform"] / target["architecture"] / target["host_linkage"] / f"{args.channel}.json"
    paths = [package, manifest_path, signature_path]
    if args.base_url:
        if not args.base_url.startswith("https://") or any(c.isspace() for c in args.base_url):
            parser.error("--base-url must use HTTPS without whitespace")
        paths += [channel_path, channel_path.with_suffix(".json.sig")]
    if not args.force and any(path.exists() for path in paths):
        parser.error("Output exists; pass --force to replace it")
    subprocess.run([args.signer, "check", str(args.signing_key)],
                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    output.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=".bml-package-", dir=output)
    os.close(fd)
    try:
        with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for name, data, mode in files:
                entry = zipfile.ZipInfo(name)
                entry.create_system = 3
                entry.external_attr = (stat.S_IFREG | mode) << 16
                entry.compress_type = zipfile.ZIP_DEFLATED
                archive.writestr(entry, data)
        package_data = Path(temporary).read_bytes()
    finally:
        Path(temporary).unlink(missing_ok=True)
    manifest = {"schemaVersion": 2, "version": args.version, "target": target,
                "packageType": "player-runtime" if target["host_linkage"] == "static" else "bml-runtime",
                "package": {"fileName": package.name, "sha256": hashlib.sha256(package_data).hexdigest()},
                "managedFiles": [{"path": name, "sha256": hashlib.sha256(data).hexdigest(), "size": len(data), "mode": mode}
                                 for name, data, mode in files],
                "preserve": ["ModLoader/Configs", "ModLoader/Mods", "ModLoader/Updater"], "removeFiles": []}
    encoded = json_bytes(manifest)
    signature = sign(encoded, args.signing_key, args.signer)
    atomic_write(package, package_data)
    atomic_write(manifest_path, encoded)
    atomic_write(signature_path, signature)
    if args.base_url:
        previous = json.loads(channel_path.read_text(encoding="utf-8")) if channel_path.exists() else {}
        channel = {"schemaVersion": 2, "target": target, "version": args.version,
                   "packageUrl": args.base_url.rstrip("/") + "/" + package.name,
                   "manifestUrl": args.base_url.rstrip("/") + "/" + manifest_path.name,
                   "revokedVersions": previous.get("revokedVersions", []),
                   "revokedManifestHashes": previous.get("revokedManifestHashes", [])}
        encoded = json_bytes(channel)
        atomic_write(channel_path.with_suffix(".json.sig"), sign(encoded, args.signing_key, args.signer))
        atomic_write(channel_path, encoded)
    print(package)
    print(manifest_path)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        message = error.stderr.decode("utf-8", errors="replace").strip() if isinstance(error, subprocess.CalledProcessError) and error.stderr else str(error)
        print(f"Error: {message}", file=sys.stderr)
        raise SystemExit(1)

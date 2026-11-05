#!/usr/bin/env python3
"""Package a freshly staged SDL runtime; sign manifests with P-256/P1363."""

import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import stat
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
    if path in {f"Managers/{prefix}BMLPlus{suffix}", f"BuildingBlocks/{prefix}AngelScript{suffix}"}:
        return True
    if not path.startswith("Bin/") or "/" in path[4:]:
        return False
    name = path[4:].lower()
    return not name.startswith(("ck2", "libck2", "vxmath", "libvxmath", "sdl3", "libsdl3", "player")) and (
        name.endswith((".dll", ".dylib", ".so")) or ".so." in name)


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
    abi = json.loads((stage / "ModLoader/BML.abi.json").read_text(encoding="utf-8"))
    target = {name: abi.get(name) for name in ("runtime", "platform", "architecture", "host_linkage")}
    if (abi.get("schema") != 1 or target["runtime"] != "sdl" or target["platform"] not in {"windows", "linux", "macos"}
            or target["architecture"] not in {"x86", "x64", "arm64"} or target["host_linkage"] not in {"dynamic", "static"}
            or (target["architecture"] == "x86" and target["platform"] != "windows")
            or not re.fullmatch(r"[0-9a-f]{64}", abi.get("sdk_abi", ""))):
        parser.error("Stage has no valid SDL ABI descriptor")
    subprocess.run([args.signer, "check", str(args.signing_key)],
                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    abi["version"] = args.version
    files = []
    seen = set()
    if args.install_manifest:
        candidates = []
        for line in args.install_manifest.read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            path = Path(line)
            resolved = path.resolve()
            if stage not in resolved.parents or path.is_symlink() or not path.exists():
                parser.error(f"Install manifest references a missing, linked or external path: {line}")
            candidates.append(resolved)
        candidates = sorted(set(candidates))
    else:
        candidates = sorted(stage.rglob("*"))
    for path in candidates:
        if path.is_symlink():
            parser.error(f"Stage contains a symlink: {path}")
        if not path.is_file() or path.name == ".DS_Store":
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
        data = json_bytes(abi) if relative == "ModLoader/BML.abi.json" else path.read_bytes()
        mode = 0o755 if path.stat().st_mode & 0o111 else 0o644
        files.append((relative, data, mode))
    prefix, suffix = {"windows": ("", ".dll"), "linux": ("lib", ".so"), "macos": ("lib", ".dylib")}[target["platform"]]
    runtime = {"Player.exe", "Bin/Player.exe"} if target["platform"] == "windows" else {"Player", "Bin/Player"}
    if target["host_linkage"] == "dynamic":
        runtime = {f"Managers/{prefix}BMLPlus{suffix}"}
    if not runtime.intersection(name for name, _, _ in files):
        parser.error("Stage is missing its Loader or static Player")
    label = f"sdl-{target['platform']}-{target['architecture']}-{target['host_linkage']}"
    stem = f"BMLPlus-Update-{args.version}-{label}"
    package = output / f"{stem}.zip"
    manifest_path = output / f"{stem}.manifest.json"
    signature_path = manifest_path.with_suffix(".json.sig")
    channel_path = output / "sdl" / target["platform"] / target["architecture"] / target["host_linkage"] / f"{args.channel}.json"
    paths = [package, manifest_path, signature_path]
    if args.base_url:
        if not args.base_url.startswith("https://") or any(c.isspace() for c in args.base_url):
            parser.error("--base-url must use HTTPS without whitespace")
        paths += [channel_path, channel_path.with_suffix(".json.sig")]
    if not args.force and any(path.exists() for path in paths):
        parser.error("Output exists; pass --force to replace it")
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

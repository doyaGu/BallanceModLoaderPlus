#!/usr/bin/env python3
"""Unified real-game test tool for BallanceModLoaderPlus.

Subcommands:
    run       Deploy modules to the game directory, launch, collect artifacts.
    smoke     Batch crash detection across multiple module combinations.
    analyze   Per-module isolation probe with bootstrap verification.
    resolve   Dry run: output resolved module set as JSON (no game launch).

Requires Python 3.11+ (stdlib only, no external dependencies).
See docs/real-game-testing.md for full usage guide.
"""
from __future__ import annotations

import sys

if sys.version_info < (3, 11):
    print("Error: bml_test.py requires Python 3.11+ (for tomllib).", file=sys.stderr)
    print(f"Current version: {sys.version}", file=sys.stderr)
    sys.exit(1)

import argparse
import dataclasses
import json
import logging
import os
import shutil
import subprocess
import time
import tomllib
from datetime import datetime
from pathlib import Path
from typing import Any

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

DEFAULT_GAME_DIR = os.environ.get("BML_GAME_DIR", "")
LOCK_FILENAME = ".bml-test.lock"

log = logging.getLogger("bml-test")

# ---------------------------------------------------------------------------
# Data types
# ---------------------------------------------------------------------------


@dataclasses.dataclass
class DepSpec:
    version: str
    optional: bool


@dataclasses.dataclass
class ModuleManifest:
    package_id: str
    name: str
    category: str  # "" for normal, "test" for test-only
    entry: str
    dependencies: dict[str, DepSpec]


@dataclasses.dataclass
class BackupState:
    backup_dir: Path
    had_bml: bool
    had_modloader: bool
    had_mods: bool


# ---------------------------------------------------------------------------
# CLI helpers
# ---------------------------------------------------------------------------


def comma_list(value: str) -> list[str]:
    return [v.strip() for v in value.split(",") if v.strip()]


# ---------------------------------------------------------------------------
# Manifest reading
# ---------------------------------------------------------------------------


def read_module_manifest(toml_path: Path) -> ModuleManifest:
    """Parse a mod.toml and return a ModuleManifest."""
    with open(toml_path, "rb") as f:
        data = tomllib.load(f)

    pkg = data.get("package", {})
    deps_raw = data.get("dependencies", {})
    deps: dict[str, DepSpec] = {}
    for dep_id, dep_value in deps_raw.items():
        if isinstance(dep_value, str):
            deps[dep_id] = DepSpec(version=dep_value, optional=False)
        elif isinstance(dep_value, dict):
            deps[dep_id] = DepSpec(
                version=dep_value.get("version", "*"),
                optional=dep_value.get("optional", False),
            )

    return ModuleManifest(
        package_id=pkg.get("id", ""),
        name=pkg.get("name", ""),
        category=pkg.get("category", ""),
        entry=pkg.get("entry", ""),
        dependencies=deps,
    )


def find_manifest_path(
    module_name: str,
    repo_root: Path,
    build_dir: Path,
    config: str,
) -> Path | None:
    """Find mod.toml for a module: build output first, then source."""
    build_path = build_dir / "Mods" / module_name / config / "mod.toml"
    if build_path.exists():
        return build_path
    source_path = repo_root / "modules" / module_name / "mod.toml"
    if source_path.exists():
        return source_path
    return None


def discover_builtin_modules(repo_root: Path) -> list[str]:
    """Scan modules/ for directories with mod.toml, exclude category='test'.
    Returns sorted list of directory names."""
    modules_dir = repo_root / "modules"
    if not modules_dir.is_dir():
        return []
    result = []
    for child in sorted(modules_dir.iterdir()):
        toml_path = child / "mod.toml"
        if not toml_path.exists():
            continue
        manifest = read_module_manifest(toml_path)
        if manifest.category == "test":
            continue
        result.append(child.name)
    return result


def build_package_id_index(
    module_names: list[str],
    repo_root: Path,
    build_dir: Path,
    config: str,
) -> dict[str, str]:
    """Build a mapping from package.id to directory name."""
    index: dict[str, str] = {}
    for name in module_names:
        path = find_manifest_path(name, repo_root, build_dir, config)
        if path is None:
            continue
        manifest = read_module_manifest(path)
        if manifest.package_id:
            index[manifest.package_id] = name
    return index


# ---------------------------------------------------------------------------
# Dependency resolution
# ---------------------------------------------------------------------------


def resolve_module_deps(
    requested: list[str],
    repo_root: Path,
    build_dir: Path,
    config: str,
    all_known_modules: list[str] | None = None,
) -> list[str]:
    """Resolve transitive dependencies via topological sort.

    Returns an ordered list: dependencies before dependents.
    Raises SystemExit on unresolvable dependencies or cycles.
    """
    if all_known_modules is None:
        all_known_modules = discover_builtin_modules(repo_root)
        for m in requested:
            if m not in all_known_modules:
                all_known_modules.append(m)

    id_index = build_package_id_index(all_known_modules, repo_root, build_dir, config)

    manifests: dict[str, ModuleManifest] = {}
    for name in all_known_modules:
        path = find_manifest_path(name, repo_root, build_dir, config)
        if path:
            manifests[name] = read_module_manifest(path)

    resolved: list[str] = []
    visited: set[str] = set()
    visiting: set[str] = set()

    deployment_set = set(requested)

    def visit(name: str) -> None:
        if name in visited:
            return
        if name in visiting:
            log.error("Circular dependency detected at module '%s'", name)
            sys.exit(1)
        visiting.add(name)

        manifest = manifests.get(name)
        if manifest is None:
            log.error(
                "Module '%s' has no mod.toml (checked build and source directories)",
                name,
            )
            sys.exit(1)

        for dep_id, dep_spec in manifest.dependencies.items():
            if dep_spec.optional and id_index.get(dep_id, "") not in deployment_set:
                continue
            dep_name = id_index.get(dep_id)
            if dep_name is None:
                log.error(
                    "Module '%s' depends on '%s' which is not found in any known module",
                    name,
                    dep_id,
                )
                sys.exit(1)
            deployment_set.add(dep_name)
            visit(dep_name)

        visiting.discard(name)
        visited.add(name)
        resolved.append(name)

    for name in requested:
        visit(name)

    return resolved


# ---------------------------------------------------------------------------
# Lock file
# ---------------------------------------------------------------------------


def acquire_lock(game_dir: Path) -> Path:
    """Create a lock file with current PID. Exits if another live process holds it."""
    lock_path = game_dir / LOCK_FILENAME
    if lock_path.exists():
        try:
            old_pid = int(lock_path.read_text().strip())
            try:
                os.kill(old_pid, 0)
                log.error(
                    "Another bml-test process (PID %d) is running against this game directory.\n"
                    "Lock file: %s",
                    old_pid,
                    lock_path,
                )
                sys.exit(1)
            except OSError:
                log.warning("Removing stale lock file (PID %d is dead)", old_pid)
        except (ValueError, OSError):
            log.warning("Removing invalid lock file: %s", lock_path)
    lock_path.write_text(str(os.getpid()))
    return lock_path


def release_lock(game_dir: Path) -> None:
    lock_path = game_dir / LOCK_FILENAME
    lock_path.unlink(missing_ok=True)


# ---------------------------------------------------------------------------
# Backup / Restore / Deploy
# ---------------------------------------------------------------------------


def backup_game_files(game_dir: Path, backup_dir: Path) -> BackupState:
    """Backup BML.dll, ModLoader.dll, and Mods/ from game directory."""
    backup_dir.mkdir(parents=True, exist_ok=True)

    bin_dir = game_dir / "Bin"
    bb_dir = game_dir / "BuildingBlocks"
    mods_dir = game_dir / "ModLoader" / "Mods"

    had_bml = False
    bml_src = bin_dir / "BML.dll"
    if bml_src.exists():
        shutil.copy2(bml_src, backup_dir / "BML.dll")
        had_bml = True

    had_modloader = False
    ml_src = bb_dir / "ModLoader.dll"
    if ml_src.exists():
        shutil.copy2(ml_src, backup_dir / "ModLoader.dll")
        had_modloader = True

    had_mods = False
    if mods_dir.exists():
        shutil.copytree(mods_dir, backup_dir / "Mods", dirs_exist_ok=True)
        had_mods = True

    return BackupState(
        backup_dir=backup_dir,
        had_bml=had_bml,
        had_modloader=had_modloader,
        had_mods=had_mods,
    )


def restore_game_files(state: BackupState, game_dir: Path) -> None:
    """Restore backed up files to game directory."""
    bin_dir = game_dir / "Bin"
    bb_dir = game_dir / "BuildingBlocks"
    mods_dir = game_dir / "ModLoader" / "Mods"

    if state.had_bml:
        shutil.copy2(state.backup_dir / "BML.dll", bin_dir / "BML.dll")
    if state.had_modloader:
        shutil.copy2(state.backup_dir / "ModLoader.dll", bb_dir / "ModLoader.dll")
    if state.had_mods:
        if mods_dir.exists():
            shutil.rmtree(mods_dir)
        shutil.copytree(state.backup_dir / "Mods", mods_dir)
    elif mods_dir.exists():
        shutil.rmtree(mods_dir)

    shutil.rmtree(state.backup_dir, ignore_errors=True)


def resolve_extra(
    extra_value: str,
    repo_root: Path,
    build_dir: Path,
    config: str,
) -> tuple[str, Path]:
    """Resolve an --extra value to (module_name, artifact_dir).

    If the value is an existing directory with mod.toml, use it directly.
    Otherwise treat it as a build module name.
    """
    as_path = Path(extra_value)
    if as_path.is_dir() and (as_path / "mod.toml").exists():
        return as_path.name, as_path

    build_path = build_dir / "Mods" / extra_value / config
    if build_path.is_dir():
        return extra_value, build_path

    source_path = repo_root / "modules" / extra_value
    if source_path.is_dir() and (source_path / "mod.toml").exists():
        if not build_path.exists():
            log.error(
                "Module '%s' found in source but not built at %s",
                extra_value,
                build_path,
            )
            sys.exit(1)

    log.error("--extra '%s': not a directory with mod.toml, nor a known build module", extra_value)
    sys.exit(1)


def deploy_artifacts(
    game_dir: Path,
    build_dir: Path,
    config: str,
    module_names: list[str],
    repo_root: Path,
    extra_dirs: dict[str, Path] | None = None,
) -> None:
    """Deploy BML.dll, ModLoader.dll, and listed modules to game directory."""
    bin_dir = game_dir / "Bin"
    bb_dir = game_dir / "BuildingBlocks"
    mods_dir = game_dir / "ModLoader" / "Mods"

    bml_dll = build_dir / "bin" / config / "BML.dll"
    ml_dll = build_dir / "bin" / config / "ModLoader.dll"
    if not bml_dll.exists():
        log.error("BML.dll not found at %s", bml_dll)
        sys.exit(1)
    if not ml_dll.exists():
        log.error("ModLoader.dll not found at %s", ml_dll)
        sys.exit(1)

    shutil.copy2(bml_dll, bin_dir / "BML.dll")
    shutil.copy2(ml_dll, bb_dir / "ModLoader.dll")

    # Clear existing modules to avoid stale manifests breaking discovery
    if mods_dir.exists():
        shutil.rmtree(mods_dir)
    mods_dir.mkdir(parents=True, exist_ok=True)
    if extra_dirs is None:
        extra_dirs = {}

    for mod_name in module_names:
        if mod_name in extra_dirs:
            src_dir = extra_dirs[mod_name]
        else:
            src_dir = build_dir / "Mods" / mod_name / config
        if not src_dir.exists():
            log.error("Module build output not found for %s at %s", mod_name, src_dir)
            sys.exit(1)

        dest_dir = mods_dir / mod_name
        dest_dir.mkdir(parents=True, exist_ok=True)
        for item in src_dir.iterdir():
            if item.is_file():
                shutil.copy2(item, dest_dir / item.name)


def deploy_core_only(game_dir: Path, build_dir: Path, config: str) -> None:
    """Deploy only BML.dll and ModLoader.dll (for smoke/analyze)."""
    bin_dir = game_dir / "Bin"
    bb_dir = game_dir / "BuildingBlocks"

    bml_dll = build_dir / "bin" / config / "BML.dll"
    ml_dll = build_dir / "bin" / config / "ModLoader.dll"
    if not bml_dll.exists():
        log.error("BML.dll not found at %s", bml_dll)
        sys.exit(1)
    if not ml_dll.exists():
        log.error("ModLoader.dll not found at %s", ml_dll)
        sys.exit(1)

    shutil.copy2(bml_dll, bin_dir / "BML.dll")
    shutil.copy2(ml_dll, bb_dir / "ModLoader.dll")


def prepare_mods_dir(
    output_dir: Path,
    module_names: list[str],
    build_dir: Path,
    config: str,
    extra_dirs: dict[str, Path] | None = None,
) -> Path:
    """Create a Mods directory under output_dir with specified modules."""
    mods_dir = output_dir / "Mods"
    mods_dir.mkdir(parents=True, exist_ok=True)
    if extra_dirs is None:
        extra_dirs = {}
    for mod_name in module_names:
        if mod_name in extra_dirs:
            src_dir = extra_dirs[mod_name]
        else:
            src_dir = build_dir / "Mods" / mod_name / config
        if not src_dir.exists():
            log.error("Module build output not found for %s at %s", mod_name, src_dir)
            sys.exit(1)
        dest_dir = mods_dir / mod_name
        dest_dir.mkdir(parents=True, exist_ok=True)
        for item in src_dir.iterdir():
            if item.is_file():
                shutil.copy2(item, dest_dir / item.name)
    return mods_dir


# ---------------------------------------------------------------------------
# Game session
# ---------------------------------------------------------------------------


def run_cmake_build(build_dir: Path, config: str, targets: list[str] | None = None) -> None:
    """Run cmake --build. Exits on failure."""
    cmd = ["cmake", "--build", str(build_dir), "--config", config]
    if targets:
        for t in targets:
            cmd.extend(["--target", t])
    log.info("Building: %s", " ".join(cmd))
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        log.error("cmake build failed:\n%s\n%s", result.stdout, result.stderr)
        sys.exit(1)
    log.info("Build succeeded")


def start_game_session(
    game_dir: Path,
    env_overrides: dict[str, str | None] | None = None,
) -> subprocess.Popen:
    """Launch Player.exe and return the process handle."""
    player_exe = game_dir / "Bin" / "Player.exe"
    if not player_exe.exists():
        log.error("Player.exe not found at %s", player_exe)
        sys.exit(1)

    env = os.environ.copy()
    if env_overrides:
        for k, v in env_overrides.items():
            if v is None:
                env.pop(k, None)
            else:
                env[k] = v

    # Don't redirect stdout/stderr — it breaks DirectX rendering on Windows.
    # The game writes its own logs to ModLoader/; we collect them afterward.
    proc = subprocess.Popen(
        [str(player_exe)],
        cwd=str(game_dir / "Bin"),
        stdin=subprocess.DEVNULL,
        env=env,
    )
    log.info("Launched Player.exe (PID %d)", proc.pid)
    return proc


def wait_game_exit(proc: subprocess.Popen, timeout: int) -> bool:
    """Wait for the game process to exit. Returns True if exited normally.
    timeout=0 means wait indefinitely."""
    try:
        if timeout <= 0:
            proc.wait()
            return True
        proc.wait(timeout=timeout)
        return True
    except subprocess.TimeoutExpired:
        log.info("Timeout (%ds) reached, terminating Player.exe", timeout)
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
        return False


def collect_artifacts(
    mods_dir: Path,
    loader_dir: Path,
    output_dir: Path,
) -> None:
    """Collect logs and reports from game directories to output_dir."""
    logs_out = output_dir / "logs"
    if mods_dir.exists():
        for log_file in mods_dir.rglob("*.log"):
            rel = log_file.relative_to(mods_dir)
            dest = logs_out / rel
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(log_file, dest)

    reports_out = output_dir / "reports"
    if loader_dir.exists():
        for report_file in loader_dir.glob("*.json"):
            reports_out.mkdir(parents=True, exist_ok=True)
            shutil.copy2(report_file, reports_out / report_file.name)
        for lf in loader_dir.glob("*.log"):
            dest = output_dir / lf.name
            if not dest.exists():
                shutil.copy2(lf, dest)


# ---------------------------------------------------------------------------
# Win32 helpers (ctypes)
# ---------------------------------------------------------------------------

if sys.platform == "win32":
    import ctypes
    import ctypes.wintypes

    _user32 = ctypes.windll.user32  # type: ignore[attr-defined]
    _gdi32 = ctypes.windll.gdi32  # type: ignore[attr-defined]

    class _RECT(ctypes.Structure):
        _fields_ = [
            ("left", ctypes.c_long),
            ("top", ctypes.c_long),
            ("right", ctypes.c_long),
            ("bottom", ctypes.c_long),
        ]

    class _KEYBDINPUT(ctypes.Structure):
        _fields_ = [
            ("wVk", ctypes.wintypes.WORD),
            ("wScan", ctypes.wintypes.WORD),
            ("dwFlags", ctypes.wintypes.DWORD),
            ("time", ctypes.wintypes.DWORD),
            ("dwExtraInfo", ctypes.POINTER(ctypes.c_ulong)),
        ]

    class _INPUTUNION(ctypes.Union):
        _fields_ = [("ki", _KEYBDINPUT)]

    class _INPUT(ctypes.Structure):
        _fields_ = [("type", ctypes.wintypes.DWORD), ("union", _INPUTUNION)]

    class _BITMAPINFOHEADER(ctypes.Structure):
        _fields_ = [
            ("biSize", ctypes.wintypes.DWORD),
            ("biWidth", ctypes.c_long),
            ("biHeight", ctypes.c_long),
            ("biPlanes", ctypes.wintypes.WORD),
            ("biBitCount", ctypes.wintypes.WORD),
            ("biCompression", ctypes.wintypes.DWORD),
            ("biSizeImage", ctypes.wintypes.DWORD),
            ("biXPelsPerMeter", ctypes.c_long),
            ("biYPelsPerMeter", ctypes.c_long),
            ("biClrUsed", ctypes.wintypes.DWORD),
            ("biClrImportant", ctypes.wintypes.DWORD),
        ]


def find_main_window(pid: int, timeout: float = 20.0) -> int:
    """Wait for a process to create a visible main window. Returns HWND or 0."""
    if sys.platform != "win32":
        return 0
    deadline = time.monotonic() + timeout
    found_hwnd = 0

    def _enum_cb(hwnd: int, _: Any) -> bool:
        nonlocal found_hwnd
        proc_id = ctypes.wintypes.DWORD()
        _user32.GetWindowThreadProcessId(hwnd, ctypes.byref(proc_id))
        if proc_id.value == pid and _user32.IsWindowVisible(hwnd):
            found_hwnd = hwnd
            return False
        return True

    cb_type = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_int, ctypes.c_int)
    while time.monotonic() < deadline:
        found_hwnd = 0
        _user32.EnumWindows(cb_type(_enum_cb), 0)
        if found_hwnd:
            return found_hwnd
        time.sleep(0.5)
    return 0


def save_window_screenshot(hwnd: int, output_path: Path) -> bool:
    """Capture a window screenshot as BMP. Returns True on success."""
    if sys.platform != "win32" or hwnd == 0:
        return False

    import struct

    rect = _RECT()
    if not _user32.GetWindowRect(hwnd, ctypes.byref(rect)):
        return False
    w = max(1, rect.right - rect.left)
    h = max(1, rect.bottom - rect.top)

    hdc_screen = _user32.GetDC(0)
    hdc_mem = _gdi32.CreateCompatibleDC(hdc_screen)
    hbmp = _gdi32.CreateCompatibleBitmap(hdc_screen, w, h)
    _gdi32.SelectObject(hdc_mem, hbmp)
    _gdi32.BitBlt(hdc_mem, 0, 0, w, h, hdc_screen, rect.left, rect.top, 0x00CC0020)

    bmi = _BITMAPINFOHEADER()
    bmi.biSize = ctypes.sizeof(_BITMAPINFOHEADER)
    bmi.biWidth = w
    bmi.biHeight = -h  # top-down
    bmi.biPlanes = 1
    bmi.biBitCount = 24
    bmi.biCompression = 0
    row_bytes = (w * 3 + 3) & ~3
    bmi.biSizeImage = row_bytes * h

    pixels = ctypes.create_string_buffer(bmi.biSizeImage)
    _gdi32.GetDIBits(hdc_mem, hbmp, 0, h, pixels, ctypes.byref(bmi), 0)

    file_size = 14 + ctypes.sizeof(_BITMAPINFOHEADER) + bmi.biSizeImage
    with open(output_path, "wb") as f:
        f.write(struct.pack("<2sIHHI", b"BM", file_size, 0, 0, 14 + ctypes.sizeof(_BITMAPINFOHEADER)))
        f.write(bytes(bmi))
        f.write(pixels.raw)

    _gdi32.DeleteObject(hbmp)
    _gdi32.DeleteDC(hdc_mem)
    _user32.ReleaseDC(0, hdc_screen)

    log.info("Screenshot saved to %s", output_path)
    return True


def send_key(hwnd: int, vk: int) -> None:
    """Send a key press+release via SendInput."""
    if sys.platform != "win32":
        return
    _user32.SetForegroundWindow(hwnd)
    inputs = (_INPUT * 2)()
    inputs[0].type = 1
    inputs[0].union.ki.wVk = vk
    inputs[1].type = 1
    inputs[1].union.ki.wVk = vk
    inputs[1].union.ki.dwFlags = 0x0002  # KEYEVENTF_KEYUP
    _user32.SendInput(2, ctypes.byref(inputs), ctypes.sizeof(_INPUT))


def run_input_scenario(hwnd: int, scenario: str) -> None:
    """Run a predefined key injection scenario. Hardcoded debug utility."""
    if not scenario or hwnd == 0:
        return

    VK_RETURN = 0x0D
    VK_TAB = 0x09
    VK_UP = 0x26
    VK_RIGHT = 0x27
    VK_SLASH = 0xBF

    _user32.ShowWindow(hwnd, 9)  # SW_RESTORE
    _user32.SetForegroundWindow(hwnd)
    time.sleep(4)

    if scenario == "console-basics":
        send_key(hwnd, VK_SLASH)
        time.sleep(0.7)
        send_key(hwnd, VK_UP)
        time.sleep(0.4)
        send_key(hwnd, VK_RETURN)
        time.sleep(1.2)
        send_key(hwnd, VK_SLASH)
        time.sleep(0.6)
        send_key(hwnd, 0x48)  # 'h'
        time.sleep(0.4)
        send_key(hwnd, VK_TAB)
        time.sleep(0.7)
        send_key(hwnd, VK_RIGHT)
        time.sleep(0.4)
        send_key(hwnd, VK_RETURN)
        time.sleep(1.2)
        send_key(hwnd, VK_SLASH)
        time.sleep(0.6)
        send_key(hwnd, VK_UP)
        time.sleep(0.4)
        send_key(hwnd, VK_RETURN)
    else:
        log.warning("Unknown input scenario: %s", scenario)


# ---------------------------------------------------------------------------
# Subcommands
# ---------------------------------------------------------------------------


def cmd_run(args: argparse.Namespace) -> int:
    repo_root = Path(__file__).resolve().parent.parent
    build_dir = Path(args.build_dir) if args.build_dir else repo_root / "build"
    config = args.config
    game_dir = Path(args.game_dir)

    if not args.modules and not args.all_builtin and not args.extra:
        log.error("run requires at least one of --modules, --all-builtin, or --extra")
        return 2

    if args.no_launch:
        args.no_restore = True

    acquire_lock(game_dir)
    try:
        if args.build:
            run_cmake_build(build_dir, config, args.target)

        if args.all_builtin:
            requested = discover_builtin_modules(repo_root)
        elif args.modules:
            requested = list(args.modules)
        else:
            requested = []

        extra_dirs: dict[str, Path] = {}
        all_known = discover_builtin_modules(repo_root)
        for extra_val in args.extra:
            name, artifact_dir = resolve_extra(extra_val, repo_root, build_dir, config)
            extra_dirs[name] = artifact_dir
            if name not in requested:
                requested.append(name)
            if name not in all_known:
                all_known.append(name)

        resolved = resolve_module_deps(
            requested, repo_root, build_dir, config, all_known
        )
        log.info("Deploying modules: %s", ", ".join(resolved))

        ts = datetime.now().strftime("%Y%m%d-%H%M%S")
        artifact_dir = repo_root / "artifacts" / f"run-{ts}"
        artifact_dir.mkdir(parents=True, exist_ok=True)

        backup = backup_game_files(game_dir, artifact_dir / "backup")
        try:
            deploy_artifacts(game_dir, build_dir, config, resolved, repo_root, extra_dirs)

            if args.no_launch:
                log.info("Deployed to %s (--no-launch, skipping game start)", game_dir)
                summary = {
                    "command": "run",
                    "modules": resolved,
                    "game_dir": str(game_dir),
                    "no_launch": True,
                }
                (artifact_dir / "summary.json").write_text(json.dumps(summary, indent=2))
                print(str(artifact_dir))
                return 0

            proc = start_game_session(game_dir)

            hwnd = 0
            if args.input_scenario or args.screenshot:
                hwnd = find_main_window(proc.pid)
            if args.input_scenario:
                run_input_scenario(hwnd, args.input_scenario)

            exited_normally = wait_game_exit(proc, args.timeout)

            if args.screenshot and hwnd:
                save_window_screenshot(hwnd, artifact_dir / "window.bmp")

            loader_dir = game_dir / "ModLoader"
            mods_dir = game_dir / "ModLoader" / "Mods"
            collect_artifacts(mods_dir, loader_dir, artifact_dir)

            summary = {
                "command": "run",
                "modules": resolved,
                "game_dir": str(game_dir),
                "timeout": args.timeout,
                "exited_normally": exited_normally,
                "exit_code": proc.returncode,
            }
            (artifact_dir / "summary.json").write_text(json.dumps(summary, indent=2))

        finally:
            if not args.no_restore:
                log.info("Restoring game files")
                restore_game_files(backup, game_dir)

        print(str(artifact_dir))
        return 0

    finally:
        release_lock(game_dir)


def cmd_smoke(args: argparse.Namespace) -> int:
    repo_root = Path(__file__).resolve().parent.parent
    build_dir = Path(args.build_dir) if args.build_dir else repo_root / "build"
    config = args.config
    game_dir = Path(args.game_dir)

    acquire_lock(game_dir)
    try:
        if args.build:
            run_cmake_build(build_dir, config)

        cases: dict[str, list[str]] = {}
        if args.cases:
            for i, group in enumerate(args.cases.split(";")):
                modules = [m.strip() for m in group.split(",") if m.strip()]
                cases[f"case_{i+1}"] = modules
        elif args.cases_file:
            cases_path = Path(args.cases_file)
            if not cases_path.is_absolute():
                cases_path = repo_root / cases_path
            with open(cases_path, "rb") as f:
                cases_data = tomllib.load(f)
            for name, modules in cases_data.get("cases", {}).items():
                cases[name] = list(modules)
        else:
            builtin = discover_builtin_modules(repo_root)
            for mod_name in builtin:
                resolved = resolve_module_deps([mod_name], repo_root, build_dir, config)
                cases[mod_name.lower()] = resolved
            cases["all_builtin"] = resolve_module_deps(builtin, repo_root, build_dir, config)

        resolved_cases: dict[str, list[str]] = {}
        for case_name, modules in cases.items():
            resolved_cases[case_name] = resolve_module_deps(modules, repo_root, build_dir, config)

        ts = datetime.now().strftime("%Y%m%d-%H%M%S")
        artifact_dir = repo_root / "artifacts" / f"smoke-{ts}"
        runs_dir = artifact_dir / "runs"

        backup = backup_game_files(game_dir, artifact_dir / "backup")
        results = []
        try:
            deploy_core_only(game_dir, build_dir, config)

            total = len(resolved_cases)
            for idx, (case_name, modules) in enumerate(resolved_cases.items(), 1):
                log.info("Running case %d/%d: %s (%s)", idx, total, case_name, ", ".join(modules))

                case_dir = runs_dir / case_name
                mods_dir = prepare_mods_dir(case_dir, modules, build_dir, config)

                proc = start_game_session(
                    game_dir,
                    env_overrides={"BML_MODS_DIR": str(mods_dir)},
                )

                time.sleep(args.duration)
                alive = proc.poll() is None

                if alive:
                    proc.terminate()
                    try:
                        proc.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        proc.kill()
                        proc.wait()

                loader_dir = game_dir / "ModLoader"
                collect_artifacts(mods_dir, loader_dir, case_dir)

                result = {
                    "case": case_name,
                    "modules": modules,
                    "survived": alive,
                    "exit_code": proc.returncode,
                }
                results.append(result)
                status = "PASS" if alive else "FAIL"
                log.info("  [%s] %s", status, case_name)

        finally:
            log.info("Restoring game files")
            restore_game_files(backup, game_dir)

        summary = {"command": "smoke", "duration": args.duration, "cases": results}
        (artifact_dir / "summary.json").write_text(json.dumps(summary, indent=2))

        print(f"\n{'Case':<30} {'Modules':<50} {'Result'}")
        print("-" * 90)
        for r in results:
            status = "PASS" if r["survived"] else "FAIL"
            print(f"{r['case']:<30} {', '.join(r['modules']):<50} {status}")

        print(f"\nArtifacts: {artifact_dir}")
        return 0 if all(r["survived"] for r in results) else 1

    finally:
        release_lock(game_dir)


def cmd_analyze(args: argparse.Namespace) -> int:
    repo_root = Path(__file__).resolve().parent.parent
    build_dir = Path(args.build_dir) if args.build_dir else repo_root / "build"
    config = args.config
    game_dir = Path(args.game_dir)

    acquire_lock(game_dir)
    try:
        if args.build:
            run_cmake_build(build_dir, config)

        builtin = discover_builtin_modules(repo_root)

        if args.modules:
            probe_targets = list(args.modules)
        else:
            probe_targets = list(builtin)

        ts = datetime.now().strftime("%Y%m%d-%H%M%S")
        artifact_dir = repo_root / "artifacts" / f"analyze-{ts}"
        runs_dir = artifact_dir / "runs"
        runs_dir.mkdir(parents=True, exist_ok=True)

        backup = backup_game_files(game_dir, artifact_dir / "backup")
        loader_dir = game_dir / "ModLoader"

        module_results: list[dict[str, Any]] = []
        baseline_report = None
        try:
            deploy_core_only(game_dir, build_dir, config)

            # Full baseline
            log.info("Running full-stack baseline")
            baseline_modules = list(builtin) + ["BML_IntegrationTest"]
            baseline_resolved = resolve_module_deps(
                baseline_modules, repo_root, build_dir, config,
                all_known_modules=list(baseline_modules),
            )
            baseline_dir = runs_dir / "baseline"
            baseline_mods = prepare_mods_dir(baseline_dir, baseline_resolved, build_dir, config)

            for report_name in ("IntegrationTestReport.json", "BuiltinModuleProbeReport.json"):
                (loader_dir / report_name).unlink(missing_ok=True)

            baseline_proc = start_game_session(
                game_dir,
                env_overrides={"BML_MODS_DIR": str(baseline_mods)},
            )
            wait_game_exit(baseline_proc, args.probe_timeout)
            collect_artifacts(baseline_mods, loader_dir, baseline_dir)

            int_report_path = loader_dir / "IntegrationTestReport.json"
            if int_report_path.exists():
                try:
                    baseline_report = json.loads(int_report_path.read_text())
                    log.info("Baseline integration report captured")
                except json.JSONDecodeError:
                    log.warning("Baseline integration report is malformed")

            time.sleep(args.pause_between)

            # Per-module probes
            driver_exe = build_dir / "bin" / config / "BMLCoreDriver.exe"

            for idx, mod_name in enumerate(probe_targets, 1):
                log.info("Probing %d/%d: %s", idx, len(probe_targets), mod_name)

                dep_set = resolve_module_deps([mod_name], repo_root, build_dir, config)
                probe_modules = list(dict.fromkeys(list(dep_set) + ["BML_IntegrationTest"]))

                probe_dir = runs_dir / mod_name.lower()
                probe_mods = prepare_mods_dir(probe_dir, probe_modules, build_dir, config)

                if driver_exe.exists():
                    driver_result = subprocess.run(
                        [str(driver_exe), "--mods", str(probe_mods), "--list"],
                        cwd=str(driver_exe.parent),
                        capture_output=True, text=True,
                    )
                    (probe_dir / "driver.stdout.log").write_text(driver_result.stdout)
                    (probe_dir / "driver.stderr.log").write_text(driver_result.stderr)

                probe_report_path = loader_dir / "BuiltinModuleProbeReport.json"
                probe_report_path.unlink(missing_ok=True)

                manifest_path = find_manifest_path(mod_name, repo_root, build_dir, config)
                module_id = ""
                if manifest_path:
                    module_id = read_module_manifest(manifest_path).package_id

                probe_proc = start_game_session(
                    game_dir,
                    env_overrides={
                        "BML_MODS_DIR": str(probe_mods),
                        "BML_INTTEST_MODE": "probe",
                        "BML_PROBE_TARGET": module_id,
                        "BML_PROBE_MIN_DURATION_MS": str(args.probe_min_duration * 1000),
                    },
                )
                wait_game_exit(probe_proc, args.probe_timeout)
                collect_artifacts(probe_mods, loader_dir, probe_dir)

                probe_report = None
                if probe_report_path.exists():
                    try:
                        probe_report = json.loads(probe_report_path.read_text())
                    except json.JSONDecodeError:
                        pass

                module_results.append({
                    "name": mod_name,
                    "id": module_id,
                    "enabled_modules": probe_modules,
                    "probe_report": probe_report,
                })

                if probe_report:
                    log.info("  Probe report captured for %s", mod_name)
                else:
                    log.warning("  Probe report missing for %s", mod_name)

                if idx < len(probe_targets):
                    time.sleep(args.pause_between)

        finally:
            log.info("Restoring game files")
            restore_game_files(backup, game_dir)

        health = {
            "command": "analyze",
            "generated_at": datetime.now().isoformat(),
            "game_dir": str(game_dir),
            "config": config,
            "baseline_report": baseline_report,
            "modules": module_results,
        }
        (artifact_dir / "builtin-module-health.json").write_text(json.dumps(health, indent=2))
        print(f"\nArtifacts: {artifact_dir}")
        return 0

    finally:
        release_lock(game_dir)


def cmd_resolve(args: argparse.Namespace) -> int:
    repo_root = Path(__file__).resolve().parent.parent
    build_dir = Path(args.build_dir) if args.build_dir else repo_root / "build"
    config = args.config

    builtin = discover_builtin_modules(repo_root)

    if args.modules:
        requested = args.modules
    else:
        requested = list(builtin)

    resolved = resolve_module_deps(requested, repo_root, build_dir, config)

    id_index = build_package_id_index(
        builtin + [m for m in resolved if m not in builtin],
        repo_root, build_dir, config,
    )
    dep_graph: dict[str, list[str]] = {}
    for name in resolved:
        path = find_manifest_path(name, repo_root, build_dir, config)
        if path:
            manifest = read_module_manifest(path)
            dep_graph[name] = [
                dep_id
                for dep_id, dep_spec in manifest.dependencies.items()
                if not dep_spec.optional or id_index.get(dep_id, "") in set(resolved)
            ]
        else:
            dep_graph[name] = []

    output = {
        "builtin_modules": builtin,
        "resolved_modules": resolved,
        "dependency_graph": dep_graph,
    }
    print(json.dumps(output, indent=2))
    return 0


# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="bml-test",
        description="Unified real-game test tool for BallanceModLoaderPlus",
    )
    parser.add_argument("-v", "--verbose", action="store_true", help="Enable verbose output")
    sub = parser.add_subparsers(dest="command", required=True)

    def add_common(p: argparse.ArgumentParser) -> None:
        p.add_argument("--game-dir", default=DEFAULT_GAME_DIR)
        p.add_argument("--build-dir", default="")
        p.add_argument("--config", default="Release")
        p.add_argument("--build", action="store_true", help="Run cmake --build first")

    # run
    p_run = sub.add_parser("run", help="Deploy modules and launch game")
    add_common(p_run)
    g = p_run.add_mutually_exclusive_group()
    g.add_argument("--modules", type=comma_list, default=None)
    g.add_argument("--all-builtin", action="store_true")
    p_run.add_argument("--extra", action="append", default=[])
    p_run.add_argument("--target", type=comma_list, default=None)
    p_run.add_argument("--timeout", type=int, default=300)
    p_run.add_argument("--screenshot", action="store_true")
    p_run.add_argument("--no-restore", action="store_true")
    p_run.add_argument("--no-launch", action="store_true")
    p_run.add_argument("--input-scenario", default="")

    # smoke
    p_smoke = sub.add_parser("smoke", help="Batch crash detection")
    add_common(p_smoke)
    p_smoke.add_argument("--cases-file", default="")
    p_smoke.add_argument("--cases", default="")
    p_smoke.add_argument("--duration", type=int, default=15)

    # analyze
    p_analyze = sub.add_parser("analyze", help="Per-module isolation probe")
    add_common(p_analyze)
    g = p_analyze.add_mutually_exclusive_group()
    g.add_argument("--modules", type=comma_list, default=None)
    g.add_argument("--all-builtin", action="store_true")
    p_analyze.add_argument("--probe-timeout", type=int, default=90)
    p_analyze.add_argument("--probe-min-duration", type=int, default=8)
    p_analyze.add_argument("--pause-between", type=int, default=3)

    # resolve
    p_resolve = sub.add_parser("resolve", help="Dry run: output resolved modules as JSON")
    p_resolve.add_argument("--build-dir", default="")
    p_resolve.add_argument("--config", default="Release")
    g = p_resolve.add_mutually_exclusive_group()
    g.add_argument("--modules", type=comma_list, default=None)
    g.add_argument("--all-builtin", action="store_true")

    return parser


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(levelname)s: %(message)s",
    )

    # Validate --game-dir for subcommands that need it
    if args.command in ("run", "smoke", "analyze"):
        if not getattr(args, "game_dir", None):
            log.error(
                "--game-dir is required (or set BML_GAME_DIR environment variable)"
            )
            return 1

    dispatch = {
        "run": cmd_run,
        "smoke": cmd_smoke,
        "analyze": cmd_analyze,
        "resolve": cmd_resolve,
    }
    return dispatch[args.command](args)


if __name__ == "__main__":
    sys.exit(main())

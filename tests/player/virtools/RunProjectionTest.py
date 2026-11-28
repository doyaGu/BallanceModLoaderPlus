"""Verify projection hooking and the retail fallback in a disposable Player."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def check_projection(args, name, alter_image, expected):
    fixture = Path(tempfile.mkdtemp(prefix=f"projection-{name}-", dir=args.output))
    game = fixture / "game"
    shutil.copytree(args.runtime, game, ignore=shutil.ignore_patterns("Mods", "*.log"))
    loader = game / "ModLoader"
    mods = loader / "Mods"
    mods.mkdir(parents=True, exist_ok=True)
    for path in (args.probe, Path(str(args.probe) + ".abi.json")):
        shutil.copy2(path, mods)
    shutil.copy2(args.prototype, game / "BuildingBlocks")

    settings = loader / "Configs/BML.cfg"
    settings.parent.mkdir(parents=True, exist_ok=True)
    settings.write_text("Graphics {\n\tB WidescreenFix 1\n}\n", encoding="utf-8")
    if alter_image:
        # A PE overlay changes the fingerprint without changing loaded code.
        with (game / "RenderEngines/CK2_3D.dll").open("ab") as renderer:
            renderer.write(b"\x00")

    env = {key: value for key, value in os.environ.items() if not key.startswith("BML_PORT_CHECK_")}
    player = game / "Bin/Player.exe"
    with (fixture / "player-output.log").open("wb") as output:
        result = subprocess.run([str(player)], cwd=player.parent, env=env, stdout=output,
                                stderr=subprocess.STDOUT, timeout=50)

    log = (loader / "ModLoader.log").read_text(encoding="utf-8", errors="replace")
    for marker in (expected, "Win32 menu exit PASS", "Win32 gameplay probe PASS", "Win32 unload PASS", "Goodbye!"):
        if marker not in log or result.returncode != 0:
            raise RuntimeError(f"{name} projection regression: missing {marker!r} or exit {result.returncode}; {fixture}")
    print(f"PASS {name}: {fixture}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("runtime", "probe", "prototype", "output"):
        parser.add_argument(f"--{name}", type=lambda value: Path(value).resolve(), required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    check_projection(args, "retail", False, "Widescreen correction enabled for the retail CK2_3D.dll")
    check_projection(args, "fallback", True, "Widescreen correction unavailable for this CK2_3D.dll")


if __name__ == "__main__":
    main()

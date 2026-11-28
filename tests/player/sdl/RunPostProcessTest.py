"""Run the SDK probe in disposable copies of a prepared SDL Player runtime."""

import argparse
import configparser
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from ScriptExamples import stage_examples


def stage(source, probe, destination, mods_from=None, scripts=False, examples=False, tutorials=False):
    runtime = destination / "runtime"
    # Materialize links so neither Player nor test staging can write back into
    # the supplied runtime through a symlink to its configuration or Mods.
    shutil.copytree(source, runtime, symlinks=False)
    mods = runtime / "ModLoader" / "Mods"
    if mods.exists():
        mods.rename(destination / "original-mods")
    mods.mkdir(parents=True)
    if mods_from:
        shutil.copytree(mods_from, mods, dirs_exist_ok=True, symlinks=False)
    identities = stage_examples(mods, tutorials) if examples else []
    if scripts:
        for name in ("DependencyProbe.mod.as", "ScriptAccessProbe.mod.as",
                     "ScriptRuntimeProbe.mod.as", "ScriptEventProbe.mod.as",
                     "BehaviorInstallationsProbe.mod.as",
                     "ScriptCommandContractProbe.mod.as"):
            shutil.copy2(Path(__file__).with_name(name), mods / name)
    installed = mods / probe.name
    shutil.copy2(probe, installed)
    shutil.copy2(Path(str(probe) + ".abi.json"), Path(str(installed) + ".abi.json"))
    if installed.read_bytes() != probe.read_bytes():
        raise RuntimeError("Staging changed the SDK-built probe")
    candidates = [runtime / "Bin" / "Player", runtime / "Bin" / "Player.exe",
                  runtime / "Player", runtime / "Player.exe"]
    player = next((path for path in candidates if path.is_file()), None)
    if player is None:
        raise RuntimeError("Player executable not found in the prepared runtime")
    config = configparser.ConfigParser()
    config.optionxform = str
    ini = player.parent / "Player.ini"
    config.read(ini, encoding="utf-8-sig")
    for section, values in {
        "Startup": {"ManualSetup": "0", "LogMode": "1", "Verbose": "1"},
        "Graphics": {"FullScreen": "0"},
        "Window": {"ClipCursor": "0", "AlwaysHandleInput": "1"},
        "Game": {"SkipOpening": "1"},
    }.items():
        if not config.has_section(section):
            config.add_section(section)
        for key, value in values.items():
            config.set(section, key, value)
    with ini.open("w", encoding="utf-8") as file:
        config.write(file)
    return runtime, player, identities


def run(args):
    probe_abi = json.loads(Path(str(args.probe) + ".abi.json").read_text(encoding="utf-8"))
    host_abi = json.loads((args.runtime / "ModLoader/BML.abi.json").read_text(encoding="utf-8"))
    for key in ("schema", "runtime", "platform", "architecture", "host_linkage", "mod_api", "mod_api_major"):
        if probe_abi.get(key) != host_abi.get(key):
            raise RuntimeError(f"Probe/Player ABI mismatch ({key}): {probe_abi.get(key)} != {host_abi.get(key)}")
    for name, required in probe_abi.get("extensions", {}).items():
        if required != host_abi.get("extension_abis", {}).get(name):
            raise RuntimeError(f"Probe/Player extension ABI mismatch ({name})")
    args.output.mkdir(parents=True, exist_ok=True)
    results = []
    scripts = host_abi.get("scripts", False) and not args.without_script_consumers
    if args.script_examples and not host_abi.get("scripts", False):
        raise RuntimeError("Script examples require a script-enabled runtime")
    for cycle in range(args.repeat):
        directory = Path(tempfile.mkdtemp(prefix=f"cycle-{cycle + 1}-", dir=args.output))
        runtime, player, identities = stage(args.runtime, args.probe, directory, args.mods_from,
                                            scripts, args.script_examples, args.tutorial_examples)
        result_path = directory / "result.json"
        env = os.environ.copy()
        env["BML_POSTPROCESS_RESULT"] = str(result_path)
        if args.dump_api:
            env["BML_SCRIPT_API_DUMP"] = str(directory / "bml-script-api.as")
        env["BML_EXPECT_MODS"] = ",".join(args.expect_mod + identities)
        env["BML_EXPECT_SCRIPTS"] = "1" if scripts else "0"
        if args.ckas_self_test:
            env["CKAS_SELFTEST_MARKER"] = str(directory / "ckas-self-test.txt")
        if args.benchmark_api:
            env["BML_BENCHMARK_API"] = "1"
        with (directory / "player-output.log").open("wb") as log:
            process = subprocess.Popen([str(player)], cwd=player.parent, env=env,
                                       stdout=log, stderr=subprocess.STDOUT)
            try:
                exit_code = process.wait(timeout=args.timeout)
            except subprocess.TimeoutExpired:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
                raise RuntimeError(f"Player timed out; logs: {directory}")
        if not result_path.exists():
            raise RuntimeError(f"Probe produced no result (Player exit {exit_code}); logs: {directory}")
        result = json.loads(result_path.read_text(encoding="utf-8"))
        log = (runtime / "ModLoader" / "ModLoader.log").read_text(encoding="utf-8", errors="replace")
        if host_abi.get("scripts", False) and (
                "Registered BML AngelScript bindings" not in log or
                "Unregistered BML AngelScript bindings" not in log or
                "Failed to unregister BML AngelScript bindings" in log):
            raise RuntimeError(f"Script extension lifecycle regression; logs: {directory}")
        if args.ckas_self_test:
            marker_path = Path(env["CKAS_SELFTEST_MARKER"])
            if not marker_path.is_file() or not {"status=ok", "stage=complete"}.issubset(
                    marker_path.read_text(encoding="utf-8").splitlines()):
                raise RuntimeError(f"CKAngelScript startup self-tests did not pass; logs: {directory}")
        if args.script_examples:
            for marker in ("BML single-file script smoke loaded resource=true",
                           "BML zip script smoke loaded resource=true",
                           "BML DataArray text smoke: PASS"):
                if marker not in log:
                    raise RuntimeError(f"Script example regression ({marker}); logs: {directory}")
        result.update(exit_code=exit_code,
                      script_examples=len(identities),
                      probe_sha256=hashlib.sha256(args.probe.read_bytes()).hexdigest(),
                      staged_sha256=hashlib.sha256((runtime / "ModLoader" / "Mods" / args.probe.name).read_bytes()).hexdigest(),
                      unloaded=Path(str(result_path) + ".unloaded").exists(),
                      clean_shutdown="Goodbye!" in log,
                      directory=str(directory))
        if args.benchmark_api:
            metrics = result.get("api_benchmark_ns", {})
            for name in ("directory", "key_name", "keyboard_blocked", "keyboard_raw"):
                value = metrics.get(name, 0)
                if not math.isfinite(value) or value <= 0:
                    raise RuntimeError(f"Missing API benchmark sample ({name}); logs: {directory}")
        results.append(result)
        (args.output / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
        if not (result["passed"] and exit_code == 0 and result["unloaded"] and result["clean_shutdown"]
                and result["probe_sha256"] == result["staged_sha256"]):
            raise RuntimeError(f"PostProcess regression: {result}")
        if args.dump_api:
            if not (directory / "bml-script-api.as").is_file():
                raise RuntimeError("API export missing; rebuild the probe with matching CKAngelScript headers")
            shutil.copy2(directory / "bml-script-api.as", args.dump_api)
        print(f"PASS cycle {cycle + 1}: {result}", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=lambda p: Path(p).resolve(), required=True)
    parser.add_argument("--probe", type=lambda p: Path(p).resolve(), required=True)
    parser.add_argument("--output", type=lambda p: Path(p).resolve(), required=True)
    parser.add_argument("--repeat", type=int, default=3)
    parser.add_argument("--mods-from", type=lambda p: Path(p).resolve())
    parser.add_argument("--expect-mod", action="append", default=[])
    parser.add_argument("--timeout", type=float, default=60)
    parser.add_argument("--dump-api", type=lambda p: Path(p).resolve())
    parser.add_argument("--script-examples", action="store_true",
                        help="Compile and load the SDK and positive smoke examples")
    parser.add_argument("--tutorial-examples", action="store_true",
                        help="Also verify documentation examples with a matching documentation checkout")
    parser.add_argument("--without-script-consumers", action="store_true",
                        help="Exercise extension registration and teardown without script modules")
    parser.add_argument("--ckas-self-test", action="store_true",
                        help="Require CKAngelScript startup self-tests in the same Player")
    parser.add_argument("--benchmark-api", action="store_true", help="Measure common SDK calls in Player")
    arguments = parser.parse_args()
    if arguments.tutorial_examples:
        arguments.script_examples = True
    if arguments.repeat < 1 or arguments.timeout <= 0:
        parser.error("repeat and timeout must be positive")
    if arguments.output == arguments.runtime or arguments.runtime in arguments.output.parents:
        parser.error("output must be outside the source runtime")
    try:
        run(arguments)
    except (RuntimeError, OSError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error))

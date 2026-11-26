#!/usr/bin/env python3
"""Signed local/HTTPS update, doctor and rollback checks on disposable runtimes."""

import argparse
import functools
import hashlib
import http.server
import json
import os
from pathlib import Path
import shutil
import ssl
import subprocess
import sys
import tempfile
import threading
import zipfile
import importlib.util


def run(*command, env=None, success=True):
    result = subprocess.run([str(part) for part in command], env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, encoding="utf-8", timeout=40)
    if (result.returncode == 0) != success:
        raise AssertionError(f"Unexpected exit {result.returncode}: {command}\n{result.stdout}")
    return result.stdout


def put_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--updater", type=Path, required=True)
    parser.add_argument("--production-updater", type=Path, required=True)
    parser.add_argument("--shared-fixture", type=Path, required=True)
    parser.add_argument("--player-fixture", type=Path, required=True)
    parser.add_argument("--packager", type=Path, required=True)
    parser.add_argument("--signer", type=Path, required=True)
    parser.add_argument("--platform", required=True)
    parser.add_argument("--architecture", required=True)
    parser.add_argument("--runtime-archive", type=Path,
                        help="Extracted BML runtime archive to apply and roll back")
    parser.add_argument("--player-runtime", type=Path,
                        help="Complete Player with the runtime archive already extracted over it")
    args = parser.parse_args()
    args.updater, args.production_updater = args.updater.resolve(), args.production_updater.resolve()
    spec = importlib.util.spec_from_file_location("packager", args.packager)
    packager = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(packager)
    signer = str(args.signer.resolve())
    target = {"runtime": "sdl", "platform": args.platform, "architecture": args.architecture, "host_linkage": "dynamic"}
    prefix, suffix = {"windows": ("", ".dll"), "linux": ("lib", ".so"), "macos": ("lib", ".dylib")}[args.platform]
    loader_relative = f"Managers/{prefix}BMLPlus{suffix}"
    player_name = "Player.exe" if args.platform == "windows" else "Player"
    with tempfile.TemporaryDirectory(prefix="bml-updater-https-") as temporary:
        root = Path(temporary).resolve()
        game = root / "中文 游戏"
        stage = root / "staged"
        output = root / "server"
        output.mkdir()
        abi = {"schema": 2, **target, "mod_api": "bml.mod", "mod_api_major": 1,
               "sdk_abi": "a" * 64, "configuration": "Debug", "scripts": False,
               "extension_abis": {"engine": "b" * 64 + "-Debug", "imgui": "c" * 64 + "-Debug"},
               "version": "v0"}
        put_json(game / "ModLoader/BML.abi.json", abi)
        # These executable copies test format/architecture diagnostics only;
        # the fixture never starts an engine or a real game scene.
        for relative in (f"Bin/{player_name}", f"Bin/{prefix}CK2{suffix}", f"Bin/{prefix}VxMath{suffix}",
                         f"RenderEngines/{prefix}CK2_3D{suffix}", loader_relative):
            path = game / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy(args.production_updater if relative == f"Bin/{player_name}" else args.shared_fixture, path)
        (game / "ModLoader/Configs").mkdir()
        (game / "ModLoader/Configs/user.cfg").write_text("user config\n")
        (game / "ModLoader/Mods").mkdir()
        (game / "ModLoader/Mods/user.bmodp").write_bytes(b"user mod")
        stage_abi = {**abi, "version": "v1"}
        put_json(stage / "ModLoader/BML.abi.json", stage_abi)
        (stage / "Managers").mkdir()
        (stage / loader_relative).write_bytes(args.shared_fixture.read_bytes() + b"new version payload")
        (stage / loader_relative).chmod(0o755)
        (stage / "ModLoader/Fonts").mkdir()
        (stage / "ModLoader/Fonts/中文.txt").write_text("字体数据", encoding="utf-8")
        (stage / "ModLoader/Fonts/empty.txt").touch()
        (stage / "ModLoader/Licenses").mkdir()
        (stage / "ModLoader/Licenses/crypto.txt").write_text("test license attribution\n")
        # The manual bootstrap executable must never enter an update ZIP.
        (stage / "Bin").mkdir()
        shutil.copy(args.production_updater, stage / f"Bin/Updater{'.exe' if os.name == 'nt' else ''}")
        key = root / "test-\u7b7e\u540d key.der"
        key.write_bytes(bytes.fromhex("30770201010420" + "0" * 63 + "1a00a06082a8648ce3d030107a14403420004"
            "6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296"
            "4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5"))
        certificate, tls_key = root / "tls.crt", root / "tls.key"
        fixtures = Path(__file__).with_name("fixtures")
        shutil.copy2(fixtures / "localhost.crt", certificate)
        shutil.copy2(fixtures / "localhost.key", tls_key)
        requests = []

        class Handler(http.server.SimpleHTTPRequestHandler):
            def log_message(self, *unused):
                pass

            def do_GET(self):
                requests.append(self.path)
                if self.path.startswith("/redirect/"):
                    self.send_response(302)
                    self.send_header("Location", self.path.removeprefix("/redirect"))
                    self.end_headers()
                    self.wfile.write(b"ignore redirect response body")
                elif self.path.startswith("/downgrade/"):
                    self.send_response(302)
                    self.send_header("Location", "http://127.0.0.1:1/forbidden")
                    self.end_headers()
                elif self.path.startswith("/incomplete/"):
                    self.send_response(200)
                    self.send_header("Content-Length", "10000")
                    self.end_headers()
                    self.wfile.write(b"partial ZIP")
                    self.close_connection = True
                else:
                    super().do_GET()

        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), functools.partial(Handler, directory=str(output)))
        tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        tls.load_cert_chain(certificate, tls_key)
        server.socket = tls.wrap_socket(server.socket, server_side=True)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            base_url = f"https://127.0.0.1:{server.server_port}"
            env = dict(os.environ, CURL_CA_BUNDLE=str(certificate), NO_PROXY="127.0.0.1,localhost", no_proxy="127.0.0.1,localhost")
            for name in ("HTTP_PROXY", "HTTPS_PROXY", "ALL_PROXY", "http_proxy", "https_proxy", "all_proxy"):
                env.pop(name, None)

            def updater(*command, success=True, runtime=game, environment=env, executable=args.updater):
                return run(executable, *command, "--game-root", runtime, env=environment, success=success)

            def publish_channel(channel):
                path = output / "sdl" / target["platform"] / target["architecture"] / target["host_linkage"] / "stable.json"
                put_json(path, channel)
                path.with_suffix(".json.sig").write_bytes(packager.sign(path.read_bytes(), key, signer))

            assert "doctor passed" in updater("doctor")
            saved_loader = (game / loader_relative).read_bytes()
            (game / loader_relative).write_bytes(b"invalid binary")
            assert "wrong-format-or-architecture" in updater("doctor", success=False)
            (game / loader_relative).write_bytes(saved_loader)
            for unsupported in ({"mod_api": "unknown"}, {"mod_api_major": 2}):
                put_json(stage / "ModLoader/BML.abi.json", {**stage_abi, **unsupported})
                rejected = run(sys.executable, args.packager, "--stage", stage,
                    "--output", root / "unsupported-protocol", "--version", "v1",
                    "--signing-key", key, "--signer", signer, success=False)
                assert "unsupported native Mod C protocol" in rejected
            put_json(stage / "ModLoader/BML.abi.json", stage_abi)
            run(sys.executable, args.packager, "--stage", stage, "--output", output, "--version", "v1",
                "--signing-key", key, "--signer", signer, "--base-url", base_url)
            package = next(output.glob("*.zip"))
            manifest_path = next(output.glob("*.manifest.json"))
            channel_path = output / "sdl" / target["platform"] / target["architecture"] / target["host_linkage"] / "stable.json"
            channel = json.loads(channel_path.read_text())
            original_channel = dict(channel)
            with zipfile.ZipFile(package) as archive:
                assert json.loads(archive.read("ModLoader/BML.abi.json")) == stage_abi
                assert not any("Updater" in name for name in archive.namelist())
                if os.name != "nt":
                    assert (archive.getinfo(loader_relative).external_attr >> 16) & 0o111
            legacy_stage = root / "schema1-stage"
            shutil.copytree(stage, legacy_stage)
            legacy_abi = {name: value for name, value in stage_abi.items()
                          if name not in {"mod_api", "mod_api_major", "extension_abis"}}
            legacy_abi["schema"] = 1
            put_json(legacy_stage / "ModLoader/BML.abi.json", legacy_abi)
            legacy_output = root / "schema1-output"
            run(sys.executable, args.packager, "--stage", legacy_stage,
                "--output", legacy_output, "--version", "v1",
                "--signing-key", key, "--signer", signer)
            with zipfile.ZipFile(next(legacy_output.glob("*.zip"))) as archive:
                assert json.loads(archive.read("ModLoader/BML.abi.json")) == legacy_abi
            before = (game / loader_relative).read_bytes()
            assert "local package verified" in updater("verify-local", package)
            assert "Detached signature verification failed" in updater("verify-local", package, success=False, executable=args.production_updater)
            updater("plan-local", package)
            assert (game / loader_relative).read_bytes() == before
            player = subprocess.Popen([str(args.player_fixture)])
            try:
                assert "Close Player" in updater("apply-local", package, success=False)
                assert "Close Player" in updater("doctor", success=False)
                assert (game / loader_relative).read_bytes() == before
            finally:
                player.terminate()
                player.wait(timeout=5)
            updater("apply-local", package)
            assert (game / loader_relative).read_bytes() == (stage / loader_relative).read_bytes()
            assert (game / "ModLoader/Licenses/crypto.txt").read_text() == "test license attribution\n"
            assert "installedVersion=v1" in updater("status")
            updater("rollback")
            assert (game / loader_relative).read_bytes() == before
            assert "installedVersion=v0" in updater("status")
            assert not (game / "ModLoader/Fonts/中文.txt").exists()
            assert not (game / "ModLoader/Licenses/crypto.txt").exists()
            assert (game / "ModLoader/Configs/user.cfg").read_text() == "user config\n"
            assert (game / "ModLoader/Mods/user.bmodp").read_bytes() == b"user mod"
            # Re-sign a ZIP with a symlink: authenticity never authorizes path escapes.
            original_package, original_manifest = package.read_bytes(), manifest_path.read_bytes()
            original_signature = manifest_path.with_suffix(".json.sig").read_bytes()
            link_data = b"../../outside"
            with zipfile.ZipFile(package, "a") as archive:
                entry = zipfile.ZipInfo("ModLoader/Fonts/link")
                entry.create_system = 3
                entry.external_attr = 0o120777 << 16
                archive.writestr(entry, link_data)
            invalid = json.loads(original_manifest)
            invalid["managedFiles"].append({"path": "ModLoader/Fonts/link", "sha256": hashlib.sha256(link_data).hexdigest(),
                                            "size": len(link_data), "mode": 420})
            invalid["package"]["sha256"] = hashlib.sha256(package.read_bytes()).hexdigest()
            put_json(manifest_path, invalid)
            manifest_path.with_suffix(".json.sig").write_bytes(packager.sign(manifest_path.read_bytes(), key, signer))
            assert "Symlinks are not allowed" in updater("verify-local", package, success=False)
            package.write_bytes(original_package)
            manifest_path.write_bytes(original_manifest)
            manifest_path.with_suffix(".json.sig").write_bytes(original_signature)
            if os.name != "nt" and os.geteuid() != 0:
                manager = game / "Managers"
                manager.chmod(0o555)
                try:
                    updater("apply-local", package, success=False)
                    assert (game / loader_relative).read_bytes() == before
                finally:
                    manager.chmod(0o755)
            updater("configure", base_url)
            updater("check")
            assert "/sdl/" in requests[-1]
            channel["manifestUrl"] = base_url + "/redirect/" + manifest_path.name
            channel["packageUrl"] = base_url + "/redirect/" + package.name
            publish_channel(channel)
            updater("update")
            assert "same trusted installed version" in updater("check")
            updater("rollback")

            wrong = {**original_channel, "target": {**target, "architecture": "x64" if target["architecture"] == "arm64" else "arm64"}}
            publish_channel(wrong)
            assert "target does not match" in updater("check", success=False)
            revoked = {**original_channel, "revokedVersions": ["v1"]}
            publish_channel(revoked)
            assert "revoked" in updater("check", success=False)
            revoked = {**original_channel, "revokedManifestHashes": [hashlib.sha256(manifest_path.read_bytes()).hexdigest()]}
            publish_channel(revoked)
            assert "revoked" in updater("download", success=False)
            publish_channel(original_channel)
            saved = manifest_path.read_bytes()
            manifest_path.write_bytes(saved + b" ")
            assert "signature verification failed" in updater("download", success=False)
            manifest_path.write_bytes(saved)
            changed = {**original_channel, "packageUrl": base_url + "/downgrade/" + package.name}
            publish_channel(changed)
            updater("download", success=False)
            changed["packageUrl"] = base_url + "/incomplete/" + package.name
            publish_channel(changed)
            updater("download", success=False)
            assert not list((game / "ModLoader/Updater/downloads").rglob("*.download-*"))
            publish_channel(original_channel)
            untrusted = dict(env)
            untrusted.pop("CURL_CA_BUNDLE", None)
            untrusted.pop("SSL_CERT_FILE", None)
            updater("check", success=False, environment=untrusted)
            # A signed channel with an unsigned alteration is rejected.
            channel_path.write_bytes(channel_path.read_bytes() + b" ")
            updater("check", success=False)
            publish_channel(original_channel)
            archive_bytes = package.read_bytes()
            package.write_bytes(b"corrupt ZIP")
            assert "hash does not match" in updater("download", success=False)
            package.write_bytes(archive_bytes)
            updater("download")  # An interrupted/failed download can be restarted.
            # Static packages replace the matching Player as one runtime, and
            # cannot be applied to a dynamic host.
            static_game, static_stage = root / "静态 游戏", root / "static-stage"
            static_abi = {**abi, "host_linkage": "static"}
            for directory in (static_game, static_stage):
                put_json(directory / "ModLoader/BML.abi.json", static_abi)
                (directory / "Bin").mkdir()
                shutil.copy(args.production_updater, directory / f"Bin/{player_name}")
            put_json(static_stage / "ModLoader/BML.abi.json", {**static_abi, "version": "v2"})
            (static_game / "Bin/Player.ini").write_text("user Player settings")
            assert "doctor passed" in updater("doctor", runtime=static_game)
            static_out = root / "static-out"
            run(sys.executable, args.packager, "--stage", static_stage, "--output", static_out, "--version", "v2",
                "--signing-key", key, "--signer", signer)
            static_package = next(static_out.glob("*.zip"))
            updater("verify-local", static_package, success=False)
            updater("apply-local", static_package, runtime=static_game)
            updater("rollback", runtime=static_game)
            assert "installedVersion=v0" in updater("status", runtime=static_game)
            assert (static_game / "Bin/Player.ini").read_text() == "user Player settings"
            # Component inventory selects owned files from a staged game.
            user_config = stage / "ModLoader/Configs/user.cfg"
            put_json(stage / "ModLoader/BML.abi.json", {**stage_abi, "version": "v3"})
            user_config.parent.mkdir()
            user_config.write_text("never publish this configuration")
            run(sys.executable, args.packager, "--stage", stage, "--output", root / "unsafe-output", "--version", "v3",
                "--signing-key", key, "--signer", signer, success=False)
            inventory = root / "install_manifest_BMLRuntime.txt"
            inventory.write_text(
                "\n".join(str(path) for path in stage.rglob("*") if path.is_file() and path != user_config) + "\n",
                encoding="utf-8",
            )
            selected = root / "selected-output"
            run(sys.executable, args.packager, "--stage", stage, "--output", selected, "--version", "v3",
                "--install-manifest", inventory, "--signing-key", key, "--signer", signer)
            with zipfile.ZipFile(next(selected.glob("*.zip"))) as archive:
                assert not any("Configs" in name or "Updater" in name for name in archive.namelist())
            if args.runtime_archive and args.player_runtime:
                install = root / "实际发行包"
                shutil.copytree(args.player_runtime, install, symlinks=False)
                descriptor = install / "ModLoader/BML.abi.json"
                package_abi = json.loads(descriptor.read_bytes())
                assert all(package_abi[name] == value for name, value in target.items())
                updater("doctor", runtime=install)
                old_abi = {**package_abi, "version": "0.0.0"}
                put_json(descriptor, old_abi)
                preserved = install / "ModLoader/Configs/release-test.cfg"
                preserved.parent.mkdir(parents=True, exist_ok=True)
                preserved.write_bytes(b"preserve user settings")
                backup_file = install / "ModLoader/Licenses/CRYPTO-NOTICES.txt"
                backup_file.write_bytes(b"previous release marker")
                bundle = root / "test-signed-release"
                run(sys.executable, args.packager, "--stage", args.runtime_archive,
                    "--output", bundle, "--version", package_abi["version"],
                    "--signing-key", key, "--signer", signer)
                update_zip = next(bundle.glob("*.zip"))
                updater("apply-local", update_zip, runtime=install)
                manifest = json.loads(next(bundle.glob("*.manifest.json")).read_bytes())
                for owned in manifest["managedFiles"]:
                    file = install / owned["path"]
                    assert hashlib.sha256(file.read_bytes()).hexdigest() == owned["sha256"], file
                    if os.name != "nt":
                        assert file.stat().st_mode & 0o777 == owned["mode"], file
                updater("doctor", runtime=install)
                updater("rollback", runtime=install)
                assert json.loads(descriptor.read_bytes()) == old_abi
                assert backup_file.read_bytes() == b"previous release marker"
                assert preserved.read_bytes() == b"preserve user settings"
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)
    print("SDL updater integration passed: signed local/HTTPS updates, TLS, redirects, revocation, target rejection, permissions, rollback and static-package routing.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

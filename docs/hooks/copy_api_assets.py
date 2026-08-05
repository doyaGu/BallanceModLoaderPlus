from pathlib import Path
from shutil import copy2


def on_post_build(config, **kwargs):
    source_dir = Path(__file__).resolve().parents[1] / "api"
    destination_dir = Path(config["site_dir"]) / "api"
    destination_dir.mkdir(parents=True, exist_ok=True)

    for name in ("as.predefined", "bml-script-mod-api.as", "bml-imgui-api.as"):
        copy2(source_dir / name, destination_dir / name)

"""Import the supplied dehinted fonts with non-reserved distribution names.

Requires fontTools only when importing, not when building or running BML+.
The archive's legacy BML.cfg is deliberately not imported.
"""

import argparse
import hashlib
import io
from pathlib import Path
import zipfile

from fontTools.ttLib import TTFont


def import_font(archive, source, digest, destination, family, style):
    data = archive.read("ModLoader/Fonts/" + source)
    if hashlib.sha256(data).hexdigest() != digest:
        raise ValueError("Unexpected font contents: " + source)

    font = TTFont(io.BytesIO(data), recalcTimestamp=False)
    names = {
        1: family,
        2: style,
        3: family.replace(" ", "") + "-Dehinted-1.0",
        4: family + " " + style,
        6: family.replace(" ", "") + "-" + style,
        16: family,
        17: style,
        18: family + " " + style,
        20: family.replace(" ", "") + "-" + style,
        21: family,
        22: style,
    }
    # Rename every existing localized family, full, unique and PostScript name.
    # Copyright, authorship, license and original version records stay intact.
    for record in font["name"].names:
        if record.nameID in names:
            record.string = names[record.nameID].encode(record.getEncoding())
    font.save(destination)
    font.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    args = parser.parse_args()
    destination = Path(__file__).resolve().parents[1] / "packaging/runtime/ModLoader/Fonts"
    with zipfile.ZipFile(args.archive) as archive:
        import_font(archive, "Terminus-dehinted.ttf",
                    "126f9ab06e4f960f4894c3d376dc0ee9248d5297d98a27734f6d3364874af3b6",
                    destination / "BMLMono-dehinted.ttf", "BML Mono", "Medium")
        import_font(archive, "IBMPlexSansSC-dehinted.ttf",
                    "5314b1f1ef1f3694bb08163cff785a2c6d5e0a17bbdd55b7c312d5032d0c96d7",
                    destination / "BMLSansSC-dehinted.ttf", "BML Sans SC", "Regular")


if __name__ == "__main__":
    main()

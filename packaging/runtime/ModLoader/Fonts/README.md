# Bundled UI fonts

The default profile uses BML Mono at logical size 32, followed by BML Sans SC
at size 30. Windows symbol and emoji fallbacks remain enabled. These sizes are
scaled from a 1200-pixel viewport height; the `font` command can change them.
Existing user settings are not replaced by these defaults.

BML Mono is the dehinted Terminus (TTF) Medium 4.49.3 supplied in the font
improvement pack. BML Sans SC is its dehinted IBM Plex Sans SC Regular 1.000.
The distributed family, full, unique and PostScript names were changed to avoid
using the upstream reserved font names for modified fonts. Outlines, metrics,
character maps and copyright records are retained. Both fonts remain under the
SIL Open Font License 1.1; see [LICENSE.txt](LICENSE.txt).

Upstream sources:

- [Terminus TTF](https://files.ax86.net/terminus-ttf/)
- [IBM Plex](https://github.com/IBM/plex)

`unifont.otf` remains available as an alternative for existing profiles. This
change does not alter the game menu fonts or install fonts into Windows.

To reproduce the renamed files, install Python fontTools and run
`python scripts/Import-BMLFontPack.py <archive>` from the repository. The input
archive's SHA-256 is
`9dd2fdd524f9b336058efe6c37014a17087e35400b13784e1efd62ad332dc0a5`.
The importer checks each source font's hash and does not import the legacy
configuration file or its unrelated gameplay settings.

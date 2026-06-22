"""Register AngelScript code fences as C++-style Pygments highlighting."""

from pygments.lexers import _mapping


def _register_angelscript_alias() -> None:
    module, name, aliases, filenames, mimetypes = _mapping.LEXERS["CppLexer"]
    if "angelscript" in aliases:
        return
    _mapping.LEXERS["CppLexer"] = (
        module,
        name,
        tuple(aliases) + ("angelscript",),
        filenames,
        mimetypes,
    )


_register_angelscript_alias()

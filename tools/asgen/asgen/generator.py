from __future__ import annotations

from pathlib import Path
from .builder import BindingBuilder
from .emit_cpp import emit_header, emit_source
from .emit_report import emit_report
from .emit_stub import emit_stub
from .source import load_cimgui_source
from .type_mapper import TypeMapper


class Generator:
    def __init__(self, root: Path) -> None:
        self.root = root
        source = load_cimgui_source(root)
        self.definitions = source.definitions
        self.structs_enums = source.structs_enums
        self.enum_groups = source.enum_groups
        self.report: list[tuple[str, str]] = []
        self.used_enums: set[str] = set()
        self.opaque_handle_types: set[str] = set()
        self.mapper = TypeMapper(self.enum_groups, self.used_enums, self.opaque_handle_types)
        self.builder = BindingBuilder(self.definitions, self.mapper, self.report)

    def generate(self) -> tuple[str, str, str, str]:
        functions = self.builder.build()

        header = emit_header()
        source = emit_source(self, functions)
        stub = emit_stub(self, functions)
        report = emit_report(self, functions)
        return header, source, stub, report

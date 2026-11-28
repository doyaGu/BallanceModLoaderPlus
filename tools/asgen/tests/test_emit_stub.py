from __future__ import annotations

import sys
import unittest
from pathlib import Path
from types import SimpleNamespace


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from asgen.emit_stub import emit_stub


class PropertyReferenceTest(unittest.TestCase):
    def test_style_is_writable_and_input_is_read_only(self):
        context = SimpleNamespace(
            used_enums=set(),
            enum_groups={},
            opaque_handle_types={"ImGuiIO", "ImGuiStyle"},
        )
        reference = emit_stub(context, [])

        self.assertIn("float get_Alpha() property;", reference)
        self.assertIn("void set_Alpha(float value) property;", reference)
        self.assertIn("void set_WindowPadding(ImVec2 value) property;", reference)
        self.assertIn("float get_DeltaTime() property;", reference)
        self.assertNotIn("set_DeltaTime(", reference)
        self.assertNotIn("set_WantCaptureMouse(", reference)


if __name__ == "__main__":
    unittest.main()

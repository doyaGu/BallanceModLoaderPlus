"""Check the runtime-export publisher without requiring a graphical Player."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("script_api", ROOT / "tools/generate_script_api.py")
api = importlib.util.module_from_spec(spec)
spec.loader.exec_module(api)


class ScriptApiCodegenTest(unittest.TestCase):
    def test_real_registration_annotations_include_wrapped_cpp_literals(self):
        declarations = api.annotations(ROOT)
        signature = ("int Register(const CommandDefinition &in definition, CommandExecute@ execute, "
                     "CommandRef@ &out command, CommandComplete@ complete = null) const")
        self.assertEqual(declarations[api.canonical(signature)], (0, 2))
        self.assertEqual(api.canonical("array<string>@"), api.canonical("string[]@"))

    def test_publishing_replaces_only_the_owned_editor_section_and_is_idempotent(self):
        source = "engine before\nnamespace BML {\nold\n} // namespace BML\nengine after\n"
        first = api.combine(source, "namespace BML {\nnew\n}\n")
        self.assertEqual(api.combine(first, "namespace BML {\nnew\n}\n"), first)
        self.assertTrue(first.startswith("engine before\n"))
        self.assertTrue(first.endswith("engine after\n"))
        self.assertNotIn("old", first)

    def test_stale_compatibility_exports_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "Obsolete"):
            api.prepare("namespace BML { class InputHook {} }", ROOT)
        with self.assertRaisesRegex(ValueError, "Missing"):
            api.prepare("namespace BML {}", ROOT)

    def test_publisher_detects_stale_output_without_requiring_checked_in_docs(self):
        export = """namespace BML {
class ModContext {
    Config@ Config() const;
    Input@ Input() const;
    int ReadMods(? &out mods) const;
}
class Commands { int ReadCommands(? &out commands) const; }
class ModRef { int CheckDependencies(bool &out satisfied) const; }
}
"""
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            assets = root / "docs/api"
            assets.mkdir(parents=True)
            editor = assets / "as.predefined"
            editor.write_text("engine before\nnamespace BML {\nold\n} // namespace BML\nengine after\n")
            source = root / "export.as"
            source.write_text(export)
            api.publish(root, source, check=False)
            api.publish(root, source, check=True)
            self.assertIn("engine before", editor.read_text())
            (assets / "bml-script-mod-api.as").write_text("stale")
            with self.assertRaisesRegex(ValueError, "Stale script declarations"):
                api.publish(root, source, check=True)


if __name__ == "__main__":
    unittest.main()

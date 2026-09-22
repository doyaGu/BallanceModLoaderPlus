#!/usr/bin/env python3

from __future__ import annotations

import csv
import unittest
from pathlib import Path


class IvpHeaderCompletionSitesTest(unittest.TestCase):
    def test_cross_owner_definitions_remain_explicit_and_locatable(self) -> None:
        root = Path(__file__).parents[1]
        inventory = root / "tools/ivp/header-completion-sites.tsv"
        with inventory.open("r", encoding="utf-8", newline="") as stream:
            rows = list(csv.DictReader(stream, delimiter="\t"))

        self.assertEqual(len(rows), 9)
        seen = set()
        for row in rows:
            defining_path = root / row["defining_header"]
            declaration_path = root / row["declaration_header"]
            self.assertTrue(defining_path.is_file(), defining_path)
            self.assertTrue(declaration_path.is_file(), declaration_path)
            defining_text = defining_path.read_text(encoding="utf-8")
            declaration_text = declaration_path.read_text(encoding="utf-8")
            owners = row["owner"].split(";")
            methods = row["methods"].split(";")
            key = (row["defining_header"], row["owner"], row["methods"])
            self.assertNotIn(key, seen)
            seen.add(key)

            for method in methods:
                self.assertIn(f"{method}(", declaration_text)
                self.assertTrue(
                    any(f"{owner}::{method}(" in defining_text for owner in owners),
                    f"{method} is no longer defined at {defining_path}",
                )


if __name__ == "__main__":
    unittest.main()

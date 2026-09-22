#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import re
import sys
import tempfile
import unittest
from pathlib import Path


def load_audit(source_root: Path):
    script = source_root / "tools" / "ivp" / "Audit-IvpPublicInterface.py"
    spec = importlib.util.spec_from_file_location("ivp_public_audit", script)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot import {script}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class IvpPublicAuditDefinitionTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source_root = Path(__file__).parents[1]
        cls.audit = load_audit(cls.source_root)

    def test_retail_manifest_lives_with_the_ivp_runtime(self) -> None:
        self.assertEqual(
            Path("src/IVP/generated/IvpSymbols.inc"),
            self.audit.RETAIL_MANIFEST_PATH,
        )
        self.assertTrue(
            (self.source_root / self.audit.RETAIL_MANIFEST_PATH).is_file()
        )

    def test_public_header_marker_accepts_source_formatting(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            spaced = root / "spaced.hxx"
            compact = root / "compact.hxx"
            unrelated = root / "unrelated.hxx"
            spaced.write_text("// IVP_EXPORT_PUBLIC\n", encoding="latin-1")
            compact.write_text("//IVP_EXPORT_PUBLIC\n", encoding="latin-1")
            unrelated.write_text("// IVP_EXPORT_PRIVATE\n", encoding="latin-1")

            self.assertEqual(
                [compact, spaced],
                self.audit.find_reference_headers(root),
            )

    def test_repository_evidence_ledger_keeps_ida_and_player_separate(self) -> None:
        entries = self.audit.load_curated_evidence(
            self.source_root / "tools" / "ivp" / "public-api-evidence.tsv"
        )
        self.assertIn(
            ("IVP_Template_Suspension", "layout-ida-only"),
            {(entry.owner, entry.kind) for entry in entries},
        )
        self.assertIn(
            ("IVP_Ray_Solver_Os", "ballance-player"),
            {(entry.owner, entry.kind) for entry in entries},
        )
        self.assertNotIn(
            ("IVP_Template_Suspension", "ballance-player"),
            {(entry.owner, entry.kind) for entry in entries},
        )
        constructor_layers = {
            entry.owner
            for entry in entries
            if entry.kind == "retail-constructor-reconstruction"
        }
        self.assertEqual(
            {
                "IVP_Actuator_Two_Point",
                "IVP_Constraint",
                "IVP_Constraint_Local",
                "IVP_Constraint_Local_Anchor",
                "IVP_U_Active_Value",
                "IVP_U_Active_Float",
                "IVP_U_Active_Int",
                "IVP_U_Active_Terminal_Double",
                "IVP_U_Active_Terminal_Int",
            },
            constructor_layers,
        )

    def test_ida_only_layout_owners_have_compile_time_drift_guards(self) -> None:
        entries = self.audit.load_curated_evidence(
            self.source_root / "tools" / "ivp" / "public-api-evidence.tsv"
        )
        owners = {
            entry.owner for entry in entries
            if entry.kind == "layout-ida-only"
        }
        header_text = "\n".join(
            path.read_text(encoding="utf-8")
            for path in (self.source_root / "include" / "BML" / "IVP")
            .rglob("*.h")
        )
        abi_text = (
            self.source_root / "tests" / "IvpAbiCompileTest.cpp"
        ).read_text(encoding="utf-8")
        corpus = header_text + "\n" + abi_text

        missing_sizes = {
            owner for owner in owners
            if not re.search(
                rf"sizeof\s*\(\s*{re.escape(owner)}\s*\)", header_text
            )
        }
        self.assertEqual(set(), missing_sizes)

        offset_owners = {
            owner for owner in owners
            if re.search(
                rf"offsetof\s*\(\s*{re.escape(owner)}\s*,", corpus
            )
        }
        probe_names = {
            "IVP_Controller_Stiff_Spring": "StiffSpringLayoutProbe",
            "IVP_Controller_Stiff_Spring_Active":
                "ActiveStiffSpringLayoutProbe",
            "IVP_Actuator_Stabilizer": "StabilizerLayoutProbe",
            "IVP_Actuator_Suspension": "SuspensionLayoutProbe",
            "IVP_Controller_Motion": "MotionLayoutProbe",
            "IVP_Controller_Golem": "GolemLayoutProbe",
            "IVP_Constraint_Fixed_Keyframed": "FixedKeyframedLayoutProbe",
            "IVP_Forcefield": "ForcefieldLayoutProbe",
            "IVP_SurfaceManager_Grid": "SurfaceManagerGridLayoutProbe",
            "IVP_Controller_Floating": "FloatingControllerLayoutProbe",
            "IVP_Controller_World_Friction":
                "WorldFrictionControllerLayoutProbe",
        }
        offset_owners.update(
            owner for owner, probe in probe_names.items()
            if re.search(rf"offsetof\s*\(\s*{probe}\s*,", corpus)
        )

        no_own_field = owners - offset_owners
        self.assertEqual(41, len(offset_owners))
        self.assertEqual(
            {
                "IVP_BetterStatisticsmanager_Callback_Interface",
                "IVP_Halfspacesoup",
                "IVP_Statisticsmanager_Console_Callback",
                "IVP_Template_Constraint_Fixed_Keyframed",
                "IVP_Vec_PCore",
            },
            no_own_field,
        )

    def test_retail_layout_owners_have_compile_time_size_guards(self) -> None:
        entries = self.audit.load_curated_evidence(
            self.source_root / "tools" / "ivp" / "public-api-evidence.tsv"
        )
        owners = {
            entry.owner for entry in entries
            if entry.kind == "layout-retail"
        }
        header_text = "\n".join(
            path.read_text(encoding="utf-8")
            for path in (self.source_root / "include" / "BML" / "IVP")
            .rglob("*.h")
        )

        template_instantiations = {
            "IVP_Attacher_To_Cores":
                r"sizeof\s*\(\s*IVP_Attacher_To_Cores\s*<",
            "IVP_U_BigVector": r"sizeof\s*\(\s*IVP_U_BigVector\s*<",
            "IVP_U_FVector": r"sizeof\s*\(\s*IVP_U_FVector\s*<",
            "IVP_U_Set": r"sizeof\s*\(\s*IVP_U_Set\s*<",
            "IVP_U_Set_Active": r"sizeof\s*\(\s*IVP_U_Set_Active\s*<",
            "IVP_U_Vector": r"sizeof\s*\(\s*IVP_U_Vector\s*<",
        }
        missing_sizes = {
            owner for owner in owners
            if not re.search(
                rf"sizeof\s*\(\s*{re.escape(owner)}\s*\)", header_text
            )
            and not re.search(template_instantiations.get(owner, r"(?!)"),
                              header_text)
        }

        self.assertEqual(87, len(owners))
        self.assertEqual(set(), missing_sizes)

    def test_retail_layout_fields_have_compile_time_offset_guards(self) -> None:
        entries = self.audit.load_curated_evidence(
            self.source_root / "tools" / "ivp" / "public-api-evidence.tsv"
        )
        owners = {
            entry.owner for entry in entries
            if entry.kind == "layout-retail"
        }
        header_text = "\n".join(
            path.read_text(encoding="utf-8")
            for path in (self.source_root / "include" / "BML" / "IVP")
            .rglob("*.h")
        )
        abi_text = (
            self.source_root / "tests" / "IvpAbiCompileTest.cpp"
        ).read_text(encoding="utf-8")
        corpus = header_text + "\n" + abi_text

        checkers = {
            "IVP_Actuator_Spring": "BML_IvpActuatorSpringLayoutCheck",
            "IVP_Anomaly_Manager": "BML_IvpAnomalyManagerLayoutCheck",
            "IVP_Attacher_To_Cores": "BML_IvpAttacherToCoresLayoutCheck",
            "IVP_Attacher_To_Cores_Buoyancy":
                "BML_IvpAttacherToCoresBuoyancyLayoutCheck",
            "IVP_BetterDebugmanager":
                "BML_IvpBetterDebugmanagerLayoutCheck",
            "IVP_BetterStatisticsmanager":
                "BML_IvpBetterStatisticsmanagerLayoutCheck",
            "IVP_Cache_Object_Manager":
                "BML_IvpCacheObjectManagerLayoutCheck",
            "IVP_Cluster": "BML_IvpClusterLayoutCheck",
            "IVP_Constraint": "BML_IvpConstraintLayoutCheck",
            "IVP_Constraint_Local": "BML_IvpConstraintLocalLayoutCheck",
            "IVP_Linear_Constraint_Solver":
                "BML_IvpLinearConstraintSolverLayoutCheck",
            "IVP_Listener_Collision": "BML_IvpCollisionListenerLayoutCheck",
            "IVP_Material_Manager": "BML_IvpMaterialManagerLayoutCheck",
            "IVP_Ray_Solver_Min": "BML_IvpRaySolverMinLayoutCheck",
            "IVP_Ray_Solver_Os": "BML_IvpRaySolverOsLayoutCheck",
            "IVP_SurfaceManager_Polygon":
                "BML_IvpSurfaceManagerPolygonLayoutCheck",
            "IVP_Template_Anchor": "BML_IvpTemplateAnchorLayoutCheck",
            "IVP_Template_Object": "BML_IvpTemplateObjectLayoutCheck",
            "IVP_Time": "BML_IvpTimeLayoutCheck",
            "IVP_U_Active_Terminal_Double":
                "BML_IvpActiveTerminalDoubleLayoutCheck",
            "IVP_U_Active_Terminal_Int":
                "BML_IvpActiveTerminalIntLayoutCheck",
            "IVP_U_Active_Value": "BML_IvpActiveValueLayoutCheck",
            "IVP_U_Active_Value_Manager":
                "BML_IvpActiveValueManagerLayoutCheck",
            "IVP_U_Set_Active": "BML_IvpActiveSetLayoutCheck",
            "IVP_VHash": "BML_IvpVHashLayoutCheck",
            "IVP_VHash_Store": "BML_IvpVHashStoreLayoutCheck",
        }
        offset_owners = {
            owner for owner in owners
            if re.search(rf"offsetof\s*\(\s*{re.escape(owner)}\s*,", corpus)
        }
        offset_owners.update(
            owner for owner, checker in checkers.items()
            if re.search(rf"{checker}[^\n]*::", corpus)
        )

        self.assertEqual(80, len(offset_owners))
        self.assertEqual(
            {
                "IVP_Controller",
                "IVP_U_BigVector",
                "IVP_U_FVector",
                "IVP_U_Float_Hesse",
                "IVP_U_Hesse",
                "IVP_U_Set",
                "IVP_U_Vector",
            },
            owners - offset_owners,
        )

    def test_exact_retail_route_requires_the_methods_own_address(self) -> None:
        mangled = "?step@IVP_Test@@QAEXXZ"
        reference = self.audit.Method(
            "IVP_Test", "step", "void()", mangled,
            False, False, False,
        )
        helper_only = self.audit.Method(
            "IVP_Test", "step", "void()", mangled,
            True, False, False,
            retail_invoke=True,
            retail_address_ids=("DuplicateString",),
        )
        exact = self.audit.Method(
            "IVP_Test", "step", "void()", mangled,
            True, False, False,
            retail_invoke=True,
            retail_address_ids=("TestStep",),
        )
        symbols = {mangled: 0x1234}
        addresses = {0x1234: {"TestStep"}, 0x5678: {"DuplicateString"}}

        self.assertEqual(
            "missing-retail-route",
            self.audit.exact_retail_body_route(
                reference, helper_only, symbols, addresses, []
            ),
        )
        self.assertEqual(
            "retail-address",
            self.audit.exact_retail_body_route(
                reference, exact, symbols, addresses, []
            ),
        )

    def test_curated_constructor_layer_accounts_for_no_self_call(self) -> None:
        mangled = "??0IVP_Test@@QAE@XZ"
        reference = self.audit.Method(
            "IVP_Test", "IVP_Test", "void()", mangled,
            False, False, False,
        )
        current = self.audit.Method(
            "IVP_Test", "IVP_Test", "void()", mangled,
            True, False, False,
        )
        evidence = [self.audit.CuratedEvidence(
            "IVP_Test", "IVP_Test", mangled,
            "retail-constructor-reconstruction", "difference.md", "",
        )]

        self.assertEqual(
            "layered-constructor-reconstruction",
            self.audit.exact_retail_body_route(
                reference, current, {mangled: 0x1234},
                {0x1234: {"TestConstruct"}}, evidence,
            ),
        )

    def test_clang_destructor_pseudoname_resolves_to_msvc_complete_body(self) -> None:
        reference = self.audit.Method(
            "IVP_Test", "~IVP_Test", "void()", "??_DIVP_Test@@QAEXXZ",
            False, False, True,
        )
        retail = {"??1IVP_Test@@UAE@XZ": 0x1234}

        self.assertEqual(
            ("??1IVP_Test@@UAE@XZ", 0x1234),
            self.audit.resolved_retail_symbol(reference, retail),
        )

    def test_curated_destructor_layer_accounts_for_no_nested_complete_call(self) -> None:
        mangled = "??_DIVP_Test@@QAEXXZ"
        reference = self.audit.Method(
            "IVP_Test", "~IVP_Test", "void()", mangled,
            False, False, True,
        )
        current = self.audit.Method(
            "IVP_Test", "~IVP_Test", "void()", mangled,
            True, False, True,
        )
        evidence = [self.audit.CuratedEvidence(
            "IVP_Test", "~IVP_Test", "",
            "retail-destructor-reconstruction", "difference.md", "",
        )]

        self.assertEqual(
            "layered-destructor-reconstruction",
            self.audit.exact_retail_body_route(
                reference, current, {"??1IVP_Test@@UAE@XZ": 0x1234},
                {0x1234: {"TestDestruct"}}, evidence,
            ),
        )

    def test_transitive_internal_record_is_not_public(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            public = root / "ivp_public.hxx"
            internal = root / "ivp_internal.hxx"
            public.write_text("class IVP_Public {};\n", encoding="latin-1")
            internal.write_text("class IVP_Internal {};\n", encoding="latin-1")

            ast = {
                "inner": [
                    self.record("IVP_Public", public),
                    self.record("IVP_Internal", internal),
                ]
            }
            surface = self.audit.extract_public_surface(ast, [public])

            self.assertIn("IVP_Public", surface)
            self.assertNotIn("IVP_Internal", surface)

    def test_omitted_clang_filename_is_resolved_from_source_offset(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            header = Path(directory) / "ivp_public.hxx"
            contents = b"// prefix\nclass IVP_Public {};\n"
            header.write_bytes(contents)
            offset = contents.index(b"IVP_Public")
            node = self.record("IVP_Public", None)
            node["loc"] = {
                "offset": offset,
                "line": 2,
                "col": 7,
                "tokLen": len("IVP_Public"),
                "includedFrom": {"file": "<stdin>"},
            }

            allowed = {
                self.audit.normalize_path(header): (header, contents),
            }
            self.assertTrue(self.audit.record_is_owned_by(node, allowed))

    def test_intern_fences_exclude_records_and_individual_methods(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            header = Path(directory) / "ivp_public.hxx"
            contents = (
                b"class IVP_Public {\n"
                b"public:\n"
                b"  void visible();\n"
                b"  // INTERN_START\n"
                b"  void hidden();\n"
                b"  // INTERN_END\n"
                b"};\n"
                b"// INTERN_START\n"
                b"class IVP_Internal {};\n"
                b"// INTERN_END\n"
            )
            header.write_bytes(contents)

            record = self.record("IVP_Public", header)
            record["loc"].update(
                offset=contents.index(b"IVP_Public"), line=1
            )
            record["inner"] = [
                {"kind": "AccessSpecDecl", "access": "public"},
                self.method_record("visible", header, contents, 3),
                self.method_record("hidden", header, contents, 5),
            ]
            internal = self.record("IVP_Internal", header)
            internal["loc"].update(
                offset=contents.index(b"IVP_Internal"), line=9
            )

            surface = self.audit.extract_public_surface(
                {"inner": [record, internal]}, [header]
            )

            self.assertEqual(
                {"visible"},
                {method.name for method in surface["IVP_Public"]},
            )
            self.assertNotIn("IVP_Internal", surface)

    def test_missing_overload_is_not_a_signature_difference(self) -> None:
        nearby_default = self.method("IVP_Matrix", "IVP_Matrix", "void()")
        nearby_sized = self.method("IVP_Matrix", "IVP_Matrix", "void(int)")
        current_exact = {nearby_default.key}
        current_names = {("IVP_Matrix", "IVP_Matrix")}
        exact_overloads = {("IVP_Matrix", "IVP_Matrix")}

        disposition = self.audit.classify(
            nearby_sized, current_exact, current_names, exact_overloads, set()
        )

        self.assertEqual("unresolved-missing", disposition)

    def test_unmatched_single_signature_remains_a_difference(self) -> None:
        nearby = self.method("IVP_Manager", "event", "void(int,int)")
        disposition = self.audit.classify(
            nearby, set(), {("IVP_Manager", "event")}, set(), set()
        )
        self.assertEqual("current-signature-diff", disposition)

    def test_confirmed_retail_signature_variant_is_not_an_unknown_difference(self) -> None:
        nearby = self.method("IVP_Manager", "event", "void(int,int)")
        evidence = [
            self.audit.CuratedEvidence(
                "IVP_Manager",
                "event",
                "",
                "retail-signature-variant",
                "retail-disassembly.md",
                "",
            )
        ]
        disposition = self.audit.classify(
            nearby,
            set(),
            {("IVP_Manager", "event")},
            set(),
            set(),
            evidence,
        )
        self.assertEqual("retail-confirmed-signature-variant", disposition)

    def test_retail_absent_method_is_a_compatible_omission(self) -> None:
        nearby = self.method(
            "IVP_Listener", "newer_callback", "void(int)", virtual=True
        )
        evidence = [
            self.audit.CuratedEvidence(
                "IVP_Listener",
                "newer_callback",
                "",
                "retail-interface-absent",
                "retail-vtable.md",
                "",
            )
        ]

        self.assertEqual(
            "retail-omitted-variant",
            self.audit.classify(
                nearby, set(), set(), set(), set(), evidence
            ),
        )

    def test_declaring_retail_absent_method_is_an_abi_violation(self) -> None:
        nearby = self.method(
            "IVP_Listener", "newer_callback", "void(int)", virtual=True
        )
        evidence = [
            self.audit.CuratedEvidence(
                "IVP_Listener",
                "newer_callback",
                "",
                "retail-interface-absent",
                "retail-vtable.md",
                "",
            )
        ]

        self.assertEqual(
            "retail-interface-absence-violation",
            self.audit.classify(
                nearby,
                {nearby.key},
                {(nearby.owner, nearby.name)},
                {(nearby.owner, nearby.name)},
                set(),
                evidence,
            ),
        )

    def test_virtual_abi_mismatch_is_independent_of_signature(self) -> None:
        nearby = self.method(
            "IVP_Interface", "query", "void(int)", virtual=True
        )
        current = self.method(
            "IVP_Interface", "query", "void(int)", virtual=False
        )

        self.assertEqual(nearby.key, current.key)
        self.assertEqual(
            "mismatch",
            self.audit.virtual_abi_disposition(
                nearby, {"IVP_Interface": {current}}
            ),
        )

    def test_missing_method_is_not_double_counted_as_virtual_mismatch(self) -> None:
        nearby = self.method(
            "IVP_Interface", "query", "void(int)", virtual=True
        )
        self.assertEqual(
            "not-current-exact",
            self.audit.virtual_abi_disposition(nearby, {}),
        )

    def test_callable_audit_rejects_nonvirtual_declaration_only(self) -> None:
        declaration = self.method("IVP_Value", "read", "int()")
        self.assertEqual(
            "declaration-only",
            self.audit.callable_disposition(declaration),
        )

    def test_callable_audit_accepts_header_and_virtual_forms(self) -> None:
        header_body = self.audit.Method(
            owner="IVP_Value",
            name="read",
            signature="int()",
            mangled="",
            has_body=True,
            pure_virtual=False,
            virtual=False,
        )
        pure_virtual = self.audit.Method(
            owner="IVP_Interface",
            name="read",
            signature="int()",
            mangled="",
            has_body=False,
            pure_virtual=True,
            virtual=True,
        )
        retail_virtual = self.method(
            "IVP_Retail_Interface", "read", "int()", virtual=True
        )

        self.assertEqual(
            "header-definition",
            self.audit.callable_disposition(header_body),
        )
        self.assertEqual(
            "pure-virtual-contract",
            self.audit.callable_disposition(pure_virtual),
        )
        self.assertEqual(
            "retail-vtable-dispatch",
            self.audit.callable_disposition(retail_virtual),
        )

    def test_out_of_class_definition_closes_matching_declaration(self) -> None:
        declaration = self.audit.Method(
            owner="IVP_Hesse",
            name="intersect",
            signature="int()",
            mangled="?intersect@IVP_Hesse@@QAEHXZ",
            has_body=False,
            pure_virtual=False,
            virtual=False,
        )
        ast = {
            "inner": [
                {
                    "kind": "CXXMethodDecl",
                    "name": "intersect",
                    "mangledName": declaration.mangled,
                    "inner": [{"kind": "CompoundStmt"}],
                }
            ]
        }

        result = self.audit.apply_out_of_class_definitions(
            {"IVP_Hesse": {declaration}}, ast
        )

        resolved = next(iter(result["IVP_Hesse"]))
        self.assertTrue(resolved.has_body)
        self.assertEqual(
            "header-definition",
            self.audit.callable_disposition(resolved),
        )

    def test_defaulted_method_is_a_callable_header_definition(self) -> None:
        ast = {
            "inner": [
                {
                    "kind": "CXXRecordDecl",
                    "completeDefinition": True,
                    "name": "IVP_Defaulted",
                    "tagUsed": "class",
                    "inner": [
                        {"kind": "AccessSpecDecl", "access": "public"},
                        {
                            "kind": "CXXConstructorDecl",
                            "name": "IVP_Defaulted",
                            "mangledName": "??0IVP_Defaulted@@QAE@XZ",
                            "type": {"qualType": "void ()"},
                            "explicitlyDefaulted": "default",
                            "inner": [],
                        },
                    ],
                }
            ]
        }

        method = next(iter(
            self.audit.extract_public_surface(ast)["IVP_Defaulted"]
        ))

        self.assertTrue(method.has_body)
        self.assertEqual(
            "header-definition",
            self.audit.callable_disposition(method),
        )

    def test_deleted_method_is_explicitly_unavailable(self) -> None:
        method = self.audit.Method(
            owner="IVP_Retail_Gap",
            name="removed",
            signature="void()",
            mangled="",
            has_body=False,
            pure_virtual=False,
            virtual=False,
            deleted=True,
        )
        self.assertEqual(
            "retail-unavailable",
            self.audit.callable_disposition(method),
        )

    def test_retail_virtual_variant_is_not_reported_as_a_mismatch(self) -> None:
        nearby = self.method(
            "IVP_Interface", "query", "void(int)", virtual=True
        )
        current = self.method(
            "IVP_Interface", "query", "void(int)", virtual=False
        )
        evidence = [
            self.audit.CuratedEvidence(
                "IVP_Interface",
                "query",
                "",
                "retail-virtual-variant",
                "retail-vtable.log",
                "",
            )
        ]
        self.assertEqual(
            "retail-confirmed-variant",
            self.audit.virtual_abi_disposition(
                nearby, {"IVP_Interface": {current}}, evidence
            ),
        )

    def test_ida_virtual_variant_is_not_misreported_as_retail_proof(self) -> None:
        nearby = self.method(
            "IVP_Interface", "query", "void(int)", virtual=True
        )
        current = self.method(
            "IVP_Interface", "query", "void(int)", virtual=False
        )
        evidence = [
            self.audit.CuratedEvidence(
                "IVP_Interface",
                "query",
                "",
                "ida-virtual-variant",
                "imported-vtable.log",
                "",
            )
        ]
        self.assertEqual(
            "ida-import-variant",
            self.audit.virtual_abi_disposition(
                nearby, {"IVP_Interface": {current}}, evidence
            ),
        )

    def test_inheritance_order_is_part_of_the_abi(self) -> None:
        self.assertEqual(
            "mismatch",
            self.audit.inheritance_abi_disposition(
                "IVP_Derived",
                {"IVP_Derived": ("IVP_First", "IVP_Second")},
                {"IVP_Derived": ("IVP_Second", "IVP_First")},
            ),
        )

    def test_new_virtual_declaration_order_is_part_of_the_abi(self) -> None:
        first = self.method("IVP_Value", "first", "void()", virtual=True)
        second = self.method("IVP_Value", "second", "void()", virtual=True)
        nearby = self.audit.direct_new_virtual_slots(
            {"IVP_Value": [first, second]}, {"IVP_Value": ()}
        )
        current = self.audit.direct_new_virtual_slots(
            {"IVP_Value": [second, first]}, {"IVP_Value": ()}
        )

        self.assertEqual(
            "mismatch",
            self.audit.vtable_order_abi_disposition(
                "IVP_Value", nearby, current, {"IVP_Value": ()}
            ),
        )

    def test_virtual_override_does_not_append_a_new_slot(self) -> None:
        base_step = self.method(
            "IVP_Base", "step", "void(int)", virtual=True
        )
        derived_step = self.method(
            "IVP_Derived", "step", "void(int)", virtual=True
        )
        surface = {
            "IVP_Base": [base_step],
            "IVP_Derived": [derived_step],
        }
        slots = self.audit.direct_new_virtual_slots(
            surface, {"IVP_Base": (), "IVP_Derived": ("IVP_Base",)}
        )

        self.assertEqual((('step', 'void(int)'),), slots["IVP_Base"])
        self.assertEqual((), slots["IVP_Derived"])

    def test_template_base_override_uses_specialized_slot_signature(self) -> None:
        listener = self.method(
            "IVP_Listener", "added", "void(IVP_Set<T>*,T*)", virtual=True
        )
        forcefield = self.method(
            "IVP_Forcefield",
            "added",
            "void(IVP_Set<IVP_Core>*,IVP_Core*)",
            virtual=True,
        )
        bases = {
            "IVP_Listener": (),
            "IVP_Forcefield": ("IVP_Listener",),
        }
        direct_bases = {
            "IVP_Listener": (),
            "IVP_Forcefield": (
                self.audit.DirectBase("IVP_Listener", ("IVP_Core",)),
            ),
        }
        slots = self.audit.direct_new_virtual_slots(
            {
                "IVP_Listener": [listener],
                "IVP_Forcefield": [forcefield],
            },
            bases,
            direct_bases,
            {"IVP_Listener": ("T",)},
        )

        self.assertEqual(
            (("added", "void(IVP_Set<T>*,T*)"),),
            slots["IVP_Listener"],
        )
        self.assertEqual((), slots["IVP_Forcefield"])

    def test_retail_vtable_variant_is_explicitly_class_scoped(self) -> None:
        evidence = [
            self.audit.CuratedEvidence(
                "IVP_Value",
                "*",
                "",
                "retail-vtable-variant",
                "retail-vtable.log",
                "",
            )
        ]
        self.assertEqual(
            "retail-confirmed-variant",
            self.audit.vtable_order_abi_disposition(
                "IVP_Value",
                {"IVP_Value": (("nearby", "void()"),)},
                {"IVP_Value": (("retail", "void()"),)},
                {"IVP_Value": ()},
                evidence,
            ),
        )

    def test_retail_absent_virtual_is_removed_from_vtable_comparison(self) -> None:
        evidence = [
            self.audit.CuratedEvidence(
                "IVP_Car_System",
                "later_slot",
                "",
                "retail-interface-absent",
                "retail-vtable.log",
                "",
            )
        ]
        self.assertEqual(
            "match",
            self.audit.vtable_order_abi_disposition(
                "IVP_Car_System",
                {
                    "IVP_Car_System": (
                        ("retained_slot", "void()"),
                        ("later_slot", "void()"),
                    )
                },
                {"IVP_Car_System": (("retained_slot", "void()"),)},
                {"IVP_Car_System": ()},
                evidence,
            ),
        )

    def test_fragile_inheritance_headers_precede_optional_headers(self) -> None:
        self.assertEqual(
            [
                "#include <ivp_physics.hxx>",
                "#include <ivp_time_event.hxx>",
                "#include <ivp_car_system.hxx>",
                "#include <ivp_forcefield.hxx>",
                "#include <ivp_halfspacesoup.hxx>",
            ],
            self.audit.reference_source([]).splitlines(),
        )

    def test_old_style_override_inherits_virtual_abi(self) -> None:
        base = self.method("IVP_Base", "step", "void(int)", virtual=True)
        derived = self.method("IVP_Derived", "step", "void(int)")
        surface = self.audit.apply_effective_virtuals(
            {"IVP_Base": {base}, "IVP_Derived": {derived}},
            {"IVP_Derived": ("IVP_Base",)},
        )
        resolved = next(iter(surface["IVP_Derived"]))
        self.assertTrue(resolved.virtual)

    def test_unmarked_base_supports_marked_derived_virtual_resolution(self) -> None:
        base = self.method("IVP_Base", "step", "void(int)", virtual=True)
        derived = self.method("IVP_Derived", "step", "void(int)")
        surface = self.audit.apply_effective_virtuals_with_support(
            {"IVP_Derived": {derived}},
            {"IVP_Base": {base}, "IVP_Derived": {derived}},
            {"IVP_Derived": ("IVP_Base",)},
        )

        self.assertEqual({"IVP_Derived"}, set(surface))
        self.assertTrue(next(iter(surface["IVP_Derived"])).virtual)

    def test_virtual_base_destructor_makes_derived_destructor_virtual(self) -> None:
        base = self.method(
            "IVP_Base", "~IVP_Base", "void()", virtual=True
        )
        derived = self.method(
            "IVP_Derived", "~IVP_Derived", "void()"
        )
        surface = self.audit.apply_effective_virtuals(
            {"IVP_Base": {base}, "IVP_Derived": {derived}},
            {"IVP_Derived": ("IVP_Base",)},
        )
        resolved = next(iter(surface["IVP_Derived"]))
        self.assertTrue(resolved.virtual)

    def test_elaborated_type_specifier_is_not_a_signature_difference(self) -> None:
        self.assertEqual(
            "void(IVP_Real_Object*)",
            self.audit.normalize_signature("void(class IVP_Real_Object *)"),
        )
        self.assertEqual(
            "void(IVP_Real_Object*)",
            self.audit.normalize_signature("void(struct IVP_Real_Object *)"),
        )

    def test_evidence_dimensions_remain_independent(self) -> None:
        method = self.audit.Method(
            owner="IVP_Test",
            name="step",
            signature="void()",
            mangled="?step@IVP_Test@@QAEXXZ",
            has_body=True,
            pure_virtual=False,
            virtual=False,
        )
        entries = [
            self.audit.CuratedEvidence(
                "IVP_Test", "*", "", "layout-ida-only", "types.idb", ""
            ),
            self.audit.CuratedEvidence(
                "IVP_Test", "step", "", "host-test", "step_test.cpp", ""
            ),
            self.audit.CuratedEvidence(
                "IVP_Test",
                "step",
                "",
                "nearby-source-basis",
                "nearby.cxx",
                "",
            ),
        ]

        evidence = self.audit.evidence_columns(
            method,
            {method.mangled: 0x1234},
            {0x1234},
            entries,
        )

        self.assertEqual("idb-exact-name", evidence.retail_body)
        self.assertEqual("direct-target", evidence.retail_callsite)
        self.assertEqual("ida-import-only", evidence.owner_layout)
        self.assertTrue(evidence.nearby_header_definition)
        self.assertTrue(evidence.nearby_source_basis)
        self.assertTrue(evidence.host_behavior_test)
        self.assertFalse(evidence.ballance_player_test)

    def test_retail_body_variant_does_not_imply_callsite_or_layout(self) -> None:
        method = self.method("IVP_Test", "changed", "void(int,int)")
        entries = [
            self.audit.CuratedEvidence(
                "IVP_Test",
                "changed",
                "",
                "retail-body-variant",
                "difference.md",
                "",
            )
        ]

        evidence = self.audit.evidence_columns(method, {}, set(), entries)

        self.assertEqual("binary-confirmed-variant", evidence.retail_body)
        self.assertEqual("none", evidence.retail_callsite)
        self.assertEqual("unclassified", evidence.owner_layout)
        self.assertFalse(evidence.nearby_source_basis)
        self.assertFalse(evidence.host_behavior_test)
        self.assertFalse(evidence.ballance_player_test)

    def test_retail_inline_body_is_reported_separately(self) -> None:
        method = self.method("IVP_Test", "inline_step", "void()")
        entries = [
            self.audit.CuratedEvidence(
                "IVP_Test",
                "inline_step",
                "",
                "retail-inline-body",
                "difference.md",
                "",
            )
        ]

        evidence = self.audit.evidence_columns(method, {}, set(), entries)

        self.assertEqual("binary-confirmed-inline", evidence.retail_body)
        self.assertEqual("none", evidence.retail_callsite)
        self.assertEqual("unclassified", evidence.owner_layout)

    def test_static_utility_owner_can_explicitly_have_no_layout(self) -> None:
        method = self.method("IVP_Static_Utility", "calculate", "float(float)")
        entries = [
            self.audit.CuratedEvidence(
                "IVP_Static_Utility",
                "*",
                "",
                "layout-not-applicable",
                "retail.asm",
                "All public entries are static and receive no this pointer.",
            )
        ]

        self.audit.validate_curated_evidence(
            entries, {"IVP_Static_Utility": {method}}
        )
        evidence = self.audit.evidence_columns(method, {}, set(), entries)
        self.assertEqual("not-applicable", evidence.owner_layout)

    def test_ambiguous_curated_method_requires_mangled_name(self) -> None:
        first = self.method("IVP_Test", "set", "void(int)")
        second = self.method("IVP_Test", "set", "void(float)")
        entry = self.audit.CuratedEvidence(
            "IVP_Test", "set", "", "host-test", "test.cpp", ""
        )

        with self.assertRaisesRegex(RuntimeError, "matches 2 public methods"):
            self.audit.validate_curated_evidence(
                [entry], {"IVP_Test": {first, second}}
            )

    def test_conflicting_layout_evidence_is_rejected(self) -> None:
        method = self.method("IVP_Test", "step", "void()")
        entries = [
            self.audit.CuratedEvidence(
                "IVP_Test", "*", "", "layout-retail", "retail.asm", ""
            ),
            self.audit.CuratedEvidence(
                "IVP_Test", "*", "", "layout-ida-only", "types.idb", ""
            ),
        ]

        with self.assertRaisesRegex(RuntimeError, "conflicting layout evidence"):
            self.audit.validate_curated_evidence(
                entries, {"IVP_Test": {method}}
            )

    @staticmethod
    def record(name: str, source: Path | None) -> dict:
        location = {"line": 1, "col": 7, "tokLen": len(name)}
        if source is not None:
            location["file"] = str(source)
        return {
            "kind": "CXXRecordDecl",
            "completeDefinition": True,
            "name": name,
            "tagUsed": "class",
            "loc": location,
            "inner": [],
        }

    @staticmethod
    def method_record(
        name: str, source: Path, contents: bytes, line: int
    ) -> dict:
        return {
            "kind": "CXXMethodDecl",
            "name": name,
            "loc": {
                "file": str(source),
                "offset": contents.index(name.encode("ascii")),
                "line": line,
                "col": 8,
                "tokLen": len(name),
            },
            "type": {"qualType": "void ()"},
            "inner": [],
        }

    @classmethod
    def method(
        cls, owner: str, name: str, signature: str, virtual: bool = False
    ):
        return cls.audit.Method(
            owner=owner,
            name=name,
            signature=signature,
            mangled="",
            has_body=False,
            pure_virtual=False,
            virtual=virtual,
        )


if __name__ == "__main__":
    unittest.main()

"""Fingerprint the engine targets actually selected by CMake, including packages."""

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class EngineAbiTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="bml-engine-abi-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.engine = self.root / "installed engine"
        self.headers = self.engine / "include"
        self.headers.mkdir(parents=True)
        (self.headers / "CKAll.h").write_text('#include "CKTypes.h"\n')
        (self.headers / "CKTypes.h").write_text("struct CKObject { int value; };\n")
        (self.headers / "VxMath.h").write_text('#include "VxDefines.h"\n')
        (self.headers / "VxDefines.h").write_text("struct VxVector { float x, y, z; };\n")
        self.source = self.root / "project"
        self.source.mkdir()
        (self.source / "CMakeLists.txt").write_text('''
cmake_minimum_required(VERSION 3.25)
project(BMLAbiTest LANGUAGES CXX)
set(PROJECT_SOURCE_DIR "${BML_SOURCE_ROOT}")
set(BML_RUNTIME sdl)
add_library(CK2 INTERFACE IMPORTED)
add_library(VxMath INTERFACE IMPORTED)
set_target_properties(CK2 VxMath PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ENGINE_ROOT}/include")
set_property(TARGET CK2 PROPERTY INTERFACE_LINK_LIBRARIES VxMath)
if(ENGINE_DEFINE)
    set_property(TARGET VxMath PROPERTY INTERFACE_COMPILE_DEFINITIONS "${ENGINE_DEFINE}")
endif()
include("${BML_SOURCE_ROOT}/cmake/BMLAbi.cmake")
file(WRITE "${CMAKE_BINARY_DIR}/result.txt" "${BML_SDK_ABI_ID}")
''', encoding="utf-8")
        self.run_number = 0

    def fingerprint(self, engine=None, unused_source=None, define=""):
        self.run_number += 1
        build = self.root / f"build-{self.run_number}"
        command = [ARGS.cmake, "-S", str(self.source), "-B", str(build),
                   "-DCMAKE_BUILD_TYPE=Debug", f"-DBML_SOURCE_ROOT={ARGS.source_root}",
                   f"-DENGINE_ROOT={engine or self.engine}", f"-DENGINE_DEFINE={define}",
                   f"-DBALLANCED_ROOT={unused_source or ''}"]
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stdout)
        return (build / "result.txt").read_text()

    def test_changed_package_headers_change_abi(self):
        before = self.fingerprint()
        (self.headers / "CKTypes.h").write_text("struct CKObject { double x, y; };\n")
        self.assertNotEqual(before, self.fingerprint())

    def test_relocating_identical_headers_preserves_abi(self):
        relocated = self.root / "relocated SDK"
        shutil.copytree(self.engine, relocated)
        self.assertEqual(self.fingerprint(), self.fingerprint(relocated))

    def test_unselected_source_tree_does_not_affect_package_abi(self):
        unused = self.root / "unused source"
        headers = unused / "Source/CK2/include"
        headers.mkdir(parents=True)
        header = headers / "CKAll.h"
        header.write_text("// unselected source revision one\n")
        before = self.fingerprint(unused_source=unused)
        header.write_text("// unselected source revision two\n")
        self.assertEqual(before, self.fingerprint(unused_source=unused))

    def test_public_compile_definitions_change_abi(self):
        self.assertNotEqual(self.fingerprint(define="VX_SIMD_USE_SIMDE=0"),
                            self.fingerprint(define="VX_SIMD_USE_SIMDE=1"))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--cmake", default="cmake")
    ARGS, remaining = parser.parse_known_args()
    unittest.main(argv=[__file__, *remaining])

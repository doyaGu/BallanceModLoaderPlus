#!/usr/bin/env python3
"""
bml_create_mod.py - BML Module Scaffolding Tool

Generates a new BML mod project from templates with the correct IDs, names,
and CMake integration pre-filled.

Usage:
    python tools/bml_create_mod.py --id com.example.mymod --name "My Mod" --dir ./MyMod
    python tools/bml_create_mod.py --id com.example.mymod  # uses id as dir name
"""

import argparse
import re
import sys
from pathlib import Path


def validate_mod_id(mod_id: str) -> bool:
    return bool(re.match(r"^[a-zA-Z][a-zA-Z0-9._-]*$", mod_id))


def target_name_from_id(mod_id: str) -> str:
    """Derive a CMake target name from a mod ID: com.example.my-mod -> MyMod."""
    # Take the last segment
    parts = mod_id.replace("-", ".").replace("_", ".").split(".")
    last = parts[-1] if parts else mod_id
    # PascalCase
    return last[0].upper() + last[1:] if last else "MyMod"


def generate_project(
    output_dir: Path,
    mod_id: str,
    mod_name: str,
    author: str,
    version: str,
    description: str,
    use_cpp_framework: bool,
) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    src_dir = output_dir / "src"
    src_dir.mkdir(exist_ok=True)

    target = target_name_from_id(mod_id)

    # --- CMakeLists.txt ---
    cmake_content = f"""\
cmake_minimum_required(VERSION 3.14)

project({target}
    VERSION {version}
    DESCRIPTION "{description}"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(BML REQUIRED)
include(BMLMod OPTIONAL)

if(COMMAND bml_add_module)
    bml_add_module(${{PROJECT_NAME}}
        SOURCES src/{target}.cpp
        ID "{mod_id}"
        VERSION "${{PROJECT_VERSION}}"
        DISPLAY_NAME "{mod_name}"
        AUTHORS "{author}"
        DESCRIPTION "{description}"
        GENERATE_MANIFEST
    )
else()
    # Fallback without BMLMod.cmake
    add_library(${{PROJECT_NAME}} SHARED src/{target}.cpp)
    target_include_directories(${{PROJECT_NAME}} PRIVATE ${{BML_INCLUDE_DIRS}})
    target_compile_definitions(${{PROJECT_NAME}} PRIVATE
        UNICODE _UNICODE
        BML_MOD_ID="{mod_id}"
        BML_MOD_VERSION="${{PROJECT_VERSION}}"
    )
    set_target_properties(${{PROJECT_NAME}} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${{CMAKE_BINARY_DIR}}/Mods/${{PROJECT_NAME}}"
    )
endif()
"""
    (output_dir / "CMakeLists.txt").write_text(cmake_content, encoding="utf-8")

    # --- mod.toml ---
    toml_content = f"""\
[package]
id = "{mod_id}"
name = "{mod_name}"
version = "{version}"
authors = ["{author}"]
description = "{description}"
capabilities = []

[dependencies]
# "com.bml.input" = ">=0.4.0"

[metadata]
tags = []
"""
    (output_dir / "mod.toml").write_text(toml_content, encoding="utf-8")

    # --- Source file ---
    if use_cpp_framework:
        source_content = f"""\
/**
 * {target} - BML v0.4 Module
 *
 * Uses the bml::Module C++ framework for minimal boilerplate.
 */

#define BML_LOADER_IMPLEMENTATION
#include "bml_module.hpp"
#include "bml_topics.h"

class {target} : public bml::Module {{
    bml::imc::SubscriptionManager m_Subs;

public:
    BML_Result OnAttach(bml::ModuleServices &services) override {{
        m_Subs = services.CreateSubscriptions();

        Services().Log().Info("=== {mod_name} Starting ===");

        m_Subs.Add(BML_TOPIC_ENGINE_INIT, [this](const bml::imc::Message &) {{
            Services().Log().Info("Engine initialized");
        }});

        return m_Subs.Empty() ? BML_RESULT_FAIL : BML_RESULT_OK;
    }}

    void OnDetach() override {{
        Services().Log().Info("=== {mod_name} Shutting Down ===");
    }}
}};

BML_DEFINE_MODULE({target})
"""
    else:
        source_content = f"""\
/**
 * {target} - BML v0.4 Module (C API)
 *
 * Uses the raw C entrypoint for maximum control.
 */

#include "bml_module.h"
#include "bml_core.h"
#include "bml_logging.h"
#include "bml_imc.h"
#include "bml_topics.h"

static const char *kTag = "{mod_id}";
static BML_Mod g_Mod = nullptr;
static BML_Subscription g_InitSub = nullptr;
static const BML_Services *g_Services = nullptr;

static void OnEngineInit(BML_Context ctx, BML_TopicId topic,
                         const BML_ImcMessage *msg, void *ud) {{
    (void)topic;
    (void)msg;
    (void)ud;
    if (g_Services && g_Services->Logging && g_Services->Logging->Log) {{
        g_Services->Logging->Log(g_Mod, ctx, BML_LOG_INFO, kTag, "Engine initialized!");
    }}
}}

static BML_Result HandleAttach(const BML_ModAttachArgs *args) {{
    if (!args || args->struct_size < sizeof(BML_ModAttachArgs))
        return BML_RESULT_INVALID_ARGUMENT;
    if (args->api_version < BML_MOD_ENTRYPOINT_API_VERSION)
        return BML_RESULT_VERSION_MISMATCH;
    if (!args->services || !args->services->Logging || !args->services->ImcBus)
        return BML_RESULT_INVALID_ARGUMENT;

    g_Mod = args->mod;
    g_Services = args->services;

    BML_TopicId topic = 0;
    BML_Result res = g_Services->ImcBus->GetTopicId(args->context, BML_TOPIC_ENGINE_INIT, &topic);
    if (res != BML_RESULT_OK) return res;
    res = g_Services->ImcBus->Subscribe(g_Mod, topic, OnEngineInit, nullptr, &g_InitSub);
    if (res != BML_RESULT_OK) return res;

    BML_Context ctx = args->context;
    g_Services->Logging->Log(g_Mod, ctx, BML_LOG_INFO, kTag, "{mod_name} loaded");
    return BML_RESULT_OK;
}}

static BML_Result HandleDetach(const BML_ModDetachArgs *args) {{
    (void)args;
    if (g_InitSub && g_Services && g_Services->ImcBus) {{
        g_Services->ImcBus->Unsubscribe(g_InitSub);
        g_InitSub = nullptr;
    }}
    g_Services = nullptr;
    g_Mod = nullptr;
    return BML_RESULT_OK;
}}

BML_MODULE_ENTRY BML_Result BML_ModEntrypoint(BML_ModEntrypointCommand cmd, void *data) {{
    switch (cmd) {{
    case BML_MOD_ENTRYPOINT_ATTACH:
        return HandleAttach((const BML_ModAttachArgs *)data);
    case BML_MOD_ENTRYPOINT_DETACH:
        return HandleDetach((const BML_ModDetachArgs *)data);
    default:
        return BML_RESULT_INVALID_ARGUMENT;
    }}
}}
"""
    (src_dir / f"{target}.cpp").write_text(source_content, encoding="utf-8")

    print(f"Created BML mod project at: {output_dir}")
    print(f"  ID:     {mod_id}")
    print(f"  Name:   {mod_name}")
    print(f"  Target: {target}")
    print(f"  Style:  {'C++ framework' if use_cpp_framework else 'C API'}")
    print()
    print("Build:")
    print(f'  cmake -B build -S "{output_dir}" -DCMAKE_PREFIX_PATH="<BML-SDK-path>"')
    print("  cmake --build build --config Release")
    print()
    print("Deploy:")
    print(f'  Directory: copy "<build-dir>/Mods/{target}/" to <GameDir>/ModLoader/Mods/')
    print(
        f'  Package:   zip "<build-dir>/Mods/{target}/", rename it to "{mod_id}.bp", '
        'and copy it to <GameDir>/ModLoader/Packages/'
    )


def generate_script_project(
    output_dir: Path,
    mod_id: str,
    mod_name: str,
    author: str,
    version: str,
    description: str,
) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)

    # --- mod.toml ---
    toml_content = f"""\
[package]
id = "{mod_id}"
name = "{mod_name}"
version = "{version}"
authors = ["{author}"]
description = "{description}"
entry = "main.as"

[dependencies]
"com.bml.scripting" = ">=0.4.0"
"""
    (output_dir / "mod.toml").write_text(toml_content, encoding="utf-8")

    # --- main.as ---
    script_content = f"""\
// {mod_name} - BML AngelScript Mod

void OnAttach() {{
    log("{mod_name} loaded!");
    subscribe("BML/Input/KeyDown", "OnKeyDown");
}}

void OnDetach() {{
    log("{mod_name} unloading");
}}

void OnInit() {{
    // Virtools engine is ready. Access game objects here.
    log("Engine ready");
}}

void OnKeyDown(int key) {{
    if (key == CKKEY_F5) {{
        print("Hello from {mod_name}!");
    }}
}}
"""
    (output_dir / "main.as").write_text(script_content, encoding="utf-8")

    print(f"Created BML script mod at: {output_dir}")
    print(f"  ID:     {mod_id}")
    print(f"  Name:   {mod_name}")
    print(f"  Entry:  main.as")
    print()
    print("Deploy:")
    print(f'  Directory: copy "{output_dir}" to <GameDir>/ModLoader/Mods/')
    print(
        f'  Package:   zip "{output_dir}", rename it to "{mod_id}.bp", '
        'and copy it to <GameDir>/ModLoader/Packages/'
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate a new BML mod project from templates."
    )
    parser.add_argument(
        "--id",
        required=True,
        help='Module ID in reverse-DNS style (e.g. "com.example.mymod")',
    )
    parser.add_argument(
        "--name",
        default=None,
        help="Human-readable display name (default: derived from ID)",
    )
    parser.add_argument(
        "--dir",
        default=None,
        help="Output directory (default: ./<TargetName>)",
    )
    parser.add_argument(
        "--author",
        default="Your Name",
        help="Author name",
    )
    parser.add_argument(
        "--version",
        default="1.0.0",
        help="Initial version (default: 1.0.0)",
    )
    parser.add_argument(
        "--description",
        default="A BML module",
        help="Module description",
    )
    parser.add_argument(
        "--c-api",
        action="store_true",
        help="Use raw C API entrypoint instead of C++ framework (default: C++ framework)",
    )
    parser.add_argument(
        "--script",
        action="store_true",
        help="Generate an AngelScript (.as) mod instead of a native DLL mod",
    )

    args = parser.parse_args()

    if not validate_mod_id(args.id):
        print(
            f"Error: Invalid module ID '{args.id}'. "
            "Use alphanumeric characters, dots, underscores, or hyphens. "
            "Must start with a letter.",
            file=sys.stderr,
        )
        return 1

    target = target_name_from_id(args.id)
    mod_name = args.name or target
    output_dir = Path(args.dir) if args.dir else Path.cwd() / target

    if output_dir.exists() and any(output_dir.iterdir()):
        print(f"Error: Directory '{output_dir}' already exists and is not empty.", file=sys.stderr)
        return 1

    if args.script:
        generate_script_project(
            output_dir=output_dir,
            mod_id=args.id,
            mod_name=mod_name,
            author=args.author,
            version=args.version,
            description=args.description,
        )
    else:
        generate_project(
            output_dir=output_dir,
            mod_id=args.id,
            mod_name=mod_name,
            author=args.author,
            version=args.version,
            description=args.description,
            use_cpp_framework=not args.c_api,
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())

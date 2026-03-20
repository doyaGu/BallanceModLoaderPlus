# BML Module Development Templates

Templates and CMake utilities for creating BML modules.

## Quick Start

### Option 1: Within BallanceModLoaderPlus

Add your module in `examples/`:

```cmake
# examples/MyModule/CMakeLists.txt
bml_add_module(MyModule
    SOURCES MyModule.cpp
    ID "com.example.mymodule"
    VERSION "1.0.0"
    DISPLAY_NAME "My Module"
    AUTHORS "Your Name"
    DESCRIPTION "My awesome module"
    GENERATE_MANIFEST
)
```

### Option 2: Standalone Project

1. Copy `CMakeLists.txt.template` → `CMakeLists.txt`
2. Copy `src/main.cpp.template` → `src/main.cpp`
3. Copy `mod.toml.template` → `mod.toml` (or use `GENERATE_MANIFEST`)
4. Build:

```bash
cmake -B build -S . -DCMAKE_PREFIX_PATH="<BML-install-path>"
cmake --build build --config Release
```

## Files

```
templates/
├── CMakeLists.txt.template   # Project template
├── mod.toml.template         # Manifest template
├── README.md
└── src/
    └── main.cpp.template     # Source template
```

## CMake Functions

### bml_add_module

```cmake
bml_add_module(<name>
    SOURCES <file1> [<file2> ...]
    [ID <module_id>]
    [VERSION <version>]
    [DISPLAY_NAME <name>]
    [AUTHORS <author1> ...]
    [DESCRIPTION <text>]
    [OUTPUT_NAME <name>]
    [INCLUDE_DIRS <dir1> ...]
    [LINK_LIBRARIES <lib1> ...]
    [COMPILE_DEFINITIONS <def1> ...]
    [CXX_STANDARD <20>]
    [MANIFEST <mod.toml>]
    [GENERATE_MANIFEST]
    [DEPENDENCIES <dep1> ...]
    [CAPABILITIES <cap1> ...]
)
```

### bml_generate_manifest

```cmake
bml_generate_manifest(
    TARGET <target>
    ID <module_id>
    NAME <display_name>
    VERSION <version>
    [AUTHORS <author1> ...]
    [DESCRIPTION <text>]
    [ENTRY <dll_filename>]
    [OUTPUT_DIR <directory>]
    [DEPENDENCIES <dep1> ...]
    [CAPABILITIES <cap1> ...]
)
```

### bml_install_module

```cmake
bml_install_module(<target>
    [DESTINATION <path>]
    [COMPONENT <component>]
)
```

### bml_add_module_resources

```cmake
bml_add_module_resources(<target>
    [FILES <file1> ...]
    [DIRECTORY <dir>]
    [DESTINATION <relative_path>]
)
```

## Output Structure

```
build/Mods/MyModule/
├── Release/
│   ├── MyModule.dll
│   └── mod.toml
└── Debug/
    ├── MyModule.dll
    └── mod.toml
```

## Deployment

Copy module folder to game:

```
Ballance/Mods/MyModule/
├── MyModule.dll
└── mod.toml
```

## See Also

- [Mod Manifest Schema](../docs/mod-manifest-schema.md)
- [Examples](../examples/)

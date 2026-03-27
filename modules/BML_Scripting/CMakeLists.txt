# AngelScript engine (static library, from upstream cmake)
add_subdirectory(deps/angelscript/angelscript/projects/cmake EXCLUDE_FROM_ALL)
target_include_directories(angelscript INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/deps/angelscript/angelscript/include>)

# AngelScript add-ons (static library)
add_library(angelscript_addon STATIC
    deps/angelscript/add_on/scriptstdstring/scriptstdstring.cpp
    deps/angelscript/add_on/scriptstdstring/scriptstdstring_utils.cpp
    deps/angelscript/add_on/scriptarray/scriptarray.cpp
    deps/angelscript/add_on/scriptbuilder/scriptbuilder.cpp
    deps/angelscript/add_on/scriptmath/scriptmath.cpp
    deps/angelscript/add_on/scripthelper/scripthelper.cpp
)
target_include_directories(angelscript_addon PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/deps/angelscript>)
target_link_libraries(angelscript_addon PUBLIC angelscript)
set_target_properties(angelscript PROPERTIES FOLDER "Dependencies")
set_target_properties(angelscript_addon PROPERTIES FOLDER "Dependencies")

# BML_Scripting module
bml_add_module(BML_Scripting
    SOURCES
        # Core
        src/ScriptingMod.cpp
        src/ScriptEngine.cpp
        src/ScriptInstanceManager.cpp
        src/RuntimeProvider.cpp
        # BML bindings
        src/bindings/BindCore.cpp
        src/bindings/BindImc.cpp
        src/bindings/BindVirtools.cpp
        src/bindings/BindConfig.cpp
        src/bindings/BindInput.cpp
        src/bindings/BindTimer.cpp
        src/bindings/BindCoroutine.cpp
        src/CoroutineManager.cpp
        src/BytecodeCache.cpp
        # Virtools bindings (from CKAngelScript)
        src/bindings/ScriptVxMath.cpp
        src/bindings/ScriptCKTypes.cpp
        src/bindings/ScriptCKEnums.cpp
        src/bindings/ScriptCKDefines.cpp
        src/bindings/ScriptCKContext.cpp
        src/bindings/ScriptCKManagers.cpp
        src/bindings/ScriptCKObjects.cpp
        src/bindings/ScriptCK2.cpp
        src/bindings/ScriptXString.cpp
        src/bindings/ScriptXObjectArray.cpp
        src/bindings/ScriptXBitArray.cpp
        src/bindings/ScriptNativePointer.cpp
        src/bindings/ScriptNativeBuffer.cpp
    MANIFEST mod.toml
    INCLUDE_DIRS
        ${VIRTOOLS_SDK_INCLUDE_DIR}
)
target_include_directories(BML_Scripting PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(BML_Scripting PRIVATE
    angelscript
    angelscript_addon
    BML_HashUtils
    VirtoolsSDK::CK2
    VirtoolsSDK::VxMath
)

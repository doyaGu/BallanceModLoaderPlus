include_guard(GLOBAL)

if(NOT BML_RUNTIME STREQUAL "sdl")
    return()
endif()

# The host supplies CK2, VxMath and SDL. Never install a second engine beside
# the Loader: all plugins must resolve the same host libraries and state.
install(TARGETS BML
    RUNTIME_DEPENDENCY_SET BMLRuntimeDependencies
    LIBRARY DESTINATION Managers COMPONENT BMLRuntime EXCLUDE_FROM_ALL
    RUNTIME DESTINATION Managers COMPONENT BMLRuntime EXCLUDE_FROM_ALL)
set(_bml_runtime_targets BML)
if(BML_WITH_ANGELSCRIPT AND TARGET CKAngelScript)
    install(TARGETS CKAngelScript
        RUNTIME_DEPENDENCY_SET BMLRuntimeDependencies
        LIBRARY DESTINATION BuildingBlocks COMPONENT BMLRuntime EXCLUDE_FROM_ALL
        RUNTIME DESTINATION BuildingBlocks COMPONENT BMLRuntime EXCLUDE_FROM_ALL)
    list(APPEND _bml_runtime_targets CKAngelScript)
endif()
install(RUNTIME_DEPENDENCY_SET BMLRuntimeDependencies
    PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*" "[Cc][Kk]2" "[Vv]x[Mm]ath" "SDL3"
    POST_EXCLUDE_REGEXES "^/System/Library/" "^/usr/lib/" "[Ww]indows[/\\\\][Ss]ystem32/"
    LIBRARY DESTINATION Bin COMPONENT BMLRuntime EXCLUDE_FROM_ALL
    RUNTIME DESTINATION Bin COMPONENT BMLRuntime EXCLUDE_FROM_ALL)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/packaging/runtime/ModLoader/Fonts"
    DESTINATION ModLoader COMPONENT BMLRuntime EXCLUDE_FROM_ALL)
install(FILES "${PROJECT_BINARY_DIR}/abi/$<CONFIG>/BML.abi.json"
    DESTINATION ModLoader COMPONENT BMLRuntime EXCLUDE_FROM_ALL)
if(APPLE)
    install(SCRIPT "${CMAKE_CURRENT_LIST_DIR}/RelocateBMLRuntime.cmake" COMPONENT BMLRuntime EXCLUDE_FROM_ALL)
endif()

set(BML_PLAYER_RUNTIME_DIR "" CACHE PATH "Existing staged SDL Player runtime for the bml-stage target")
if(BML_PLAYER_RUNTIME_DIR)
    add_custom_target(bml-stage
        COMMAND "${CMAKE_COMMAND}" --install "${PROJECT_BINARY_DIR}"
            --config "$<CONFIG>" --prefix "${BML_PLAYER_RUNTIME_DIR}" --component BMLRuntime
        DEPENDS ${_bml_runtime_targets} VERBATIM)
endif()

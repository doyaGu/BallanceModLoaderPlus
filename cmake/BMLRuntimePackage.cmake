include_guard(GLOBAL)

if(NOT BML_RUNTIME STREQUAL "sdl")
    return()
endif()

# The host supplies CK2, VxMath and SDL. Never install a second engine beside
# the Loader: all plugins must resolve the same host libraries and state.
function(bml_refresh_runtime_binary target destination)
    if(APPLE)
        # CMake preserves source timestamps after install_name_tool edits. A
        # repeated install can otherwise reapply RPATH edits to relocated data.
        # Refresh only this component's owned binary before the normal target
        # install; build-tree RPATHs remain available to developer builds.
        install(CODE "
            file(MAKE_DIRECTORY \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${destination}\")
            file(COPY_FILE \"$<TARGET_FILE:${target}>\" \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${destination}/$<TARGET_FILE_NAME:${target}>\")
            " COMPONENT BMLRuntime EXCLUDE_FROM_ALL)
    endif()
endfunction()
bml_refresh_runtime_binary(BML Managers)
install(TARGETS BML
    RUNTIME_DEPENDENCY_SET BMLRuntimeDependencies
    LIBRARY DESTINATION Managers COMPONENT BMLRuntime EXCLUDE_FROM_ALL
    RUNTIME DESTINATION Managers COMPONENT BMLRuntime EXCLUDE_FROM_ALL)
set(_bml_runtime_targets BML)
if(BML_WITH_ANGELSCRIPT AND TARGET CKAngelScript)
    bml_refresh_runtime_binary(CKAngelScript BuildingBlocks)
    install(TARGETS CKAngelScript
        RUNTIME_DEPENDENCY_SET BMLRuntimeDependencies
        LIBRARY DESTINATION BuildingBlocks COMPONENT BMLRuntime EXCLUDE_FROM_ALL
        RUNTIME DESTINATION BuildingBlocks COMPONENT BMLRuntime EXCLUDE_FROM_ALL)
    list(APPEND _bml_runtime_targets CKAngelScript)
endif()
install(RUNTIME_DEPENDENCY_SET BMLRuntimeDependencies
    PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*" "[Cc][Kk]2" "[Vv]x[Mm]ath" "SDL3"
    POST_EXCLUDE_REGEXES "^/System/Library/" "^/usr/lib(64)?/" "^/lib(64)?/" "[Ww]indows[/\\\\][Ss]ystem32/"
    LIBRARY DESTINATION Bin COMPONENT BMLRuntime EXCLUDE_FROM_ALL
    RUNTIME DESTINATION Bin COMPONENT BMLRuntime EXCLUDE_FROM_ALL)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/packaging/runtime/ModLoader/Fonts"
    DESTINATION ModLoader COMPONENT BMLRuntime EXCLUDE_FROM_ALL)
install(FILES "${PROJECT_SOURCE_DIR}/deps/p256-m/apache-2.0.txt"
    DESTINATION ModLoader/Licenses RENAME p256-m.txt COMPONENT BMLRuntime EXCLUDE_FROM_ALL)
install(FILES "${PROJECT_SOURCE_DIR}/deps/rhash-sha256/COPYING"
    DESTINATION ModLoader/Licenses RENAME rhash-sha256.txt COMPONENT BMLRuntime EXCLUDE_FROM_ALL)
install(FILES "${PROJECT_SOURCE_DIR}/deps/CRYPTO-NOTICES.txt"
    DESTINATION ModLoader/Licenses COMPONENT BMLRuntime EXCLUDE_FROM_ALL)
if(BML_WITH_ANGELSCRIPT)
    set(_bml_runtime_scripts true)
else()
    set(_bml_runtime_scripts false)
endif()
string(REGEX REPLACE "}$" ",\"version\":\"${PROJECT_VERSION}\",\"scripts\":${_bml_runtime_scripts}}"
    _bml_runtime_abi "${BML_ABI_JSON}")
file(GENERATE OUTPUT "${PROJECT_BINARY_DIR}/abi/$<CONFIG>/BML.runtime.abi.json" CONTENT "${_bml_runtime_abi}\n")
install(FILES "${PROJECT_BINARY_DIR}/abi/$<CONFIG>/BML.runtime.abi.json"
    DESTINATION ModLoader RENAME BML.abi.json COMPONENT BMLRuntime EXCLUDE_FROM_ALL)
set(BML_PLAYER_RUNTIME_DIR "" CACHE PATH "Existing staged SDL Player runtime for the bml-stage target")
if(BML_PLAYER_RUNTIME_DIR)
    add_custom_target(bml-stage
        COMMAND "${CMAKE_COMMAND}" --install "${PROJECT_BINARY_DIR}"
            --config "$<CONFIG>" --prefix "${BML_PLAYER_RUNTIME_DIR}" --component BMLRuntime
        DEPENDS ${_bml_runtime_targets} VERBATIM)
endif()

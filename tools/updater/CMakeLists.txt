find_package(CURL 7.68 REQUIRED)

# Publisher tooling owns private-key signing; runtime binaries only verify.
add_library(BMLUpdateSigning STATIC UpdaterSigning.cpp)
target_link_libraries(BMLUpdateSigning PUBLIC BML_P256)
target_compile_features(BMLUpdateSigning PUBLIC cxx_std_20)
target_include_directories(BMLUpdateSigning PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(BMLUpdateSigner SignerMain.cpp)
target_include_directories(BMLUpdateSigner PRIVATE "${BML_SOURCE_DIR}")
target_link_libraries(BMLUpdateSigner PRIVATE BMLUpdateSigning BML_CryptoUtils BML_PathUtils)
if(NOT APPLE)
    target_link_libraries(BMLUpdateSigner PRIVATE BML_Sha256)
endif()
set_target_properties(BMLUpdateSigner PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/Bin")
install(TARGETS BMLUpdateSigner RUNTIME DESTINATION bin)
set(BML_UPDATER_PUBLIC_KEY_X
"e4512bf8b6eb5606bec92ea1c3c36baee6a465492b16868b6feff50cb25288f4"
CACHE STRING "ECDSA P-256 public key X coordinate for updater manifest verification")
set(BML_UPDATER_PUBLIC_KEY_Y
"f93917a0c5b0d3cb51bebf89b6cb6f732153466975b64d4af79d92083f962331"
CACHE STRING "ECDSA P-256 public key Y coordinate for updater manifest verification")


set(_bml_updater_sources
    UpdaterManifest.cpp UpdaterPathsSdl.cpp UpdaterService.cpp
    UpdaterRemote.cpp UpdaterZip.cpp UpdaterSdl.cpp UpdaterTarget.cpp
    UpdaterTransactionSdl.cpp UpdaterDoctorSdl.cpp UpdaterNetworkCurl.cpp)
function(bml_configure_sdl_updater target key_x key_y)
    target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/deps/zip/src" PRIVATE "${BML_SOURCE_DIR}")
    target_compile_features(${target} PUBLIC cxx_std_20)
    target_compile_definitions(${target} PUBLIC BML_RUNTIME_SDL=1
        BML_UPDATER_PLATFORM="${BML_TARGET_PLATFORM}" BML_UPDATER_ARCH="${BML_TARGET_ARCH}"
        BML_UPDATER_PUBLIC_KEY_X="${key_x}" BML_UPDATER_PUBLIC_KEY_Y="${key_y}")
    target_link_libraries(${target} PUBLIC BML_CryptoUtils BML_JsonUtils
        BML_PathUtils BML_StringUtils zip SDL3::SDL3 CURL::libcurl)
    set_target_properties(${target} PROPERTIES FOLDER Tools)
endfunction()
add_library(BMLUpdaterCore STATIC ${_bml_updater_sources})
bml_configure_sdl_updater(BMLUpdaterCore "${BML_UPDATER_PUBLIC_KEY_X}" "${BML_UPDATER_PUBLIC_KEY_Y}")
add_executable(Updater main.cpp)
target_link_libraries(Updater PRIVATE BMLUpdaterCore)
set_target_properties(Updater PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/Bin")
if(APPLE)
    set_target_properties(Updater PROPERTIES INSTALL_RPATH "@executable_path;@executable_path/..")
elseif(UNIX)
    set_target_properties(Updater PROPERTIES INSTALL_RPATH "$ORIGIN;$ORIGIN/..")
endif()
if(COMMAND bml_refresh_runtime_binary)
    bml_refresh_runtime_binary(Updater Bin)
endif()
install(TARGETS Updater RUNTIME_DEPENDENCY_SET BMLRuntimeDependencies
    RUNTIME DESTINATION Bin COMPONENT BMLRuntime EXCLUDE_FROM_ALL)
if(TARGET bml-stage)
    add_dependencies(bml-stage Updater)
endif()
if(BML_BUILD_TESTS)
    # A separate binary uses the public P-256 generator point (private key
    # 1) for deterministic signatures. Production keys are never replaced.
    add_library(BMLUpdaterTestCore STATIC ${_bml_updater_sources})
    bml_configure_sdl_updater(BMLUpdaterTestCore
        "6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296"
        "4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5")
    add_executable(UpdaterSdlTestRunner main.cpp)
    target_link_libraries(UpdaterSdlTestRunner PRIVATE BMLUpdaterTestCore)
    set_target_properties(UpdaterSdlTestRunner PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/tests/Bin")
endif()

add_library(BMLUpdaterCore STATIC
    UpdaterManifest.cpp
    UpdaterManifest.h
    UpdaterPaths.cpp
    UpdaterPaths.h
    UpdaterService.cpp
    UpdaterService.h
    UpdaterTypes.h
    UpdaterNetwork.cpp
    UpdaterNetwork.h
    UpdaterZip.cpp
    UpdaterZip.h
)

set(BML_UPDATER_PUBLIC_KEY_X
    "e4512bf8b6eb5606bec92ea1c3c36baee6a465492b16868b6feff50cb25288f4"
    CACHE STRING "ECDSA P-256 public key X coordinate for updater manifest verification")
set(BML_UPDATER_PUBLIC_KEY_Y
    "f93917a0c5b0d3cb51bebf89b6cb6f732153466975b64d4af79d92083f962331"
    CACHE STRING "ECDSA P-256 public key Y coordinate for updater manifest verification")

target_include_directories(BMLUpdaterCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/deps/zip/src
)

target_compile_definitions(BMLUpdaterCore PUBLIC UNICODE _UNICODE)

target_link_libraries(BMLUpdaterCore PUBLIC
    BML_CryptoUtils
    BML_JsonUtils
    BML_StringUtils
    zip
    version
    winhttp
)

target_compile_definitions(BMLUpdaterCore PUBLIC
    BML_UPDATER_PUBLIC_KEY_X="${BML_UPDATER_PUBLIC_KEY_X}"
    BML_UPDATER_PUBLIC_KEY_Y="${BML_UPDATER_PUBLIC_KEY_Y}"
)

set_target_properties(BMLUpdaterCore PROPERTIES
    FOLDER "Tools/Updater"
)

add_executable(Updater
    main.cpp
)

target_link_libraries(Updater PRIVATE
    BMLUpdaterCore
    winhttp
)


set_target_properties(Updater PROPERTIES
    OUTPUT_NAME "Updater"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/Bin"
    FOLDER "Tools/Updater"
)

foreach(config ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${config}" config_upper)
    set_target_properties(Updater PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${PROJECT_BINARY_DIR}/Bin")
endforeach()

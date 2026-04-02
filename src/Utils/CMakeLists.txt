# ============================================================================
# Cross-platform utilities
# ============================================================================

# -- BML_HashUtils - FNV-1a hashing, CRC-16, SplitMix64 PRNG ----------------
add_library(BML_HashUtils STATIC
    HashUtils.h
    HashUtils.cpp
    Crc16.h
    SplitMix64.h
)

target_include_directories(BML_HashUtils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(BML_HashUtils PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    FOLDER "Utils"
)

# -- BML_JsonUtils - JSON parsing/writing (yyjson wrapper) -------------------
add_library(BML_JsonUtils STATIC
    JsonUtils.h
    JsonUtils.cpp
)

target_include_directories(BML_JsonUtils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/deps/yyjson/src
)

target_link_libraries(BML_JsonUtils PUBLIC
    BML_StringUtils
    yyjson
)

set_target_properties(BML_JsonUtils PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    FOLDER "Utils"
)

# -- BML_ReedSolomon - RS(30,14) error correction codec ---------------------
add_library(BML_ReedSolomon STATIC
    ReedSolomon.h
    ReedSolomon.cpp
)

target_include_directories(BML_ReedSolomon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(BML_ReedSolomon PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    FOLDER "Utils"
)

# -- BML_StringUtils - String manipulation and encoding conversion -----------
add_library(BML_StringUtils STATIC
    StringUtils.h
    StringUtils.cpp
)

target_include_directories(BML_StringUtils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(BML_StringUtils PUBLIC utf8)

set_target_properties(BML_StringUtils PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    FOLDER "Utils"
)

# -- BML_TimeUtils - Timestamp formatting ------------------------------------
add_library(BML_TimeUtils STATIC
    TimeUtils.h
    TimeUtils.cpp
)

target_include_directories(BML_TimeUtils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(BML_TimeUtils PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    FOLDER "Utils"
)

# ============================================================================
# Windows-only utilities
# ============================================================================
if (WIN32)
    # -- BML_CryptoUtils - HMAC-SHA256, HKDF-SHA256, KeyToSeed --------------
    add_library(BML_CryptoUtils STATIC
        CryptoUtils.h
        CryptoUtils.cpp
    )

    target_include_directories(BML_CryptoUtils PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(BML_CryptoUtils PUBLIC bcrypt)

    set_target_properties(BML_CryptoUtils PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        FOLDER "Utils"
    )

    # -- BML_HardwareFingerprint - Machine GUID SHA-256 fingerprint ----------
    add_library(BML_HardwareFingerprint STATIC
        HardwareFingerprint.h
        HardwareFingerprint.cpp
    )

    target_include_directories(BML_HardwareFingerprint PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(BML_HardwareFingerprint PUBLIC bcrypt)

    set_target_properties(BML_HardwareFingerprint PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        FOLDER "Utils"
    )

    # -- BML_HookUtils - Memory manipulation and virtual table hooking -------
    add_library(BML_HookUtils STATIC
        HookUtils.h
        HookUtils.cpp
    )

    target_include_directories(BML_HookUtils PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(BML_HookUtils PUBLIC BML_StringUtils)

    set_target_properties(BML_HookUtils PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        FOLDER "Utils"
    )

    # -- BML_IniFile - INI file parsing with UTF-8 and comment preservation --
    add_library(BML_IniFile STATIC
        IniFile.h
        IniFile.cpp
    )

    target_include_directories(BML_IniFile PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(BML_IniFile PUBLIC
        BML_PathUtils
        BML_StringUtils
        utf8
    )

    set_target_properties(BML_IniFile PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        FOLDER "Utils"
    )

    # -- BML_PathUtils - Windows file system operations and runtime layout ---
    add_library(BML_PathUtils STATIC
        PathUtils.h
        PathUtilsDetail.h
        PathUtils.cpp
    )

    target_include_directories(BML_PathUtils PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(BML_PathUtils PUBLIC
        BML_StringUtils
        zip
    )

    set_target_properties(BML_PathUtils PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        FOLDER "Utils"
    )
endif ()

# ============================================================================
# BML_Utils - Umbrella target
#
# Links all individual Utils targets. Existing consumers that link BML_Utils
# continue to work unchanged. New consumers can link individual targets for
# minimal dependencies.
# ============================================================================
add_library(BML_Utils INTERFACE)
target_link_libraries(BML_Utils INTERFACE
    BML_HashUtils
    BML_JsonUtils
    BML_ReedSolomon
    BML_StringUtils
    BML_TimeUtils
)
if (WIN32)
    target_link_libraries(BML_Utils INTERFACE
        BML_CryptoUtils
        BML_HardwareFingerprint
        BML_HookUtils
        BML_IniFile
        BML_PathUtils
    )
endif ()

add_library(BMLUtils ALIAS BML_Utils)

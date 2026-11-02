# ============================================================================
# Cross-platform utilities
# ============================================================================

# -- BML_HashUtils - FNV-1a hashing, CRC-16 ----------------
add_library(BML_HashUtils STATIC
    HashUtils.h
    HashUtils.cpp
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

# Platform implementations preserve the original Win32 filesystem backend.
if(WIN32)
    add_library(BML_CryptoUtils STATIC CryptoUtils.cpp)
    target_link_libraries(BML_CryptoUtils PUBLIC bcrypt)
    add_library(BML_HookUtils STATIC HookUtils.cpp)
else()
    find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)
    add_library(BML_CryptoUtils STATIC CryptoUtilsOpenSSL.cpp)
    target_link_libraries(BML_CryptoUtils PUBLIC OpenSSL::Crypto BML_StringUtils SDL3::SDL3)
    add_library(BML_HookUtils STATIC ../Platform/DebugOutput.cpp)
    target_link_libraries(BML_HookUtils PUBLIC BML_StringUtils)
endif()

if(BML_RUNTIME_SDL)
    add_library(BML_PathUtils STATIC PathUtilsSdl.cpp ../Platform/FileSystemNative.cpp)
    target_link_libraries(BML_PathUtils PUBLIC SDL3::SDL3)
else()
    add_library(BML_PathUtils STATIC PathUtils.cpp)
endif()
target_link_libraries(BML_PathUtils PUBLIC BML_StringUtils zip)
if(BML_RUNTIME_SDL)
    target_link_libraries(BML_JsonUtils PUBLIC BML_PathUtils)
endif()
add_library(BML_IniFile STATIC IniFile.cpp)
target_link_libraries(BML_IniFile PUBLIC BML_PathUtils BML_StringUtils utf8)
foreach(_target BML_CryptoUtils BML_HookUtils BML_IniFile BML_PathUtils)
    target_include_directories(${_target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} PRIVATE ${BML_SOURCE_DIR})
    set_target_properties(${_target} PROPERTIES POSITION_INDEPENDENT_CODE ON FOLDER Utils)
endforeach()

add_library(BML_Platform STATIC ../Platform/Module.cpp)
target_include_directories(BML_Platform PUBLIC ${BML_SOURCE_DIR})
target_link_libraries(BML_Platform PUBLIC BML_PathUtils ${CMAKE_DL_LIBS})
set_target_properties(BML_Platform PROPERTIES POSITION_INDEPENDENT_CODE ON)

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
    BML_StringUtils
)
    target_link_libraries(BML_Utils INTERFACE
        BML_CryptoUtils
        BML_HookUtils
        BML_IniFile
        BML_PathUtils
        BML_Platform
    )

add_library(BMLUtils ALIAS BML_Utils)

# zip
add_subdirectory(zip EXCLUDE_FROM_ALL)

# Only the portable SHA-256 and P-256 primitives are compiled; no TLS stack.
add_library(BML_Sha256 STATIC rhash-sha256/sha256.c rhash-sha256/byte_order.c)
target_include_directories(BML_Sha256 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/rhash-sha256")
add_library(BML_P256 STATIC p256-m/p256-m.c CryptoRandom.c)
target_include_directories(BML_P256 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/p256-m")
if(WIN32)
    target_link_libraries(BML_P256 PRIVATE bcrypt)
elseif(APPLE)
    target_link_libraries(BML_P256 PRIVATE "-framework Security")
endif()
set_target_properties(BML_Sha256 BML_P256 PROPERTIES
    POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden FOLDER Dependencies)

# Oniguruma
set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
add_subdirectory(oniguruma EXCLUDE_FROM_ALL)
set_target_properties(onig PROPERTIES POSITION_INDEPENDENT_CODE ON)

# utf8.h
add_library(utf8 INTERFACE)
target_include_directories(utf8 INTERFACE ${CMAKE_CURRENT_LIST_DIR}/utf8.h)

# yyjson
add_subdirectory(yyjson EXCLUDE_FROM_ALL)

# ImGui
set(IMGUI_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui PARENT_SCOPE)

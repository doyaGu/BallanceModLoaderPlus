# MinHook
add_subdirectory(minhook EXCLUDE_FROM_ALL)

# zip
add_subdirectory(zip EXCLUDE_FROM_ALL)

# tomlplusplus
set(TOML_BUILD_TESTS OFF CACHE BOOL "")
set(TOML_BUILD_EXAMPLES OFF CACHE BOOL "")
add_subdirectory(tomlplusplus EXCLUDE_FROM_ALL)

# Oniguruma
set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
add_subdirectory(oniguruma EXCLUDE_FROM_ALL)

# utf8.h
add_library(utf8 INTERFACE)
target_include_directories(utf8 INTERFACE ${CMAKE_CURRENT_LIST_DIR}/utf8.h)

# efsw
set(EFSW_BUILD_TEST_APP OFF CACHE BOOL "" FORCE)
add_subdirectory(efsw EXCLUDE_FROM_ALL)

# ImGui
set(IMGUI_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui PARENT_SCOPE)
# MinHook
if (WIN32)
    add_subdirectory(minhook EXCLUDE_FROM_ALL)
endif ()

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

# cimgui / Dear ImGui — custom build (cimgui's CMakeLists hardcodes imgui/ subdir)
# Use cimgui's own imgui submodule so #include "./imgui/imgui.h" resolves correctly.
set(CIMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cimgui)
set(IMGUI_DIR ${CIMGUI_DIR}/imgui)

add_library(cimgui STATIC
    ${CIMGUI_DIR}/cimgui.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
)
target_include_directories(cimgui PUBLIC ${CIMGUI_DIR} ${IMGUI_DIR} ${CMAKE_SOURCE_DIR})
target_compile_definitions(cimgui PUBLIC
    IMGUI_USER_CONFIG="${CMAKE_SOURCE_DIR}/src/Utils/imconfig.h"
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS=1
)
target_compile_definitions(cimgui PRIVATE IMGUI_EXPORT)
if (WIN32)
    target_link_libraries(cimgui PRIVATE imm32)
endif ()

set(CIMGUI_SOURCE_DIR ${CIMGUI_DIR} PARENT_SCOPE)
set(IMGUI_SOURCE_DIR ${IMGUI_DIR} PARENT_SCOPE)

# LuaJIT — for cimgui generator
include(FetchContent)
FetchContent_Declare(luajit
    GIT_REPOSITORY https://luajit.org/git/luajit.git
    GIT_TAG        v2.1
)
FetchContent_MakeAvailable(luajit)
# Build LuaJIT using its MSVC build script
if (WIN32 AND NOT TARGET luajit_exe)
    add_custom_command(
        OUTPUT "${luajit_SOURCE_DIR}/src/luajit.exe"
        COMMAND cmd /c "cd /d \"${luajit_SOURCE_DIR}/src\" && msvcbuild.bat static"
        COMMENT "Building LuaJIT..."
        VERBATIM
    )
    add_custom_target(luajit_exe DEPENDS "${luajit_SOURCE_DIR}/src/luajit.exe")
endif ()
set(LUAJIT_EXECUTABLE "${luajit_SOURCE_DIR}/src/luajit.exe" PARENT_SCOPE)
set(LUAJIT_TARGET luajit_exe PARENT_SCOPE)

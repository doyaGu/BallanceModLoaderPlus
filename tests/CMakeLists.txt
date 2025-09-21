include(CTest)
enable_testing()

include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.16.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

function(add_bml_test test_name)
    cmake_parse_arguments(TEST "" "" "SOURCES;DEPENDENCIES" ${ARGN})

    add_executable(${test_name} ${TEST_SOURCES})

    target_include_directories(${test_name} PRIVATE
            "${BML_INCLUDE_DIR}"
            "${BML_SOURCE_DIR}"
    )

    target_link_libraries(${test_name} PRIVATE
            gtest_main
            gmock
            ${TEST_DEPENDENCIES}
    )

    target_compile_definitions(${test_name} PRIVATE "BML_EXPORTS" "BML_TEST")

    set_target_properties(${test_name} PROPERTIES FOLDER "Tests")
endfunction()

add_bml_test(TimerTest
        SOURCES TimerTest.cpp
)

add_bml_test(StringUtilsTest
        SOURCES
        StringUtilsTest.cpp
        ${BML_SOURCE_DIR}/StringUtils.cpp
        DEPENDENCIES
        utf8
)

add_bml_test(PathUtilsTest
        SOURCES
        PathUtilsTest.cpp
        ${BML_SOURCE_DIR}/StringUtils.cpp
        ${BML_SOURCE_DIR}/PathUtils.cpp
        DEPENDENCIES
        utf8 zip
)

add_bml_test(ConfigTest
        SOURCES
        ConfigTest.cpp
        ${BML_SOURCE_DIR}/Config.cpp
        ${BML_SOURCE_DIR}/Config.h
        ${BML_SOURCE_DIR}/StringUtils.cpp
        ${BML_SOURCE_DIR}/PathUtils.cpp
        DEPENDENCIES
        utf8 zip
        CK2
        VxMath
)

add_bml_test(IniFileTest
        SOURCES
        IniFileTest.cpp
        ${BML_SOURCE_DIR}/IniFile.cpp
        ${BML_SOURCE_DIR}/IniFile.h
        ${BML_SOURCE_DIR}/StringUtils.cpp
        ${BML_SOURCE_DIR}/PathUtils.cpp
        DEPENDENCIES
        utf8 zip
)

add_bml_test(AnsiPaletteTest
        SOURCES
        AnsiPaletteTest.cpp
        ${BML_SOURCE_DIR}/AnsiPalette.cpp
        ${BML_SOURCE_DIR}/IniFile.cpp
        ${BML_SOURCE_DIR}/IniFile.h
        ${BML_SOURCE_DIR}/StringUtils.cpp
        ${BML_SOURCE_DIR}/PathUtils.cpp
        DEPENDENCIES
        utf8 zip
)
# Need ImGui headers for AnsiPalette
target_include_directories(AnsiPaletteTest PRIVATE ${IMGUI_SOURCE_DIR})

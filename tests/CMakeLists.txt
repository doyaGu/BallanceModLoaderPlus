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

    add_test(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)
endfunction()

add_bml_test(TimerTest
        SOURCES TimerTest.cpp
)

add_bml_test(StringUtilsTest
        SOURCES
        StringUtilsTest.cpp
        DEPENDENCIES
        BMLUtils
)

add_bml_test(PathUtilsTest
        SOURCES
        PathUtilsTest.cpp
        DEPENDENCIES
        BMLUtils
)

add_bml_test(ConfigTest
        SOURCES
        ConfigTest.cpp
        ${BML_SOURCE_DIR}/Config.cpp
        ${BML_SOURCE_DIR}/Config.h
        DEPENDENCIES
        BMLUtils
        CK2
        VxMath
)

add_bml_test(IniFileTest
        SOURCES
        IniFileTest.cpp
        DEPENDENCIES
        BMLUtils
)

add_bml_test(AnsiPaletteTest
        SOURCES
        AnsiPaletteTest.cpp
        ${BML_SOURCE_DIR}/AnsiPalette.cpp
        DEPENDENCIES
        BMLUtils
)
# Need ImGui headers for AnsiPalette
target_include_directories(AnsiPaletteTest PRIVATE ${IMGUI_SOURCE_DIR})

add_bml_test(AnsiTextTest
        SOURCES
        AnsiTextTest.cpp
        ${BML_SOURCE_DIR}/AnsiText.cpp
        ${BML_SOURCE_DIR}/AnsiPalette.cpp
        ${IMGUI_SOURCE_DIR}/imgui.cpp
        ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
        ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
        ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
        DEPENDENCIES
        BMLUtils
)
target_include_directories(AnsiTextTest PRIVATE ${IMGUI_SOURCE_DIR})
target_compile_definitions(AnsiTextTest PRIVATE IMGUI_EXPORT)

add_bml_test(FpsCounterTest
        SOURCES
        FpsCounterTest.cpp
        ${BML_SOURCE_DIR}/FpsCounter.cpp
)

add_bml_test(SRTimerTest
        SOURCES
        SRTimerTest.cpp
        ${BML_SOURCE_DIR}/SRTimer.cpp
)

add_bml_test(CommandContextTest
        SOURCES
        CommandContextTest.cpp
        ${BML_SOURCE_DIR}/CommandContext.cpp
        DEPENDENCIES
        BMLUtils
)


add_executable(BuiAutomationTest
        BuiAutomationMain.cpp
        BuiAutomationTests.cpp
        ${BML_SOURCE_DIR}/UI/Bui.cpp
        ${IMGUI_SOURCE_DIR}/imgui.cpp
        ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
        ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
        ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
        ${IMGUI_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
        ${IMGUI_TEST_ENGINE_SOURCES}
)

target_include_directories(BuiAutomationTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${BML_INCLUDE_DIR}
        ${BML_SOURCE_DIR}
        ${BML_SOURCE_DIR}/Utils
        ${IMGUI_SOURCE_DIR}
        ${VIRTOOLS_SDK_INCLUDE_DIR}
        ${imgui_test_engine_SOURCE_DIR}
        ${IMGUI_TEST_ENGINE_DIR}
)

target_compile_definitions(BuiAutomationTest PRIVATE
        BML_EXPORTS
        BML_ENABLE_ANGELSCRIPT=0
        BML_UI_AUTOMATION_TEST
        IMGUI_USER_CONFIG="BuiAutomationImGuiConfig.h"
)
target_compile_options(BuiAutomationTest PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/Zc:strictStrings->
)
target_compile_features(BuiAutomationTest PRIVATE cxx_std_20)
set_target_properties(BuiAutomationTest PROPERTIES FOLDER "Tests/UI")

add_test(NAME BuiAutomationTest COMMAND $<TARGET_FILE:BuiAutomationTest>)
set_tests_properties(BuiAutomationTest PROPERTIES
        LABELS "ui;imgui;automation"
        TIMEOUT 30
)

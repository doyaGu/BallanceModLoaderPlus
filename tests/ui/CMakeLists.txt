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

add_executable(UiFrameworkTest
        UiFrameworkTest.cpp
        UiAutomationProtocol.cpp
)
target_compile_features(UiFrameworkTest PRIVATE cxx_std_20)
set_target_properties(UiFrameworkTest PROPERTIES FOLDER "Tests/UI")

add_test(NAME UiFrameworkTest
        COMMAND $<TARGET_FILE:UiFrameworkTest>
                "${CMAKE_CURRENT_SOURCE_DIR}/scenarios"
                "${BML_SOURCE_DIR}/UI/Automation/Scenarios"
                "${CMAKE_CURRENT_SOURCE_DIR}"
)
set_tests_properties(UiFrameworkTest PROPERTIES
        LABELS "ui;imgui;automation;framework"
        TIMEOUT 30
)

add_executable(UiAutomationSessionTest
        UiAutomationSessionTest.cpp
        ${BML_SOURCE_DIR}/UI/Automation/UiAutomationSession.cpp
)
target_include_directories(UiAutomationSessionTest PRIVATE ${BML_SOURCE_DIR})
target_compile_features(UiAutomationSessionTest PRIVATE cxx_std_20)
set_target_properties(UiAutomationSessionTest PROPERTIES FOLDER "Tests/UI")

add_test(NAME UiAutomationSessionTest COMMAND $<TARGET_FILE:UiAutomationSessionTest>)
set_tests_properties(UiAutomationSessionTest PROPERTIES
        LABELS "ui;automation;framework;session"
        TIMEOUT 30
)

if (WIN32)
    add_executable(UiPlayerRunner
            UiPlayerRunner.cpp
            UiPlayerHarness.cpp
            UiAutomationProtocol.cpp
            ${BML_SOURCE_DIR}/UI/Automation/UiAutomationSession.cpp
    )
    target_include_directories(UiPlayerRunner PRIVATE ${BML_SOURCE_DIR})
    target_compile_features(UiPlayerRunner PRIVATE cxx_std_20)
    target_compile_definitions(UiPlayerRunner PRIVATE
            UNICODE
            _UNICODE
            NOMINMAX
    )
    target_link_libraries(UiPlayerRunner PRIVATE
            user32
            gdi32
            imm32
    )
    set_target_properties(UiPlayerRunner PROPERTIES FOLDER "Tests/UI")
endif ()

set(BML_BALLANCE_ROOT "$ENV{BML_BALLANCE_ROOT}" CACHE PATH
        "Optional Ballance installation used by real Player UI automation")
if (WIN32 AND BML_BALLANCE_ROOT AND
        EXISTS "${BML_BALLANCE_ROOT}/Bin/Player.exe")
    file(GLOB BML_UI_SCENARIOS CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/scenarios/*.scenario")
    foreach (BML_UI_SCENARIO_FILE IN LISTS BML_UI_SCENARIOS)
        get_filename_component(BML_UI_SCENARIO
                "${BML_UI_SCENARIO_FILE}" NAME_WE)
        file(STRINGS "${BML_UI_SCENARIO_FILE}" BML_UI_REQUIREMENTS
                REGEX "^requires=")
        if ("requires=angelscript" IN_LIST BML_UI_REQUIREMENTS AND
                NOT BML_WITH_ANGELSCRIPT)
            continue()
        endif ()

        set(BML_UI_TEST "UiPlayerAutomation.${BML_UI_SCENARIO}")
        add_test(NAME "${BML_UI_TEST}"
                COMMAND $<TARGET_FILE:UiPlayerRunner>
                        --scenario-dir "${CMAKE_CURRENT_SOURCE_DIR}/scenarios"
                        --source-root "${BML_SOURCE_DIR}/UI/Automation/Scenarios"
                        --ballance-root "${BML_BALLANCE_ROOT}"
                        --build-dll "$<TARGET_FILE:BML>"
                        --scenario "${BML_UI_SCENARIO}"
                        --artifacts
                        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/UiAutomation/${BML_UI_SCENARIO}"
        )
        set_tests_properties("${BML_UI_TEST}" PROPERTIES
                LABELS "ui;imgui;automation;player;scenario;${BML_UI_SCENARIO}"
                RUN_SERIAL TRUE
                TIMEOUT 210
        )
    endforeach ()
endif ()

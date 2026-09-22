add_executable(UiFrameworkTest
        UiFrameworkTest.cpp
        UiAutomationProtocol.cpp
)
target_compile_features(UiFrameworkTest PRIVATE cxx_std_20)
set_target_properties(UiFrameworkTest PROPERTIES FOLDER "Tests/UI")

add_test(NAME UiFrameworkTest
        COMMAND $<TARGET_FILE:UiFrameworkTest>
                "${BML_UI_TEST_ROOT}/scenarios"
                "${BML_UI_TEST_ROOT}"
                "${BML_UI_TEST_ROOT}"
)
set_tests_properties(UiFrameworkTest PROPERTIES
        LABELS "ui;imgui;automation;framework"
        TIMEOUT 30
)

if (WIN32)
    add_executable(UiPlayerRunner
            UiPlayerRunner.cpp
            UiPlayerHarness.cpp
            UiAutomationProtocol.cpp
            ${BML_UI_TEST_ROOT}/session/UiAutomationSession.cpp
    )
    target_include_directories(UiPlayerRunner PRIVATE ${BML_UI_TEST_ROOT})
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
    add_dependencies(UiPlayerRunner PublicAuthoringTest)
    set_target_properties(UiPlayerRunner PROPERTIES FOLDER "Tests/UI")
endif ()

if (WIN32 AND BML_BALLANCE_ROOT AND
        EXISTS "${BML_BALLANCE_ROOT}/Bin/Player.exe")
    file(GLOB BML_UI_SCENARIOS CONFIGURE_DEPENDS
            "${BML_UI_TEST_ROOT}/scenarios/*.scenario")
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
                        --scenario-dir "${BML_UI_TEST_ROOT}/scenarios"
                        --source-root "${BML_UI_TEST_ROOT}"
                        --ballance-root "${BML_BALLANCE_ROOT}"
                        --build-dll "$<TARGET_FILE:BML>"
                        --public-authoring-mod "$<TARGET_FILE:PublicAuthoringTest>"
                        --scenario "${BML_UI_SCENARIO}"
                        --artifacts
                        "${BML_UI_TEST_BINARY_ROOT}/$<CONFIG>/UiAutomation/${BML_UI_SCENARIO}"
        )
        set_tests_properties("${BML_UI_TEST}" PROPERTIES
                LABELS "ui;imgui;automation;player;scenario;${BML_UI_SCENARIO}"
                RUN_SERIAL TRUE
                TIMEOUT 210
        )
    endforeach ()
endif ()

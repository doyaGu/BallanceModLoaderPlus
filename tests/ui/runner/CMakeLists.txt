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

add_executable(UiAutomationSessionTest
        UiAutomationSessionTest.cpp
        UiAutomationSession.cpp
)
target_include_directories(UiAutomationSessionTest PRIVATE ${BML_UI_TEST_ROOT})
target_compile_features(UiAutomationSessionTest PRIVATE cxx_std_20)
set_target_properties(UiAutomationSessionTest PROPERTIES FOLDER "Tests/UI")

add_test(NAME UiAutomationSessionTest COMMAND $<TARGET_FILE:UiAutomationSessionTest>)
set_tests_properties(UiAutomationSessionTest PROPERTIES
        LABELS "ui;automation;framework;session"
        TIMEOUT 30
)

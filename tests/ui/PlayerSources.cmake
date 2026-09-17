# Included from src/CMakeLists.txt after the BML target exists. This keeps the
# test-only Player implementation and its source inventory under tests/ui while
# compiling scenario registration objects directly into the acceptance DLL.
file(GLOB BML_UI_SCENARIO_DEFINITIONS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/scenarios/*.scenario")
if (NOT BML_UI_SCENARIO_DEFINITIONS)
    message(FATAL_ERROR "No UI automation scenarios found")
endif ()

set(BML_UI_SCENARIO_SOURCES)
foreach (BML_UI_SCENARIO_DEFINITION IN LISTS BML_UI_SCENARIO_DEFINITIONS)
    file(STRINGS "${BML_UI_SCENARIO_DEFINITION}" BML_UI_SOURCE_ENTRY REGEX "^source=")
    list(LENGTH BML_UI_SOURCE_ENTRY BML_UI_SOURCE_COUNT)
    if (NOT BML_UI_SOURCE_COUNT EQUAL 1)
        message(FATAL_ERROR "Expected one source entry in ${BML_UI_SCENARIO_DEFINITION}")
    endif ()
    string(REGEX REPLACE "^source=" "" BML_UI_SOURCE "${BML_UI_SOURCE_ENTRY}")
    if (NOT BML_UI_SOURCE MATCHES
            "^player/journeys/[a-z][a-z0-9-]*/[A-Za-z][A-Za-z0-9_]*Scenario[.]cpp$")
        message(FATAL_ERROR "Invalid scenario source in ${BML_UI_SCENARIO_DEFINITION}: ${BML_UI_SOURCE}")
    endif ()
    set(BML_UI_SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/${BML_UI_SOURCE}")
    if (NOT EXISTS "${BML_UI_SOURCE_PATH}")
        message(FATAL_ERROR "Missing scenario source: ${BML_UI_SOURCE_PATH}")
    endif ()
    if (BML_UI_SOURCE_PATH IN_LIST BML_UI_SCENARIO_SOURCES)
        message(FATAL_ERROR "Duplicate scenario source: ${BML_UI_SOURCE_PATH}")
    endif ()
    list(APPEND BML_UI_SCENARIO_SOURCES "${BML_UI_SOURCE_PATH}")
endforeach ()

set(BML_UI_PLAYER_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/player/UiAutomation.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/session/UiAutomationSession.cpp"
        ${BML_UI_SCENARIO_SOURCES}
)
target_sources(BML PRIVATE
        ${BML_UI_PLAYER_SOURCES}
        ${IMGUI_TEST_ENGINE_SOURCES}
        "${CMAKE_CURRENT_LIST_DIR}/player/UiAutomation.h"
        "${CMAKE_CURRENT_LIST_DIR}/player/UiTestFramework.h"
        "${CMAKE_CURRENT_LIST_DIR}/session/UiAutomationSession.h"
)
target_include_directories(BML PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}"
        ${imgui_test_engine_SOURCE_DIR}
        ${IMGUI_TEST_ENGINE_DIR}
)
set_source_files_properties(
        ${BML_UI_PLAYER_SOURCES}
        ${IMGUI_TEST_ENGINE_SOURCES}
        PROPERTIES COMPILE_DEFINITIONS BML_IMGUI_TEST_ENGINE_SOURCE
)

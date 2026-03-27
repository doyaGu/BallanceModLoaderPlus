if (NOT DEFINED REPO_ROOT)
    message(FATAL_ERROR "REPO_ROOT is required")
endif ()

set(required_modules
    BML_UI
    BML_Menu
    BML_Console
    BML_ModMenu
    BML_MapMenu
    BML_Input
    BML_Render
    BML_Physics
    BML_ObjectLoad
    BML_GameEvent
    BML_Gameplay
    BML_NewBallType
)

set(scorecard_files
    "${REPO_ROOT}/docs/migration/README.md"
    "${REPO_ROOT}/docs/migration/SUMMARY.md"
)

foreach(scorecard IN LISTS scorecard_files)
    if (NOT EXISTS "${scorecard}")
        message(FATAL_ERROR "Missing migration scorecard: ${scorecard}")
    endif ()

    file(READ "${scorecard}" scorecard_content)

    foreach(module_name IN LISTS required_modules)
        string(FIND "${scorecard_content}" "${module_name}" module_pos)
        if (module_pos EQUAL -1)
            message(FATAL_ERROR "Migration scorecard ${scorecard} is missing module ${module_name}")
        endif ()
    endforeach ()
endforeach ()

file(READ "${REPO_ROOT}/docs/migration/README.md" migration_readme)
file(READ "${REPO_ROOT}/docs/migration/SUMMARY.md" migration_summary)

string(FIND "${migration_readme}" "BML_IntegrationTest" integration_readme_pos)
string(FIND "${migration_summary}" "BML_IntegrationTest" integration_summary_pos)
if (integration_readme_pos EQUAL -1 OR integration_summary_pos EQUAL -1)
    message(FATAL_ERROR "Migration docs must mention BML_IntegrationTest as a validation module")
endif ()

set(forbidden_refs)
file(GLOB module_cmakes "${REPO_ROOT}/modules/*/CMakeLists.txt")
foreach(module_cmake IN LISTS module_cmakes)
    file(READ "${module_cmake}" cmake_content)
    string(REGEX MATCHALL "\\$\\{CMAKE_SOURCE_DIR\\}/src[^ \t\r\n\"\\)]*" source_refs "${cmake_content}")
    foreach(source_ref IN LISTS source_refs)
        if (NOT source_ref MATCHES "^\\$\\{CMAKE_SOURCE_DIR\\}/src/(Core|Utils|ModLoader|CoreDriver)(/.*)?$")
            list(APPEND forbidden_refs "${module_cmake}: ${source_ref}")
        endif ()
    endforeach ()
endforeach ()

if (forbidden_refs)
    list(REMOVE_DUPLICATES forbidden_refs)
    string(JOIN "\n" forbidden_message ${forbidden_refs})
    message(FATAL_ERROR "Forbidden legacy root src references found:\n${forbidden_message}")
endif ()

set(migration_module_docs
    "${REPO_ROOT}/docs/migration/01-BML_UI.md"
    "${REPO_ROOT}/docs/migration/03-BML_Menu.md"
    "${REPO_ROOT}/docs/migration/02-BML_Console.md"
    "${REPO_ROOT}/docs/migration/07-BML_Render.md"
    "${REPO_ROOT}/docs/migration/11-BML_Gameplay.md"
    "${REPO_ROOT}/docs/migration/12-BML_NewBallType.md"
)

foreach(module_doc IN LISTS migration_module_docs)
    if (NOT EXISTS "${module_doc}")
        message(FATAL_ERROR "Missing module migration doc: ${module_doc}")
    endif ()
endforeach ()

set(active_legacy_scan_files
    "${REPO_ROOT}/modules/BML_UI/src/UIMod.cpp"
    "${REPO_ROOT}/modules/BML_Menu/src/MenuMod.cpp"
    "${REPO_ROOT}/modules/BML_Menu/src/Menu.cpp"
    "${REPO_ROOT}/modules/BML_Input/src/InputMod.cpp"
    "${REPO_ROOT}/modules/BML_Console/src/ConsoleMod.cpp"
    "${REPO_ROOT}/modules/BML_Console/src/MessageBoard.h"
    "${REPO_ROOT}/modules/BML_ModMenu/src/ModMenuMod.cpp"
    "${REPO_ROOT}/modules/BML_MapMenu/src/MapMenuMod.cpp"
    "${REPO_ROOT}/modules/BML_IntegrationTest/src/BuiltinModuleProbe.cpp"
)

set(active_legacy_patterns
    "BML_UI_ApiTable"
    "BML_InputExtension"
    "SetMenuLifecycleCallbacks"
    "BML_TOPIC_UI_DRAW"
    "bmlUIGetApiTable"
    "bmlUIGetImGuiContext"
    "BML_EXT_Input"
    "BML/Bui.h"
    "Bui::"
    "bml_menuui.h"
    "BML_MenuUI"
    "BML_MENUUI"
    "com.bml.menuui"
    "bml.menuui"
)

set(active_legacy_hits)
foreach(scan_file IN LISTS active_legacy_scan_files)
    if (NOT EXISTS "${scan_file}")
        continue ()
    endif ()

    file(READ "${scan_file}" scan_content)
    foreach(pattern IN LISTS active_legacy_patterns)
        string(FIND "${scan_content}" "${pattern}" pattern_pos)
        if (NOT pattern_pos EQUAL -1)
            list(APPEND active_legacy_hits "${scan_file}: ${pattern}")
        endif ()
    endforeach ()
endforeach ()

if (active_legacy_hits)
    list(REMOVE_DUPLICATES active_legacy_hits)
    string(JOIN "\n" active_legacy_message ${active_legacy_hits})
    message(FATAL_ERROR "Active builtin modules still reference forbidden legacy UI/Input APIs:\n${active_legacy_message}")
endif ()

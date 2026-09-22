add_test(
        NAME TagReleaseFilesTest
        COMMAND pwsh -NoLogo -NoProfile -File
                "${CMAKE_CURRENT_SOURCE_DIR}/TagReleaseFilesTest.ps1"
                -SourceRoot "${PROJECT_SOURCE_DIR}"
)

add_test(
        NAME DocsDeploymentPreservesUpdaterFilesTest
        COMMAND pwsh -NoLogo -NoProfile -File
                "${CMAKE_CURRENT_SOURCE_DIR}/DocsDeploymentPreservesUpdaterFilesTest.ps1"
                -SourceRoot "${PROJECT_SOURCE_DIR}"
)

add_test(
        NAME PinnedReleaseDependenciesTest
        COMMAND pwsh -NoLogo -NoProfile -File
                "${CMAKE_CURRENT_SOURCE_DIR}/PinnedReleaseDependenciesTest.ps1"
                -SourceRoot "${PROJECT_SOURCE_DIR}"
)

add_test(
        NAME SignAndCopyReleaseFilesTest
        COMMAND pwsh -NoLogo -NoProfile -File
                "${CMAKE_CURRENT_SOURCE_DIR}/SignAndCopyReleaseFilesTest.ps1"
                -SourceRoot "${PROJECT_SOURCE_DIR}"
)

if (NOT BML_ENABLE_BEHAVIOR_TEST_INTERFACE)
    add_test(
            NAME ProductionRuntimeContentsTest
            COMMAND pwsh -NoLogo -NoProfile -File
                    "${CMAKE_CURRENT_SOURCE_DIR}/ProductionRuntimeContentsTest.ps1"
                    -RuntimeDll "$<TARGET_FILE:BML>"
    )
endif ()

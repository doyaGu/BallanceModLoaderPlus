if (WIN32)
    add_bml_test(VTablePatchTest
            SOURCES
            VTablePatchTest.cpp
            ${BML_SOURCE_DIR}/Hooks/VTablePatch.cpp
            ${BML_SOURCE_DIR}/Hooks/VTablePatch.h
    )

    add_bml_test(RenderHookTest
            SOURCES
            RenderHookTest.cpp
            ${BML_SOURCE_DIR}/Hooks/RenderHook.cpp
            ${BML_SOURCE_DIR}/Hooks/RenderHook.h
            ${BML_SOURCE_DIR}/Hooks/VTablePatch.cpp
            ${BML_SOURCE_DIR}/Hooks/VTablePatch.h
            DEPENDENCIES
            BML_HookUtils
            VirtoolsSDK::CK2
            VirtoolsSDK::VxMath
    )
endif ()

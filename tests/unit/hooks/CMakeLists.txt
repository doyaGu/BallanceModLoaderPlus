if (WIN32)
    add_bml_test(VTablePatchTest
            SOURCES
            VTablePatchTest.cpp
            ${BML_SOURCE_DIR}/Hooks/VTablePatch.cpp
            ${BML_SOURCE_DIR}/Hooks/VTablePatch.h
    )
endif ()

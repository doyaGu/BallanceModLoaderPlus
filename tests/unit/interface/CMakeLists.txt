add_bml_test(InterfaceHeaderTest
        SOURCES InterfaceHeaderTest.cpp
)

add_bml_test(InterfaceAuthoringTest
        SOURCES InterfaceAuthoringTest.cpp
)

add_bml_test(CommandAuthoringTest
        SOURCES CommandAuthoringTest.cpp
)

add_bml_test(InterfaceRegistryTest
        SOURCES
        InterfaceRegistryTest.cpp
        ${BML_SOURCE_DIR}/Api/InterfaceRegistry.cpp
        ${BML_SOURCE_DIR}/Api/InterfaceRegistry.h
)

add_bml_test(InterfaceStructOffsetsTest
        SOURCES
        InterfaceStructOffsetsTest.cpp
)

add_bml_test(LegacyModVersionTest
        SOURCES
        LegacyModVersionTest.cpp
        DEPENDENCIES
        VirtoolsSDK::CK2
)

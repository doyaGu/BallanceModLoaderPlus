add_bml_test(ResultTest
        SOURCES ResultTest.cpp
)

add_bml_test(ConfigTest
            SOURCES
            ConfigTest.cpp
            ${BML_SOURCE_DIR}/Config/Config.cpp
            ${BML_SOURCE_DIR}/Config/Config.h
            ${BML_SOURCE_DIR}/Config/ConfigStore.cpp
            ${BML_SOURCE_DIR}/Config/ConfigStore.h
            ${BML_SOURCE_DIR}/Mods/BMLConfigMigration.cpp
            ${BML_SOURCE_DIR}/Mods/BMLConfigMigration.h
            DEPENDENCIES
            BMLUtils
            CK2
            VxMath
    )
    target_compile_definitions(ConfigTest PRIVATE
            BML_TEST_FONT_DIRECTORY="${PROJECT_SOURCE_DIR}/packaging/runtime/ModLoader/Fonts"
    )

add_bml_test(NativeModRegistryTest
        SOURCES
        NativeModRegistryTest.cpp
        ${BML_SOURCE_DIR}/Loader/NativeModRegistry.cpp
        ${BML_SOURCE_DIR}/Loader/NativeModRegistry.h
)

add_bml_test(ObjectRefsTest
        SOURCES
        ObjectRefsTest.cpp
        ${BML_SOURCE_DIR}/Api/ObjectRefs.cpp
)

add_bml_test(DataShareTest
        SOURCES
        DataShareTest.cpp
        ${BML_SOURCE_DIR}/DataShare/DataShare.cpp
)

add_bml_test(ModInvocationGateTest
        SOURCES
        ModInvocationGateTest.cpp
)

add_bml_test(ModProtocolTest
        SOURCES ModProtocolTest.cpp
)

add_bml_test(ModDefinitionTest
        SOURCES ModDefinitionTest.cpp
)

add_bml_test(NativeModTest
    SOURCES NativeModTest.cpp ${BML_SOURCE_DIR}/Loader/NativeMod.cpp
    DEPENDENCIES CK2 VxMath)

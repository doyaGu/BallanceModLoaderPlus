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
        DEPENDENCIES
        BMLUtils
        VirtoolsSDK::CK2
        VirtoolsSDK::VxMath
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
target_include_directories(ObjectRefsTest PRIVATE "${VIRTOOLS_SDK_INCLUDE_DIR}")

add_bml_test(DataShareTest
        SOURCES
        DataShareTest.cpp
        ${BML_SOURCE_DIR}/DataShare/DataShare.cpp
)

add_bml_test(ModInvocationGateTest
        SOURCES
        ModInvocationGateTest.cpp
)

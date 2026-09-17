add_bml_test(TimerTest
        SOURCES TimerTest.cpp
)

add_bml_test(StringUtilsTest
        SOURCES
        StringUtilsTest.cpp
        DEPENDENCIES
        BMLUtils
)

add_bml_test(PathUtilsTest
        SOURCES
        PathUtilsTest.cpp
        DEPENDENCIES
        BMLUtils
)

add_bml_test(IniFileTest
        SOURCES
        IniFileTest.cpp
        DEPENDENCIES
        BMLUtils
)

if (WIN32)
    add_bml_test(HookUtilsTest
            SOURCES
            HookUtilsTest.cpp
            DEPENDENCIES
            BML_HookUtils
    )

    add_bml_test(CryptoUtilsTest
            SOURCES
            CryptoUtilsTest.cpp
            DEPENDENCIES
            BML_CryptoUtils
            BML_PathUtils
            BML_StringUtils
    )
endif ()

add_bml_test(TypeConvertTest
        SOURCES TypeConvertTest.cpp
        DEPENDENCIES VirtoolsSDK::VxMath
)

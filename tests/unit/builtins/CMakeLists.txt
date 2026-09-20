add_bml_test(ConsoleLayoutTest
        SOURCES
        ConsoleLayoutTest.cpp
        ${BML_SOURCE_DIR}/Console/ConsoleLayout.cpp
)

add_bml_test(CommandInputTest
        SOURCES
        CommandInputTest.cpp
        ${BML_SOURCE_DIR}/Console/CommandInput.cpp
)

add_bml_test(CommandBarThemeTest
        SOURCES
        CommandBarThemeTest.cpp
        ${BML_SOURCE_DIR}/Console/CommandBarTheme.cpp
        ${BML_SOURCE_DIR}/UI/ColorCodec.cpp
)
target_include_directories(CommandBarThemeTest PRIVATE
        "${VIRTOOLS_SDK_INCLUDE_DIR}"
)

add_bml_test(ModMenuSessionTest
        SOURCES
        ModMenuSessionTest.cpp
        ${BML_SOURCE_DIR}/ModMenu/ModMenuSession.cpp
)
target_include_directories(ModMenuSessionTest PRIVATE
        "${VIRTOOLS_SDK_INCLUDE_DIR}"
)

add_bml_test(ModMenuPagesTest
        SOURCES
        ModMenuPagesTest.cpp
        ${BML_SOURCE_DIR}/ModMenu/ModMenuPages.cpp
)

add_bml_test(FpsCounterTest
        SOURCES
        FpsCounterTest.cpp
        ${BML_SOURCE_DIR}/HUD/FpsCounter.cpp
)

add_bml_test(HUDAnimationTest
        SOURCES
        HUDAnimationTest.cpp
        ${BML_SOURCE_DIR}/HUD/HUDAnimation.cpp
        DEPENDENCIES
        VirtoolsSDK::CK2
        VirtoolsSDK::VxMath
)
target_include_directories(HUDAnimationTest BEFORE PRIVATE ${IMGUI_SOURCE_DIR})

add_bml_test(MapCatalogTest
        SOURCES
        MapCatalogTest.cpp
        ${BML_SOURCE_DIR}/CustomMaps/MapCatalog.cpp
        ${BML_SOURCE_DIR}/CustomMaps/MapCatalog.h
        DEPENDENCIES
        BMLUtils
)

add_bml_test(MapMenuStateTest
        SOURCES
        MapMenuStateTest.cpp
        ${BML_SOURCE_DIR}/CustomMaps/MapCatalog.cpp
        ${BML_SOURCE_DIR}/CustomMaps/MapCatalog.h
        ${BML_SOURCE_DIR}/CustomMaps/MapMenuState.cpp
        ${BML_SOURCE_DIR}/CustomMaps/MapMenuState.h
        DEPENDENCIES
        BMLUtils
)
target_include_directories(MapMenuStateTest BEFORE PRIVATE
        ${IMGUI_SOURCE_DIR}
        ${VIRTOOLS_SDK_INCLUDE_DIR}
)

add_bml_test(SRTimerTest
        SOURCES
        SRTimerTest.cpp
        ${BML_SOURCE_DIR}/HUD/SRTimer.cpp
)

add_bml_test(CommandContextTest
        SOURCES
        CommandContextTest.cpp
        ${BML_SOURCE_DIR}/Console/CommandContext.cpp
        DEPENDENCIES
        BMLUtils
)

set(BML_SHELL_CORE_SOURCES
        ${BML_SOURCE_DIR}/Console/Shell/ShellExecutor.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellExpander.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellLexer.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellParser.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellQuoting.cpp
)

add_bml_test(ShellLexerTest
        SOURCES
        ShellLexerTest.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellLexer.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellQuoting.cpp
        DEPENDENCIES
        BML_StringUtils
        utf8
)

add_bml_test(ShellQuotingTest
        SOURCES
        ShellQuotingTest.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellLexer.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellQuoting.cpp
        DEPENDENCIES
        BML_StringUtils
        utf8
)

add_bml_test(ShellParserTest
        SOURCES
        ShellParserTest.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellLexer.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellParser.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellQuoting.cpp
        DEPENDENCIES
        BML_StringUtils
        utf8
)

add_bml_test(ShellEditingTest
        SOURCES
        ShellEditingTest.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellEditing.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellLexer.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellParser.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellQuoting.cpp
        DEPENDENCIES
        BML_StringUtils
        utf8
)

add_bml_test(ShellExecutorTest
        SOURCES
        ShellExecutorTest.cpp
        ${BML_SHELL_CORE_SOURCES}
        DEPENDENCIES
        BML_StringUtils
        utf8
)

add_bml_test(ShellEnvironmentTest
        SOURCES
        ShellEnvironmentTest.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellEnvironment.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellQuoting.cpp
        DEPENDENCIES
        BMLUtils
)

add_bml_test(FilterCommandsTest
        SOURCES
        FilterCommandsTest.cpp
        ${BML_SOURCE_DIR}/Console/CommandContext.cpp
        ${BML_SOURCE_DIR}/Console/Shell/FilterCommands.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellBuiltins.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellEnvironment.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellIo.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellQuoting.cpp
        DEPENDENCIES
        BMLUtils
        onig
)
target_include_directories(FilterCommandsTest PRIVATE
        "${VIRTOOLS_SDK_INCLUDE_DIR}"
)

add_bml_test(ShellHistoryTest
        SOURCES
        ShellHistoryTest.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellHistory.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellQuoting.cpp
        DEPENDENCIES
        BMLUtils
)

add_bml_test(ShellCompletionTest
        SOURCES
        ShellCompletionTest.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellCompletion.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellEditing.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellHighlighter.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellLexer.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellParser.cpp
        ${BML_SOURCE_DIR}/Console/Shell/ShellQuoting.cpp
        DEPENDENCIES
        BML_StringUtils
        utf8
)

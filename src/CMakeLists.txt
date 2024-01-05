set(BML_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

configure_file(Version.h.in "${BML_INCLUDE_DIR}/BML/Version.h")

set(BML_PUBLIC_HEADERS
        ${BML_INCLUDE_DIR}/BML/Export.h
        ${BML_INCLUDE_DIR}/BML/Version.h
        ${BML_INCLUDE_DIR}/BML/Defines.h

        ${BML_INCLUDE_DIR}/BML/ILogger.h
        ${BML_INCLUDE_DIR}/BML/ICommand.h
        ${BML_INCLUDE_DIR}/BML/IConfig.h
        ${BML_INCLUDE_DIR}/BML/IMessageReceiver.h
        ${BML_INCLUDE_DIR}/BML/IBML.h
        ${BML_INCLUDE_DIR}/BML/IMod.h

        ${BML_INCLUDE_DIR}/BML/Bui.h
        ${BML_INCLUDE_DIR}/BML/Gui.h
        ${BML_INCLUDE_DIR}/BML/InputHook.h

        ${BML_INCLUDE_DIR}/BML/ExecuteBB.h
        ${BML_INCLUDE_DIR}/BML/ScriptHelper.h

        ${BML_INCLUDE_DIR}/BML/BMLAll.h
)

set(BML_PRIVATE_HEADERS
        ModLoader.h

        BMLMod.h
        NewBallTypeMod.h
        Commands.h

        Overlay.h
        Logger.h
        Config.h
        Timer.h

        StringUtils.h
        PathUtils.h
        WindowUtils.h
)

set(BML_SOURCES
        BML.cpp
        ModLoader.cpp

        IMod.cpp
        BMLMod.cpp
        NewBallTypeMod.cpp
        Commands.cpp

        Bui.cpp
        Overlay.cpp

        Gui/Element.cpp
        Gui/Text.cpp
        Gui/Panel.cpp
        Gui/Label.cpp
        Gui/Button.cpp
        Gui/Input.cpp
        Gui/KeyInput.cpp
        Gui/Gui.cpp

        InputHook.cpp
        ObjectLoadHook.cpp
        PhysicalizeHook.cpp

        ExecuteBB.cpp
        ScriptHelper.cpp

        Logger.cpp
        Config.cpp
        Timer.cpp

        PathUtils.cpp
        WindowUtils.cpp

        BML.rc
)

set(IMGUI_HEADERS
        ${IMGUI_SOURCE_DIR}/imgui.h
        ${IMGUI_SOURCE_DIR}/imgui_internal.h
        ${IMGUI_SOURCE_DIR}/imstb_rectpack.h
        ${IMGUI_SOURCE_DIR}/imstb_textedit.h
        ${IMGUI_SOURCE_DIR}/imstb_truetype.h
        ${IMGUI_SOURCE_DIR}/backends/imgui_impl_win32.h
        ${IMGUI_SOURCE_DIR}/backends/imgui_impl_ck2.h
)

set(IMGUI_SOURCES
        ${IMGUI_SOURCE_DIR}/imgui.cpp
        ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
        ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
        ${IMGUI_SOURCE_DIR}/imgui_demo.cpp
        ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
        ${IMGUI_SOURCE_DIR}/backends/imgui_impl_win32.cpp
        ${IMGUI_SOURCE_DIR}/backends/imgui_impl_ck2.cpp
)

add_library(BML SHARED
        ${BML_SOURCES} ${BML_PUBLIC_HEADERS} ${BML_PRIVATE_HEADERS}
        ${IMGUI_SOURCES} ${IMGUI_HEADERS}
)

target_include_directories(BML
        PUBLIC
        $<BUILD_INTERFACE:${BML_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${IMGUI_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(BML
        PUBLIC
        CK2 VxMath
        PRIVATE
        zip
)

target_compile_definitions(BML PRIVATE "BML_EXPORTS" "IMGUI_EXPORT")

set_target_properties(BML PROPERTIES
        OUTPUT_NAME "BMLPlus"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

install(TARGETS BML EXPORT BMLTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(DIRECTORY "${BML_INCLUDE_DIR}/BML" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(FILES
        "${IMGUI_SOURCE_DIR}/imgui.h"
        "${IMGUI_SOURCE_DIR}/imgui_internal.h"
        "${IMGUI_SOURCE_DIR}/imstb_textedit.h"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT BMLTargets DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BML")

configure_package_config_file("${PROJECT_SOURCE_DIR}/cmake/BMLConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/BMLConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BML"
)

write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/BMLConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMinorVersion
)

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/BMLConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/BMLConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BML"
)
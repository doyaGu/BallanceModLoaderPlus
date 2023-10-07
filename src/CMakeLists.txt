set(BMLPLUS_INC_DIR ${PROJECT_SOURCE_DIR}/include)

configure_file(Version.h.in "${BMLPLUS_INC_DIR}/BMLPlus/Version.h")

set(BMLPLUS_PUBLIC_HDRS
        ${BMLPLUS_INC_DIR}/BMLPlus/Export.h
        ${BMLPLUS_INC_DIR}/BMLPlus/Version.h
        ${BMLPLUS_INC_DIR}/BMLPlus/Defines.h

        ${BMLPLUS_INC_DIR}/BMLPlus/ILogger.h
        ${BMLPLUS_INC_DIR}/BMLPlus/ICommand.h
        ${BMLPLUS_INC_DIR}/BMLPlus/IConfig.h
        ${BMLPLUS_INC_DIR}/BMLPlus/IMessageReceiver.h
        ${BMLPLUS_INC_DIR}/BMLPlus/IBML.h
        ${BMLPLUS_INC_DIR}/BMLPlus/IMod.h

        ${BMLPLUS_INC_DIR}/BMLPlus/Gui.h
        ${BMLPLUS_INC_DIR}/BMLPlus/InputHook.h

        ${BMLPLUS_INC_DIR}/BMLPlus/ExecuteBB.h
        ${BMLPLUS_INC_DIR}/BMLPlus/ScriptHelper.h

        ${BMLPLUS_INC_DIR}/BMLPlus/BMLAll.h
        )

set(BMLPLUS_PRIVATE_HDRS
        ModLoader.h
        ModDll.h

        BMLMod.h
        NewBallTypeMod.h

        Logger.h
        Commands.h
        Config.h

        Timer.h
        )

set(BMLPLUS_SRCS
        BML.cpp
        ModLoader.cpp
        ModDll.cpp

        IMod.cpp
        BMLMod.cpp
        NewBallTypeMod.cpp

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
        Commands.cpp
        Config.cpp
        Timer.cpp

        BML.rc
        )

add_library(BMLPlus SHARED ${BMLPLUS_SRCS} ${BMLPLUS_PUBLIC_HDRS} ${BMLPLUS_PRIVATE_HDRS})
target_include_directories(BMLPlus
        PUBLIC
            $<BUILD_INTERFACE:${BMLPLUS_INC_DIR}>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${BMLPLUS_INC_DIR}/BMLPlus
        )

target_link_libraries(BMLPlus
        PUBLIC
            CK2 VxMath
        PRIVATE
            CKZlib ImGui
        )

target_compile_definitions(BMLPlus PRIVATE "BML_EXPORTS")

set_target_properties(BMLPlus PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

if (NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL)
    target_include_directories(BMLPlus PUBLIC
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
            )

    install(TARGETS BMLPlus
            EXPORT BMLPlus
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            )

    install(EXPORT BMLPlus
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BMLPlus"
            )

    write_basic_package_version_file(${PROJECT_BINARY_DIR}/BMLPlus-config-version.cmake
            COMPATIBILITY SameMajorVersion
            )

    set(BML_CONFIG_CONTENT "@PACKAGE_INIT@\n")
    string(APPEND BML_CONFIG_CONTENT "include(\"\${PROJECT_SOURCE_DIR}/BMLPlus.cmake\")")
    file(WRITE ${PROJECT_BINARY_DIR}/BMLPlus-config.cmake.in ${BML_CONFIG_CONTENT})

    configure_package_config_file(${PROJECT_BINARY_DIR}/BMLPlus-config.cmake.in
            ${PROJECT_BINARY_DIR}/BMLPlus-config.cmake
            INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BMLPlus"
            )

    install(FILES
            ${PROJECT_BINARY_DIR}/BMLPlus-config-version.cmake
            ${PROJECT_BINARY_DIR}/BMLPlus-config.cmake
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BMLPlus"
            )
endif ()

if (NOT SKIP_INSTALL_HEADERS AND NOT SKIP_INSTALL_ALL)
    install(DIRECTORY ${BMLPLUS_INC_DIR}/BMLPlus DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif ()
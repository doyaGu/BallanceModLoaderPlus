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
        ${BMLPLUS_INC_DIR}/BMLPlus/RegisterBB.h
        ${BMLPLUS_INC_DIR}/BMLPlus/ScriptHelper.h

        ${BMLPLUS_INC_DIR}/BMLPlus/BMLAll.h
        )

set(BMLPLUS_PRIVATE_HDRS
        ModLoader.h
        ModManager.h

        BMLMod.h
        NewBallTypeMod.h

        Logger.h
        Commands.h
        Config.h

        Timer.h
        Util.h
        )

set(BMLPLUS_SRCS
        BML.cpp
        ModLoader.cpp
        ModManager.cpp

        HookObjectLoad.cpp
        HookPhysicalize.cpp

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
        ExecuteBB.cpp
        RegisterBB.cpp
        ScriptHelper.cpp

        Logger.cpp
        Commands.cpp
        Config.cpp
        Timer.cpp
        Util.cpp

        BML.rc
        )

add_library(BMLPlus SHARED ${BMLPLUS_SRCS} ${BMLPLUS_PUBLIC_HDRS} ${BMLPLUS_PRIVATE_HDRS})
target_include_directories(BMLPlus
        PUBLIC
            $<BUILD_INTERFACE:${BMLPLUS_INC_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${BMLPLUS_INC_DIR}/BMLPlus
        )

target_link_libraries(BMLPlus
        PUBLIC
            CK2 VxMath
        PRIVATE
            CKZlib asmjit::asmjit
        )

target_compile_definitions(BMLPlus PRIVATE "BML_EXPORTS")

# Disable msvc unsafe warnings
target_compile_definitions(BMLPlus PRIVATE
        $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
        $<$<C_COMPILER_ID:MSVC>:_CRT_NONSTDC_NO_WARNINGS>
        $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_DEPRECATE>
        $<$<C_COMPILER_ID:MSVC>:_CRT_NONSTDC_NO_DEPRECATE>
        )

if (NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL)
    configure_package_config_file(${PROJECT_SOURCE_DIR}/CMake/BMLPlusConfig.cmake.in
            "${CMAKE_CURRENT_BINARY_DIR}/BMLPlusConfig.cmake"
            INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BMLPlus"
            )

    write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/BMLPlusConfigVersion.cmake"
            COMPATIBILITY SameMinorVersion
    )

    install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/BMLPlusConfig.cmake"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BMLPlus"
            )

    install(TARGETS BMLPlus
            EXPORT BMLPlusTargets
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            )

    install(EXPORT "BMLPlusTargets"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BMLPlus")
endif ()

if (NOT SKIP_INSTALL_HEADERS AND NOT SKIP_INSTALL_ALL)
    install(DIRECTORY ${BMLPLUS_INC_DIR}/BMLPlus DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif ()
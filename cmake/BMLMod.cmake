include_guard(GLOBAL)

function(bml_add_mod TARGET_NAME)
    if ("${TARGET_NAME}" STREQUAL "")
        message(FATAL_ERROR "bml_add_mod requires a target name")
    endif ()
    if (TARGET "${TARGET_NAME}")
        message(FATAL_ERROR "bml_add_mod target already exists: ${TARGET_NAME}")
    endif ()
    if (ARGC LESS 2)
        message(FATAL_ERROR "bml_add_mod(${TARGET_NAME}) requires at least one source file")
    endif ()
    if (NOT BML_RUNTIME STREQUAL "sdl")
    if (NOT MSVC AND NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
        message(FATAL_ERROR
                "bml_add_mod requires an MSVC-compatible C++ ABI because the legacy BML native interface crosses the DLL boundary; use MSVC or clang-cl")
    endif ()
    if (NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR
                "bml_add_mod requires a 32-bit target because Ballance Player is a 32-bit process; configure an x86/Win32 build")
    endif ()
    else()
        if(NOT CMAKE_CXX_COMPILER_ID STREQUAL BML_SDK_COMPILER_ID OR
           NOT CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL BML_SDK_COMPILER_VERSION)
            message(FATAL_ERROR "This BML SDK requires ${BML_SDK_COMPILER_ID} ${BML_SDK_COMPILER_VERSION}; use the matching compiler and C++ standard library")
        endif()
        if(NOT CMAKE_SIZEOF_VOID_P EQUAL BML_SDK_POINTER_SIZE OR
           NOT CMAKE_SYSTEM_NAME STREQUAL BML_SDK_SYSTEM_NAME)
            message(FATAL_ERROR "The Mod target does not match this BML SDK platform/word size")
        endif()
        set(_bml_mod_arch "${CMAKE_SYSTEM_PROCESSOR}")
        if(CMAKE_OSX_ARCHITECTURES)
            set(_bml_mod_arch "${CMAKE_OSX_ARCHITECTURES}")
        elseif(CMAKE_GENERATOR_PLATFORM)
            set(_bml_mod_arch "${CMAKE_GENERATOR_PLATFORM}")
        elseif(CMAKE_CXX_COMPILER_ARCHITECTURE_ID)
            set(_bml_mod_arch "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}")
        endif()
        string(TOLOWER "${_bml_mod_arch}" _bml_mod_arch)
        if(_bml_mod_arch MATCHES "^(aarch64|arm64)$")
            set(_bml_mod_arch arm64)
        elseif(CMAKE_SIZEOF_VOID_P EQUAL 4 AND _bml_mod_arch MATCHES "^(x86|win32|i[3-6]86|amd64|x86_64)$")
            set(_bml_mod_arch x86)
        elseif(_bml_mod_arch MATCHES "^(x64|amd64|x86_64)$")
            set(_bml_mod_arch x64)
        endif()
        if(NOT _bml_mod_arch STREQUAL BML_TARGET_ARCH)
            message(FATAL_ERROR "The Mod architecture ${_bml_mod_arch} does not match this BML SDK (${BML_TARGET_ARCH}); build one architecture per SDK")
        endif()
    endif()
    # BML_MSVC_RUNTIME_LIBRARY is set by BMLConfig.cmake to the runtime the
    # packaged loader was built against. The legacy native interface passes C++
    # objects across the DLL boundary, so a Mod that links the other runtime
    # corrupts allocations and iterator debug state at load time instead of
    # failing to build.
    if (NOT "${BML_MSVC_RUNTIME_LIBRARY}" STREQUAL "")
        if (NOT "${CMAKE_MSVC_RUNTIME_LIBRARY}" STREQUAL ""
            AND NOT "${CMAKE_MSVC_RUNTIME_LIBRARY}" STREQUAL "${BML_MSVC_RUNTIME_LIBRARY}")
            message(FATAL_ERROR
                    "bml_add_mod requires the ${BML_MSVC_RUNTIME_LIBRARY} MSVC runtime to match this BML+ SDK, "
                    "but CMAKE_MSVC_RUNTIME_LIBRARY is ${CMAKE_MSVC_RUNTIME_LIBRARY}; "
                    "use the SDK archive that matches the loader you install, or clear CMAKE_MSVC_RUNTIME_LIBRARY")
        endif ()
        cmake_policy(GET CMP0091 BML_MSVC_RUNTIME_POLICY)
        if (NOT BML_MSVC_RUNTIME_POLICY STREQUAL "NEW")
            message(FATAL_ERROR
                    "bml_add_mod needs policy CMP0091 set to NEW so it can pin the ${BML_MSVC_RUNTIME_LIBRARY} "
                    "MSVC runtime required by this BML+ SDK; raise cmake_minimum_required to 3.15 or newer")
        endif ()
    endif ()

    if(BML_RUNTIME STREQUAL "sdl")
        add_library("${TARGET_NAME}" MODULE ${ARGN})
    else()
        add_library("${TARGET_NAME}" SHARED ${ARGN})
    endif()
    target_link_libraries("${TARGET_NAME}" PRIVATE BML::BML)
    target_compile_features("${TARGET_NAME}" PRIVATE cxx_std_20)
    if(MSVC OR CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    target_link_options("${TARGET_NAME}" PRIVATE
            "/EXPORT:BMLEntry"
            "/EXPORT:BMLExit"
    )
    endif()
    set_target_properties("${TARGET_NAME}" PROPERTIES
            CXX_EXTENSIONS OFF
            PREFIX ""
            SUFFIX ".bmodp"
            BML_MOD_PACKAGE_NAME "${TARGET_NAME}.bmodp"
            FOLDER "Mods"
    )
    if(BML_RUNTIME STREQUAL "sdl")
        file(GENERATE OUTPUT "$<TARGET_FILE:${TARGET_NAME}>.abi.json" CONTENT "${BML_ABI_JSON}\n")
        # Modules run inside Player. Use its layout from the first link, so
        # repeated installs do not edit an already-relocated Mach-O in place.
        set_target_properties("${TARGET_NAME}" PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
        if(APPLE)
            if(CMAKE_VERSION VERSION_LESS 3.19)
                message(FATAL_ERROR "Building macOS SDL Mods requires CMake 3.19 or newer")
            endif()
            set_target_properties("${TARGET_NAME}" PROPERTIES INSTALL_RPATH "@loader_path/../../Managers;@loader_path/../../Bin;@loader_path/../..")
            # A development SDL may have an absolute install name. RPATH alone
            # cannot override it: bind the finished Mod to Player's SDL instead.
            set(_bml_finalize_mod "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/FinalizeSdlMod.cmake")
            set_property(TARGET "${TARGET_NAME}" APPEND PROPERTY LINK_DEPENDS "${_bml_finalize_mod}")
            target_link_options("${TARGET_NAME}" PRIVATE "LINKER:-headerpad_max_install_names")
            add_custom_command(TARGET "${TARGET_NAME}" POST_BUILD
                COMMAND "${CMAKE_COMMAND}" "-DBML_MOD_FILE=$<TARGET_FILE:${TARGET_NAME}>"
                    -P "${_bml_finalize_mod}"
                COMMENT "Binding ${TARGET_NAME} to Player's SDL"
                VERBATIM)
        elseif(UNIX)
            set_target_properties("${TARGET_NAME}" PROPERTIES INSTALL_RPATH "$ORIGIN/../../Managers;$ORIGIN/../../Bin;$ORIGIN/../../lib;$ORIGIN/../..")
        endif()
    endif()
    if (NOT "${BML_MSVC_RUNTIME_LIBRARY}" STREQUAL "")
        # Pin the runtime instead of letting it follow the build configuration,
        # so a Debug-configured Mod still links the runtime its loader uses.
        set_target_properties("${TARGET_NAME}" PROPERTIES
                MSVC_RUNTIME_LIBRARY "${BML_MSVC_RUNTIME_LIBRARY}"
        )
    endif ()
endfunction()

function(bml_install_mod TARGET_NAME)
    if (NOT TARGET "${TARGET_NAME}")
        message(FATAL_ERROR "bml_install_mod target does not exist: ${TARGET_NAME}")
    endif ()

    install(TARGETS "${TARGET_NAME}" RUNTIME DESTINATION Mods LIBRARY DESTINATION Mods)
    if(BML_RUNTIME STREQUAL "sdl")
        install(FILES "$<TARGET_FILE:${TARGET_NAME}>.abi.json" DESTINATION Mods)
    endif()
endfunction()

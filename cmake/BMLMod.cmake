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
    if (NOT MSVC AND NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
        message(FATAL_ERROR
                "bml_add_mod requires an MSVC-compatible C++ ABI because the legacy BML native interface crosses the DLL boundary; use MSVC or clang-cl")
    endif ()
    if (NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR
                "bml_add_mod requires a 32-bit target because Ballance Player is a 32-bit process; configure an x86/Win32 build")
    endif ()
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

    add_library("${TARGET_NAME}" SHARED ${ARGN})
    target_link_libraries("${TARGET_NAME}" PRIVATE BML::BML)
    target_compile_features("${TARGET_NAME}" PRIVATE cxx_std_20)
    target_link_options("${TARGET_NAME}" PRIVATE
            "/EXPORT:BMLEntry"
            "/EXPORT:BMLExit"
    )
    set_target_properties("${TARGET_NAME}" PROPERTIES
            CXX_EXTENSIONS OFF
            PREFIX ""
            SUFFIX ".bmodp"
            FOLDER "Mods"
    )
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

    install(TARGETS "${TARGET_NAME}" RUNTIME DESTINATION Mods)
endfunction()

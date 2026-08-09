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
endfunction()

function(bml_install_mod TARGET_NAME)
    if (NOT TARGET "${TARGET_NAME}")
        message(FATAL_ERROR "bml_install_mod target does not exist: ${TARGET_NAME}")
    endif ()

    install(TARGETS "${TARGET_NAME}" RUNTIME DESTINATION Mods)
endfunction()

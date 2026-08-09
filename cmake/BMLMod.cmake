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

    add_library("${TARGET_NAME}" SHARED ${ARGN})
    target_link_libraries("${TARGET_NAME}" PRIVATE BML::BML)
    target_compile_features("${TARGET_NAME}" PRIVATE cxx_std_20)
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

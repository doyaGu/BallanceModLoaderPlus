include_guard(GLOBAL)

include(CMakeParseArguments)

# Add one supported Ballance Mod. The Mod directory only declares its sources,
# optional private libraries, and runtime resources; the workspace owns the
# BML+ ABI, installation, packaging, and IDE layout rules.
function(ballance_add_mod TARGET_NAME)
    set(options)
    set(one_value_args)
    set(multi_value_args SOURCES LIBRARIES RESOURCES)
    cmake_parse_arguments(BALLANCE_MOD
            "${options}"
            "${one_value_args}"
            "${multi_value_args}"
            ${ARGN})

    if(BALLANCE_MOD_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
                "ballance_add_mod(${TARGET_NAME}) received unknown arguments: "
                "${BALLANCE_MOD_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT BALLANCE_MOD_SOURCES)
        message(FATAL_ERROR
                "ballance_add_mod(${TARGET_NAME}) requires SOURCES")
    endif()

    foreach(SOURCE IN LISTS BALLANCE_MOD_SOURCES)
        if(IS_ABSOLUTE "${SOURCE}")
            set(SOURCE_PATH "${SOURCE}")
        else()
            set(SOURCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE}")
        endif()
        if(NOT EXISTS "${SOURCE_PATH}")
            message(FATAL_ERROR
                    "${TARGET_NAME} declares a missing source: ${SOURCE_PATH}")
        endif()
    endforeach()

    bml_add_mod(${TARGET_NAME} ${BALLANCE_MOD_SOURCES})

    if(BALLANCE_MOD_LIBRARIES)
        target_link_libraries(${TARGET_NAME} PRIVATE ${BALLANCE_MOD_LIBRARIES})
    endif()

    if(NOT BALLANCE_MOD_RESOURCES)
        bml_install_mod(${TARGET_NAME})
        return()
    endif()

    set(PACKAGE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${TARGET_NAME}-package")
    set(PACKAGE_FILE "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${TARGET_NAME}.zip")
    set(PACKAGE_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${PACKAGE_DIRECTORY}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PACKAGE_DIRECTORY}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:${TARGET_NAME}>" "${PACKAGE_DIRECTORY}")
    set(PACKAGE_ENTRIES "$<TARGET_FILE_NAME:${TARGET_NAME}>")

    foreach(RESOURCE IN LISTS BALLANCE_MOD_RESOURCES)
        if(IS_ABSOLUTE "${RESOURCE}")
            set(RESOURCE_PATH "${RESOURCE}")
        else()
            set(RESOURCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${RESOURCE}")
        endif()
        if(NOT IS_DIRECTORY "${RESOURCE_PATH}")
            message(FATAL_ERROR
                    "${TARGET_NAME} declares a missing resource directory: "
                    "${RESOURCE_PATH}")
        endif()

        get_filename_component(RESOURCE_NAME "${RESOURCE_PATH}" NAME)
        list(APPEND PACKAGE_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${RESOURCE_PATH}"
                        "${PACKAGE_DIRECTORY}/${RESOURCE_NAME}")
        list(APPEND PACKAGE_ENTRIES "${RESOURCE_NAME}")
    endforeach()

    # A package target also runs when only a resource changes. The resources
    # are small, so rebuilding the archive avoids tracking directory contents
    # at configure time and removes deleted files from the package.
    add_custom_target(${TARGET_NAME}Package ALL
            ${PACKAGE_COMMANDS}
            COMMAND ${CMAKE_COMMAND} -E remove -f "${PACKAGE_FILE}"
            COMMAND ${CMAKE_COMMAND} -E chdir "${PACKAGE_DIRECTORY}"
                    ${CMAKE_COMMAND} -E tar "cf" "${PACKAGE_FILE}" --format=zip
                    ${PACKAGE_ENTRIES}
            DEPENDS ${TARGET_NAME}
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            COMMENT "Packaging ${TARGET_NAME}.zip"
            VERBATIM)
    set_property(TARGET ${TARGET_NAME}Package PROPERTY FOLDER "Mods/Packages")

    install(FILES "${PACKAGE_FILE}" DESTINATION Mods)
endfunction()

# Add a Mod directory and verify that it produces a target with the same name.
# GROUP controls IDE organization only.
function(ballance_add_mod_directory TARGET_NAME GROUP)
    set(MOD_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${TARGET_NAME}")
    if(NOT EXISTS "${MOD_DIRECTORY}/CMakeLists.txt")
        message(FATAL_ERROR
                "Supported Mod ${TARGET_NAME} has no CMakeLists.txt")
    endif()

    add_subdirectory("${TARGET_NAME}")
    if(NOT TARGET ${TARGET_NAME})
        message(FATAL_ERROR
                "${TARGET_NAME}/CMakeLists.txt must create target ${TARGET_NAME}")
    endif()
    set_property(TARGET ${TARGET_NAME} PROPERTY FOLDER "Mods/${GROUP}")
endfunction()

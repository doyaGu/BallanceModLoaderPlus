# =============================================================================
# BMLMod.cmake - BallanceModLoaderPlus Mod Development CMake Module
# =============================================================================
#
# This module provides convenient CMake functions for building BML v0.4+ modules.
#
# Usage:
#   include(BMLMod)
#
# Functions:
#   bml_add_module(NAME ...)         - Create a v0.4+ module
#   bml_generate_manifest(...)       - Generate mod.toml manifest
#   bml_install_module(NAME ...)     - Install module to output directory
#   bml_add_module_resources(...)    - Copy additional resources
#
# =============================================================================

include_guard(GLOBAL)

# Default output directory for modules
if(NOT DEFINED BML_MODS_OUTPUT_DIR)
    set(BML_MODS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/Mods")
endif()

# =============================================================================
# Helper: Validate module ID format
# =============================================================================
function(_bml_validate_module_id MODULE_ID OUT_VALID)
    if(NOT MODULE_ID)
        set(${OUT_VALID} FALSE PARENT_SCOPE)
        return()
    endif()
    
    # Valid: alphanumeric, dots, underscores, hyphens; must start with letter
    string(REGEX MATCH "^[a-zA-Z][a-zA-Z0-9._-]*$" MATCH_RESULT "${MODULE_ID}")
    if(MATCH_RESULT)
        set(${OUT_VALID} TRUE PARENT_SCOPE)
    else()
        set(${OUT_VALID} FALSE PARENT_SCOPE)
    endif()
endfunction()

# =============================================================================
# bml_add_module - Create a v0.4+ module
# =============================================================================
#
# Synopsis:
#   bml_add_module(<name>
#       SOURCES <source1> [<source2> ...]
#       [ID <module_id>]
#       [VERSION <version>]
#       [DISPLAY_NAME <display_name>]
#       [AUTHORS <author1> ...]
#       [DESCRIPTION <description>]
#       [OUTPUT_NAME <output_name>]
#       [INCLUDE_DIRS <dir1> ...]
#       [LINK_LIBRARIES <lib1> ...]
#       [COMPILE_DEFINITIONS <def1> ...]
#       [CXX_STANDARD <standard>]
#       [MANIFEST <manifest_file>]
#       [GENERATE_MANIFEST]
#       [DEPENDENCIES <dep1> ...]
#       [CAPABILITIES <cap1> ...]
#   )
#
# Description:
#   Creates a BML v0.4+ module with dynamic API loading.
#   Can optionally generate or copy a mod.toml manifest file.
#
# Arguments:
#   <name>              - Target name for the module
#   SOURCES             - List of source files (required)
#   ID                  - Module ID, e.g. "com.example.mymod" (default: <name>)
#   VERSION             - Semantic version (default: 1.0.0)
#   DISPLAY_NAME        - Human-readable name (default: <name>)
#   AUTHORS             - List of authors
#   DESCRIPTION         - Module description
#   OUTPUT_NAME         - Output DLL name (default: <name>)
#   INCLUDE_DIRS        - Additional include directories
#   LINK_LIBRARIES      - Additional libraries to link
#   COMPILE_DEFINITIONS - Additional compile definitions
#   CXX_STANDARD        - C++ standard version (default: 20)
#   MANIFEST            - Path to existing mod.toml to copy
#   GENERATE_MANIFEST   - Auto-generate mod.toml from arguments
#   DEPENDENCIES        - Dependencies (format: "id>=version" or "id")
#   CAPABILITIES        - Capabilities this module provides
#
# Example:
#   bml_add_module(EventLogger
#       SOURCES EventLogger.cpp
#       ID "com.example.eventlogger"
#       VERSION "1.0.0"
#       DISPLAY_NAME "Event Logger"
#       AUTHORS "BML Team"
#       DESCRIPTION "Logs BML events"
#       GENERATE_MANIFEST
#   )
#
function(bml_add_module NAME)
    cmake_parse_arguments(MOD
        "GENERATE_MANIFEST"
        "ID;VERSION;DISPLAY_NAME;DESCRIPTION;OUTPUT_NAME;CXX_STANDARD;MANIFEST"
        "SOURCES;AUTHORS;INCLUDE_DIRS;LINK_LIBRARIES;COMPILE_DEFINITIONS;DEPENDENCIES;CAPABILITIES"
        ${ARGN}
    )
    
    # Validate required arguments
    if(NOT MOD_SOURCES)
        message(FATAL_ERROR "bml_add_module: SOURCES is required")
    endif()
    
    # Set defaults
    if(NOT MOD_ID)
        set(MOD_ID "${NAME}")
    endif()
    if(NOT MOD_VERSION)
        set(MOD_VERSION "1.0.0")
    endif()
    if(NOT MOD_DISPLAY_NAME)
        set(MOD_DISPLAY_NAME "${NAME}")
    endif()
    if(NOT MOD_OUTPUT_NAME)
        set(MOD_OUTPUT_NAME "${NAME}")
    endif()
    if(NOT MOD_CXX_STANDARD)
        set(MOD_CXX_STANDARD 20)
    endif()
    if(NOT MOD_DESCRIPTION)
        set(MOD_DESCRIPTION "")
    endif()
    if(NOT MOD_AUTHORS)
        set(MOD_AUTHORS "Unknown")
    endif()
    
    # Validate module ID
    _bml_validate_module_id("${MOD_ID}" ID_VALID)
    if(NOT ID_VALID)
        message(WARNING "bml_add_module: Module ID '${MOD_ID}' may not be valid. "
                        "Use alphanumeric characters, dots, underscores, or hyphens.")
    endif()
    
    # Create shared library
    add_library(${NAME} SHARED ${MOD_SOURCES})
    
    # C++ standard
    set_target_properties(${NAME} PROPERTIES
        CXX_STANDARD ${MOD_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    
    # Output directory
    set(MOD_OUTPUT_DIR "${BML_MODS_OUTPUT_DIR}/${NAME}")
    
    set_target_properties(${NAME} PROPERTIES
        OUTPUT_NAME "${MOD_OUTPUT_NAME}"
        SUFFIX ".dll"
        FOLDER "Mods"
        RUNTIME_OUTPUT_DIRECTORY "${MOD_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${MOD_OUTPUT_DIR}"
    )
    
    # Multi-config generator support (Visual Studio)
    foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${CONFIG} CONFIG_UPPER)
        set_target_properties(${NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${MOD_OUTPUT_DIR}/${CONFIG}"
            LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${MOD_OUTPUT_DIR}/${CONFIG}"
        )
    endforeach()
    
    # Include directories (BML headers)
    if(DEFINED BML_INCLUDE_DIR)
        target_include_directories(${NAME} PRIVATE ${BML_INCLUDE_DIR})
    elseif(DEFINED BML_INCLUDE_DIRS)
        target_include_directories(${NAME} PRIVATE ${BML_INCLUDE_DIRS})
    endif()
    
    if(MOD_INCLUDE_DIRS)
        target_include_directories(${NAME} PRIVATE ${MOD_INCLUDE_DIRS})
    endif()
    
    # Link libraries (usually none needed for v0.4 modules)
    if(MOD_LINK_LIBRARIES)
        target_link_libraries(${NAME} PRIVATE ${MOD_LINK_LIBRARIES})
    endif()
    
    # Compile definitions
    target_compile_definitions(${NAME} PRIVATE
        UNICODE _UNICODE
        BML_MOD_ID="${MOD_ID}"
        BML_MOD_VERSION="${MOD_VERSION}"
    )
    if(MOD_COMPILE_DEFINITIONS)
        target_compile_definitions(${NAME} PRIVATE ${MOD_COMPILE_DEFINITIONS})
    endif()
    
    # Store metadata as target properties
    set_target_properties(${NAME} PROPERTIES
        BML_MOD_ID "${MOD_ID}"
        BML_MOD_VERSION "${MOD_VERSION}"
        BML_MOD_DISPLAY_NAME "${MOD_DISPLAY_NAME}"
        BML_MOD_OUTPUT_DIR "${MOD_OUTPUT_DIR}"
    )
    
    # Handle manifest
    if(MOD_MANIFEST)
        _bml_copy_manifest(${NAME} "${MOD_MANIFEST}" "${MOD_OUTPUT_DIR}")
    elseif(MOD_GENERATE_MANIFEST)
        bml_generate_manifest(
            TARGET ${NAME}
            ID "${MOD_ID}"
            NAME "${MOD_DISPLAY_NAME}"
            VERSION "${MOD_VERSION}"
            AUTHORS ${MOD_AUTHORS}
            DESCRIPTION "${MOD_DESCRIPTION}"
            ENTRY "${MOD_OUTPUT_NAME}.dll"
            OUTPUT_DIR "${MOD_OUTPUT_DIR}"
            DEPENDENCIES ${MOD_DEPENDENCIES}
            CAPABILITIES ${MOD_CAPABILITIES}
        )
    endif()
    
    message(STATUS "BML: Added module '${NAME}' (${MOD_ID} v${MOD_VERSION})")
endfunction()

# =============================================================================
# bml_generate_manifest - Generate mod.toml manifest file
# =============================================================================
function(bml_generate_manifest)
    cmake_parse_arguments(MANIFEST
        ""
        "TARGET;ID;NAME;VERSION;DESCRIPTION;ENTRY;OUTPUT_DIR;OUTPUT_FILE"
        "AUTHORS;DEPENDENCIES;CAPABILITIES"
        ${ARGN}
    )
    
    # Validate required
    if(NOT MANIFEST_ID)
        message(FATAL_ERROR "bml_generate_manifest: ID is required")
    endif()
    if(NOT MANIFEST_NAME)
        message(FATAL_ERROR "bml_generate_manifest: NAME is required")
    endif()
    if(NOT MANIFEST_VERSION)
        message(FATAL_ERROR "bml_generate_manifest: VERSION is required")
    endif()
    
    # Defaults
    if(NOT MANIFEST_ENTRY)
        set(MANIFEST_ENTRY "${MANIFEST_ID}.dll")
    endif()
    if(NOT MANIFEST_OUTPUT_FILE)
        set(MANIFEST_OUTPUT_FILE "mod.toml")
    endif()
    if(NOT MANIFEST_AUTHORS)
        set(MANIFEST_AUTHORS "Unknown")
    endif()
    
    # Build authors array
    set(AUTHORS_STR "")
    foreach(AUTHOR ${MANIFEST_AUTHORS})
        if(AUTHORS_STR)
            set(AUTHORS_STR "${AUTHORS_STR}, ")
        endif()
        set(AUTHORS_STR "${AUTHORS_STR}\"${AUTHOR}\"")
    endforeach()
    
    # Build manifest content
    set(CONTENT "[package]\n")
    string(APPEND CONTENT "id = \"${MANIFEST_ID}\"\n")
    string(APPEND CONTENT "name = \"${MANIFEST_NAME}\"\n")
    string(APPEND CONTENT "version = \"${MANIFEST_VERSION}\"\n")
    string(APPEND CONTENT "authors = [${AUTHORS_STR}]\n")
    if(MANIFEST_DESCRIPTION)
        string(APPEND CONTENT "description = \"${MANIFEST_DESCRIPTION}\"\n")
    endif()
    string(APPEND CONTENT "entry = \"${MANIFEST_ENTRY}\"\n")
    
    # Dependencies
    if(MANIFEST_DEPENDENCIES)
        string(APPEND CONTENT "\n[dependencies]\n")
        foreach(DEP ${MANIFEST_DEPENDENCIES})
            if(DEP MATCHES "^([^>=<~^]+)([>=<~^]+.+)$")
                string(APPEND CONTENT "\"${CMAKE_MATCH_1}\" = \"${CMAKE_MATCH_2}\"\n")
            else()
                string(APPEND CONTENT "\"${DEP}\" = \"*\"\n")
            endif()
        endforeach()
    endif()
    
    # Capabilities
    if(MANIFEST_CAPABILITIES)
        string(APPEND CONTENT "\ncapabilities = [")
        set(FIRST TRUE)
        foreach(CAP ${MANIFEST_CAPABILITIES})
            if(NOT FIRST)
                string(APPEND CONTENT ", ")
            endif()
            string(APPEND CONTENT "\"${CAP}\"")
            set(FIRST FALSE)
        endforeach()
        string(APPEND CONTENT "]\n")
    endif()
    
    # Write template
    set(TEMPLATE_FILE "${CMAKE_CURRENT_BINARY_DIR}/${MANIFEST_TARGET}_mod.toml.in")
    file(WRITE "${TEMPLATE_FILE}" "${CONTENT}")
    
    # Generate at build time
    if(MANIFEST_TARGET AND MANIFEST_OUTPUT_DIR)
        if(CMAKE_CONFIGURATION_TYPES)
            foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
                set(OUT_PATH "${MANIFEST_OUTPUT_DIR}/${CONFIG}/${MANIFEST_OUTPUT_FILE}")
                add_custom_command(TARGET ${MANIFEST_TARGET} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${MANIFEST_OUTPUT_DIR}/${CONFIG}"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TEMPLATE_FILE}" "${OUT_PATH}"
                    COMMENT "Generating ${MANIFEST_OUTPUT_FILE} for ${MANIFEST_TARGET} (${CONFIG})"
                )
            endforeach()
        else()
            set(OUT_PATH "${MANIFEST_OUTPUT_DIR}/${MANIFEST_OUTPUT_FILE}")
            add_custom_command(TARGET ${MANIFEST_TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${MANIFEST_OUTPUT_DIR}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TEMPLATE_FILE}" "${OUT_PATH}"
                COMMENT "Generating ${MANIFEST_OUTPUT_FILE} for ${MANIFEST_TARGET}"
            )
        endif()
    endif()
endfunction()

# =============================================================================
# Internal: Copy manifest file
# =============================================================================
function(_bml_copy_manifest TARGET MANIFEST_FILE OUTPUT_DIR)
    if(NOT EXISTS "${MANIFEST_FILE}")
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${MANIFEST_FILE}")
            set(MANIFEST_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${MANIFEST_FILE}")
        else()
            message(FATAL_ERROR "Manifest not found: ${MANIFEST_FILE}")
        endif()
    endif()
    
    if(CMAKE_CONFIGURATION_TYPES)
        foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
            add_custom_command(TARGET ${TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}/${CONFIG}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${MANIFEST_FILE}" "${OUTPUT_DIR}/${CONFIG}/mod.toml"
                COMMENT "Copying mod.toml for ${TARGET} (${CONFIG})"
            )
        endforeach()
    else()
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MANIFEST_FILE}" "${OUTPUT_DIR}/mod.toml"
            COMMENT "Copying mod.toml for ${TARGET}"
        )
    endif()
endfunction()

# =============================================================================
# bml_install_module - Install module to destination
# =============================================================================
#
# Synopsis:
#   bml_install_module(<target>
#       [DESTINATION <path>]
#       [COMPONENT <component>]
#   )
#
function(bml_install_module TARGET)
    cmake_parse_arguments(INSTALL "" "DESTINATION;COMPONENT" "" ${ARGN})
    
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "bml_install_module: Target '${TARGET}' does not exist")
    endif()
    
    if(NOT INSTALL_DESTINATION)
        set(INSTALL_DESTINATION "ModLoader/Mods/${TARGET}")
    endif()
    if(NOT INSTALL_COMPONENT)
        set(INSTALL_COMPONENT "mods")
    endif()
    
    # Install DLL
    install(TARGETS ${TARGET}
        RUNTIME DESTINATION "${INSTALL_DESTINATION}"
        LIBRARY DESTINATION "${INSTALL_DESTINATION}"
        COMPONENT ${INSTALL_COMPONENT}
    )
    
    # Install mod.toml
    get_target_property(MOD_OUTPUT_DIR ${TARGET} BML_MOD_OUTPUT_DIR)
    if(MOD_OUTPUT_DIR AND EXISTS "${MOD_OUTPUT_DIR}/mod.toml")
        install(FILES "${MOD_OUTPUT_DIR}/mod.toml"
            DESTINATION "${INSTALL_DESTINATION}"
            COMPONENT ${INSTALL_COMPONENT}
        )
    endif()
endfunction()

# =============================================================================
# bml_add_module_resources - Copy additional resources
# =============================================================================
#
# Synopsis:
#   bml_add_module_resources(<target>
#       [FILES <file1> ...]
#       [DIRECTORY <dir>]
#       [DESTINATION <relative_path>]
#   )
#
function(bml_add_module_resources TARGET)
    cmake_parse_arguments(RES "" "DIRECTORY;DESTINATION" "FILES" ${ARGN})
    
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "bml_add_module_resources: Target '${TARGET}' does not exist")
    endif()
    
    get_target_property(MOD_OUTPUT_DIR ${TARGET} BML_MOD_OUTPUT_DIR)
    if(NOT MOD_OUTPUT_DIR)
        get_target_property(MOD_OUTPUT_DIR ${TARGET} RUNTIME_OUTPUT_DIRECTORY)
    endif()
    
    if(RES_DESTINATION)
        set(DEST_DIR "${MOD_OUTPUT_DIR}/${RES_DESTINATION}")
    else()
        set(DEST_DIR "${MOD_OUTPUT_DIR}")
    endif()
    
    if(RES_DIRECTORY)
        if(CMAKE_CONFIGURATION_TYPES)
            foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
                add_custom_command(TARGET ${TARGET} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${RES_DIRECTORY}" "${DEST_DIR}/${CONFIG}"
                    COMMENT "Copying resources for ${TARGET} (${CONFIG})"
                )
            endforeach()
        else()
            add_custom_command(TARGET ${TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${RES_DIRECTORY}" "${DEST_DIR}"
                COMMENT "Copying resources for ${TARGET}"
            )
        endif()
    endif()
    
    if(RES_FILES)
        if(CMAKE_CONFIGURATION_TYPES)
            foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
                add_custom_command(TARGET ${TARGET} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${DEST_DIR}/${CONFIG}"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        ${RES_FILES} "${DEST_DIR}/${CONFIG}/"
                    COMMENT "Copying files for ${TARGET} (${CONFIG})"
                )
            endforeach()
        else()
            add_custom_command(TARGET ${TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${DEST_DIR}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${RES_FILES} "${DEST_DIR}/"
                COMMENT "Copying files for ${TARGET}"
            )
        endif()
    endif()
endfunction()

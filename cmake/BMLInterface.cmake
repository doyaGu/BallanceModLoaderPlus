include_guard(GLOBAL)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set(BML_INTERFACE_PACKAGE_CONFIG_TEMPLATE
        "${CMAKE_CURRENT_LIST_DIR}/BMLInterfacePackageConfig.cmake.in")

# Creates one header-only package for a provider interface. The build-tree and
# installed target are both named <packageName>::Interface. Consumers link that
# target for headers and the BML SDK; no provider binary enters their link line.
function(bml_add_interface_package packageName)
    if("${packageName}" STREQUAL "")
        message(FATAL_ERROR "bml_add_interface_package requires a package name")
    endif()
    if(NOT packageName MATCHES "^[A-Za-z][A-Za-z0-9_]*$")
        message(FATAL_ERROR
                "bml_add_interface_package: package name '${packageName}' must be a CMake identifier")
    endif()

    cmake_parse_arguments(BML_INTERFACE "" "VERSION;INCLUDE_DIRECTORY" "HEADERS" ${ARGN})
    if(BML_INTERFACE_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
                "bml_add_interface_package: unexpected arguments: ${BML_INTERFACE_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT BML_INTERFACE_VERSION)
        message(FATAL_ERROR "bml_add_interface_package(${packageName}): VERSION is required")
    endif()
    if(NOT BML_INTERFACE_VERSION MATCHES
            "^[0-9]+(\\.[0-9]+)?(\\.[0-9]+)?(\\.[0-9]+)?$")
        message(FATAL_ERROR
                "bml_add_interface_package(${packageName}): VERSION must contain one to four numeric parts")
    endif()
    if(NOT BML_INTERFACE_HEADERS)
        message(FATAL_ERROR "bml_add_interface_package(${packageName}): HEADERS is required")
    endif()
    if(NOT TARGET BML::BML)
        message(FATAL_ERROR
                "bml_add_interface_package(${packageName}) requires find_package(BML CONFIG REQUIRED) first")
    endif()
    if(NOT EXISTS "${BML_INTERFACE_PACKAGE_CONFIG_TEMPLATE}")
        message(FATAL_ERROR
                "bml_add_interface_package: missing package template: ${BML_INTERFACE_PACKAGE_CONFIG_TEMPLATE}")
    endif()

    if(BML_INTERFACE_INCLUDE_DIRECTORY)
        get_filename_component(includeDirectory "${BML_INTERFACE_INCLUDE_DIRECTORY}" ABSOLUTE
                               BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    else()
        get_filename_component(includeDirectory "${CMAKE_CURRENT_SOURCE_DIR}/include" ABSOLUTE)
    endif()
    if(NOT IS_DIRECTORY "${includeDirectory}")
        message(FATAL_ERROR
                "bml_add_interface_package(${packageName}): INCLUDE_DIRECTORY does not exist: ${includeDirectory}")
    endif()

    set(localTarget "${packageName}_interface")
    set(exportSet "${packageName}Targets")
    if(TARGET "${localTarget}" OR TARGET "${packageName}::Interface")
        message(FATAL_ERROR
                "bml_add_interface_package(${packageName}): target already exists")
    endif()

    foreach(header IN LISTS BML_INTERFACE_HEADERS)
        get_filename_component(headerPath "${header}" ABSOLUTE
                               BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        if(NOT EXISTS "${headerPath}" OR IS_DIRECTORY "${headerPath}")
            message(FATAL_ERROR
                    "bml_add_interface_package(${packageName}): header does not exist: ${headerPath}")
        endif()
        file(RELATIVE_PATH headerRelative "${includeDirectory}" "${headerPath}")
        if(headerRelative STREQUAL ".." OR headerRelative MATCHES "^\\.\\./")
            message(FATAL_ERROR
                    "bml_add_interface_package(${packageName}): header must be inside "
                    "INCLUDE_DIRECTORY: ${headerPath}")
        endif()
        get_filename_component(headerDirectory "${headerRelative}" DIRECTORY)
        if(headerDirectory STREQUAL "")
            install(FILES "${headerPath}" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
        else()
            install(FILES "${headerPath}"
                    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${headerDirectory}")
        endif()
    endforeach()

    add_library("${localTarget}" INTERFACE)
    add_library("${packageName}::Interface" ALIAS "${localTarget}")
    target_include_directories("${localTarget}" INTERFACE
            $<BUILD_INTERFACE:${includeDirectory}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    target_link_libraries("${localTarget}" INTERFACE BML::BML)
    target_compile_features("${localTarget}" INTERFACE cxx_std_20)
    set_target_properties("${localTarget}" PROPERTIES EXPORT_NAME Interface)

    install(TARGETS "${localTarget}" EXPORT "${exportSet}")
    set(packageDirectory "${CMAKE_INSTALL_LIBDIR}/cmake/${packageName}")
    install(EXPORT "${exportSet}"
            FILE "${packageName}Targets.cmake"
            NAMESPACE "${packageName}::"
            DESTINATION "${packageDirectory}")

    set(configDirectory
            "${CMAKE_CURRENT_BINARY_DIR}/bml-interface-packages/${packageName}")
    file(MAKE_DIRECTORY "${configDirectory}")
    set(BML_INTERFACE_PACKAGE_NAME "${packageName}")
    configure_package_config_file(
            "${BML_INTERFACE_PACKAGE_CONFIG_TEMPLATE}"
            "${configDirectory}/${packageName}Config.cmake"
            INSTALL_DESTINATION "${packageDirectory}")
    write_basic_package_version_file(
            "${configDirectory}/${packageName}ConfigVersion.cmake"
            VERSION "${BML_INTERFACE_VERSION}"
            COMPATIBILITY SameMajorVersion)
    install(FILES
            "${configDirectory}/${packageName}Config.cmake"
            "${configDirectory}/${packageName}ConfigVersion.cmake"
            DESTINATION "${packageDirectory}")
endfunction()

# Generates a plain-C interface header from a short *.bml-interface definition,
# then exposes it through the same standalone package as a hand-written header.
# Generation happens while configuring so both the provider compiler and CMake's
# install rules see a concrete file. The adjacent lock protects member order.
function(bml_add_generated_interface_package packageName)
    cmake_parse_arguments(BML_GENERATED_INTERFACE ""
            "VERSION;INPUT;PROVIDER_ID;PROVIDER_VERSION;NAMESPACE;OUTPUT_NAME" "" ${ARGN})
    if(BML_GENERATED_INTERFACE_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
                "bml_add_generated_interface_package: unexpected arguments: "
                "${BML_GENERATED_INTERFACE_UNPARSED_ARGUMENTS}")
    endif()
    foreach(required_argument VERSION INPUT PROVIDER_ID PROVIDER_VERSION NAMESPACE)
        if(NOT BML_GENERATED_INTERFACE_${required_argument})
            message(FATAL_ERROR
                    "bml_add_generated_interface_package(${packageName}): "
                    "${required_argument} is required")
        endif()
    endforeach()
    if(NOT BML_INTERFACE_CODEGEN OR NOT EXISTS "${BML_INTERFACE_CODEGEN}")
        message(FATAL_ERROR
                "bml_add_generated_interface_package: BML_INTERFACE_CODEGEN "
                "does not name interface_codegen.py")
    endif()

    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
    get_filename_component(input "${BML_GENERATED_INTERFACE_INPUT}" ABSOLUTE
                           BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT EXISTS "${input}")
        message(FATAL_ERROR
                "bml_add_generated_interface_package(${packageName}): "
                "INPUT does not exist: ${input}")
    endif()
    if(NOT input MATCHES "\\.bml-interface$")
        message(FATAL_ERROR
                "bml_add_generated_interface_package(${packageName}): "
                "INPUT must use the .bml-interface extension")
    endif()
    set(interface_lock "${input}.lock")
    if(NOT EXISTS "${interface_lock}")
        message(FATAL_ERROR
                "bml_add_generated_interface_package(${packageName}): missing "
                "${interface_lock}; run 'bml interface update'")
    endif()

    if(BML_GENERATED_INTERFACE_OUTPUT_NAME)
        set(output_name "${BML_GENERATED_INTERFACE_OUTPUT_NAME}")
    else()
        set(output_name "Interface.h")
    endif()
    if(output_name MATCHES "[/\\\\]" OR NOT output_name MATCHES "\\.h$")
        message(FATAL_ERROR
                "bml_add_generated_interface_package(${packageName}): "
                "OUTPUT_NAME must be one .h filename")
    endif()
    if(NOT BML_GENERATED_INTERFACE_NAMESPACE MATCHES "^[A-Za-z][A-Za-z0-9]*$")
        message(FATAL_ERROR
                "bml_add_generated_interface_package(${packageName}): "
                "NAMESPACE must begin with a letter and contain only letters and digits")
    endif()

    set(include_directory
            "${CMAKE_CURRENT_BINARY_DIR}/bml-interfaces/${packageName}/include")
    set(output_directory
            "${include_directory}/${BML_GENERATED_INTERFACE_NAMESPACE}")
    set(output "${output_directory}/${output_name}")
    file(MAKE_DIRECTORY "${output_directory}")
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${BML_INTERFACE_CODEGEN}"
                --input "${input}"
                --output "${output}"
                --provider-id "${BML_GENERATED_INTERFACE_PROVIDER_ID}"
                --provider-version "${BML_GENERATED_INTERFACE_PROVIDER_VERSION}"
                --namespace "${BML_GENERATED_INTERFACE_NAMESPACE}"
        RESULT_VARIABLE generation_status
        OUTPUT_VARIABLE generation_output
        ERROR_VARIABLE generation_error
    )
    if(NOT generation_status EQUAL 0)
        message(FATAL_ERROR
                "Could not generate ${packageName} interface.\n"
                "${generation_output}${generation_error}")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${input}" "${interface_lock}" "${BML_INTERFACE_CODEGEN}")

    bml_add_interface_package("${packageName}"
            VERSION "${BML_GENERATED_INTERFACE_VERSION}"
            INCLUDE_DIRECTORY "${include_directory}"
            HEADERS "${output}")
endfunction()

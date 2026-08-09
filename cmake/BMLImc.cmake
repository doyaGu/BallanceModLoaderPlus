include_guard(GLOBAL)

function(bml_target_imc_api target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "bml_target_imc_api: target '${target}' does not exist")
    endif()

    cmake_parse_arguments(IMC "" "INPUT;API_ID;OUTPUT_DIR" "" ${ARGN})
    if(IMC_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
                "bml_target_imc_api: unexpected arguments: ${IMC_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT IMC_INPUT)
        message(FATAL_ERROR "bml_target_imc_api: INPUT is required")
    endif()
    if(NOT BML_IMC_CODEGEN OR NOT EXISTS "${BML_IMC_CODEGEN}")
        message(FATAL_ERROR
                "bml_target_imc_api: BML_IMC_CODEGEN does not name imc_codegen.py")
    endif()

    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)

    get_filename_component(input "${IMC_INPUT}" ABSOLUTE
                           BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT EXISTS "${input}")
        message(FATAL_ERROR "bml_target_imc_api: INPUT does not exist: ${input}")
    endif()
    get_filename_component(input_name "${input}" NAME)
    if(NOT input_name MATCHES "\\.imc$")
        message(FATAL_ERROR
                "bml_target_imc_api: INPUT must use the .imc extension: ${input}")
    endif()
    set(interface_lock "${input}.lock")

    if(IMC_OUTPUT_DIR)
        get_filename_component(output_dir "${IMC_OUTPUT_DIR}" ABSOLUTE
                               BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    else()
        set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/bml-imc")
    endif()

    if(DEFINED IMC_API_ID AND NOT "${IMC_API_ID}" STREQUAL "")
        set(expected_api_id "${IMC_API_ID}")
        set(api_id_source "API_ID '${IMC_API_ID}'")
    else()
        string(REGEX REPLACE "\\.imc$" "" expected_api_id "${input_name}")
        set(api_id_source "API ID '${expected_api_id}' derived from '${input_name}'")
    endif()
    set(api_id_pattern
        "^[a-z0-9]+(\\.[a-z0-9]+)*$")
    if(NOT expected_api_id MATCHES "${api_id_pattern}")
        message(FATAL_ERROR
                "bml_target_imc_api: invalid ${api_id_source}; expected non-empty "
                "dot-separated segments containing only lowercase ASCII letters and digits")
    endif()
    set(stem "${expected_api_id}")
    string(REPLACE "." "_" stem "${stem}")
    set(output "${output_dir}/${stem}_imc.hpp")
    set(update_lock_target "bml_update_imc_locks")
    set(update_lock_command
            "cmake --build \"${CMAKE_BINARY_DIR}\" --target ${update_lock_target}")
    set(arguments
            --out-dir "${output_dir}"
            --input "${input}"
            --expected-api-id "${expected_api_id}"
            --update-lock-command "${update_lock_command}"
    )
    set(dependencies "${BML_IMC_CODEGEN}" "${input}")
    if(EXISTS "${interface_lock}")
        list(APPEND dependencies "${interface_lock}")
    endif()

    get_property(update_target_initialized GLOBAL
                 PROPERTY BML_IMC_UPDATE_TARGET_INITIALIZED)
    if(NOT update_target_initialized)
        if(TARGET "${update_lock_target}")
            message(FATAL_ERROR
                    "bml_target_imc_api: target '${update_lock_target}' is reserved")
        endif()
        add_custom_target("${update_lock_target}")
        set_property(TARGET "${update_lock_target}" PROPERTY FOLDER "BML/IMC")
        set_property(GLOBAL PROPERTY BML_IMC_UPDATE_TARGET_INITIALIZED TRUE)
    endif()

    string(SHA256 update_step_key
           "${target}|${input}|${expected_api_id}|${output_dir}")
    string(SUBSTRING "${update_step_key}" 0 16 update_step_key)
    set(update_step_target "bml_update_imc_lock_${update_step_key}")
    if(TARGET "${update_step_target}")
        message(FATAL_ERROR
                "bml_target_imc_api: duplicate IMC interface registration for ${input}")
    endif()
    add_custom_target("${update_step_target}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_dir}"
            COMMAND "${Python3_EXECUTABLE}" "${BML_IMC_CODEGEN}"
                    ${arguments} --update-lock
            DEPENDS "${BML_IMC_CODEGEN}" "${input}"
            COMMENT "Updating IMC interface lock ${input_name}"
            VERBATIM
    )
    set_property(TARGET "${update_step_target}" PROPERTY FOLDER "BML/IMC")
    add_dependencies("${update_lock_target}" "${update_step_target}")

    add_custom_command(
            OUTPUT "${output}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_dir}"
            COMMAND "${Python3_EXECUTABLE}" "${BML_IMC_CODEGEN}" ${arguments}
            DEPENDS ${dependencies}
            COMMENT "Generating typed IMC binding ${stem}_imc.hpp"
            VERBATIM
    )
    set_source_files_properties("${output}" PROPERTIES GENERATED TRUE)
    target_sources("${target}" PRIVATE "${output}")
    target_include_directories("${target}" PRIVATE "${output_dir}")
    target_compile_features("${target}" PRIVATE cxx_std_20)
    set_property(TARGET "${target}" APPEND PROPERTY
                 BML_IMC_GENERATED_HEADERS "${output}")
endfunction()

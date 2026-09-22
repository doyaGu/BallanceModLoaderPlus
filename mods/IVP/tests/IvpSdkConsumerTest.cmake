foreach(required IN ITEMS CMAKE_EXECUTABLE SOURCE_ROOT MAIN_BUILD_DIR WORK_ROOT
                          GENERATOR CONFIG DUMPBIN)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "IVP SDK consumer test needs ${required}.")
    endif()
endforeach()
if(NOT EXISTS "${DUMPBIN}")
    message(FATAL_ERROR "IVP SDK consumer test needs dumpbin.exe.")
endif()

file(REMOVE_RECURSE "${WORK_ROOT}")
file(MAKE_DIRECTORY "${WORK_ROOT}")
set(install_root "${WORK_ROOT}/install")
set(consumer_build "${WORK_ROOT}/build")

execute_process(
        COMMAND "${CMAKE_EXECUTABLE}" --install "${MAIN_BUILD_DIR}"
                --config "${CONFIG}" --prefix "${install_root}"
        RESULT_VARIABLE install_status
        OUTPUT_VARIABLE install_output
        ERROR_VARIABLE install_error)
if(NOT install_status EQUAL 0)
    message(FATAL_ERROR
            "Installing IVP SDK failed.\n${install_output}\n${install_error}")
endif()

# A standalone IVP build uses its installed BML SDK. An in-tree build stages
# BML and IVP together, so the consumer must use BML from that same install.
if(NOT BML_DIR)
    set(BML_DIR "${install_root}/lib/cmake/BML")
endif()
if(NOT EXISTS "${BML_DIR}/BMLConfig.cmake")
    message(FATAL_ERROR "Installed BML SDK not found: ${BML_DIR}")
endif()

set(configure_command
        "${CMAKE_EXECUTABLE}"
        -S "${SOURCE_ROOT}/tests/sdk-consumer"
        -B "${consumer_build}"
        -G "${GENERATOR}"
        "-DBML_DIR=${BML_DIR}"
        "-DIVP_DIR=${install_root}/lib/cmake/IVP")
if(DEFINED GENERATOR_PLATFORM AND NOT "${GENERATOR_PLATFORM}" STREQUAL "")
    list(APPEND configure_command -A "${GENERATOR_PLATFORM}")
endif()
execute_process(
        COMMAND ${configure_command}
        RESULT_VARIABLE configure_status
        OUTPUT_VARIABLE configure_output
        ERROR_VARIABLE configure_error)
if(NOT configure_status EQUAL 0)
    message(FATAL_ERROR
            "Configuring IVP SDK consumer failed.\n"
            "${configure_output}\n${configure_error}")
endif()

execute_process(
        COMMAND "${CMAKE_EXECUTABLE}" --build "${consumer_build}"
                --config "${CONFIG}"
        RESULT_VARIABLE build_status
        OUTPUT_VARIABLE build_output
        ERROR_VARIABLE build_error)
if(NOT build_status EQUAL 0)
    message(FATAL_ERROR
            "Building IVP SDK consumer failed.\n${build_output}\n${build_error}")
endif()

file(GLOB_RECURSE consumer_binaries
        "${consumer_build}/IvpSdkConsumer.bmodp")
list(LENGTH consumer_binaries binary_count)
if(NOT binary_count EQUAL 1)
    message(FATAL_ERROR
            "Expected one IvpSdkConsumer.bmodp, found ${binary_count}.")
endif()
list(GET consumer_binaries 0 consumer_binary)

execute_process(
        COMMAND "${DUMPBIN}" /imports "${consumer_binary}"
        RESULT_VARIABLE dumpbin_status
        OUTPUT_VARIABLE imports
        ERROR_VARIABLE dumpbin_error)
if(NOT dumpbin_status EQUAL 0)
    message(FATAL_ERROR "dumpbin /imports failed: ${dumpbin_error}")
endif()

string(FIND "${imports}" "BMLPlus.dll" bml_import)
if(bml_import EQUAL -1)
    message(FATAL_ERROR "Installed IVP SDK consumer does not import BMLPlus.dll.")
endif()
foreach(forbidden IN ITEMS
        IVP.bmodp
        IVP_GetInterface
        BML_IVP_GetInterface
        IVP_QueryInterface)
    string(FIND "${imports}" "${forbidden}" forbidden_import)
    if(NOT forbidden_import EQUAL -1)
        message(FATAL_ERROR
                "Installed IVP SDK consumer has forbidden import: ${forbidden}")
    endif()
endforeach()

message(STATUS
        "Verified installed IVP::API consumer imports BMLPlus.dll and no IVP API symbol.")

include(CTest)
enable_testing()
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)

include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        # v1.16.0, pinned so a moved tag cannot change release CI inputs.
        GIT_TAG 6910c9d9165801d8827d628cb72eb7ea9dd538c5
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

function(add_bml_test test_name)
    cmake_parse_arguments(TEST "" "" "SOURCES;DEPENDENCIES" ${ARGN})

    add_executable(${test_name} ${TEST_SOURCES})

    target_include_directories(${test_name} PRIVATE
            "${BML_INCLUDE_DIR}"
            "${BML_SOURCE_DIR}"
            "${BML_SOURCE_DIR}/Utils"
    )
    if (BML_WITH_ANGELSCRIPT)
        target_include_directories(${test_name} PRIVATE
                "${BML_SOURCE_DIR}/AngelScript"
                "${IMGUI_SOURCE_DIR}"
                "${VIRTOOLS_SDK_INCLUDE_DIR}"
                "${CKANGELSCRIPT_INCLUDE_DIR}"
                "${CKANGELSCRIPT_ANGELSCRIPT_INCLUDE_DIR}"
        )
    endif ()

    target_link_libraries(${test_name} PRIVATE
            gtest_main
            gmock
            ${TEST_DEPENDENCIES}
    )

    target_compile_definitions(${test_name} PRIVATE
            "BML_EXPORTS"
            "BML_TEST"
            "IMGUI_USER_CONFIG=\"BMLImGuiConfig.h\""
            "BML_ENABLE_ANGELSCRIPT=$<BOOL:${BML_WITH_ANGELSCRIPT}>"
    )
    target_compile_options(${test_name} PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/Zc:strictStrings->
    )

    set_target_properties(${test_name} PROPERTIES FOLDER "Tests")

    add_test(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)
endfunction()

set(BML_IMC_SAMPLE_INPUT "${PROJECT_SOURCE_DIR}/tests/codegen/imc/test.sample.imc")
set(BML_IMC_SAMPLE_GENERATED_DIR "${PROJECT_SOURCE_DIR}/tests/codegen/imc/generated")

if (MSVC)
    get_filename_component(BML_TOOL_DIRECTORY "${CMAKE_LINKER}" DIRECTORY)
    set(BML_DUMPBIN "${BML_TOOL_DIRECTORY}/dumpbin.exe")
endif ()

add_subdirectory(abi)
add_subdirectory(codegen)
add_subdirectory(sdk)
add_subdirectory(unit)
if (BML_BUILD_UI_AUTOMATION)
    add_subdirectory(ui)
endif ()

add_subdirectory(smoke)
add_subdirectory(player)
if (BML_BUILD_UPDATER)
    add_subdirectory(updater)
endif ()

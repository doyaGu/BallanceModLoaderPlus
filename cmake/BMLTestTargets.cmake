# cmake/BMLTestTargets.cmake
# Registers custom targets for the unified bml_test.py tool.
# Include this from the root CMakeLists.txt.

find_package(Python3 COMPONENTS Interpreter QUIET)
if(NOT Python3_FOUND)
    message(STATUS "Python3 not found; bml-test targets disabled.")
    return()
endif()

if(Python3_VERSION VERSION_LESS "3.11")
    message(STATUS "Python ${Python3_VERSION} found but bml-test requires 3.11+; targets disabled.")
    return()
endif()

set(BML_TEST_SCRIPT "${CMAKE_SOURCE_DIR}/tools/bml_test.py")
set(BML_GAME_DIR "$ENV{BML_GAME_DIR}" CACHE PATH "Ballance game installation directory")
set(BML_TEST_CONFIG "Release" CACHE STRING "Build configuration for test targets")
set(BML_TEST_TIMEOUT "300" CACHE STRING "Default timeout in seconds for run targets")

# --- Core targets ---

add_custom_target(bml-run
    COMMAND ${Python3_EXECUTABLE} ${BML_TEST_SCRIPT} run
        --all-builtin --build
        --game-dir "${BML_GAME_DIR}"
        --build-dir "${CMAKE_BINARY_DIR}"
        --config "${BML_TEST_CONFIG}"
        --timeout "${BML_TEST_TIMEOUT}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Deploy all builtin modules and launch Ballance"
)

add_custom_target(bml-deploy
    COMMAND ${Python3_EXECUTABLE} ${BML_TEST_SCRIPT} run
        --all-builtin --build --no-launch
        --game-dir "${BML_GAME_DIR}"
        --build-dir "${CMAKE_BINARY_DIR}"
        --config "${BML_TEST_CONFIG}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Deploy all builtin modules without launching"
)

add_custom_target(bml-smoke
    COMMAND ${Python3_EXECUTABLE} ${BML_TEST_SCRIPT} smoke
        --build
        --game-dir "${BML_GAME_DIR}"
        --build-dir "${CMAKE_BINARY_DIR}"
        --config "${BML_TEST_CONFIG}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Run smoke tests with auto-generated module cases"
)

add_custom_target(bml-analyze
    COMMAND ${Python3_EXECUTABLE} ${BML_TEST_SCRIPT} analyze
        --all-builtin --build
        --game-dir "${BML_GAME_DIR}"
        --build-dir "${CMAKE_BINARY_DIR}"
        --config "${BML_TEST_CONFIG}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Run per-module isolation analysis"
)

# --- Per-module run targets ---

file(GLOB _bml_module_tomls "${CMAKE_SOURCE_DIR}/modules/*/mod.toml")
foreach(_toml IN LISTS _bml_module_tomls)
    get_filename_component(_mod_dir "${_toml}" DIRECTORY)
    get_filename_component(_mod_name "${_mod_dir}" NAME)
    add_custom_target(bml-run-${_mod_name}
        COMMAND ${Python3_EXECUTABLE} ${BML_TEST_SCRIPT} run
            --modules "${_mod_name}" --build
            --game-dir "${BML_GAME_DIR}"
            --build-dir "${CMAKE_BINARY_DIR}"
            --config "${BML_TEST_CONFIG}"
            --timeout "${BML_TEST_TIMEOUT}"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Deploy ${_mod_name} (+ deps) and launch Ballance"
    )
endforeach()

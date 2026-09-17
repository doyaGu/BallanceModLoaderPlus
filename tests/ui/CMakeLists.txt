set(BML_UI_TEST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(BML_UI_TEST_BINARY_ROOT "${CMAKE_CURRENT_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BML_UI_TEST_BINARY_ROOT}")
set(BML_BALLANCE_ROOT "$ENV{BML_BALLANCE_ROOT}" CACHE PATH
        "Optional Ballance installation used by real Player UI automation")

add_subdirectory(headless)
add_subdirectory(session)
add_subdirectory(runner)

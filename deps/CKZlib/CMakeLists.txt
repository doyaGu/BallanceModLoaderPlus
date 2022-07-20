cmake_minimum_required(VERSION 3.12)

add_library(CKZlib INTERFACE)
target_include_directories(CKZlib INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(CKZlib INTERFACE ${VIRTOOLS_SDK_PATH}/Lib/CKZlib.lib)
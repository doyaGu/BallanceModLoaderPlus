cmake_minimum_required(VERSION 3.14)

project(HelloMod VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(BML REQUIRED)

add_library(HelloMod SHARED src/HelloMod.cpp)

target_include_directories(HelloMod PRIVATE ${BML_INCLUDE_DIRS})
target_link_libraries(HelloMod PRIVATE BML)

set_target_properties(HelloMod PROPERTIES
    OUTPUT_NAME "HelloMod"
)


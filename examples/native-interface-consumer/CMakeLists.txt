cmake_minimum_required(VERSION 3.15)
project(BMLExampleValueConsumer VERSION 1.0.0 LANGUAGES CXX)

find_package(BML CONFIG REQUIRED)
find_package(BMLExampleValue 1 CONFIG REQUIRED)

bml_add_mod(BMLExampleValueConsumer
        src/ValueConsumer.cpp
)
target_link_libraries(BMLExampleValueConsumer PRIVATE
        BMLExampleValue::Interface
)
bml_install_mod(BMLExampleValueConsumer)

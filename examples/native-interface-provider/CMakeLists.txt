cmake_minimum_required(VERSION 3.15)
project(BMLExampleValueProvider VERSION 1.0.0 LANGUAGES CXX)

find_package(BML CONFIG REQUIRED)

# Produces the build-tree target BMLExampleValue::Interface and installs a
# standalone find_package(BMLExampleValue) package for consumers.
bml_add_interface_package(BMLExampleValue
        VERSION ${PROJECT_VERSION}
        HEADERS include/BMLExample/ValueInterface.h
)

bml_add_mod(BMLExampleValueProvider
        src/ValueProvider.cpp
)
target_link_libraries(BMLExampleValueProvider PRIVATE
        BMLExampleValue::Interface
)
bml_install_mod(BMLExampleValueProvider)

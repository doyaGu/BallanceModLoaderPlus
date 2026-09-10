cmake_minimum_required(VERSION 3.15)

project(HelloMod VERSION 1.0.0 LANGUAGES CXX)

find_package(BML CONFIG REQUIRED)

bml_add_generated_interface_package(HelloModInterface
        VERSION ${PROJECT_VERSION}
        INPUT api/value.bml-interface
        PROVIDER_ID "__MOD_ID__"
        PROVIDER_VERSION "${PROJECT_VERSION}"
        NAMESPACE HelloMod
        OUTPUT_NAME ValueInterface.h
)

bml_add_mod(HelloMod src/HelloMod.cpp)
target_link_libraries(HelloMod PRIVATE HelloModInterface::Interface)
bml_install_mod(HelloMod)

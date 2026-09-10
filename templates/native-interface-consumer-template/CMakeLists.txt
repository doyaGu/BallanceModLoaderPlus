cmake_minimum_required(VERSION 3.15)

project(HelloMod VERSION 1.0.0 LANGUAGES CXX)

find_package(BML CONFIG REQUIRED)
find_package(__PROVIDER_PACKAGE__ 1 CONFIG REQUIRED)

bml_add_mod(HelloMod src/HelloMod.cpp)
target_link_libraries(HelloMod PRIVATE __PROVIDER_PACKAGE__::Interface)
bml_install_mod(HelloMod)

cmake_minimum_required(VERSION 3.15)
project(IvpSdkConsumer LANGUAGES CXX)

find_package(BML CONFIG REQUIRED)
find_package(IVP CONFIG REQUIRED)

bml_add_mod(IvpSdkConsumer Mod.cpp)
target_link_libraries(IvpSdkConsumer PRIVATE IVP::API)

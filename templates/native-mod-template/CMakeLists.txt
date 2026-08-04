cmake_minimum_required(VERSION 3.14)

project(HelloMod VERSION 1.0.0 LANGUAGES CXX)

find_package(BML CONFIG REQUIRED)

bml_add_mod(HelloMod src/HelloMod.cpp)


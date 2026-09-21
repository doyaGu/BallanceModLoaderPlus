# Build the public IMC headers as strict C. This object target verifies source
# compatibility without introducing a C++ link boundary.
add_library(ImcCAbiCompile OBJECT ImcCAbiCompile.c)
target_include_directories(ImcCAbiCompile PRIVATE "${BML_INCLUDE_DIR}")
set_target_properties(ImcCAbiCompile PROPERTIES FOLDER "Tests")

add_library(BmlCAbiCompile OBJECT BmlCAbiCompile.c)
target_include_directories(BmlCAbiCompile PRIVATE "${BML_INCLUDE_DIR}")
set_target_properties(BmlCAbiCompile PROPERTIES FOLDER "Tests")

add_library(CommandCppAbiCompile OBJECT CommandCppAbiCompile.cpp)
target_include_directories(CommandCppAbiCompile PRIVATE "${BML_INCLUDE_DIR}")
set_target_properties(CommandCppAbiCompile PROPERTIES FOLDER "Tests")

add_library(BehaviorCAbiCompile OBJECT BehaviorCAbiCompile.c)
target_include_directories(BehaviorCAbiCompile PRIVATE "${BML_INCLUDE_DIR}")
set_target_properties(BehaviorCAbiCompile PROPERTIES FOLDER "Tests")

# This target deliberately does not use add_bml_test(): its ImGui configuration
# must arrive through BML's public usage requirements, exactly as it does for a
# Native Mod built against the build-tree package.
add_library(ImGuiAbiConsumerCompile OBJECT ImGuiAbiConsumerCompile.cpp)
target_link_libraries(ImGuiAbiConsumerCompile PRIVATE BML)
set_target_properties(ImGuiAbiConsumerCompile PROPERTIES FOLDER "Tests")

# Every public C++ Behavior domain header must compile on its own. The umbrella
# remains the convenient include, not a hidden ordering requirement.
set(BML_BEHAVIOR_HEADER_COMPILE_SOURCES)
foreach(BML_BEHAVIOR_HEADER IN LISTS BML_BEHAVIOR_CPP_PUBLIC_RELATIVE_HEADERS)
    string(REPLACE "/" "_" BML_BEHAVIOR_HEADER_STEM
            "${BML_BEHAVIOR_HEADER}")
    set(BML_BEHAVIOR_HEADER_SOURCE
            "${CMAKE_CURRENT_BINARY_DIR}/behavior-public-headers/${BML_BEHAVIOR_HEADER_STEM}.cpp")
    file(WRITE "${BML_BEHAVIOR_HEADER_SOURCE}"
            "#include <${BML_BEHAVIOR_HEADER}>\n")
    list(APPEND BML_BEHAVIOR_HEADER_COMPILE_SOURCES
            "${BML_BEHAVIOR_HEADER_SOURCE}")
endforeach()
add_library(BehaviorHeadersCompile OBJECT
        ${BML_BEHAVIOR_HEADER_COMPILE_SOURCES})
target_include_directories(BehaviorHeadersCompile PRIVATE "${BML_INCLUDE_DIR}")
target_link_libraries(BehaviorHeadersCompile PRIVATE
        VirtoolsSDK::CK2 VirtoolsSDK::VxMath)
target_compile_definitions(BehaviorHeadersCompile PRIVATE "BML_EXPORTS")
set_target_properties(BehaviorHeadersCompile PROPERTIES FOLDER "Tests")
set_property(TARGET BehaviorHeadersCompile PROPERTY CXX_STANDARD 17)

# Every named Block adapter must compile through its own installed include.
set(BML_BEHAVIOR_BLOCK_HEADER_COMPILE_SOURCES)
foreach(BML_BEHAVIOR_BLOCK_HEADER
        IN LISTS BML_BEHAVIOR_BLOCK_PUBLIC_RELATIVE_HEADERS)
    string(REPLACE "/" "_" BML_BEHAVIOR_BLOCK_HEADER_STEM
            "${BML_BEHAVIOR_BLOCK_HEADER}")
    set(BML_BEHAVIOR_BLOCK_HEADER_SOURCE
            "${CMAKE_CURRENT_BINARY_DIR}/behavior-block-public-headers/${BML_BEHAVIOR_BLOCK_HEADER_STEM}.cpp")
    file(WRITE "${BML_BEHAVIOR_BLOCK_HEADER_SOURCE}"
            "#include <${BML_BEHAVIOR_BLOCK_HEADER}>\n")
    list(APPEND BML_BEHAVIOR_BLOCK_HEADER_COMPILE_SOURCES
            "${BML_BEHAVIOR_BLOCK_HEADER_SOURCE}")
endforeach()
set(BML_BEHAVIOR_BLOCK_WINDOWS_FIRST_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/behavior-block-public-headers/WindowsFirst.cpp")
file(WRITE "${BML_BEHAVIOR_BLOCK_WINDOWS_FIRST_SOURCE}"
        "#include <Windows.h>\n#include <BML/Behavior/Blocks.hpp>\n")
list(APPEND BML_BEHAVIOR_BLOCK_HEADER_COMPILE_SOURCES
        "${BML_BEHAVIOR_BLOCK_WINDOWS_FIRST_SOURCE}")
add_library(BehaviorBlockHeadersCompile OBJECT
        ${BML_BEHAVIOR_BLOCK_HEADER_COMPILE_SOURCES})
target_include_directories(BehaviorBlockHeadersCompile PRIVATE
        "${BML_INCLUDE_DIR}")
target_link_libraries(BehaviorBlockHeadersCompile PRIVATE
        VirtoolsSDK::CK2 VirtoolsSDK::VxMath)
target_compile_definitions(BehaviorBlockHeadersCompile PRIVATE "BML_EXPORTS")
set_target_properties(BehaviorBlockHeadersCompile PROPERTIES FOLDER "Tests")
set_property(TARGET BehaviorBlockHeadersCompile PROPERTY CXX_STANDARD 17)

if (MSVC AND EXISTS "${BML_DUMPBIN}")
    add_test(
            NAME ImcDllExportAbiTest
            COMMAND "${CMAKE_COMMAND}"
            "-DDUMPBIN=${BML_DUMPBIN}"
            "-DBINARY=$<TARGET_FILE:BML>"
            "-P" "${CMAKE_CURRENT_SOURCE_DIR}/ImcDllExportAbiTest.cmake"
    )
    set_tests_properties(ImcDllExportAbiTest PROPERTIES LABELS "imc;abi")

    add_test(
            NAME InterfaceDllExportAbiTest
            COMMAND "${CMAKE_COMMAND}"
            "-DDUMPBIN=${BML_DUMPBIN}"
            "-DBINARY=$<TARGET_FILE:BML>"
            "-P" "${CMAKE_CURRENT_SOURCE_DIR}/InterfaceDllExportAbiTest.cmake"
    )
    set_tests_properties(InterfaceDllExportAbiTest PROPERTIES
            LABELS "interface;abi")

    add_test(
            NAME LegacyNativeDllExportAbiTest
            COMMAND "${CMAKE_COMMAND}"
            "-DDUMPBIN=${BML_DUMPBIN}"
            "-DBINARY=$<TARGET_FILE:BML>"
            "-DBASELINE=${CMAKE_CURRENT_SOURCE_DIR}/legacy-native-exports-x86-msvc.txt"
            "-P" "${CMAKE_CURRENT_SOURCE_DIR}/LegacyNativeDllExportAbiTest.cmake"
    )
    set_tests_properties(LegacyNativeDllExportAbiTest PROPERTIES LABELS "legacy-native;abi")

endif ()

if (MSVC)
    get_filename_component(BML_TOOL_DIRECTORY "${CMAKE_LINKER}" DIRECTORY)
    set(BML_DUMPBIN "${BML_TOOL_DIRECTORY}/dumpbin.exe")
endif ()

add_subdirectory(abi)
add_subdirectory(codegen)
add_subdirectory(sdk)

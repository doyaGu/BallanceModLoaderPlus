# ModLoader - Game Engine Integration Layer
# Provides CK2 Manager, core hooks, and Virtools plugin entry points

set(MODLOADER_SOURCES
    Entry.cpp
    ModManager.cpp
    Behaviors/HookBlock.cpp
)

set(MODLOADER_HEADERS
    ModManager.h
    Behaviors/HookBlock.h
)

add_library(ModLoader SHARED
    ${MODLOADER_SOURCES}
    ${MODLOADER_HEADERS}
)

target_include_directories(ModLoader
    PUBLIC
    $<BUILD_INTERFACE:${BML_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ModLoader
    PUBLIC
    CK2 VxMath
    PRIVATE
    minhook
)

target_compile_definitions(ModLoader PRIVATE
    MODLOADER_EXPORTS
)

set_target_properties(ModLoader PROPERTIES
    OUTPUT_NAME "ModLoader"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Install targets
install(TARGETS ModLoader
    EXPORT ModLoaderTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

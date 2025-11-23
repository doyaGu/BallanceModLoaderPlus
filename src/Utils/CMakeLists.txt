set(BML_UTILS_HEADERS
        IniFile.h
        StringUtils.h
        PathUtils.h
        HookUtils.h
)

set(BML_UTILS_SOURCES
        IniFile.cpp
        StringUtils.cpp
        PathUtils.cpp
        HookUtils.cpp
)

add_library(BMLUtils STATIC
    ${BML_UTILS_SOURCES}
    ${BML_UTILS_HEADERS}
)

target_include_directories(BMLUtils
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(BMLUtils
        PUBLIC
        utf8 zip
)

set_target_properties(BMLUtils PROPERTIES
        FOLDER "Core"
)

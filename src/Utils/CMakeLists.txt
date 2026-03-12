set(BML_UTILS_HEADERS
        StringUtils.h
)

set(BML_UTILS_SOURCES
        StringUtils.cpp
)

if (WIN32)
    list(APPEND BML_UTILS_HEADERS
            IniFile.h
            PathUtils.h
            HookUtils.h
    )
    list(APPEND BML_UTILS_SOURCES
            IniFile.cpp
            PathUtils.cpp
            HookUtils.cpp
    )
endif ()

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
        POSITION_INDEPENDENT_CODE ON
        FOLDER "Core"
)

set(BML_UTILS_HEADERS
        StringUtils.h
        JsonUtils.h
        TimeUtils.h
        HashUtils.h
)

set(BML_UTILS_SOURCES
        StringUtils.cpp
        JsonUtils.cpp
        TimeUtils.cpp
        HashUtils.cpp
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

add_library(BML_Utils STATIC
    ${BML_UTILS_SOURCES}
    ${BML_UTILS_HEADERS}
)

add_library(BMLUtils ALIAS BML_Utils)

target_include_directories(BML_Utils
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/deps/yyjson/src
)

target_link_libraries(BML_Utils
        PUBLIC
        utf8 zip yyjson
)

set_target_properties(BML_Utils PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        OUTPUT_NAME "BML_Utils"
        FOLDER "Core"
)

cmake_minimum_required(VERSION 3.12)

if (NOT CMAKE_PROJECT_NAME OR "${CMAKE_PROJECT_NAME}" STREQUAL "MinHook")
    project(MinHook)
endif()

set(MINHOOK_PUBLIC_HDRS
        MinHook.h
        )

set(MINHOOK_PRIVATE_HDRS
        trampoline.h
        buffer.h
        hde/hde32.h
        hde/table32.h
        hde/pstdint.h
        )

set(MINHOOK_SRCS
        hook.c
        trampoline.c
        buffer.c
        hde/hde32.c
        )

add_library(MinHook STATIC ${MINHOOK_SRCS} ${MINHOOK_PUBLIC_HDRS} ${MINHOOK_PRIVATE_HDRS})
target_include_directories(MinHook PUBLIC ${CMAKE_CURRENT_LIST_DIR})
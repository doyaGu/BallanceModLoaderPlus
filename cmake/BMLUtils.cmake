# - Some useful macros and functions for creating mod

macro(add_bml_mod NAME)
    add_library(${NAME} SHARED)
    target_sources(${NAME} PRIVATE ${ARGN})
    target_link_libraries(${NAME} PRIVATE BML CK2 VxMath)
    set_target_properties(${NAME} PROPERTIES
            SUFFIX ".bmodp"
            FOLDER "Mods")
endmacro()

macro(install_bml_mod NAME)
    install(TARGETS ${NAME} RUNTIME DESTINATION mods)
endmacro()

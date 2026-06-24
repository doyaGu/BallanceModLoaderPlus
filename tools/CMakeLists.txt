if (BML_WITH_ANGELSCRIPT)
    find_package(Python3 COMPONENTS Interpreter)
    if (Python3_Interpreter_FOUND)
        add_custom_target(bml-asgen-check
                COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/asgen/asgen.py check
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                VERBATIM)

        add_custom_target(bml-asgen-negative-check
                COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/asgen/asgen.py negative-check
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                VERBATIM)

        add_custom_target(bml-as-facade-negative-check
                COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/asgen/check_bml_facade.py --root ${PROJECT_SOURCE_DIR}
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                VERBATIM)

        add_custom_target(bml-as-negative-check)
        add_dependencies(bml-as-negative-check bml-asgen-negative-check bml-as-facade-negative-check)

        set_target_properties(bml-asgen-check bml-asgen-negative-check bml-as-facade-negative-check bml-as-negative-check PROPERTIES
                FOLDER "Tools")
    endif ()
endif ()

add_library(bml_test STATIC
    TestContext.cpp
)

target_include_directories(bml_test PUBLIC
    ${BML_INCLUDE_DIR}
    ${BML_SOURCE_DIR}
)

target_link_libraries(bml_test PUBLIC BML)

set_target_properties(bml_test PROPERTIES
    FOLDER "TestSupport"
)

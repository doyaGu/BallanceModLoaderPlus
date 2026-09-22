add_library(IVPCrypto STATIC
        CryptoUtils.cpp
        CryptoUtils.h)
target_include_directories(IVPCrypto PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(IVPCrypto PUBLIC bcrypt)
set_target_properties(IVPCrypto PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        FOLDER "IVP")

add_library(IVPAPI INTERFACE)
target_include_directories(IVPAPI
        INTERFACE
        $<BUILD_INTERFACE:${IVP_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(IVPAPI INTERFACE BML::BML)
target_compile_features(IVPAPI INTERFACE cxx_std_20)
set_target_properties(IVPAPI PROPERTIES
        EXPORT_NAME API
        FOLDER "IVP")
add_library(IVP::API ALIAS IVPAPI)

bml_add_mod(IVP
        Mod.cpp
        IVP/Runtime.cpp
        IVP/Runtime.h
        IVP/generated/IvpRetailImage.inc
        IVP/generated/IvpSymbols.inc
        ${IVP_PUBLIC_HEADERS})
target_include_directories(IVP PRIVATE
        "${IVP_INCLUDE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(IVP PRIVATE IVPCrypto)
target_compile_definitions(IVP PRIVATE
        IVP_VERSION="${PROJECT_VERSION}")
set_target_properties(IVP PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")

install(TARGETS IVPAPI
        EXPORT IVPTargets)
install(TARGETS IVP
        RUNTIME DESTINATION "Mods")

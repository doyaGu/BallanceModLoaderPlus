
bml_add_module(BML_Network
    SOURCES
        src/NetworkMod.cpp
        src/WinHttpClient.cpp
        src/SocketWorker.cpp
    MANIFEST mod.toml
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    LINK_LIBRARIES
        BML_StringUtils
        winhttp
        ws2_32
)

bml_register_module_api(BML_Network
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include
)

install(TARGETS BML_Network
    RUNTIME DESTINATION Mods/BML_Network
    LIBRARY DESTINATION Mods/BML_Network
)

install(FILES mod.toml DESTINATION Mods/BML_Network)

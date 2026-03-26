
bml_add_module(BML_InputRecorder
    SOURCES
        src/InputRecorderMod.cpp
        src/RecordingFormat.cpp
        src/RecorderEngine.cpp
    MANIFEST mod.toml
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    LINK_LIBRARIES
        BML_PathUtils
)

install(TARGETS BML_InputRecorder
    RUNTIME DESTINATION Mods/BML_InputRecorder
    LIBRARY DESTINATION Mods/BML_InputRecorder
)

install(FILES mod.toml DESTINATION Mods/BML_InputRecorder)

cmake_minimum_required(VERSION 3.15)

# Standard BML Modules
# These modules implement core functionality that was previously part of BML Core

# BML_Render: Rendering system (ImGui, Overlay, HUD)
add_subdirectory(BML_Render)

# BML_Input: Input handling (DirectInput hooks, event publishing)
add_subdirectory(BML_Input)

# BML_Physics: Physics system hooks (CKIpionManager, Physicalize)
add_subdirectory(BML_Physics)

# BML_ObjectLoad: Object loading hooks (OnLoadObject, OnLoadScript)
add_subdirectory(BML_ObjectLoad)

# BML_Event: Game event topic registration (lifecycle, gameplay events)
add_subdirectory(BML_Event)

# BML_UI: ImGui overlay and base UI infrastructure
add_subdirectory(BML_UI)

# BML_Console: module-native message board and command bar
add_subdirectory(BML_Console)

# BML_HUD: module-native HUD slice
add_subdirectory(BML_HUD)

# BML_ModMenu: module-native module/config browser
add_subdirectory(BML_ModMenu)

# BML_MapMenu: module-native custom map browser and loader
add_subdirectory(BML_MapMenu)

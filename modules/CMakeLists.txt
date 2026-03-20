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

# BML_Menu: shared Ballance menu framework and Menu provider
add_subdirectory(BML_Menu)

# BML_Console: module-native message board and command bar
add_subdirectory(BML_Console)

# BML_Gameplay: gameplay tweak migration module
add_subdirectory(BML_Gameplay)

# BML_NewBallType: custom ball/floor registration module
add_subdirectory(BML_NewBallType)

# BML_ModMenu: module-native module/config browser
add_subdirectory(BML_ModMenu)

# BML_MapMenu: module-native custom map browser and loader
add_subdirectory(BML_MapMenu)

# BML_Network: async HTTP client (WinHTTP) + TCP/UDP sockets (Winsock2)
add_subdirectory(BML_Network)

# BML_InputRecorder: input recording and playback for TAS/speedrun verification
add_subdirectory(BML_InputRecorder)

# BML_DevTools: runtime inspection panels (IMC sniffer, interface browser, hook list)
add_subdirectory(BML_DevTools)

# BML_Scripting: AngelScript runtime for script-based mods
add_subdirectory(BML_Scripting)

# BML_IntegrationTest: in-game integration test module
add_subdirectory(BML_IntegrationTest)


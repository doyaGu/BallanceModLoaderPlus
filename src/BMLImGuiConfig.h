#pragma once

// This file is part of BML's Native Mod ABI. Consumers must receive it through
// the exported BML CMake target; changing either representation requires Native
// Mods that use Dear ImGui types to be rebuilt.

#if defined(BML_IMGUI_TEST_ENGINE_SOURCE)
#define IMGUI_API
#elif defined(IMGUI_EXPORT)
#define IMGUI_API __declspec(dllexport)
#else
#define IMGUI_API __declspec(dllimport)
#endif

#define IMGUI_USE_BGRA_PACKED_COLOR
#define IMGUI_USE_WCHAR32

#if BML_ENABLE_UI_AUTOMATION
#define IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL 1
#define IMGUI_TEST_ENGINE_ENABLE_CAPTURE 0
#include "imgui_te_imconfig.h"
#endif

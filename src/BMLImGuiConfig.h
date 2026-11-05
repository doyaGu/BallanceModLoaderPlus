#pragma once

// This file is part of BML's Native Mod ABI. Consumers must receive it through
// the exported BML CMake target; changing either representation requires Native
// Mods that use Dear ImGui types to be rebuilt.

#if defined(BML_IMGUI_TEST_ENGINE_SOURCE)
#define IMGUI_API
#elif !defined(_WIN32)
#define IMGUI_API __attribute__((visibility("default")))
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

#define IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS
struct SDL_IOStream;
using ImFileHandle = SDL_IOStream *;
IMGUI_API ImFileHandle ImFileOpen(const char *, const char *);
IMGUI_API bool ImFileClose(ImFileHandle);
IMGUI_API unsigned long long ImFileGetSize(ImFileHandle);
IMGUI_API unsigned long long ImFileRead(void *, unsigned long long, unsigned long long, ImFileHandle);
IMGUI_API unsigned long long ImFileWrite(const void *, unsigned long long, unsigned long long, ImFileHandle);

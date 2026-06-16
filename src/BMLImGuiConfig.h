#pragma once

#ifdef IMGUI_EXPORT
#define IMGUI_API __declspec(dllexport)
#else
#define IMGUI_API __declspec(dllimport)
#endif

#define IMGUI_USE_BGRA_PACKED_COLOR

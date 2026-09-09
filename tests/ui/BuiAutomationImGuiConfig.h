#pragma once

// Match the pixel format used by the loader while keeping this headless test
// executable independent of BMLPlus.dll's import/export configuration.
#define IMGUI_USE_BGRA_PACKED_COLOR

// The Test Engine runs sequential test code across frames through this portable
// implementation. Captures are disabled because this target has no renderer.
#define IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL 1
#define IMGUI_TEST_ENGINE_ENABLE_CAPTURE 0
#include "imgui_te_imconfig.h"

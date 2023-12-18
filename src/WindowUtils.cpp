#include "WindowUtils.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace utils {
    int GetScreenWidth() {
        return ::GetSystemMetrics(SM_CXSCREEN);
    }

    int GetScreenHeight() {
        return ::GetSystemMetrics(SM_CYSCREEN);
    }
}
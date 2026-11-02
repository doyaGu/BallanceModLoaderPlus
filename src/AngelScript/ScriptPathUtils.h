#pragma once
#include "Utils/StringUtils.h"

namespace BML {
inline int ScriptPathCaseCompare(const wchar_t *left, const wchar_t *right) {
#if BML_RUNTIME_SDL
    return utils::CompareString(std::wstring(left), std::wstring(right), utils::kLinguisticIgnoreCase);
#else
    return _wcsicmp(left, right);
#endif
}
}

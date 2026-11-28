#pragma once

#include <CKDataArray.h>
#include <string>

namespace BML::Platform {
// The native string is converted by its caller when it crosses the 0.4 API.
inline bool ReadDataArrayString(CKDataArray *array, int row, int column, std::string &value) {
    if (!array || row < 0 || row >= array->GetRowCount() ||
        column < 0 || column >= array->GetColumnCount())
        return false;
#if BML_RUNTIME_SDL
    const int size = array->GetElementStringValue(row, column, nullptr, 0);
#else
    const int size = array->GetElementStringValue(row, column, nullptr);
#endif
    if (size <= 0)
        return false;
    std::string result(static_cast<std::size_t>(size), '\0');
#if BML_RUNTIME_SDL
    const int read = array->GetElementStringValue(row, column, result.data(), size);
#else
    const int read = array->GetElementStringValue(row, column, result.data());
#endif
    if (read <= 0 && result.front() == '\0')
        return false;
    const auto terminator = result.find('\0');
    if (terminator != std::string::npos)
        result.resize(terminator);
    value = std::move(result);
    return true;
}
}

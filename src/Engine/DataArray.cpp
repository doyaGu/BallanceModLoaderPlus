#include "Engine/DataArray.h"
#include "Engine/Text.h"

#include <CKDataArray.h>

namespace BML::CKData {

static bool HasCell(CKDataArray *array, int row, int column) {
    return array && row >= 0 && row < array->GetRowCount() &&
        column >= 0 && column < array->GetColumnCount();
}

bool ReadString(CKDataArray *array, int row, int column, std::string &value) {
    if (!HasCell(array, row, column))
        return false;
#if BML_RUNTIME_SDL
    const int size = array->GetElementStringValue(row, column, nullptr, 0);
#else
    const int size = array->GetElementStringValue(row, column, nullptr);
#endif
    if (size <= 0)
        return false;

    std::string native(static_cast<std::size_t>(size), '\0');
#if BML_RUNTIME_SDL
    const int read = array->GetElementStringValue(row, column, native.data(), size);
#else
    const int read = array->GetElementStringValue(row, column, native.data());
#endif
    if (read < 0)
        return false;
    const auto terminator = native.find('\0');
    if (terminator != std::string::npos)
        native.resize(terminator);

    std::string storage;
    const auto utf8 = CKText::Utf8Text(native, storage);
    if (utf8.empty() && !native.empty())
        return false;
    value.assign(utf8);
    return true;
}

bool WriteString(CKDataArray *array, int row, int column, std::string_view value) {
    if (!HasCell(array, row, column) || value.find('\0') != std::string_view::npos)
        return false;
    std::string native;
    return CKText::EncodeNativeText(value, native) &&
        array->SetElementStringValue(row, column, native.data()) != FALSE;
}

std::string ColumnName(CKDataArray *array, int column) {
    if (!array || column < 0 || column >= array->GetColumnCount())
        return {};
    const char *name = array->GetColumnName(column);
    std::string storage;
    return name ? std::string(CKText::Utf8Text(name, storage)) : std::string{};
}

int FindColumn(CKDataArray *array, std::string_view name) {
    if (!array || name.find('\0') != std::string_view::npos)
        return -1;
    std::string native;
    if (!CKText::EncodeNativeText(name, native))
        return -1;
    const int count = array->GetColumnCount();
    for (int column = 0; column < count; ++column) {
        const char *candidate = array->GetColumnName(column);
        if (candidate && native == candidate)
            return column;
    }
    return -1;
}

} // namespace BML::CKData

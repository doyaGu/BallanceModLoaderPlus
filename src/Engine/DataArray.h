#ifndef BML_ENGINE_DATAARRAY_H
#define BML_ENGINE_DATAARRAY_H

#include <string>
#include <string_view>

class CKDataArray;

namespace BML::CKData {

bool ReadString(CKDataArray *array, int row, int column, std::string &value);
bool WriteString(CKDataArray *array, int row, int column, std::string_view value);
std::string ColumnName(CKDataArray *array, int column);
int FindColumn(CKDataArray *array, std::string_view name);

} // namespace BML::CKData

#endif // BML_ENGINE_DATAARRAY_H

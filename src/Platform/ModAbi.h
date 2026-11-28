#pragma once
#include <string>
namespace BML::Platform {
// Checked before opening a module, so rejected artifacts cannot run constructors.
bool ValidateModAbi(const std::wstring &modulePath, std::string &error);
}

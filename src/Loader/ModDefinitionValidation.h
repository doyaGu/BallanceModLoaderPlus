#ifndef BML_MODDEFINITIONVALIDATION_H
#define BML_MODDEFINITIONVALIDATION_H

#include "Loader/ModVersion.h"
#include <limits>
#include <string_view>

namespace BML {
inline bool IsModId(std::string_view value) noexcept {
    if (value.empty() || value == "." || value == "..")
        return false;
    for (const unsigned char ch : value)
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
              ch == '_' || ch == '-' || ch == '.'))
            return false;
    return true;
}

// Native declarations and script metadata share the canonical version grammar.
inline bool ParseModVersion(std::string_view value, BMLVersion &version) noexcept {
    size_t pos = 0;
    int parts[3]{};
    for (int index = 0; index != 3; ++index) {
        if (index && (pos == value.size() || value[pos++] != '.'))
            return false;
        if (pos == value.size() || value[pos] < '0' || value[pos] > '9')
            return false;
        if (value[pos] == '0' && pos + 1 < value.size() && value[pos + 1] >= '0' &&
            value[pos + 1] <= '9')
            return false;
        do {
            int digit = value[pos++] - '0';
            if (parts[index] > (std::numeric_limits<int>::max() - digit) / 10)
                return false;
            parts[index] = parts[index] * 10 + digit;
        } while (pos < value.size() && value[pos] >= '0' && value[pos] <= '9');
    }
    if (pos != value.size())
        return false;
    version = BMLVersion(parts[0], parts[1], parts[2]);
    return true;
}
} // namespace BML
#endif

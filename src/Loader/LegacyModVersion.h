#ifndef BML_LEGACYMODVERSION_H
#define BML_LEGACYMODVERSION_H

#include <limits>

#include "BML/IMod.h"

namespace BML {

// IMod::GetVersion() historically accepted arbitrary text around the first
// three digit runs. Keep that contract at the frozen facade boundary.
inline BMLVersion ParseLegacyModVersion(const char *text) noexcept {
    int parts[3] = {};
    int partCount = 0;

    while (text && *text && partCount < 3) {
        while (*text && (*text < '0' || *text > '9'))
            ++text;
        if (!*text)
            break;

        int value = 0;
        while (*text >= '0' && *text <= '9') {
            const int digit = *text - '0';
            if (value > ((std::numeric_limits<int>::max)() - digit) / 10)
                value = (std::numeric_limits<int>::max)();
            else
                value = value * 10 + digit;
            ++text;
        }
        parts[partCount++] = value;
    }

    return BMLVersion(parts[0], parts[1], parts[2]);
}

} // namespace BML

#endif // BML_LEGACYMODVERSION_H

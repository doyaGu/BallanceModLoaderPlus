#ifndef BML_TIMEUTILS_H
#define BML_TIMEUTILS_H

#include <chrono>
#include <string>

namespace utils {
    std::string FormatLocalIso8601(const std::chrono::system_clock::time_point &time_point);
    std::string FormatLocalCompactTimestamp(const std::chrono::system_clock::time_point &time_point);
    std::string FormatUtcIso8601(const std::chrono::system_clock::time_point &time_point);
    std::string FormatUtcCompactTimestamp(const std::chrono::system_clock::time_point &time_point);
    std::string GetCurrentLocalIso8601();
    std::string GetCurrentLocalCompactTimestamp();
    std::string GetCurrentUtcIso8601();
    std::string GetCurrentUtcCompactTimestamp();
}

#endif // BML_TIMEUTILS_H

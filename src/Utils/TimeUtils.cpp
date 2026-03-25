#include "TimeUtils.h"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace utils {
    namespace {
        std::tm ToUtcTm(const std::chrono::system_clock::time_point &time_point) {
            const auto timeValue = std::chrono::system_clock::to_time_t(time_point);
            std::tm tmBuf{};
#if defined(_WIN32)
            gmtime_s(&tmBuf, &timeValue);
#else
            gmtime_r(&timeValue, &tmBuf);
#endif
            return tmBuf;
        }

        std::tm ToLocalTm(const std::chrono::system_clock::time_point &time_point) {
            const auto timeValue = std::chrono::system_clock::to_time_t(time_point);
            std::tm tmBuf{};
#if defined(_WIN32)
            localtime_s(&tmBuf, &timeValue);
#else
            localtime_r(&timeValue, &tmBuf);
#endif
            return tmBuf;
        }
    } // namespace

    std::string FormatLocalIso8601(const std::chrono::system_clock::time_point &time_point) {
        const std::tm tmBuf = ToLocalTm(time_point);
        std::ostringstream oss;
        oss << std::put_time(&tmBuf, "%Y-%m-%dT%H:%M:%S");
        return oss.str();
    }

    std::string FormatLocalCompactTimestamp(const std::chrono::system_clock::time_point &time_point) {
        const std::tm tmBuf = ToLocalTm(time_point);
        std::ostringstream oss;
        oss << std::put_time(&tmBuf, "%Y%m%d_%H%M%S");
        return oss.str();
    }

    std::string FormatUtcIso8601(const std::chrono::system_clock::time_point &time_point) {
        const std::tm tmBuf = ToUtcTm(time_point);
        std::ostringstream oss;
        oss << std::put_time(&tmBuf, "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }

    std::string FormatUtcCompactTimestamp(const std::chrono::system_clock::time_point &time_point) {
        const std::tm tmBuf = ToUtcTm(time_point);
        std::ostringstream oss;
        oss << std::put_time(&tmBuf, "%Y%m%dT%H%M%SZ");
        return oss.str();
    }

    std::string GetCurrentLocalIso8601() {
        return FormatLocalIso8601(std::chrono::system_clock::now());
    }

    std::string GetCurrentLocalCompactTimestamp() {
        return FormatLocalCompactTimestamp(std::chrono::system_clock::now());
    }

    std::string GetCurrentUtcIso8601() {
        return FormatUtcIso8601(std::chrono::system_clock::now());
    }

    std::string GetCurrentUtcCompactTimestamp() {
        return FormatUtcCompactTimestamp(std::chrono::system_clock::now());
    }
} // namespace utils

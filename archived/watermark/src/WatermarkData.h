#ifndef BML_WATERMARK_DATA_H
#define BML_WATERMARK_DATA_H

#include <array>
#include <cstdint>

#include "WatermarkLayout.h"

namespace watermark {
    enum class TraceSource {
        DeviceFallback,
    };

    struct WatermarkIdentity {
        std::array<uint8_t, 6> traceId = {};
        uint32_t sessionStart = 0;
        uint16_t buildId = 0;
        TraceSource traceSource = TraceSource::DeviceFallback;
    };

    [[nodiscard]] uint16_t PackBuildId(uint8_t major, uint8_t minor, uint8_t patch);
    [[nodiscard]] WatermarkIdentity BuildDefaultIdentity();
    [[nodiscard]] bool HasZeroTraceId(const WatermarkIdentity &identity);
    void BuildPayload(const WatermarkIdentity &identity, uint8_t outPayload[14]);

    struct DerivedWatermarkKeys {
        std::array<uint8_t, 16> messageKey = {};
        std::array<uint8_t, 16> syncKey = {};
        uint64_t messageSeed = 0;
        uint64_t syncSeed = 0;
    };

    [[nodiscard]] DerivedWatermarkKeys DeriveWatermarkKeys(const uint8_t masterKey[16], uint16_t buildId);
} // namespace watermark

#endif // BML_WATERMARK_DATA_H

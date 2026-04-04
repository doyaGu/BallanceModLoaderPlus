#include "WatermarkData.h"

#include <algorithm>
#include <cstring>
#include <ctime>

#include "BML/Version.h"

#include "Crc16.h"
#include "CryptoUtils.h"
#include "HardwareFingerprint.h"

// ============================================================================
// Identity
// ============================================================================

namespace watermark {
    uint16_t PackBuildId(uint8_t major, uint8_t minor, uint8_t patch) {
        return static_cast<uint16_t>(
            ((major & 0x1F) << 11) |
            ((minor & 0x1F) << 6) |
            (patch & 0x3F));
    }

    WatermarkIdentity BuildDefaultIdentity() {
        WatermarkIdentity identity = {};
        identity.traceSource = TraceSource::DeviceFallback;
        identity.sessionStart = static_cast<uint32_t>(time(nullptr));
        identity.buildId = PackBuildId(BML_MAJOR_VERSION, BML_MINOR_VERSION, BML_PATCH_VERSION);

        utils::HardwareFingerprint fingerprint = utils::GetHardwareFingerprint();
        std::memcpy(identity.traceId.data(), fingerprint.hash, identity.traceId.size());
        return identity;
    }

    bool HasZeroTraceId(const WatermarkIdentity &identity) {
        return std::all_of(identity.traceId.begin(), identity.traceId.end(),
                           [](uint8_t b) { return b == 0; });
    }

    void BuildPayload(const WatermarkIdentity &identity, uint8_t outPayload[14]) {
        std::memcpy(outPayload, identity.traceId.data(), identity.traceId.size());

        outPayload[6] = static_cast<uint8_t>(identity.sessionStart >> 24);
        outPayload[7] = static_cast<uint8_t>(identity.sessionStart >> 16);
        outPayload[8] = static_cast<uint8_t>(identity.sessionStart >> 8);
        outPayload[9] = static_cast<uint8_t>(identity.sessionStart);

        outPayload[10] = static_cast<uint8_t>(identity.buildId >> 8);
        outPayload[11] = static_cast<uint8_t>(identity.buildId);

        const uint16_t crc = utils::Crc16CcittFalse(outPayload, 12);
        outPayload[12] = static_cast<uint8_t>(crc >> 8);
        outPayload[13] = static_cast<uint8_t>(crc);
    }

// ============================================================================
// Key derivation
// ============================================================================

    DerivedWatermarkKeys DeriveWatermarkKeys(const uint8_t masterKey[16], uint16_t buildId) {
        DerivedWatermarkKeys keys = {};
        const uint8_t buildBytes[2] = {
            static_cast<uint8_t>(buildId >> 8),
            static_cast<uint8_t>(buildId),
        };

        std::array<uint8_t, 8> messageInfo = {
            'w', 'm', '-', 'm', 's', 'g', buildBytes[0], buildBytes[1]
        };
        std::array<uint8_t, 9> syncInfo = {
            'w', 'm', '-', 's', 'y', 'n', 'c', buildBytes[0], buildBytes[1]
        };

        if (!utils::HkdfSha256(masterKey, 16, nullptr, 0,
                        messageInfo.data(), messageInfo.size(),
                        keys.messageKey.data(), keys.messageKey.size())) {
            return keys;
        }

        if (!utils::HkdfSha256(masterKey, 16, nullptr, 0,
                        syncInfo.data(), syncInfo.size(),
                        keys.syncKey.data(), keys.syncKey.size())) {
            keys.messageKey.fill(0);
            return keys;
        }

        keys.messageSeed = utils::KeyToSeed(keys.messageKey.data(), keys.messageKey.size());
        keys.syncSeed = utils::KeyToSeed(keys.syncKey.data(), keys.syncKey.size());
        return keys;
    }
} // namespace watermark

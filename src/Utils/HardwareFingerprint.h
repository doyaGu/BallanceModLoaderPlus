#ifndef BML_HARDWARE_FINGERPRINT_H
#define BML_HARDWARE_FINGERPRINT_H

#include <cstdint>

namespace utils {
    // Reads HKLM\SOFTWARE\Microsoft\Cryptography\MachineGuid,
    // hashes with SHA-256 via BCrypt, returns first 6 bytes.
    // On failure, returns all-zero.
    struct HardwareFingerprint {
        uint8_t hash[6];
    };

    HardwareFingerprint GetHardwareFingerprint();
} // namespace utils

#endif // BML_HARDWARE_FINGERPRINT_H

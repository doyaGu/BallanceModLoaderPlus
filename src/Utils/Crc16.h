#ifndef BML_CRC16_H
#define BML_CRC16_H

#include <cstddef>
#include <cstdint>

namespace utils {
    inline uint16_t Crc16CcittFalse(const uint8_t *data, size_t len) {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < len; ++i) {
            crc ^= static_cast<uint16_t>(data[i]) << 8;
            for (int j = 0; j < 8; ++j) {
                if (crc & 0x8000)
                    crc = (crc << 1) ^ 0x1021;
                else
                    crc <<= 1;
            }
        }
        return crc;
    }
} // namespace utils

#endif // BML_CRC16_H

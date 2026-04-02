#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>

#include "WatermarkData.h"

#include "Crc16.h"
#include "CryptoUtils.h"
#include "ReedSolomon.h"
#include "SplitMix64.h"

// --- SplitMix64 Tests ---

TEST(SplitMix64Test, Deterministic) {
    uint64_t state1 = 0;
    uint64_t v1 = utils::SplitMix64(state1);
    uint64_t v2 = utils::SplitMix64(state1);

    uint64_t state2 = 0;
    EXPECT_EQ(utils::SplitMix64(state2), v1);
    EXPECT_EQ(utils::SplitMix64(state2), v2);
}

TEST(SplitMix64Test, NonZeroOutput) {
    uint64_t state = 0;
    EXPECT_NE(utils::SplitMix64(state), 0u);
}

TEST(SplitMix64Test, DifferentOutputs) {
    uint64_t state = 0;
    uint64_t v1 = utils::SplitMix64(state);
    uint64_t v2 = utils::SplitMix64(state);
    EXPECT_NE(v1, v2);
}

TEST(SplitMix64Test, DifferentSeeds) {
    uint64_t s1 = 42, s2 = 43;
    EXPECT_NE(utils::SplitMix64(s1), utils::SplitMix64(s2));
}

// --- CRC-16 Tests ---

TEST(Crc16Test, KnownVector) {
    // "123456789" -> 0x29B1 for CRC-16/CCITT-FALSE
    const uint8_t data[] = "123456789";
    EXPECT_EQ(utils::Crc16CcittFalse(data, 9), 0x29B1);
}

TEST(Crc16Test, EmptyInput) {
    EXPECT_EQ(utils::Crc16CcittFalse(nullptr, 0), 0xFFFF);
}

TEST(Crc16Test, SingleByte) {
    const uint8_t data[] = {0x00};
    uint16_t crc = utils::Crc16CcittFalse(data, 1);
    EXPECT_NE(crc, 0xFFFF); // Should differ from empty
}

// --- Reed-Solomon Tests ---

TEST(ReedSolomonTest, SystematicEncoding) {
    uint8_t data[utils::kRsDataLen] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
    uint8_t coded[utils::kRsCodeLen] = {};
    utils::RsEncode(data, coded);

    // Data portion unchanged
    EXPECT_EQ(memcmp(coded, data, utils::kRsDataLen), 0);
}

TEST(ReedSolomonTest, ParityNonZero) {
    uint8_t data[utils::kRsDataLen] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
    uint8_t coded[utils::kRsCodeLen] = {};
    utils::RsEncode(data, coded);

    bool anyNonZero = false;
    for (int i = utils::kRsDataLen; i < utils::kRsCodeLen; ++i) {
        if (coded[i] != 0) { anyNonZero = true; break; }
    }
    EXPECT_TRUE(anyNonZero);
}

TEST(ReedSolomonTest, Deterministic) {
    uint8_t data[utils::kRsDataLen] = {0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x67,
                                   0xF3, 0x1A, 0x00, 0x00, 0xCB, 0x00, 0x00};
    uint8_t coded1[utils::kRsCodeLen] = {};
    uint8_t coded2[utils::kRsCodeLen] = {};
    utils::RsEncode(data, coded1);
    utils::RsEncode(data, coded2);
    EXPECT_EQ(memcmp(coded1, coded2, utils::kRsCodeLen), 0);
}

TEST(ReedSolomonTest, DifferentDataDifferentParity) {
    uint8_t data1[utils::kRsDataLen] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
    uint8_t data2[utils::kRsDataLen] = {};
    data2[0] = 0xFF;
    uint8_t coded1[utils::kRsCodeLen] = {};
    uint8_t coded2[utils::kRsCodeLen] = {};
    utils::RsEncode(data1, coded1);
    utils::RsEncode(data2, coded2);
    EXPECT_NE(memcmp(coded1 + utils::kRsDataLen, coded2 + utils::kRsDataLen, utils::kRsParityLen), 0);
}

TEST(ReedSolomonTest, CrossValidation) {
    // Known payload for cross-validation with Python detector
    uint8_t payload[utils::kRsDataLen] = {
        0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, // hardware hash
        0x67, 0xF3, 0x1A, 0x00,               // timestamp
        0x00, 0xCB,                             // version 0.3.11
        0x00, 0x00                              // CRC placeholder
    };
    uint16_t crc = utils::Crc16CcittFalse(payload, 12);
    payload[12] = static_cast<uint8_t>(crc >> 8);
    payload[13] = static_cast<uint8_t>(crc);

    uint8_t coded[utils::kRsCodeLen] = {};
    utils::RsEncode(payload, coded);

    // Print for Python cross-validation
    printf("RS coded bytes for Python: [");
    for (int i = 0; i < utils::kRsCodeLen; ++i) {
        printf("0x%02X%s", coded[i], i < utils::kRsCodeLen - 1 ? ", " : "");
    }
    printf("]\n");

    // Systematic property
    EXPECT_EQ(memcmp(coded, payload, utils::kRsDataLen), 0);
}

// --- GF(2^8) Arithmetic Tests ---

TEST(GfArithmeticTest, MulIdentity) {
    EXPECT_EQ(utils::GfMul(1, 1), 1);
}

TEST(GfArithmeticTest, MulZero) {
    EXPECT_EQ(utils::GfMul(0, 42), 0);
    EXPECT_EQ(utils::GfMul(42, 0), 0);
}

TEST(GfArithmeticTest, Inverse) {
    // a * a^-1 = 1
    for (int a = 1; a < 256; ++a) {
        uint8_t inv = utils::GfInv(static_cast<uint8_t>(a));
        EXPECT_EQ(utils::GfMul(static_cast<uint8_t>(a), inv), 1)
            << "Failed for a=" << a;
    }
}

// --- Payload Bit Packing ---

TEST(PayloadTest, VersionPacking) {
    uint16_t ver = ((0 & 0x1F) << 11) | ((3 & 0x1F) << 6) | (11 & 0x3F);
    EXPECT_EQ((ver >> 11) & 0x1F, 0);  // major
    EXPECT_EQ((ver >> 6) & 0x1F, 3);   // minor
    EXPECT_EQ(ver & 0x3F, 11);         // patch
}

TEST(PayloadTest, VersionPackingMax) {
    uint16_t ver = ((31 & 0x1F) << 11) | ((31 & 0x1F) << 6) | (63 & 0x3F);
    EXPECT_EQ((ver >> 11) & 0x1F, 31);
    EXPECT_EQ((ver >> 6) & 0x1F, 31);
    EXPECT_EQ(ver & 0x3F, 63);
}

TEST(WatermarkLayoutTest, PilotCoordinatesAreExactAndNonOverlapping) {
    const auto &pilotCoords = watermark::GetPilotCoordinates();
    ASSERT_EQ(pilotCoords.size(), watermark::kPilotBlockCount);

    EXPECT_EQ(pilotCoords[0], (watermark::TileCoordinate {0, 0}));
    EXPECT_EQ(pilotCoords[1], (watermark::TileCoordinate {15, 0}));
    EXPECT_EQ(pilotCoords[2], (watermark::TileCoordinate {0, 15}));
    EXPECT_EQ(pilotCoords[3], (watermark::TileCoordinate {15, 15}));
    EXPECT_EQ(pilotCoords[4], (watermark::TileCoordinate {7, 0}));
    EXPECT_EQ(pilotCoords[5], (watermark::TileCoordinate {8, 0}));
    EXPECT_EQ(pilotCoords[6], (watermark::TileCoordinate {0, 7}));
    EXPECT_EQ(pilotCoords[7], (watermark::TileCoordinate {0, 8}));
    EXPECT_EQ(pilotCoords[8], (watermark::TileCoordinate {15, 7}));
    EXPECT_EQ(pilotCoords[9], (watermark::TileCoordinate {15, 8}));
    EXPECT_EQ(pilotCoords[10], (watermark::TileCoordinate {7, 15}));
    EXPECT_EQ(pilotCoords[11], (watermark::TileCoordinate {8, 15}));
    EXPECT_EQ(pilotCoords[12], (watermark::TileCoordinate {5, 5}));
    EXPECT_EQ(pilotCoords[13], (watermark::TileCoordinate {10, 5}));
    EXPECT_EQ(pilotCoords[14], (watermark::TileCoordinate {5, 10}));
    EXPECT_EQ(pilotCoords[15], (watermark::TileCoordinate {10, 10}));

    bool seen[watermark::kTileBlockCount] = {};
    for (const auto &coord : pilotCoords) {
        ASSERT_GE(coord.x, 0);
        ASSERT_LT(coord.x, watermark::kTileCols);
        ASSERT_GE(coord.y, 0);
        ASSERT_LT(coord.y, watermark::kTileRows);

        const int blockIndex = coord.y * watermark::kTileCols + coord.x;
        EXPECT_FALSE(seen[blockIndex]);
        seen[blockIndex] = true;
        EXPECT_TRUE(watermark::IsPilotBlock(blockIndex));
    }
}

TEST(WatermarkLayoutTest, MessageBlockIndicesSkipPilotsInRowMajorOrder) {
    const auto &messageBlocks = watermark::GetMessageBlockIndices();
    ASSERT_EQ(messageBlocks.size(), watermark::kMessageBlockCount);

    bool seen[watermark::kTileBlockCount] = {};
    for (int blockIndex : messageBlocks) {
        ASSERT_GE(blockIndex, 0);
        ASSERT_LT(blockIndex, watermark::kTileBlockCount);
        EXPECT_FALSE(seen[blockIndex]);
        seen[blockIndex] = true;
        EXPECT_FALSE(watermark::IsPilotBlock(blockIndex));
    }

    for (int blockIndex = 0; blockIndex < watermark::kTileBlockCount; ++blockIndex) {
        EXPECT_EQ(seen[blockIndex], !watermark::IsPilotBlock(blockIndex))
            << "Unexpected coverage for block index " << blockIndex;
    }

    EXPECT_EQ(messageBlocks.front(), 1);
    EXPECT_EQ(messageBlocks[5], 6);
    EXPECT_EQ(messageBlocks[6], 9);
    EXPECT_EQ(messageBlocks.back(), 254);
}

TEST(WatermarkCryptoTest, DeriveKeysIsDeterministic) {
    const uint8_t masterKey[16] = {
        0x9E, 0x57, 0xB2, 0x6D, 0x3F, 0xEB, 0x74, 0xA3,
        0xDA, 0x46, 0xF0, 0x09, 0xCD, 0xB1, 0x1C, 0x88
    };

    watermark::DerivedWatermarkKeys keys = watermark::DeriveWatermarkKeys(masterKey, 0x00CB);

    const uint8_t expectedMsgKey[16] = {
        0x36, 0x31, 0x6A, 0x34, 0x70, 0xCE, 0x57, 0x22,
        0xD0, 0x20, 0x8D, 0xC2, 0x27, 0xC6, 0xBD, 0x29
    };
    const uint8_t expectedSyncKey[16] = {
        0x42, 0xC8, 0xB4, 0xC0, 0x31, 0x30, 0x0D, 0x1B,
        0xC8, 0x8B, 0x30, 0xD1, 0xA1, 0x72, 0x7A, 0x7D
    };

    EXPECT_EQ(memcmp(keys.messageKey.data(), expectedMsgKey, sizeof(expectedMsgKey)), 0);
    EXPECT_EQ(memcmp(keys.syncKey.data(), expectedSyncKey, sizeof(expectedSyncKey)), 0);
    EXPECT_NE(keys.messageSeed, keys.syncSeed);
}

TEST(WatermarkPayloadTest, PayloadPackingKeepsTraceSessionBuildAndCrc) {
    watermark::WatermarkIdentity identity = {};
    identity.traceSource = watermark::TraceSource::DeviceFallback;
    identity.traceId = {0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6};
    identity.sessionStart = 0x67F31A00u;
    identity.buildId = 0x00CBu;

    uint8_t payload[14] = {};
    watermark::BuildPayload(identity, payload);

    const uint8_t expectedPrefix[12] = {
        0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6,
        0x67, 0xF3, 0x1A, 0x00,
        0x00, 0xCB
    };
    EXPECT_EQ(memcmp(payload, expectedPrefix, sizeof(expectedPrefix)), 0);

    const uint16_t crc = static_cast<uint16_t>((payload[12] << 8) | payload[13]);
    EXPECT_EQ(crc, utils::Crc16CcittFalse(payload, 12));
}

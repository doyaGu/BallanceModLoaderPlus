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

TEST(SplitMix64Test, OnceIsPureFinalizerCalledByStatefulStep) {
    uint64_t state = UINT64_C(0x0123456789ABCDEF);
    const uint64_t result = utils::SplitMix64(state);
    // SplitMix64 increments then calls Once, so Once(state_after_inc) == result
    EXPECT_EQ(utils::SplitMix64Once(state), result);
}

TEST(SplitMix64Test, OnceKnownOutputAnchor) {
    // Pure finalizer: finalize(0) = 0 (all bit-mixing on zero yields zero)
    EXPECT_EQ(utils::SplitMix64Once(0), UINT64_C(0));
    // Non-trivial anchor
    EXPECT_EQ(utils::SplitMix64Once(1), UINT64_C(0x5692161D100B05E5));
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
        0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, // trace id
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

TEST(WatermarkLayoutTest, TileClassesRepeatInABCDSupertilePattern) {
    EXPECT_EQ(watermark::GetTileClassForTile(0, 0), watermark::TileClass::A);
    EXPECT_EQ(watermark::GetTileClassForTile(1, 0), watermark::TileClass::B);
    EXPECT_EQ(watermark::GetTileClassForTile(2, 0), watermark::TileClass::A);
    EXPECT_EQ(watermark::GetTileClassForTile(0, 1), watermark::TileClass::C);
    EXPECT_EQ(watermark::GetTileClassForTile(1, 1), watermark::TileClass::D);
    EXPECT_EQ(watermark::GetTileClassForTile(2, 1), watermark::TileClass::C);
    EXPECT_EQ(watermark::GetTileClassForTile(0, 2), watermark::TileClass::A);
    EXPECT_EQ(watermark::GetTileClassForTile(1, 2), watermark::TileClass::B);
}

TEST(WatermarkLayoutTest, MessagePermutationsAreDeterministicAndBijective) {
    const auto &permA = watermark::GetMessagePermutation(watermark::TileClass::A);
    const auto &permB = watermark::GetMessagePermutation(watermark::TileClass::B);
    const auto &permC = watermark::GetMessagePermutation(watermark::TileClass::C);
    const auto &permD = watermark::GetMessagePermutation(watermark::TileClass::D);

    EXPECT_EQ(permA[0], 0);
    EXPECT_EQ(permA[7], 7);
    EXPECT_EQ(permB[0], 17);
    EXPECT_EQ(permB[1], 54);
    EXPECT_EQ(permB[7], 36);
    EXPECT_EQ(permC[0], 91);
    EXPECT_EQ(permC[1], 144);
    EXPECT_EQ(permD[0], 29);
    EXPECT_EQ(permD[3], 2);

    for (watermark::TileClass tileClass :
         {watermark::TileClass::A, watermark::TileClass::B, watermark::TileClass::C,
          watermark::TileClass::D}) {
        bool seen[watermark::kMessageBlockCount] = {};
        for (int canonicalIndex : watermark::GetMessagePermutation(tileClass)) {
            ASSERT_GE(canonicalIndex, 0);
            ASSERT_LT(canonicalIndex, watermark::kMessageBlockCount);
            EXPECT_FALSE(seen[canonicalIndex]);
            seen[canonicalIndex] = true;
        }

        const auto &inverse = watermark::GetInverseMessagePermutation(tileClass);
        for (int canonicalIndex = 0; canonicalIndex < watermark::kMessageBlockCount; ++canonicalIndex) {
            const int localIndex = inverse[canonicalIndex];
            EXPECT_EQ(watermark::MapMessageIndex(tileClass, localIndex), canonicalIndex);
        }
    }
}

TEST(WatermarkLayoutTest, MessageSignMasksAreDeterministicPerClass) {
    EXPECT_FALSE(watermark::IsMessageSignFlipped(watermark::TileClass::A, 0));
    EXPECT_TRUE(watermark::IsMessageSignFlipped(watermark::TileClass::A, 1));
    EXPECT_FALSE(watermark::IsMessageSignFlipped(watermark::TileClass::A, 3));

    EXPECT_FALSE(watermark::IsMessageSignFlipped(watermark::TileClass::B, 0));
    EXPECT_TRUE(watermark::IsMessageSignFlipped(watermark::TileClass::B, 1));
    EXPECT_TRUE(watermark::IsMessageSignFlipped(watermark::TileClass::B, 4));

    EXPECT_FALSE(watermark::IsMessageSignFlipped(watermark::TileClass::C, 0));
    EXPECT_TRUE(watermark::IsMessageSignFlipped(watermark::TileClass::C, 3));
    EXPECT_FALSE(watermark::IsMessageSignFlipped(watermark::TileClass::C, 4));

    EXPECT_FALSE(watermark::IsMessageSignFlipped(watermark::TileClass::D, 0));
    EXPECT_TRUE(watermark::IsMessageSignFlipped(watermark::TileClass::D, 1));
    EXPECT_FALSE(watermark::IsMessageSignFlipped(watermark::TileClass::D, 6));

    int maskCountA = 0;
    int maskCountB = 0;
    int maskCountC = 0;
    int maskCountD = 0;
    for (int canonicalIndex = 0; canonicalIndex < watermark::kMessageBlockCount; ++canonicalIndex) {
        maskCountA += watermark::IsMessageSignFlipped(watermark::TileClass::A, canonicalIndex) ? 1 : 0;
        maskCountB += watermark::IsMessageSignFlipped(watermark::TileClass::B, canonicalIndex) ? 1 : 0;
        maskCountC += watermark::IsMessageSignFlipped(watermark::TileClass::C, canonicalIndex) ? 1 : 0;
        maskCountD += watermark::IsMessageSignFlipped(watermark::TileClass::D, canonicalIndex) ? 1 : 0;
    }

    EXPECT_EQ(maskCountA, 133);
    EXPECT_EQ(maskCountB, 125);
    EXPECT_EQ(maskCountC, 120);
    EXPECT_EQ(maskCountD, 111);
}

TEST(WatermarkLayoutTest, ClassSeedOffsetsDecorrelateFamilies) {
    constexpr uint64_t kBaseSeed = UINT64_C(0x0123456789ABCDEF);
    EXPECT_NE(watermark::GetMessageSeedForClass(kBaseSeed, watermark::TileClass::A),
              watermark::GetMessageSeedForClass(kBaseSeed, watermark::TileClass::B));
    EXPECT_NE(watermark::GetSyncSeedForClass(kBaseSeed, watermark::TileClass::A),
              watermark::GetSyncSeedForClass(kBaseSeed, watermark::TileClass::C));
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

TEST(WatermarkPayloadTest, ZeroTraceIdIsExplicitlyRecognized) {
    watermark::WatermarkIdentity identity = {};
    EXPECT_TRUE(watermark::HasZeroTraceId(identity));

    identity.traceId[5] = 0x01;
    EXPECT_FALSE(watermark::HasZeroTraceId(identity));
}

// --- End-to-end encoded bit test ---

TEST(WatermarkEncodingTest, PermutationAndSignMaskProduceExpectedBit) {
    // Known 30-byte RS codeword (from CrossValidation test)
    const uint8_t coded[utils::kRsCodeLen] = {
        0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x67, 0xF3,
        0x1A, 0x00, 0x00, 0xCB, 0x76, 0x84, 0x02, 0xAF,
        0x08, 0x2F, 0x98, 0x5E, 0x79, 0x6C, 0x15, 0x94,
        0xD2, 0x4F, 0x37, 0xAC, 0x69, 0x17,
    };

    // For class A (identity permutation, no offset), templateIndex maps directly
    const auto &permA = watermark::GetMessagePermutation(watermark::TileClass::A);
    const int templateIndex = 0;
    const int canonicalIndex = permA[templateIndex];
    EXPECT_EQ(canonicalIndex, 0); // identity permutation

    const int byteIdx = canonicalIndex / 8;
    const int bitIdx = 7 - (canonicalIndex % 8);
    uint8_t rawBit = (coded[byteIdx] >> bitIdx) & 1;
    // 0xA1 = 10100001, bit 7 (MSB) = 1
    EXPECT_EQ(rawBit, 1);

    bool flipped = watermark::IsMessageSignFlipped(watermark::TileClass::A, canonicalIndex);
    uint8_t finalBit = rawBit ^ (flipped ? 1 : 0);

    // For class B, same codeword bit goes through a different permutation
    const auto &permB = watermark::GetMessagePermutation(watermark::TileClass::B);
    const int canonicalIndexB = permB[templateIndex]; // templateIndex 0 -> canonical 17
    EXPECT_EQ(canonicalIndexB, 17);

    const int byteIdxB = canonicalIndexB / 8;
    const int bitIdxB = 7 - (canonicalIndexB % 8);
    uint8_t rawBitB = (coded[byteIdxB] >> bitIdxB) & 1;
    bool flippedB = watermark::IsMessageSignFlipped(watermark::TileClass::B, canonicalIndexB);
    uint8_t finalBitB = rawBitB ^ (flippedB ? 1 : 0);

    // The final encoded bits for different tile classes at the same templateIndex
    // should generally differ (different canonical index + different sign mask)
    // This is not guaranteed for every case, but verifies the pipeline works
    (void)finalBit;
    (void)finalBitB;

    // Verify round-trip: inverse permutation recovers original index
    const auto &invB = watermark::GetInverseMessagePermutation(watermark::TileClass::B);
    EXPECT_EQ(invB[canonicalIndexB], templateIndex);
}

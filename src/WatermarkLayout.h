#ifndef BML_WATERMARK_LAYOUT_H
#define BML_WATERMARK_LAYOUT_H

#include <array>
#include <cstdint>

#include "SplitMix64.h"

namespace watermark {
    struct TileCoordinate {
        int x;
        int y;

        constexpr bool operator==(const TileCoordinate &other) const = default;
    };

    enum class TileClass : uint8_t {
        A = 0,
        B = 1,
        C = 2,
        D = 3,
    };

    inline constexpr int kBlockSize = 32;
    inline constexpr int kTileCols = 16;
    inline constexpr int kTileRows = 16;
    inline constexpr int kTileWidth = kTileCols * kBlockSize;
    inline constexpr int kTileHeight = kTileRows * kBlockSize;
    inline constexpr int kTileClassCount = 4;
    inline constexpr int kPilotBlockCount = 16;
    inline constexpr int kMessageBlockCount = 240;
    inline constexpr int kTileBlockCount = kPilotBlockCount + kMessageBlockCount;
    inline constexpr uint8_t kMessageDelta = 4;
    inline constexpr uint8_t kPilotDelta = 6;

    inline constexpr std::array<TileCoordinate, kPilotBlockCount> kPilotCoordinates = {{
        {0, 0}, {15, 0}, {0, 15}, {15, 15},
        {7, 0}, {8, 0}, {0, 7}, {0, 8},
        {15, 7}, {15, 8}, {7, 15}, {8, 15},
        {5, 5}, {10, 5}, {5, 10}, {10, 10},
    }};

    constexpr int ToBlockIndex(const TileCoordinate &coord) {
        return coord.y * kTileCols + coord.x;
    }

    constexpr std::array<int, kPilotBlockCount> BuildPilotBlockIndices() {
        std::array<int, kPilotBlockCount> indices = {};
        for (int i = 0; i < kPilotBlockCount; ++i) {
            indices[i] = ToBlockIndex(kPilotCoordinates[i]);
        }
        return indices;
    }

    inline constexpr std::array<int, kPilotBlockCount> kPilotBlockIndices = BuildPilotBlockIndices();

    constexpr int GetTileClassIndex(TileClass tileClass) {
        return static_cast<int>(tileClass);
    }

    constexpr TileClass GetTileClassForTile(int tileX, int tileY) {
        const bool oddTileX = (tileX & 1) != 0;
        const bool oddTileY = (tileY & 1) != 0;

        if (!oddTileY) {
            return oddTileX ? TileClass::B : TileClass::A;
        }
        return oddTileX ? TileClass::D : TileClass::C;
    }

    inline constexpr std::array<uint64_t, kTileClassCount> kMessageSeedOffsets = {
        UINT64_C(0x13579BDF2468ACE0),
        UINT64_C(0x2A3B4C5D6E7F8091),
        UINT64_C(0x55AA33CC77EE11FF),
        UINT64_C(0x89ABCDEF01234567),
    };

    inline constexpr std::array<uint64_t, kTileClassCount> kSyncSeedOffsets = {
        UINT64_C(0x0F1E2D3C4B5A6978),
        UINT64_C(0x1122334455667788),
        UINT64_C(0x99AABBCCDDEEFF00),
        UINT64_C(0x7766554433221100),
    };

    constexpr uint64_t GetMessageSeedOffset(TileClass tileClass) {
        return kMessageSeedOffsets[GetTileClassIndex(tileClass)];
    }

    constexpr uint64_t GetSyncSeedOffset(TileClass tileClass) {
        return kSyncSeedOffsets[GetTileClassIndex(tileClass)];
    }

    [[nodiscard]] inline uint64_t GetMessageSeedForClass(uint64_t baseSeed, TileClass tileClass) {
        return baseSeed ^ GetMessageSeedOffset(tileClass);
    }

    [[nodiscard]] inline uint64_t GetSyncSeedForClass(uint64_t baseSeed, TileClass tileClass) {
        return baseSeed ^ GetSyncSeedOffset(tileClass);
    }

    constexpr bool IsPilotBlock(int blockIndex) {
        for (int pilotIndex : kPilotBlockIndices) {
            if (pilotIndex == blockIndex) {
                return true;
            }
        }
        return false;
    }

    constexpr int GetPilotTemplateIndex(int blockIndex) {
        for (int i = 0; i < kPilotBlockCount; ++i) {
            if (kPilotBlockIndices[i] == blockIndex) {
                return i;
            }
        }
        return -1;
    }

    constexpr std::array<int, kMessageBlockCount> BuildMessageBlockIndices() {
        std::array<int, kMessageBlockCount> indices = {};
        int outIndex = 0;
        for (int blockIndex = 0; blockIndex < kTileBlockCount; ++blockIndex) {
            if (IsPilotBlock(blockIndex)) {
                continue;
            }
            indices[outIndex++] = blockIndex;
        }
        return indices;
    }

    inline constexpr std::array<int, kMessageBlockCount> kMessageBlockIndices = BuildMessageBlockIndices();

    using MessagePermutation = std::array<int, kMessageBlockCount>;
    using MessageSignMask = std::array<bool, kMessageBlockCount>;

    constexpr MessagePermutation BuildAffinePermutation(int multiplier, int offset) {
        MessagePermutation permutation = {};
        for (int localIndex = 0; localIndex < kMessageBlockCount; ++localIndex) {
            permutation[localIndex] = (localIndex * multiplier + offset) % kMessageBlockCount;
        }
        return permutation;
    }

    constexpr std::array<MessagePermutation, kTileClassCount> BuildMessagePermutations() {
        return {{
            BuildAffinePermutation(1, 0),
            BuildAffinePermutation(37, 17),
            BuildAffinePermutation(53, 91),
            BuildAffinePermutation(71, 29),
        }};
    }

    inline constexpr std::array<MessagePermutation, kTileClassCount> kMessagePermutations =
        BuildMessagePermutations();

    constexpr std::array<int, kMessageBlockCount> BuildInversePermutation(
        const MessagePermutation &permutation) {
        std::array<int, kMessageBlockCount> inverse = {};
        for (int localIndex = 0; localIndex < kMessageBlockCount; ++localIndex) {
            inverse[permutation[localIndex]] = localIndex;
        }
        return inverse;
    }

    constexpr std::array<std::array<int, kMessageBlockCount>, kTileClassCount>
    BuildInverseMessagePermutations() {
        std::array<std::array<int, kMessageBlockCount>, kTileClassCount> inversePermutations = {};
        for (int classIndex = 0; classIndex < kTileClassCount; ++classIndex) {
            inversePermutations[classIndex] = BuildInversePermutation(kMessagePermutations[classIndex]);
        }
        return inversePermutations;
    }

    inline constexpr std::array<std::array<int, kMessageBlockCount>, kTileClassCount>
        kInverseMessagePermutations = BuildInverseMessagePermutations();

    constexpr MessageSignMask BuildMessageSignMask(uint64_t seed) {
        MessageSignMask mask = {};
        for (int canonicalIndex = 0; canonicalIndex < kMessageBlockCount; ++canonicalIndex) {
            const uint64_t mixed = utils::SplitMix64Once(seed + static_cast<uint64_t>(canonicalIndex));
            mask[canonicalIndex] = (mixed & 1u) != 0;
        }
        return mask;
    }

    constexpr std::array<MessageSignMask, kTileClassCount> BuildMessageSignMasks() {
        return {{
            BuildMessageSignMask(UINT64_C(0xA16B2C3D4E5F6071)),
            BuildMessageSignMask(UINT64_C(0xB27C3D4E5F607182)),
            BuildMessageSignMask(UINT64_C(0xC38D4E5F60718293)),
            BuildMessageSignMask(UINT64_C(0xD49E5F60718293A4)),
        }};
    }

    inline constexpr std::array<MessageSignMask, kTileClassCount> kMessageSignMasks =
        BuildMessageSignMasks();

    constexpr int GetMessageTemplateIndex(int blockIndex) {
        for (int i = 0; i < kMessageBlockCount; ++i) {
            if (kMessageBlockIndices[i] == blockIndex) {
                return i;
            }
        }
        return -1;
    }

    [[nodiscard]] inline constexpr const std::array<TileCoordinate, kPilotBlockCount> &GetPilotCoordinates() {
        return kPilotCoordinates;
    }

    [[nodiscard]] inline constexpr const std::array<int, kPilotBlockCount> &GetPilotBlockIndices() {
        return kPilotBlockIndices;
    }

    [[nodiscard]] inline constexpr const std::array<int, kMessageBlockCount> &GetMessageBlockIndices() {
        return kMessageBlockIndices;
    }

    [[nodiscard]] inline constexpr const MessagePermutation &GetMessagePermutation(TileClass tileClass) {
        return kMessagePermutations[GetTileClassIndex(tileClass)];
    }

    [[nodiscard]] inline constexpr const std::array<int, kMessageBlockCount> &GetInverseMessagePermutation(
        TileClass tileClass) {
        return kInverseMessagePermutations[GetTileClassIndex(tileClass)];
    }

    [[nodiscard]] inline constexpr int MapMessageIndex(TileClass tileClass, int localIndex) {
        return GetMessagePermutation(tileClass)[localIndex];
    }

    [[nodiscard]] inline constexpr const MessageSignMask &GetMessageSignMask(TileClass tileClass) {
        return kMessageSignMasks[GetTileClassIndex(tileClass)];
    }

    [[nodiscard]] inline constexpr bool IsMessageSignFlipped(TileClass tileClass, int canonicalIndex) {
        return GetMessageSignMask(tileClass)[canonicalIndex];
    }
} // namespace watermark

#endif // BML_WATERMARK_LAYOUT_H

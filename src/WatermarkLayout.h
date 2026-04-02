#ifndef BML_WATERMARK_LAYOUT_H
#define BML_WATERMARK_LAYOUT_H

#include <array>
#include <cstdint>

namespace watermark {
    struct TileCoordinate {
        int x;
        int y;

        constexpr bool operator==(const TileCoordinate &other) const = default;
    };

    inline constexpr int kBlockSize = 32;
    inline constexpr int kTileCols = 16;
    inline constexpr int kTileRows = 16;
    inline constexpr int kTileWidth = kTileCols * kBlockSize;
    inline constexpr int kTileHeight = kTileRows * kBlockSize;
    inline constexpr int kPilotBlockCount = 16;
    inline constexpr int kMessageBlockCount = 240;
    inline constexpr int kTileBlockCount = kPilotBlockCount + kMessageBlockCount;
    inline constexpr uint8_t kMessageDelta = 2;
    inline constexpr uint8_t kPilotDelta = 3;

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
} // namespace watermark

#endif // BML_WATERMARK_LAYOUT_H

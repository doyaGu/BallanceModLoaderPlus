#include "BML/Detail/Api.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {

int CopyText(const std::string &text, char *buffer, size_t capacity, size_t *required) {
    *required = text.size() + 1;
    if (capacity < *required)
        return BML_ERROR_BUFFER_TOO_SMALL;
    std::memcpy(buffer, text.c_str(), *required);
    return BML_OK;
}

TEST(ApiBufferTest, ReadsCommonTextWithoutASeparateLengthQuery) {
    for (const std::string text : {"", "A", "路径/中文.txt", "a short string"}) {
        unsigned calls = 0;
        auto value = BML::Detail::ReadBuffer<std::string>(
            [&](char *buffer, size_t capacity, size_t *required) {
                ++calls;
                EXPECT_NE(buffer, nullptr);
                return CopyText(text, buffer, capacity, required);
            });
        ASSERT_TRUE(value);
        EXPECT_EQ(value.Value(), text);
        EXPECT_EQ(calls, 1u);
    }
}

TEST(ApiBufferTest, OwnsTextIndependentlyOfTheProvider) {
    std::string source = "borrowed provider storage";
    auto value =
        BML::Detail::ReadBuffer<std::string>([&](char *buffer, size_t capacity, size_t *required) {
            return CopyText(source, buffer, capacity, required);
        });
    source.assign(1000, 'x');
    ASSERT_TRUE(value);
    EXPECT_EQ(value.Value(), "borrowed provider storage");
}

TEST(ApiBufferTest, FallsBackForLongTextAndKeepsExactTerminatorBoundaries) {
    for (size_t length : {255u, 256u, 1024u, 8192u}) {
        const std::string text(length, 'x');
        unsigned calls = 0;
        auto value = BML::Detail::ReadBuffer<std::string>(
            [&](char *buffer, size_t capacity, size_t *required) {
                ++calls;
                return CopyText(text, buffer, capacity, required);
            });
        ASSERT_TRUE(value);
        EXPECT_EQ(value.Value(), text);
        EXPECT_LE(calls, 2u);
    }
}

TEST(ApiBufferTest, RetriesGrowthAndAcceptsAShorterFinalValue) {
    unsigned calls = 0;
    auto value =
        BML::Detail::ReadBuffer<std::string>([&](char *buffer, size_t capacity, size_t *required) {
            if (++calls <= 2) {
                *required = capacity + 37;
                return BML_ERROR_BUFFER_TOO_SMALL;
            }
            return CopyText("updated", buffer, capacity, required);
        });
    ASSERT_TRUE(value);
    EXPECT_EQ(value.Value(), "updated");
    EXPECT_EQ(calls, 3u);
}

TEST(ApiBufferTest, DoesNotLoopIndefinitelyWhenTheValueKeepsGrowing) {
    unsigned calls = 0;
    auto value =
        BML::Detail::ReadBuffer<std::string>([&](char *, size_t capacity, size_t *required) {
            ++calls;
            *required = capacity + 1;
            return BML_ERROR_BUFFER_TOO_SMALL;
        });
    EXPECT_EQ(value.Code(), BML_ERROR_BUFFER_TOO_SMALL);
    EXPECT_EQ(calls, 3u);
}

TEST(ApiBufferTest, PreservesHostErrorsOnTheInitialReadAndOnRetry) {
    for (int error : {BML_ERROR_INVALID_HANDLE, BML_ERROR_UNAVAILABLE, BML_ERROR_FROZEN}) {
        auto first =
            BML::Detail::ReadBuffer<std::string>([&](char *, size_t, size_t *) { return error; });
        EXPECT_EQ(first.Code(), error);
        unsigned calls = 0;
        auto retry =
            BML::Detail::ReadBuffer<std::string>([&](char *, size_t capacity, size_t *required) {
                *required = capacity + 1;
                return ++calls == 1 ? BML_ERROR_BUFFER_TOO_SMALL : error;
            });
        EXPECT_EQ(retry.Code(), error);
        EXPECT_EQ(calls, 2u);
    }
}

TEST(ApiBufferTest, RejectsInvalidSuccessLengthsAndMissingTerminators) {
    for (int invalid = 0; invalid != 3; ++invalid) {
        auto value = BML::Detail::ReadBuffer<std::string>(
            [&](char *buffer, size_t capacity, size_t *required) {
                std::memset(buffer, 'x', capacity);
                *required = invalid == 0 ? 0 : invalid == 1 ? capacity + 1 : capacity;
                return BML_OK;
            });
        EXPECT_EQ(value.Code(), BML_ERROR_MALFORMED_MESSAGE);
    }
}

TEST(ApiBufferTest, RejectsOversizedAllocationRequests) {
    auto value = BML::Detail::ReadBuffer<std::string>([](char *, size_t, size_t *required) {
        *required = std::numeric_limits<size_t>::max();
        return BML_ERROR_BUFFER_TOO_SMALL;
    });
    EXPECT_EQ(value.Code(), BML_ERROR_OUT_OF_MEMORY);
}

TEST(ApiBufferTest, PreservesBinaryResourcesIncludingZeroBytesAndEmptyValues) {
    for (const std::vector<uint8_t> bytes : {std::vector<uint8_t>{}, {1, 0, 2, 0, 3}}) {
        unsigned calls = 0;
        auto value = BML::Detail::ReadBuffer<std::vector<uint8_t>>(
            [&](uint8_t *buffer, size_t capacity, size_t *required) {
                ++calls;
                *required = bytes.size();
                if (capacity < bytes.size())
                    return BML_ERROR_BUFFER_TOO_SMALL;
                if (!bytes.empty())
                    std::memcpy(buffer, bytes.data(), bytes.size());
                return BML_OK;
            });
        ASSERT_TRUE(value);
        EXPECT_EQ(value.Value(), bytes);
        EXPECT_EQ(calls, 2u);
    }
}

} // namespace

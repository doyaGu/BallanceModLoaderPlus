#include <gtest/gtest.h>

#include <string>

#include "BML/DataShare.h"

namespace BML {
namespace Test {

namespace {

struct RequestProbe {
    int callbacks = 0;
    int cleanups = 0;
    std::string key;
    std::string value;
    size_t size = 0;
    bool exists = false;
};

size_t BoundedStringLength(const char *value, size_t maxSize) {
    size_t length = 0;
    while (length < maxSize && value[length] != '\0')
        ++length;
    return length;
}

void __cdecl RecordRequest(const char *key, const void *data, size_t size, void *userdata) {
    auto *probe = static_cast<RequestProbe *>(userdata);
    ASSERT_NE(nullptr, probe);
    probe->callbacks++;
    probe->key = key ? key : "";
    probe->size = size;
    probe->exists = data != nullptr && size > 0;
    if (data && size > 0)
        probe->value.assign(static_cast<const char *>(data), BoundedStringLength(static_cast<const char *>(data), size));
}

void __cdecl RecordCleanup(const char *, void *userdata) {
    auto *probe = static_cast<RequestProbe *>(userdata);
    ASSERT_NE(nullptr, probe);
    probe->cleanups++;
}

class DataShareTest : public ::testing::Test {
protected:
    void SetUp() override { BML_DataShare_DestroyAll(); }
    void TearDown() override { BML_DataShare_DestroyAll(); }
};

} // namespace

TEST_F(DataShareTest, RequestFiresOnceAndCleansUp) {
    BML_DataShare *share = BML_GetDataShare("reload-lifecycle");
    ASSERT_NE(nullptr, share);

    RequestProbe probe;
    BML_DataShare_Request(share, "pending-key", RecordRequest, &probe, RecordCleanup);

    const char first[] = "first";
    EXPECT_EQ(1, BML_DataShare_Set(share, "pending-key", first, sizeof(first)));
    const char second[] = "second";
    EXPECT_EQ(1, BML_DataShare_Set(share, "pending-key", second, sizeof(second)));

    EXPECT_EQ(1, probe.callbacks);
    EXPECT_EQ(1, probe.cleanups);
    EXPECT_EQ("pending-key", probe.key);
    EXPECT_TRUE(probe.exists);
    EXPECT_EQ("first", probe.value);

    BML_DataShare_Release(share);
}

TEST_F(DataShareTest, RemoveWakesPendingRequestWithMissingValue) {
    BML_DataShare *share = BML_GetDataShare("reload-lifecycle");
    ASSERT_NE(nullptr, share);

    RequestProbe probe;
    BML_DataShare_Request(share, "missing-key", RecordRequest, &probe, RecordCleanup);
    BML_DataShare_Remove(share, "missing-key");

    EXPECT_EQ(1, probe.callbacks);
    EXPECT_EQ(1, probe.cleanups);
    EXPECT_EQ("missing-key", probe.key);
    EXPECT_FALSE(probe.exists);
    EXPECT_EQ(0u, probe.size);

    BML_DataShare_Release(share);
}

TEST_F(DataShareTest, DestroyAllCancelsPendingRequestWithoutCallback) {
    BML_DataShare *share = BML_GetDataShare("reload-lifecycle");
    ASSERT_NE(nullptr, share);

    RequestProbe probe;
    BML_DataShare_Request(share, "never-set", RecordRequest, &probe, RecordCleanup);
    BML_DataShare_DestroyAll();

    EXPECT_EQ(0, probe.callbacks);
    EXPECT_EQ(1, probe.cleanups);
}

} // namespace Test
} // namespace BML

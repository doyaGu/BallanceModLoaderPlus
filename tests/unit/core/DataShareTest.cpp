#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "BML/DataShare.h"
#include "BML/DataShare.hpp"
#include "CustomMaps/CustomMapLoad.h"
#include "DataShare/DataShare.h"
#include "Loader/ModInvocationGate.h"

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

struct ReentrantRetireProbe : RequestProbe {
    const char *owner = nullptr;
    bool retiredInsideCallback = true;
};

struct InvocationProbe : RequestProbe {
    ModInvocationGate *gate = nullptr;
    bool callbackProtected = false;
    bool cleanupProtected = false;
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

void __cdecl ThrowingRequest(const char *key, const void *data, size_t size, void *userdata) {
    RecordRequest(key, data, size, userdata);
    throw std::runtime_error("request failed");
}

void __cdecl ThrowingCleanup(const char *, void *userdata) {
    auto *probe = static_cast<RequestProbe *>(userdata);
    ASSERT_NE(nullptr, probe);
    probe->cleanups++;
    throw std::runtime_error("cleanup failed");
}

void __cdecl RejectedRequest(const char *, const void *, size_t, void *userdata) {
    auto *probe = static_cast<RequestProbe *>(userdata);
    ASSERT_NE(nullptr, probe);
    probe->callbacks++;
}

void __cdecl RejectedCleanup(const char *, void *userdata) {
    auto *probe = static_cast<RequestProbe *>(userdata);
    ASSERT_NE(nullptr, probe);
    probe->cleanups++;
}

bool ResolveValidatedOwner(const void *, const char *,
                           const void *callbackAddress,
                           const void *cleanupAddress,
                           std::string &owner) {
    if (callbackAddress == reinterpret_cast<const void *>(&RejectedRequest) ||
        cleanupAddress == reinterpret_cast<const void *>(&RejectedCleanup)) {
        return false;
    }
    owner = "validated.owner";
    return true;
}

class OwnerResolverScope final {
public:
    OwnerResolverScope() {
        DataShareStore::SetOwnerResolver(&ResolveValidatedOwner);
    }

    ~OwnerResolverScope() { DataShareStore::SetOwnerResolver(nullptr); }

    OwnerResolverScope(const OwnerResolverScope &) = delete;
    OwnerResolverScope &operator=(const OwnerResolverScope &) = delete;
};

void __cdecl TryRetireFromCallback(const char *key, const void *data,
                                   size_t size, void *userdata) {
    auto *probe = static_cast<ReentrantRetireProbe *>(userdata);
    ASSERT_NE(nullptr, probe);
    RecordRequest(key, data, size, probe);
    probe->retiredInsideCallback =
        DataShareStore::RetireCallbacksFromOwner(probe->owner);
}

void __cdecl RecordProtectedRequest(const char *key, const void *data,
                                    size_t size, void *userdata) {
    auto *probe = static_cast<InvocationProbe *>(userdata);
    ASSERT_NE(nullptr, probe);
    probe->callbackProtected = probe->gate->IsCallActiveOnCurrentThread();
    RecordRequest(key, data, size, probe);
}

void __cdecl RecordProtectedCleanup(const char *key, void *userdata) {
    auto *probe = static_cast<InvocationProbe *>(userdata);
    ASSERT_NE(nullptr, probe);
    probe->cleanupProtected = probe->gate->IsCallActiveOnCurrentThread();
    RecordCleanup(key, probe);
}

class DataShareTest : public ::testing::Test {
protected:
    void SetUp() override {
        DataShareStore::SetInvocationGate(nullptr);
        DataShareStore::ResetRegistryForTests();
    }

    void TearDown() override {
        DataShareStore::SetInvocationGate(nullptr);
        DataShareStore::ResetRegistryForTests();
    }
};

} // namespace

TEST_F(DataShareTest, RequestFiresOnceAndCleansUp) {
    BML_DataShare *share = BML_GetDataShare("reload-lifecycle");
    ASSERT_NE(nullptr, share);

    RequestProbe probe;
    const BML_DataShareRequest request = BML_DataShare_Request(
        share, "pending-key", RecordRequest, &probe, RecordCleanup);
    EXPECT_NE(BML_DATASHARE_INVALID_REQUEST, request);

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

TEST_F(DataShareTest, InternalResetKeepsOutstandingHandleValid) {
    BML_DataShare *share = BML_GetDataShare("reload-lifecycle");
    ASSERT_NE(nullptr, share);

    RequestProbe probe;
    BML_DataShare_Request(share, "never-set", RecordRequest, &probe, RecordCleanup);
    DataShareStore::ResetRegistryForTests();

    EXPECT_EQ(0, probe.callbacks);
    EXPECT_EQ(1, probe.cleanups);

    const char value[] = "still-valid";
    EXPECT_EQ(1, BML_DataShare_Set(share, "after-reset", value, sizeof(value)));
    BML_DataShare_Release(share);
}

TEST_F(DataShareTest, RequestCanBeCancelledExactlyOnce) {
    BML_DataShare *share = BML_GetDataShare("request-cancel");
    ASSERT_NE(nullptr, share);

    RequestProbe probe;
    const BML_DataShareRequest request = BML_DataShare_Request(
        share, "pending-key", RecordRequest, &probe, RecordCleanup);
    ASSERT_NE(BML_DATASHARE_INVALID_REQUEST, request);

    EXPECT_EQ(1, BML_DataShare_CancelRequest(share, request));
    EXPECT_EQ(0, BML_DataShare_CancelRequest(share, request));
    EXPECT_EQ(0, probe.callbacks);
    EXPECT_EQ(1, probe.cleanups);

    const char value[] = "ignored";
    EXPECT_EQ(1, BML_DataShare_Set(share, "pending-key", value, sizeof(value)));
    EXPECT_EQ(0, probe.callbacks);
    EXPECT_EQ(1, probe.cleanups);
    BML_DataShare_Release(share);
}

TEST_F(DataShareTest, CppFacadeOwnsHandlesAndPendingRequests) {
    RequestProbe probe;
    DataShare share("cpp-facade");
    ASSERT_TRUE(share);

    DataShare copy = share;
    const char value[] = "value";
    ASSERT_TRUE(copy.Set("present", value, sizeof(value)));
    EXPECT_TRUE(share.Has("present"));

    DataShareRequest request = share.Request(
        "pending", &RecordRequest, &probe, &RecordCleanup);
    ASSERT_TRUE(request);
    EXPECT_TRUE(request.Cancel());
    EXPECT_FALSE(request);
    EXPECT_EQ(probe.callbacks, 0);
    EXPECT_EQ(probe.cleanups, 1);
}

TEST_F(DataShareTest, EmptyNameUsesTheLoaderDataShare) {
    DataShare defaultShare;
    DataShare emptyNameShare("");
    ASSERT_TRUE(defaultShare);
    ASSERT_TRUE(emptyNameShare);

    const char value[] = "shared";
    ASSERT_TRUE(defaultShare.Set("empty-name", value, sizeof(value)));
    EXPECT_TRUE(emptyNameShare.Has("empty-name"));
}

TEST_F(DataShareTest, EmptyValueRemainsPresentButReadsAsNull) {
    DataShare share("empty-value");
    ASSERT_TRUE(share);
    ASSERT_TRUE(share.Set("key", nullptr, 0));

    std::size_t size = 1;
    EXPECT_EQ(share.Get("key", &size), nullptr);
    EXPECT_EQ(size, 0u);
    EXPECT_TRUE(share.Has("key"));
    EXPECT_EQ(share.SizeOf("key"), 0u);
    EXPECT_TRUE(share.Copy("key", nullptr, 0));
}

TEST_F(DataShareTest, CppFacadeDoesNotKeepAnImmediateRequest) {
    DataShare share("cpp-facade-immediate");
    ASSERT_TRUE(share);
    const char value[] = "ready";
    ASSERT_TRUE(share.Set("present", value, sizeof(value)));

    RequestProbe probe;
    DataShareRequest request = share.Request(
        "present", &RecordRequest, &probe, &RecordCleanup);
    EXPECT_FALSE(request);
    EXPECT_EQ(probe.callbacks, 1);
    EXPECT_EQ(probe.cleanups, 1);
    EXPECT_EQ(probe.value, "ready");
}

TEST_F(DataShareTest, RequestCannotBeCancelledByAnotherOwner) {
    BML_DataShare *share = BML_GetDataShare("request-owner");
    ASSERT_NE(nullptr, share);
    auto *implementation = reinterpret_cast<DataShareStore *>(share);
    ASSERT_TRUE(DataShareStore::ActivateCallbacksFromOwner("first.owner"));
    ASSERT_TRUE(DataShareStore::ActivateCallbacksFromOwner("other.owner"));

    RequestProbe probe;
    const BML_DataShareRequest request = implementation->Request(
        "pending-key", RecordRequest, RecordCleanup, &probe, "first.owner");
    ASSERT_NE(BML_DATASHARE_INVALID_REQUEST, request);

    const std::string otherOwner = "other.owner";
    const std::string firstOwner = "first.owner";
    EXPECT_FALSE(implementation->CancelRequest(request, &otherOwner));
    EXPECT_EQ(0, probe.cleanups);
    EXPECT_TRUE(implementation->CancelRequest(request, &firstOwner));
    EXPECT_EQ(1, probe.cleanups);

    BML_DataShare_Release(share);
}

TEST_F(DataShareTest, RetiringOwnerCancelsOnlyItsRequests) {
    BML_DataShare *share = BML_GetDataShare("owner-retirement");
    ASSERT_NE(nullptr, share);
    auto *implementation = reinterpret_cast<DataShareStore *>(share);
    ASSERT_TRUE(DataShareStore::ActivateCallbacksFromOwner("retired.owner"));
    ASSERT_TRUE(DataShareStore::ActivateCallbacksFromOwner("active.owner"));

    RequestProbe retired;
    RequestProbe active;
    EXPECT_NE(BML_DATASHARE_INVALID_REQUEST,
              implementation->Request("key", RecordRequest, RecordCleanup,
                                      &retired, "retired.owner"));
    EXPECT_NE(BML_DATASHARE_INVALID_REQUEST,
              implementation->Request("key", RecordRequest, RecordCleanup,
                                      &active, "active.owner"));

    ASSERT_TRUE(DataShareStore::RetireCallbacksFromOwner("retired.owner"));
    EXPECT_EQ(0, retired.callbacks);
    EXPECT_EQ(1, retired.cleanups);
    EXPECT_EQ(0, active.cleanups);

    const char value[] = "value";
    EXPECT_EQ(1, BML_DataShare_Set(share, "key", value, sizeof(value)));
    EXPECT_EQ(0, retired.callbacks);
    EXPECT_EQ(1, retired.cleanups);
    EXPECT_EQ(1, active.callbacks);
    EXPECT_EQ(1, active.cleanups);
    BML_DataShare_Release(share);
}

TEST_F(DataShareTest, OwnerCannotRetireFromInsideItsCallback) {
    BML_DataShare *share = BML_GetDataShare("owner-reentrant-retirement");
    ASSERT_NE(nullptr, share);
    auto *implementation = reinterpret_cast<DataShareStore *>(share);
    constexpr const char *owner = "callback.owner";
    ASSERT_TRUE(DataShareStore::ActivateCallbacksFromOwner(owner));

    ReentrantRetireProbe probe;
    probe.owner = owner;
    ASSERT_NE(BML_DATASHARE_INVALID_REQUEST,
              implementation->Request("key", TryRetireFromCallback,
                                      RecordCleanup, &probe, owner));

    const char value[] = "value";
    EXPECT_EQ(1, BML_DataShare_Set(share, "key", value, sizeof(value)));
    EXPECT_FALSE(probe.retiredInsideCallback);
    EXPECT_EQ(1, probe.callbacks);
    EXPECT_EQ(1, probe.cleanups);
    EXPECT_TRUE(DataShareStore::RetireCallbacksFromOwner(owner));
    BML_DataShare_Release(share);
}

TEST_F(DataShareTest, CallbackAndCleanupEnterModInvocationGate) {
    ModInvocationGate gate;
    DataShareStore::SetInvocationGate(&gate);
    BML_DataShare *share = BML_GetDataShare("invocation-gate");
    ASSERT_NE(nullptr, share);

    InvocationProbe probe;
    probe.gate = &gate;
    ASSERT_NE(BML_DATASHARE_INVALID_REQUEST,
              BML_DataShare_Request(share, "key", RecordProtectedRequest,
                                    &probe, RecordProtectedCleanup));

    const char value[] = "value";
    EXPECT_EQ(BML_DataShare_Set(share, "key", value, sizeof(value)), 1);
    EXPECT_TRUE(probe.callbackProtected);
    EXPECT_TRUE(probe.cleanupProtected);
    EXPECT_EQ(probe.callbacks, 1);
    EXPECT_EQ(probe.cleanups, 1);

    BML_DataShare_Release(share);
}

TEST_F(DataShareTest, RetiredOwnerRequestStillRunsCleanup) {
    BML_DataShare *share = BML_GetDataShare("retired-owner-request");
    ASSERT_NE(nullptr, share);
    auto *implementation = reinterpret_cast<DataShareStore *>(share);
    constexpr const char *owner = "retired.owner";
    ASSERT_TRUE(DataShareStore::ActivateCallbacksFromOwner(owner));
    ASSERT_TRUE(DataShareStore::RetireCallbacksFromOwner(owner));

    RequestProbe probe;
    EXPECT_EQ(implementation->Request(
                  "pending", RecordRequest, RecordCleanup, &probe, owner),
              BML_DATASHARE_INVALID_REQUEST);
    EXPECT_EQ(probe.callbacks, 0);
    EXPECT_EQ(probe.cleanups, 1);
    BML_DataShare_Release(share);
}

TEST_F(DataShareTest, RequestCallbacksDoNotThrowThroughCApi) {
    BML_DataShare *share = BML_GetDataShare("reload-lifecycle");
    ASSERT_NE(nullptr, share);

    RequestProbe probe;
    BML_DataShare_Request(share, "throwing-key", ThrowingRequest, &probe, RecordCleanup);

    const char value[] = "value";
    EXPECT_NO_THROW(EXPECT_EQ(1, BML_DataShare_Set(share, "throwing-key", value, sizeof(value))));

    EXPECT_EQ(1, probe.callbacks);
    EXPECT_EQ(1, probe.cleanups);
    EXPECT_EQ("throwing-key", probe.key);
    EXPECT_TRUE(probe.exists);

    BML_DataShare_Release(share);
}

TEST_F(DataShareTest, CleanupDoesNotThrowThroughCApi) {
    BML_DataShare *share = BML_GetDataShare("reload-lifecycle");
    ASSERT_NE(nullptr, share);

    RequestProbe probe;
    EXPECT_NO_THROW(BML_DataShare_Request(share, "", RecordRequest, &probe, ThrowingCleanup));

    EXPECT_EQ(0, probe.callbacks);
    EXPECT_EQ(1, probe.cleanups);

    BML_DataShare_Release(share);
}

TEST_F(DataShareTest, RejectedCallbackOnlyInvokesValidatedCleanup) {
    OwnerResolverScope resolver;
    ASSERT_TRUE(DataShareStore::ActivateCallbacksFromOwner("validated.owner"));
    BML_DataShare *share = BML_GetDataShare("rejected-callback");
    ASSERT_NE(nullptr, share);

    RequestProbe probe;
    EXPECT_EQ(BML_DataShare_Request(
                  share, "key", RejectedRequest, &probe, RecordCleanup),
              BML_DATASHARE_INVALID_REQUEST);
    EXPECT_EQ(probe.callbacks, 0);
    EXPECT_EQ(probe.cleanups, 1);

    BML_DataShare_Release(share);
}

TEST_F(DataShareTest, RejectedCleanupAddressIsNeverInvoked) {
    OwnerResolverScope resolver;
    ASSERT_TRUE(DataShareStore::ActivateCallbacksFromOwner("validated.owner"));
    BML_DataShare *share = BML_GetDataShare("rejected-cleanup");
    ASSERT_NE(nullptr, share);

    RequestProbe probe;
    EXPECT_EQ(BML_DataShare_Request(
                  share, "key", RecordRequest, &probe, RejectedCleanup),
              BML_DATASHARE_INVALID_REQUEST);
    EXPECT_EQ(probe.callbacks, 0);
    EXPECT_EQ(probe.cleanups, 0);

    BML_DataShare_Release(share);
}

TEST_F(DataShareTest, EmptyExplicitOwnerInvokesValidatedCleanup) {
    OwnerResolverScope resolver;
    ASSERT_TRUE(DataShareStore::ActivateCallbacksFromOwner("validated.owner"));
    BML_DataShare *share = BML_GetDataShare("empty-explicit-owner");
    ASSERT_NE(nullptr, share);

    RequestProbe probe;
    EXPECT_EQ(BML_DataShare_RequestForOwner(
                  share, "", "key", RecordRequest, &probe, RecordCleanup),
              BML_DATASHARE_INVALID_REQUEST);
    EXPECT_EQ(probe.callbacks, 0);
    EXPECT_EQ(probe.cleanups, 1);

    BML_DataShare_Release(share);
}

TEST_F(DataShareTest, CustomMapLoadProtocolRoundTripsAttemptIdentityAndOutcome) {
    BML_DataShare *share = BML_GetDataShare("custom-map-load-protocol");
    ASSERT_NE(nullptr, share);

    ASSERT_TRUE(CustomMapLoad::WriteRequest(share, 42));
    CustomMapLoad::Request request;
    ASSERT_TRUE(CustomMapLoad::ReadRequest(share, request));
    EXPECT_EQ(request.Attempt, 42u);

    ASSERT_TRUE(CustomMapLoad::WriteResult(
        share, request.Attempt, CustomMapLoad::Outcome::Failed));
    CustomMapLoad::Result result;
    ASSERT_TRUE(CustomMapLoad::ReadResult(share, result));
    EXPECT_EQ(result.Attempt, request.Attempt);
    EXPECT_EQ(result.Value, CustomMapLoad::Outcome::Failed);

    BML_DataShare_Release(share);
}

} // namespace Test
} // namespace BML

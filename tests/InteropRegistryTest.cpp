#include "InteropRegistry.h"
#include "InteropSessionService.h"
#include "BML/Generated/bml_runtime_api.h"

#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>

#include <gtest/gtest.h>

namespace {

constexpr BML_InteropFieldDescriptor kSnapshotFields[] = {
    {1, "value", BML_INTEROP_FIELD_INT, 0},
    {2, "label", BML_INTEROP_FIELD_STRING, 1},
};
constexpr BML_InteropFieldDescriptor kQueryFields[] = {
    {1, "needle", BML_INTEROP_FIELD_STRING, 0},
};
constexpr BML_InteropSchemaDescriptor kSchemas[] = {
    {1, "snapshot", kSnapshotFields, 2},
    {2, "query", kQueryFields, 1},
};
constexpr BML_InteropEndpointDescriptor kEndpoints[] = {
    {"state", BML_INTEROP_ENDPOINT_RESOURCE, 0, 1, 0},
    {"nested", BML_INTEROP_ENDPOINT_RESOURCE, 0, 1, 0},
    {"updates", BML_INTEROP_ENDPOINT_STREAM, 0, 1, 0},
    {"find", BML_INTEROP_ENDPOINT_QUERY, 2, 1, 0},
};
constexpr BML_InteropApiDescriptor kApi = {
    sizeof(BML_InteropApiDescriptor), "test.api", 1, 0, 0xA340BB9A58AA04E2ULL,
    kSchemas, 2, kEndpoints, 4,
    nullptr, 0,
};

constexpr uint64_t kCompatibleHashes[] = {0x9E3779B97F4A7C14ULL};
constexpr BML_InteropApiDescriptor kCompatibleApi = {
    sizeof(BML_InteropApiDescriptor), "test.compatible", 1, 1, 0x461A4626223FE8F9ULL,
    kSchemas, 2, kEndpoints, 4,
    kCompatibleHashes, 1,
};

struct Provider {
    BML::InteropRegistry *Registry = nullptr;
    const BML_InteropCallContext *Consumer = nullptr;
    int Value = 7;
    std::string Label;
};

int SetInt(Provider *provider, BML_InteropRecordBuilder *record, int value) {
    return provider->Registry->BuilderSetValue(record, 1, BML_INTEROP_FIELD_INT, &value, 1);
}

int ReadState(const BML_InteropProviderRequest *request, BML_InteropRecordBuilder *record, void *userdata) {
    Provider *provider = static_cast<Provider *>(userdata);
    if (!provider)
        return BML_ERROR_INVALID_PARAMETER;
    if (request && std::strcmp(request->Endpoint, "nested") == 0) {
        if (!provider->Consumer)
            return BML_ERROR_INTEROP_UNSUPPORTED;
        BML_RecordRef nested{};
        int status = provider->Registry->ReadResource(provider->Consumer, "test.api", "state", &nested);
        if (status != BML_OK)
            return status;
        int value = 0;
        status = provider->Registry->RecordGetInt(provider->Consumer, nested, 1, &value);
        const int releaseStatus = provider->Registry->ReleaseRecord(provider->Consumer, nested);
        if (status != BML_OK)
            return status;
        if (releaseStatus != BML_OK)
            return releaseStatus;
        return SetInt(provider, record, value);
    }
    const int status = SetInt(provider, record, provider->Value);
    if (status != BML_OK || provider->Label.empty())
        return status;
    return provider->Registry->BuilderSetValue(record, 2, BML_INTEROP_FIELD_STRING,
                                               provider->Label.data(), provider->Label.size());
}

int InvokeFind(const BML_InteropProviderRequest *request, BML_InteropRecordBuilder *record, void *userdata) {
    Provider *provider = static_cast<Provider *>(userdata);
    if (!provider || !request || !request->Input)
        return BML_ERROR_INVALID_PARAMETER;
    size_t required = 0;
    int status = provider->Registry->RecordViewGetString(request->Input, 1, nullptr, 0, &required);
    if (status != BML_OK)
        return status;
    std::string needle(required ? required - 1 : 0, '\0');
    status = provider->Registry->RecordViewGetString(request->Input, 1,
                                                       needle.empty() ? nullptr : needle.data(),
                                                       needle.size() + 1, &required);
    if (status != BML_OK)
        return status;
    return SetInt(provider, record, static_cast<int>(needle.size()));
}

BML_InteropProviderCallbacks Callbacks() {
    BML_InteropProviderCallbacks callbacks{};
    callbacks.Size = sizeof(callbacks);
    callbacks.ReadResource = &ReadState;
    callbacks.InvokeQuery = &InvokeFind;
    return callbacks;
}

constexpr BML_InteropFieldDescriptor kProbeFields[] = {
    {1, "value", BML_INTEROP_FIELD_INT, 0},
};
constexpr BML_InteropSchemaDescriptor kProbeSchemas[] = {
    {1, "snapshot", kProbeFields, 1},
};
constexpr BML_InteropEndpointDescriptor kProbeEndpoints[] = {
    {"state", BML_INTEROP_ENDPOINT_RESOURCE, 0, 1, 1},
};
constexpr BML_InteropApiDescriptor kProbeApi = {
    sizeof(BML_InteropApiDescriptor), "test.probe", 1, 0, 0x7BC60E814B4E1B3DULL,
    kProbeSchemas, 1, kProbeEndpoints, 1,
    nullptr, 0,
};

struct ReentrantProbeProvider {
    BML::InteropRegistry *Registry = nullptr;
    bool ReadCalled = false;
};

int UnregisterDuringProbe(const BML_InteropProviderRequest *, void *userdata) {
    auto *provider = static_cast<ReentrantProbeProvider *>(userdata);
    return provider->Registry->UnregisterProvider("provider", "test.probe");
}

int ReadAfterReentrantProbe(const BML_InteropProviderRequest *, BML_InteropRecordBuilder *, void *userdata) {
    static_cast<ReentrantProbeProvider *>(userdata)->ReadCalled = true;
    return BML_OK;
}

int ThrowingRead(const BML_InteropProviderRequest *, BML_InteropRecordBuilder *, void *) {
    throw std::runtime_error("provider failure");
}

int TryDestroyCallbackBuilder(const BML_InteropProviderRequest *,
                              BML_InteropRecordBuilder *record,
                              void *userdata) {
    return static_cast<Provider *>(userdata)->Registry->DestroyRecordBuilder(record);
}

class InteropRegistryTest : public ::testing::Test {
protected:
    InteropRegistryTest() : Registry(Sessions) {}

    void SetUp() override {
        Sessions.RegisterMod("consumer");
        Sessions.RegisterMod("provider");
        ProviderState.Registry = &Registry;
        ASSERT_EQ(BML_OK, Registry.RegisterProvider("provider", &kApi, &CallbacksState, &ProviderState));
        Consumer = Sessions.CreateContextForOwner("consumer");
        ProviderState.Consumer = &Consumer;
    }

    void TearDown() override {
        EXPECT_EQ(BML_OK, Registry.InvalidateOwner("consumer"));
        EXPECT_EQ(BML_OK, Registry.InvalidateOwner("provider"));
        Sessions.InvalidateMod("consumer");
        Sessions.InvalidateMod("provider");
    }

    BML::InteropSessionService Sessions;
    BML::InteropRegistry Registry;
    Provider ProviderState;
    BML_InteropProviderCallbacks CallbacksState = Callbacks();
    BML_InteropCallContext Consumer{};
};

TEST_F(InteropRegistryTest, ReadsAtomicSnapshotAndRetainsItAfterProviderUnload) {
    BML_RecordRef record{};
    ASSERT_EQ(BML_OK, Registry.ReadResource(&Consumer, "test.api", "state", &record));
    ASSERT_NE(0u, record.Value);

    int value = 0;
    EXPECT_EQ(BML_OK, Registry.RecordGetInt(&Consumer, record, 1, &value));
    EXPECT_EQ(7, value);

    EXPECT_EQ(BML_OK, Registry.UnregisterProvider("provider", "test.api"));
    EXPECT_EQ(BML_OK, Registry.RecordGetInt(&Consumer, record, 1, &value));
    EXPECT_EQ(7, value);
    EXPECT_EQ(BML_OK, Registry.ReleaseRecord(&Consumer, record));
}

TEST_F(InteropRegistryTest, BorrowedPayloadSurvivesRecordRelease) {
    BML_RecordRef record{};
    ASSERT_EQ(BML_OK, Registry.ReadResource(&Consumer, "test.api", "state", &record));

    const void *data = nullptr;
    size_t count = 0;
    size_t elementSize = 0;
    ASSERT_EQ(BML_OK,
              Registry.RecordBorrowValue(&Consumer, record, 1, BML_INTEROP_FIELD_INT,
                                         &data, &count, &elementSize));
    ASSERT_NE(nullptr, data);
    ASSERT_EQ(BML_OK, Registry.ReleaseRecord(&Consumer, record));
    EXPECT_EQ(7, *static_cast<const int *>(data));
    EXPECT_EQ(1u, count);
    EXPECT_EQ(sizeof(int), elementSize);
}

TEST_F(InteropRegistryTest, StringRoundTripPreservesEmbeddedNul) {
    ProviderState.Label.assign("ab\0cd", 5);
    BML_RecordRef record{};
    ASSERT_EQ(BML_OK, Registry.ReadResource(&Consumer, "test.api", "state", &record));

    size_t required = 0;
    ASSERT_EQ(BML_OK, Registry.RecordGetString(&Consumer, record, 2, nullptr, 0, &required));
    ASSERT_EQ(6u, required);
    std::string storage(required, '\0');
    ASSERT_EQ(BML_OK,
              Registry.RecordGetString(&Consumer, record, 2, storage.data(), storage.size(), &required));
    EXPECT_EQ(std::string("ab\0cd", 5), std::string(storage.data(), required - 1));
    EXPECT_EQ(BML_OK, Registry.ReleaseRecord(&Consumer, record));
}

TEST_F(InteropRegistryTest, ProviderCanSynchronouslyConsumeAnotherEndpoint) {
    BML_RecordRef record{};
    ASSERT_EQ(BML_OK, Registry.ReadResource(&Consumer, "test.api", "nested", &record));

    int value = 0;
    EXPECT_EQ(BML_OK, Registry.RecordGetInt(&Consumer, record, 1, &value));
    EXPECT_EQ(7, value);
    EXPECT_EQ(BML_OK, Registry.ReleaseRecord(&Consumer, record));
}

TEST_F(InteropRegistryTest, RequiresGeneratedApiIdentityBeforeTypedUse) {
    EXPECT_EQ(BML_OK, Registry.RequireApi(&Consumer,
                                                 "test.api",
                                                 1,
                                                 0xA340BB9A58AA04E2ULL));
    EXPECT_EQ(BML_ERROR_INTEROP_API_MISMATCH,
              Registry.RequireApi(&Consumer,
                                         "test.api",
                                         2,
                                         0xA340BB9A58AA04E2ULL));
    EXPECT_EQ(BML_ERROR_INTEROP_API_MISMATCH,
              Registry.RequireApi(&Consumer,
                                         "test.api",
                                         1,
                                         0xA340BB9A58AA04E3ULL));
    EXPECT_EQ(BML_ERROR_INTEROP_ENDPOINT_NOT_FOUND,
              Registry.RequireApi(&Consumer, "test.missing", 1, 1));
}

TEST_F(InteropRegistryTest, QueryUsesSessionBoundInputSnapshot) {
    BML_InteropRecordBuilder *input = nullptr;
    ASSERT_EQ(BML_OK, Registry.CreateInputRecord(&Consumer, "test.api", 2, &input));
    ASSERT_NE(nullptr, input);
    const char needle[] = "needle";
    ASSERT_EQ(BML_OK, Registry.BuilderSetValue(input, 1, BML_INTEROP_FIELD_STRING, needle, sizeof(needle) - 1));

    BML_RecordRef result{};
    ASSERT_EQ(BML_OK, Registry.Invoke(&Consumer, "test.api", "find", BML_INTEROP_ENDPOINT_QUERY, input, &result));
    int length = 0;
    EXPECT_EQ(BML_OK, Registry.RecordGetInt(&Consumer, result, 1, &length));
    EXPECT_EQ(6, length);
    EXPECT_EQ(BML_OK, Registry.ReleaseRecord(&Consumer, result));
    EXPECT_EQ(BML_OK, Registry.DestroyRecordBuilder(input));
}

TEST_F(InteropRegistryTest, StreamDropsOldestAndInvalidatesWithConsumerSession) {
    BML_StreamRef stream{};
    ASSERT_EQ(BML_OK, Registry.OpenStream(&Consumer, "test.api", "updates", 1, &stream));

    for (int value : {1, 2}) {
        BML_InteropRecordBuilder *record = nullptr;
        ASSERT_EQ(BML_OK, Registry.CreateStreamRecord("provider", "test.api", "updates", &record));
        ASSERT_NE(nullptr, record);
        ASSERT_EQ(BML_OK, SetInt(&ProviderState, record, value));
        EXPECT_EQ(BML_OK, Registry.Publish("provider", record));
        EXPECT_EQ(BML_OK, Registry.DestroyRecordBuilder(record));
    }

    int dropped = 0;
    ASSERT_EQ(BML_OK, Registry.DroppedStreamCount(&Consumer, stream, &dropped));
    EXPECT_EQ(1, dropped);

    BML_RecordRef record{};
    ASSERT_EQ(BML_OK, Registry.PollStream(&Consumer, stream, &record));
    int value = 0;
    EXPECT_EQ(BML_OK, Registry.RecordGetInt(&Consumer, record, 1, &value));
    EXPECT_EQ(2, value);

    EXPECT_EQ(BML_OK, Registry.InvalidateOwner("consumer"));
    Sessions.InvalidateMod("consumer");
    EXPECT_EQ(BML_ERROR_INTEROP_HANDLE_STALE, Registry.RecordGetInt(&Consumer, record, 1, &value));
}

TEST(InteropApiValidationTest, RejectsACommandWithoutInputSchema) {
    BML::InteropSessionService sessions;
    BML::InteropRegistry registry(sessions);
    const BML_InteropFieldDescriptor fields[] = {{1, "value", BML_INTEROP_FIELD_INT, 0}};
    const BML_InteropSchemaDescriptor schemas[] = {{1, "record", fields, 1}};
    const BML_InteropEndpointDescriptor endpoints[] = {{"invalid", BML_INTEROP_ENDPOINT_COMMAND, 0, 1, 0}};
    const BML_InteropApiDescriptor descriptor = {
        sizeof(BML_InteropApiDescriptor), "test.invalid", 1, 0, 1,
        schemas, 1, endpoints, 1,
        nullptr, 0,
    };
    BML_InteropProviderCallbacks callbacks{};
    callbacks.Size = sizeof(callbacks);
    EXPECT_EQ(BML_ERROR_INTEROP_API_INVALID,
              registry.RegisterProvider("provider", &descriptor, &callbacks, nullptr));
}

TEST(InteropApiValidationTest, RejectsDescriptorWhoseClaimedHashDoesNotMatchItsSchema) {
    BML::InteropSessionService sessions;
    BML::InteropRegistry registry(sessions);
    sessions.RegisterMod("provider");
    BML_InteropApiDescriptor descriptor = kApi;
    descriptor.Hash ^= 0x1ULL;
    BML_InteropProviderCallbacks callbacks{};
    callbacks.Size = sizeof(callbacks);
    EXPECT_EQ(BML_ERROR_INTEROP_API_INVALID,
              registry.RegisterProvider("provider", &descriptor, &callbacks, nullptr));
}

TEST(InteropApiValidationTest, AcceptsGeneratedCanonicalDescriptorHash) {
    BML::InteropSessionService sessions;
    BML::InteropRegistry registry(sessions);
    sessions.RegisterMod("provider");
    BML_InteropProviderCallbacks callbacks{};
    callbacks.Size = sizeof(callbacks);
    EXPECT_EQ(BML_OK,
              registry.RegisterProvider("provider",
                                         &BML::Interop::Generated::Bml::Runtime::Descriptor,
                                         &callbacks,
                                         nullptr));
}

TEST(InteropApiValidationTest, AcceptsExplicitAppendOnlyPredecessorHash) {
    BML::InteropSessionService sessions;
    BML::InteropRegistry registry(sessions);
    sessions.RegisterMod("consumer");
    sessions.RegisterMod("provider");
    const BML_InteropCallContext consumer = sessions.CreateContextForOwner("consumer");
    BML_InteropProviderCallbacks callbacks{};
    callbacks.Size = sizeof(callbacks);

    ASSERT_EQ(BML_OK, registry.RegisterProvider("provider", &kCompatibleApi, &callbacks, nullptr));
    EXPECT_EQ(BML_OK,
              registry.RequireApi(&consumer, "test.compatible", 1, kCompatibleHashes[0]));
    EXPECT_EQ(BML_OK,
              registry.RequireApi(&consumer, "test.compatible", 1, kCompatibleApi.Hash));
    EXPECT_EQ(BML_ERROR_INTEROP_API_MISMATCH,
              registry.RequireApi(&consumer, "test.compatible", 1, 0x9E3779B97F4A7C13ULL));
}

TEST_F(InteropRegistryTest, RejectsProviderUnregistrationFromANonGameThread) {
    int status = BML_OK;
    std::thread worker([&] {
        status = Registry.UnregisterProvider("provider", "test.api");
    });
    worker.join();
    EXPECT_EQ(BML_ERROR_INTEROP_WRONG_THREAD, status);

    BML_RecordRef record{};
    EXPECT_EQ(BML_OK, Registry.ReadResource(&Consumer, "test.api", "state", &record));
    EXPECT_EQ(BML_OK, Registry.ReleaseRecord(&Consumer, record));
}

TEST_F(InteropRegistryTest, RejectsOwnerInvalidationFromANonGameThread) {
    int status = BML_OK;
    std::thread worker([&] {
        status = Registry.InvalidateOwner("provider");
    });
    worker.join();
    EXPECT_EQ(BML_ERROR_INTEROP_WRONG_THREAD, status);

    BML_RecordRef record{};
    EXPECT_EQ(BML_OK, Registry.ReadResource(&Consumer, "test.api", "state", &record));
    EXPECT_EQ(BML_OK, Registry.ReleaseRecord(&Consumer, record));
}

TEST(InteropRegistryCallbackTest, RevalidatesProviderAfterAReentrantProbe) {
    BML::InteropSessionService sessions;
    BML::InteropRegistry registry(sessions);
    sessions.RegisterMod("consumer");
    sessions.RegisterMod("provider");
    const BML_InteropCallContext consumer = sessions.CreateContextForOwner("consumer");
    ReentrantProbeProvider provider{&registry};
    BML_InteropProviderCallbacks callbacks{};
    callbacks.Size = sizeof(callbacks);
    callbacks.Probe = &UnregisterDuringProbe;
    callbacks.ReadResource = &ReadAfterReentrantProbe;
    ASSERT_EQ(BML_OK, registry.RegisterProvider("provider", &kProbeApi, &callbacks, &provider));

    BML_RecordRef record{};
    EXPECT_EQ(BML_ERROR_INTEROP_PROVIDER_UNLOADED,
              registry.ReadResource(&consumer, "test.probe", "state", &record));
    EXPECT_FALSE(provider.ReadCalled);
}

TEST(InteropRegistryCallbackTest, ConvertsProviderExceptionsToExecutionFailure) {
    BML::InteropSessionService sessions;
    BML::InteropRegistry registry(sessions);
    sessions.RegisterMod("consumer");
    sessions.RegisterMod("provider");
    const BML_InteropCallContext consumer = sessions.CreateContextForOwner("consumer");
    BML_InteropProviderCallbacks callbacks{};
    callbacks.Size = sizeof(callbacks);
    callbacks.ReadResource = &ThrowingRead;
    ASSERT_EQ(BML_OK, registry.RegisterProvider("provider", &kApi, &callbacks, nullptr));

    BML_RecordRef record{};
    EXPECT_EQ(BML_ERROR_INTEROP_TARGET_EXECUTION_FAILED,
              registry.ReadResource(&consumer, "test.api", "state", &record));
}

TEST(InteropRegistryBuilderTest, RejectsUnknownBuilderOwnership) {
    BML::InteropSessionService sessions;
    BML::InteropRegistry registry(sessions);
    EXPECT_EQ(BML_ERROR_INVALID_PARAMETER, registry.DestroyRecordBuilder(nullptr));
}

TEST(InteropRegistryBuilderTest, RejectsDestroyingACallbackBorrowedBuilder) {
    BML::InteropSessionService sessions;
    BML::InteropRegistry registry(sessions);
    sessions.RegisterMod("consumer");
    sessions.RegisterMod("provider");
    const BML_InteropCallContext consumer = sessions.CreateContextForOwner("consumer");
    Provider provider{&registry};
    BML_InteropProviderCallbacks callbacks{};
    callbacks.Size = sizeof(callbacks);
    callbacks.ReadResource = &TryDestroyCallbackBuilder;
    ASSERT_EQ(BML_OK, registry.RegisterProvider("provider", &kApi, &callbacks, &provider));

    BML_RecordRef record{};
    EXPECT_EQ(BML_ERROR_INVALID_PARAMETER,
              registry.ReadResource(&consumer, "test.api", "state", &record));
}

TEST_F(InteropRegistryTest, OwnerInvalidationReclaimsOutstandingBuilders) {
    BML_InteropRecordBuilder *input = nullptr;
    ASSERT_EQ(BML_OK, Registry.CreateInputRecord(&Consumer, "test.api", 2, &input));
    ASSERT_NE(nullptr, input);

    ASSERT_EQ(BML_OK, Registry.InvalidateOwner("consumer"));
    EXPECT_EQ(BML_ERROR_INVALID_PARAMETER, Registry.DestroyRecordBuilder(input));
}

} // namespace

// Runtime coverage for what the IMC generator emits: the typed Client, the
// Provider registration slots, the topic subscription, and the RPC futures. The
// mocks below stand in for the loader's IMC transport, so none of this needs a
// running loader.
//
// These tests drive test.sample, an interface that exists for them alone.
// Pinning them to one of the loader's own interfaces made every change to that
// interface churn this file, and a third-party mod is what the generated layer
// is for in the first place. tests/imc/test.sample.imc is the authoring input
// and tests/imc/generated holds the committed output.
#include "test_sample_imc.hpp"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <vector>

struct BML_ImcClient_T {};
struct BML_ImcFuture_T {
    std::vector<std::uint8_t> Data;
    BML_ImcMessage Result{};
};
struct BML_ImcSubscription_T { std::uint64_t Dropped = 0; };
struct BML_ImcResponse_T {
    std::vector<std::uint8_t> Data;
    BML_ImcPayloadTypeId PayloadType = BML_IMC_INVALID_ID;
};

namespace Sample = BML::Imc::Generated::Test::Sample;

namespace {
std::uint32_t StableId(std::string_view name) {
    std::uint32_t value = 2166136261u;
    for (const unsigned char ch : name) { value ^= ch; value *= 16777619u; }
    return value ? value : 1u;
}

std::atomic<int> g_RpcLookups{0};
std::atomic<int> g_TopicLookups{0};
std::atomic<int> g_PayloadLookups{0};
int g_FutureReleases = 0;
std::atomic<int> g_OpenClients{0};
std::atomic<int> g_CloseClients{0};
std::atomic<int> g_CloseClientStatus{BML_OK};
std::atomic<int> g_UnsubscribeStatus{BML_OK};
std::atomic<std::uint32_t> g_LastSubscribeCapacity{0};
std::mutex g_LiveClientMutex;
std::unordered_set<BML_ImcClient> g_LiveClients;
std::vector<std::uint8_t> g_LastRequest;
BML_ImcPayloadTypeId g_LastRequestPayload = 0;
std::vector<std::uint8_t> g_LastPublish;
BML_ImcPayloadTypeId g_LastPublishPayload = 0;
std::string g_ProvidedText;
void *g_LastProviderUserdata = nullptr;
BML_ImcClient g_RegisteredClient = nullptr;
BML_ImcRpcHandler g_RegisteredHandler = nullptr;
void *g_RegisteredUserdata = nullptr;
BML_ImcRpcId g_RegisteredRpc = BML_IMC_INVALID_ID;
BML_ImcExecution g_RegisteredExecution = BML_IMC_EXECUTION_GAME_THREAD;
BML_ImcTopicHandler g_TopicHandler = nullptr;
void *g_TopicUserdata = nullptr;

void ResetMock() {
    g_RpcLookups = 0; g_TopicLookups = 0; g_PayloadLookups = 0; g_FutureReleases = 0;
    g_OpenClients = 0; g_CloseClients = 0;
    g_CloseClientStatus = BML_OK; g_UnsubscribeStatus = BML_OK;
    g_LastSubscribeCapacity = 0;
    g_LastRequest.clear(); g_LastRequestPayload = 0;
    g_LastPublish.clear(); g_LastPublishPayload = 0;
    g_ProvidedText.clear(); g_LastProviderUserdata = nullptr;
    g_RegisteredClient = nullptr; g_RegisteredHandler = nullptr;
    g_RegisteredUserdata = nullptr; g_RegisteredRpc = BML_IMC_INVALID_ID;
    g_RegisteredExecution = BML_IMC_EXECUTION_GAME_THREAD;
    g_TopicHandler = nullptr; g_TopicUserdata = nullptr;
}

int EncodeScalarStateResult(BML_ImcFuture_T &future) {
    Sample::ScalarStateValue value{};
    value.Flag = true; value.Count = 7; value.Ratio = 1.5f;
    future.Data.resize(Sample::EncodedScalarStateSize(value));
    const int status = Sample::EncodeScalarState(value, future.Data.data(), future.Data.size());
    future.Result = {};
    future.Result.Size = sizeof(BML_ImcMessage);
    future.Result.Data = future.Data.data();
    future.Result.DataSize = future.Data.size();
    future.Result.PayloadType = StableId(Sample::ScalarStatePayload);
    return status;
}

int EncodeTransformStateResult(BML_ImcFuture_T &future) {
    Sample::TransformStateValue value{};
    value.Position = {1.0f, 2.0f, 3.0f}; value.Scale = {1.0f, 1.0f, 1.0f};
    value.Parent = {9, 8, 7}; value.ChildCount = 4;
    future.Data.resize(Sample::EncodedTransformStateSize(value));
    const int status = Sample::EncodeTransformState(value, future.Data.data(), future.Data.size());
    future.Result = {}; future.Result.Size = sizeof(BML_ImcMessage);
    future.Result.Data = future.Data.data(); future.Result.DataSize = future.Data.size();
    future.Result.PayloadType = StableId(Sample::TransformStatePayload); return status;
}

int EncodeArrayStateResult(BML_ImcFuture_T &future) {
    Sample::ArrayStateValue value{};
    value.Names = {"alpha.nmo", "beta.nmo"}; value.Values = {2, 4};
    future.Data.resize(Sample::EncodedArrayStateSize(value));
    const int status = Sample::EncodeArrayState(value, future.Data.data(), future.Data.size());
    future.Result = {}; future.Result.Size = sizeof(BML_ImcMessage);
    future.Result.Data = future.Data.data(); future.Result.DataSize = future.Data.size();
    future.Result.PayloadType = StableId(Sample::ArrayStatePayload); return status;
}

int ProvideScalarState(Sample::ScalarStateValue &out, void *userdata) {
    g_LastProviderUserdata = userdata;
    out.Flag = true; out.Count = 3; out.Ratio = 0.5f;
    return BML_OK;
}

int ProvideArrayState(Sample::ArrayStateValue &, void *) {
    return BML_OK;
}

int ProvideText(const Sample::TextInputValue &input, void *) {
    g_ProvidedText = input.Text;
    return BML_OK;
}
} // namespace

extern "C" {
int BML_Imc_OpenClient(const char *, BML_ImcClient *outClient) {
    if (!outClient) return BML_ERROR_INVALID_PARAMETER;
    *outClient = new (std::nothrow) BML_ImcClient_T;
    if (*outClient) {
        {
            std::lock_guard lock(g_LiveClientMutex);
            g_LiveClients.insert(*outClient);
        }
        ++g_OpenClients;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return *outClient ? BML_OK : BML_ERROR_OUT_OF_MEMORY;
}
int BML_Imc_CloseClient(BML_ImcClient client) {
    if (!client) return BML_ERROR_INVALID_HANDLE;
    const int configured = g_CloseClientStatus.load(std::memory_order_relaxed);
    if (configured != BML_OK) return configured;
    {
        std::lock_guard lock(g_LiveClientMutex);
        if (g_LiveClients.erase(client) == 0) return BML_ERROR_INVALID_HANDLE;
    }
    ++g_CloseClients;
    if (g_RegisteredClient == client) {
        g_RegisteredClient = nullptr;
        g_RegisteredHandler = nullptr;
        g_RegisteredUserdata = nullptr;
        g_RegisteredRpc = BML_IMC_INVALID_ID;
    }
    delete client;
    return BML_OK;
}
int BML_Imc_GetRpcId(BML_ImcClient, const char *name, BML_ImcRpcId *outId) {
    ++g_RpcLookups; if (!name || !outId) return BML_ERROR_INVALID_PARAMETER; *outId = StableId(name); return BML_OK;
}
int BML_Imc_GetTopicId(BML_ImcClient, const char *name, BML_ImcTopicId *outId) {
    ++g_TopicLookups; if (!name || !outId) return BML_ERROR_INVALID_PARAMETER; *outId = StableId(name); return BML_OK;
}
int BML_Imc_GetPayloadTypeId(BML_ImcClient, const char *name, BML_ImcPayloadTypeId *outId) {
    ++g_PayloadLookups; if (!name || !outId) return BML_ERROR_INVALID_PARAMETER; *outId = StableId(name); return BML_OK;
}
int BML_Imc_IsRpcAvailable(BML_ImcClient client, BML_ImcRpcId rpcId, int *outAvailable) {
    if (!client) return BML_ERROR_INVALID_HANDLE;
    if (rpcId == BML_IMC_INVALID_ID || !outAvailable) return BML_ERROR_INVALID_PARAMETER;
    *outAvailable = g_RegisteredHandler && g_RegisteredRpc == rpcId ? 1 : 0;
    return BML_OK;
}
int BML_Imc_RegisterRpc(BML_ImcClient client, BML_ImcRpcId rpcId,
                        const BML_ImcRpcRegistrationOptions *options, BML_ImcRpcHandler handler, void *userdata) {
    if (!handler || g_RegisteredHandler) return handler ? BML_ERROR_ALREADY_EXISTS : BML_ERROR_INVALID_PARAMETER;
    g_RegisteredClient = client; g_RegisteredRpc = rpcId; g_RegisteredHandler = handler;
    g_RegisteredUserdata = userdata;
    g_RegisteredExecution = options ? options->Execution : BML_IMC_EXECUTION_GAME_THREAD;
    return BML_OK;
}
int BML_Imc_UnregisterRpc(BML_ImcClient client, BML_ImcRpcId rpcId) {
    if (!g_RegisteredHandler || rpcId != g_RegisteredRpc) return BML_ERROR_NOT_FOUND;
    if (client != g_RegisteredClient) return BML_ERROR_ACCESS_DENIED;
    g_RegisteredClient = nullptr; g_RegisteredHandler = nullptr;
    g_RegisteredUserdata = nullptr; g_RegisteredRpc = BML_IMC_INVALID_ID;
    return BML_OK;
}
int BML_Imc_ResponseReserve(BML_ImcResponse *response, std::size_t size, void **outData) {
    if (!response || !outData) return BML_ERROR_INVALID_PARAMETER;
    try { response->Data.resize(size); } catch (...) { return BML_ERROR_OUT_OF_MEMORY; }
    *outData = response->Data.data(); return BML_OK;
}
int BML_Imc_ResponseCommit(BML_ImcResponse *response, std::size_t size, BML_ImcPayloadTypeId payloadType) {
    if (!response || response->Data.size() != size) return BML_ERROR_INVALID_PARAMETER;
    response->PayloadType = payloadType; return BML_OK;
}int BML_Imc_CallRpc(BML_ImcClient, BML_ImcRpcId rpcId, const BML_ImcMessage *request,
                    const BML_ImcCallOptions *, BML_ImcFuture *outFuture) {
    if (!outFuture) return BML_ERROR_INVALID_PARAMETER;
    g_LastRequest.clear(); g_LastRequestPayload = 0;
    if (request) {
        const auto *bytes = static_cast<const std::uint8_t *>(request->Data);
        g_LastRequest.assign(bytes, bytes + request->DataSize);
        g_LastRequestPayload = request->PayloadType;
    }
    auto *future = new (std::nothrow) BML_ImcFuture_T;
    if (!future) return BML_ERROR_OUT_OF_MEMORY;
    int status = BML_ERROR_NOT_FOUND;
    if (rpcId == StableId(Sample::StateRoute)) status = EncodeScalarStateResult(*future);
    if (rpcId == StableId(Sample::WriteRoute)) status = BML_OK;
    if (rpcId == StableId(Sample::EntityRoute)) status = EncodeTransformStateResult(*future);
    if (rpcId == StableId(Sample::ArraysRoute)) status = EncodeArrayStateResult(*future);
    if (status != BML_OK) { delete future; return status; }
    *outFuture = future; return BML_OK;
}
int BML_Imc_FutureAwait(BML_ImcFuture, std::uint32_t) { return BML_OK; }
int BML_Imc_FutureGetResult(BML_ImcFuture future, BML_ImcMessage *outMessage) {
    if (!future || !outMessage) return BML_ERROR_INVALID_PARAMETER; *outMessage = future->Result; return BML_OK;
}
int BML_Imc_FutureRelease(BML_ImcFuture future) { ++g_FutureReleases; delete future; return BML_OK; }
int BML_Imc_Subscribe(BML_ImcClient, BML_ImcTopicId, const BML_ImcSubscribeOptions *options,
                      BML_ImcTopicHandler handler, void *userdata, BML_ImcSubscription *outSubscription) {
    if (!handler || !outSubscription) return BML_ERROR_INVALID_PARAMETER;
    auto *subscription = new (std::nothrow) BML_ImcSubscription_T;
    if (!subscription) return BML_ERROR_OUT_OF_MEMORY;
    g_LastSubscribeCapacity = options ? options->Capacity : 0;
    g_TopicHandler = handler; g_TopicUserdata = userdata; *outSubscription = subscription; return BML_OK;
}
int BML_Imc_Unsubscribe(BML_ImcClient, BML_ImcSubscription subscription) {
    if (!subscription) return BML_ERROR_INVALID_HANDLE;
    const int configured = g_UnsubscribeStatus.load(std::memory_order_relaxed);
    if (configured != BML_OK) return configured;
    delete subscription; g_TopicHandler = nullptr; g_TopicUserdata = nullptr; return BML_OK;
}
int BML_Imc_GetSubscriptionDroppedCount(BML_ImcClient, BML_ImcSubscription subscription, std::uint64_t *outCount) {
    if (!subscription || !outCount) return BML_ERROR_INVALID_PARAMETER; *outCount = subscription->Dropped; return BML_OK;
}int BML_Imc_Publish(BML_ImcClient, BML_ImcTopicId, const BML_ImcMessage *message, std::size_t *outDelivered) {
    if (!message) return BML_ERROR_INVALID_PARAMETER;
    const auto *bytes = static_cast<const std::uint8_t *>(message->Data);
    g_LastPublish.assign(bytes, bytes + message->DataSize); g_LastPublishPayload = message->PayloadType;
    if (outDelivered) *outDelivered = 1; return BML_OK;
}
}

TEST(ImcGeneratedClientTest, LazyClientOpensTheTransportOnce) {
    ResetMock();
    constexpr int ThreadCount = 8;
    BML::Imc::LazyClient<Sample::Client> lazy;
    std::atomic<bool> start{false};
    std::vector<std::thread> callers;
    callers.reserve(ThreadCount);
    std::array<int, ThreadCount> statuses{};
    for (int index = 0; index < ThreadCount; ++index) {
        callers.emplace_back([&, index] {
            while (!start.load(std::memory_order_acquire))
                std::this_thread::yield();
            statuses[index] = lazy.EnsureOpen("test.consumer");
        });
    }
    start.store(true, std::memory_order_release);
    for (auto &caller : callers) caller.join();
    for (const int status : statuses) EXPECT_EQ(status, BML_OK);
    EXPECT_EQ(g_OpenClients.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(lazy.Get().Close(), BML_OK);
}

TEST(ImcGeneratedClientTest, ClientCloseRetainsHandleUntilRuntimeTeardownSucceeds) {
    ResetMock();
    Sample::Client client;
    ASSERT_EQ(client.Open("test.consumer"), BML_OK);
    BML_ImcClient handle = client.Handle();
    g_CloseClientStatus = BML_ERROR_BUSY;
    EXPECT_EQ(client.Close(), BML_ERROR_BUSY);
    EXPECT_EQ(client.Handle(), handle);
    EXPECT_EQ(client.Open("replacement.consumer"), BML_ERROR_BUSY);
    EXPECT_EQ(client.Handle(), handle);
    g_CloseClientStatus = BML_OK;
    EXPECT_EQ(client.Close(), BML_OK);
    EXPECT_EQ(client.Handle(), nullptr);
}

TEST(ImcGeneratedClientTest, SubscriptionCloseCanRetryAfterBusy) {
    ResetMock();
    Sample::Client client;
    Sample::NoticesSubscription subscription;
    ASSERT_EQ(client.Open("test.consumer"), BML_OK);
    ASSERT_EQ(client.SubscribeNotices(subscription,
        [](int, Sample::NoticeValue *, const BML_ImcMessage *, void *) noexcept {},
        nullptr, 2), BML_OK);
    EXPECT_EQ(g_LastSubscribeCapacity.load(std::memory_order_relaxed), 2u);
    g_UnsubscribeStatus = BML_ERROR_BUSY;
    EXPECT_EQ(subscription.Close(), BML_ERROR_BUSY);
    EXPECT_TRUE(subscription.IsOpen());
    EXPECT_NE(g_TopicHandler, nullptr);
    g_UnsubscribeStatus = BML_OK;
    EXPECT_EQ(subscription.Close(), BML_OK);
    EXPECT_FALSE(subscription.IsOpen());
    EXPECT_EQ(g_TopicHandler, nullptr);
}

TEST(ImcGeneratedClientTest, ProviderCloseRetainsRegisteredSlotsAfterBusy) {
    ResetMock();
    Sample::Provider provider;
    ASSERT_EQ(provider.Open("test.provider"), BML_OK);
    ASSERT_EQ(provider.RegisterState(&ProvideScalarState), BML_OK);
    BML_ImcClient handle = provider.Transport().Handle();
    g_CloseClientStatus = BML_ERROR_BUSY;
    EXPECT_EQ(provider.Close(), BML_ERROR_BUSY);
    EXPECT_EQ(provider.Transport().Handle(), handle);
    EXPECT_EQ(provider.UnregisterState(), BML_OK);
    g_CloseClientStatus = BML_OK;
    EXPECT_EQ(provider.Close(), BML_OK);
}

TEST(ImcGeneratedClientTest, ProviderStartOwnsTheCommonRegistrationLifecycle) {
    ResetMock();
    Sample::Provider provider;
    Sample::Provider::Handlers handlers{};
    int ownerState = 42;
    handlers.Userdata = &ownerState;
    handlers.Execution = BML_IMC_EXECUTION_CALLER_THREAD;
    handlers.State = &ProvideScalarState;

    ASSERT_EQ(provider.Start(handlers, "test.provider"), BML_OK);
    EXPECT_TRUE(provider.IsOpen());
    EXPECT_EQ(g_OpenClients.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(g_RegisteredExecution, BML_IMC_EXECUTION_CALLER_THREAD);
    BML_ImcMessage request = BML_IMC_MESSAGE_INIT;
    BML_ImcResponse_T response;
    ASSERT_EQ(g_RegisteredHandler(
        g_RegisteredRpc, &request, &response, g_RegisteredUserdata), BML_OK);
    EXPECT_EQ(g_LastProviderUserdata, &ownerState);
    EXPECT_EQ(provider.Start(handlers, "test.provider"), BML_ERROR_ALREADY_EXISTS);
    EXPECT_EQ(g_OpenClients.load(std::memory_order_relaxed), 1);

    EXPECT_EQ(provider.Close(), BML_OK);
    EXPECT_FALSE(provider.IsOpen());
    EXPECT_EQ(g_RegisteredHandler, nullptr);
    EXPECT_EQ(g_CloseClients.load(std::memory_order_relaxed), 1);
}

TEST(ImcGeneratedClientTest, ProviderStartRejectsEmptyHandlersAndRollsBackFailure) {
    ResetMock();
    Sample::Provider provider;
    Sample::Provider::Handlers handlers{};
    EXPECT_EQ(provider.Start(handlers, "test.provider"), BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(g_OpenClients.load(std::memory_order_relaxed), 0);

    handlers.State = &ProvideScalarState;
    handlers.Execution = static_cast<BML_ImcExecution>(99);
    EXPECT_EQ(provider.Start(handlers, "test.provider"), BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(g_OpenClients.load(std::memory_order_relaxed), 0);

    handlers.Execution = BML_IMC_EXECUTION_GAME_THREAD;
    handlers.Arrays = &ProvideArrayState;
    EXPECT_EQ(provider.Start(handlers, "test.provider"), BML_ERROR_ALREADY_EXISTS);
    EXPECT_FALSE(provider.IsOpen());
    EXPECT_EQ(g_OpenClients.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(g_CloseClients.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(g_RegisteredHandler, nullptr);

    handlers.Arrays = nullptr;
    ASSERT_EQ(provider.Start(handlers, "test.provider"), BML_OK);
    EXPECT_TRUE(provider.IsOpen());
    EXPECT_EQ(provider.Close(), BML_OK);
}

TEST(ImcGeneratedClientTest, GeneratedAvailabilityIsAnAdvisoryHandlerSnapshot) {
    ResetMock();
    Sample::Client consumer;
    bool available = true;
    EXPECT_EQ(consumer.IsStateAvailable(available), BML_ERROR_INVALID_HANDLE);
    EXPECT_TRUE(available);

    Sample::Provider provider;
    ASSERT_EQ(consumer.Open("test.consumer"), BML_OK);
    ASSERT_EQ(provider.Open("test.provider"), BML_OK);
    ASSERT_EQ(consumer.IsStateAvailable(available), BML_OK);
    EXPECT_FALSE(available);

    ASSERT_EQ(provider.RegisterState(&ProvideScalarState), BML_OK);
    ASSERT_EQ(consumer.IsStateAvailable(available), BML_OK);
    EXPECT_TRUE(available);

    ASSERT_EQ(provider.UnregisterState(), BML_OK);
    ASSERT_EQ(consumer.IsStateAvailable(available), BML_OK);
    EXPECT_FALSE(available);
}

TEST(ImcGeneratedClientTest, AdoptsInternallyOwnedClientWithoutReopeningIt) {
    ResetMock();
    Sample::Client client;
    auto *raw = new BML_ImcClient_T;
    {
        std::lock_guard lock(g_LiveClientMutex);
        g_LiveClients.insert(raw);
    }
    ASSERT_EQ(client.Adopt(raw), BML_OK);
    EXPECT_EQ(client.Handle(), raw);
    EXPECT_EQ(g_OpenClients.load(std::memory_order_relaxed), 0);
    EXPECT_EQ(g_PayloadLookups.load(std::memory_order_relaxed), 7);
    EXPECT_EQ(g_RpcLookups.load(std::memory_order_relaxed), 4);
    EXPECT_EQ(g_TopicLookups.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(client.Close(), BML_OK);
    EXPECT_EQ(g_CloseClients.load(std::memory_order_relaxed), 1);
}

TEST(ImcGeneratedClientTest, ResolvesIdsOnceAndReleasesEveryRpcFuture) {
    ResetMock();
    Sample::Client client;
    ASSERT_EQ(client.Open("test.consumer"), BML_OK);
    EXPECT_EQ(g_RpcLookups.load(std::memory_order_relaxed), 4);
    EXPECT_EQ(g_PayloadLookups.load(std::memory_order_relaxed), 7);
    Sample::ScalarStateValue state{};
    ASSERT_EQ(client.CallState(state), BML_OK);
    EXPECT_TRUE(state.Flag);
    EXPECT_EQ(state.Count, 7);
    EXPECT_EQ(g_RpcLookups.load(std::memory_order_relaxed), 4);
    EXPECT_EQ(g_PayloadLookups.load(std::memory_order_relaxed), 7);
    EXPECT_EQ(g_FutureReleases, 1);
}

TEST(ImcGeneratedClientTest, TypedAsyncRpcOwnsAndDecodesFutureWithoutRawCPlumbing) {
    ResetMock();
    Sample::Client client;
    ASSERT_EQ(client.Open("test.consumer"), BML_OK);

    Sample::Client::StateFuture future;
    EXPECT_FALSE(future.IsValid());
    ASSERT_EQ(client.BeginCallState(future), BML_OK);
    EXPECT_TRUE(future.IsValid());
    EXPECT_EQ(g_FutureReleases, 0);

    Sample::ScalarStateValue state{};
    ASSERT_EQ(future.AwaitResult(state, 0), BML_OK);
    EXPECT_TRUE(state.Flag);
    EXPECT_EQ(state.Count, 7);

    EXPECT_EQ(client.BeginCallState(future), BML_ERROR_BUSY);
    EXPECT_EQ(g_FutureReleases, 0);
    EXPECT_EQ(future.Release(), BML_OK);
    EXPECT_FALSE(future.IsValid());
    EXPECT_EQ(g_FutureReleases, 1);
}

TEST(ImcGeneratedClientTest, EncodesTypedRequestForResponseLessRpc) {
    ResetMock();
    Sample::Client client;
    ASSERT_EQ(client.Open("test.consumer"), BML_OK);
    Sample::TextInputValue input{}; input.Text = "hello IMC";
    ASSERT_EQ(client.CallWrite(input), BML_OK);
    ASSERT_FALSE(g_LastRequest.empty());
    BML_ImcMessage request = BML_IMC_MESSAGE_INIT;
    request.Data = g_LastRequest.data(); request.DataSize = g_LastRequest.size();
    request.PayloadType = g_LastRequestPayload;
    Sample::TextInputValue decoded{};
    ASSERT_EQ(Sample::DecodeTextInput(request, decoded), BML_OK);
    EXPECT_EQ(decoded.Text, input.Text);
    EXPECT_EQ(g_FutureReleases, 1);
}

TEST(ImcGeneratedClientTest, PublishesTypedTopicPayload) {
    ResetMock();
    Sample::Client client;
    ASSERT_EQ(client.Open("test.publisher"), BML_OK);
    Sample::NoticeValue notice{}; notice.Kind = 42; notice.HasTags = true;
    notice.Tags = {"one", "two"};
    std::size_t delivered = 0;
    ASSERT_EQ(client.PublishNotices(notice, &delivered), BML_OK);
    EXPECT_EQ(delivered, 1u);
    EXPECT_EQ(g_LastPublishPayload, StableId(Sample::NoticePayload));
    BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
    message.Data = g_LastPublish.data(); message.DataSize = g_LastPublish.size();
    message.PayloadType = g_LastPublishPayload;
    Sample::NoticeValue decoded{};
    ASSERT_EQ(Sample::DecodeNotice(message, decoded), BML_OK);
    EXPECT_EQ(decoded.Kind, 42);
    EXPECT_TRUE(decoded.HasTags);
    EXPECT_EQ(decoded.Tags, notice.Tags);
}
TEST(ImcGeneratedClientTest, ProviderTrampolineEncodesTypedResponseDirectly) {
    ResetMock();
    Sample::Provider provider;
    ASSERT_EQ(provider.Open("test.provider"), BML_OK);
    ASSERT_EQ(provider.RegisterState(&ProvideScalarState, nullptr, BML_IMC_EXECUTION_CALLER_THREAD), BML_OK);
    ASSERT_NE(g_RegisteredHandler, nullptr);

    BML_ImcMessage request = BML_IMC_MESSAGE_INIT;
    BML_ImcResponse_T response;
    ASSERT_EQ(g_RegisteredHandler(g_RegisteredRpc, &request, &response, g_RegisteredUserdata), BML_OK);
    BML_ImcMessage result = BML_IMC_MESSAGE_INIT;
    result.Data = response.Data.data(); result.DataSize = response.Data.size(); result.PayloadType = response.PayloadType;
    Sample::ScalarStateValue decoded{};
    ASSERT_EQ(Sample::DecodeScalarState(result, decoded), BML_OK);
    EXPECT_TRUE(decoded.Flag);
    EXPECT_EQ(decoded.Count, 3);
    EXPECT_FLOAT_EQ(decoded.Ratio, 0.5f);
    EXPECT_EQ(response.PayloadType, StableId(Sample::ScalarStatePayload));
}

TEST(ImcGeneratedClientTest, ResponseLessProviderDoesNotRequireOrWriteResponse) {
    ResetMock();
    Sample::Provider provider;
    ASSERT_EQ(provider.Open("test.provider"), BML_OK);
    ASSERT_EQ(provider.RegisterWrite(
        &ProvideText, nullptr, BML_IMC_EXECUTION_CALLER_THREAD), BML_OK);

    Sample::TextInputValue input{};
    input.Text = "provider call";
    std::vector<std::uint8_t> bytes(Sample::EncodedTextInputSize(input));
    ASSERT_EQ(Sample::EncodeTextInput(input, bytes.data(), bytes.size()), BML_OK);
    BML_ImcMessage request = BML_IMC_MESSAGE_INIT;
    request.Data = bytes.data();
    request.DataSize = bytes.size();
    request.PayloadType = StableId(Sample::TextInputPayload);

    ASSERT_EQ(g_RegisteredHandler(
        g_RegisteredRpc, &request, nullptr, g_RegisteredUserdata), BML_OK);
    EXPECT_EQ(g_ProvidedText, input.Text);
}
TEST(ImcGeneratedClientTest, RpcUsesTypedObjectRequestSchema) {
    ResetMock();
    Sample::Client client;
    ASSERT_EQ(client.Open("test.consumer"), BML_OK);
    const BML_ObjectRef object{3, 20, 9};
    Sample::ObjectRequestValue input{}; input.Object = object;
    Sample::TransformStateValue transform{};
    ASSERT_EQ(client.CallEntity(input, transform), BML_OK);
    EXPECT_FLOAT_EQ(transform.Position.y, 2.0f);
    EXPECT_EQ(transform.ChildCount, 4);
    BML_ImcMessage request = BML_IMC_MESSAGE_INIT;
    request.Data = g_LastRequest.data(); request.DataSize = g_LastRequest.size(); request.PayloadType = g_LastRequestPayload;
    Sample::ObjectRequestValue decoded{};
    ASSERT_EQ(Sample::DecodeObjectRequest(request, decoded), BML_OK);
    EXPECT_EQ(decoded.Object.Domain, object.Domain);
    EXPECT_EQ(decoded.Object.Slot, object.Slot);
    EXPECT_EQ(decoded.Object.Generation, object.Generation);
    EXPECT_EQ(request.PayloadType, StableId(Sample::ObjectRequestPayload));
}

TEST(ImcGeneratedClientTest, RpcResponseCarriesCountedArrays) {
    ResetMock();
    Sample::Client client;
    ASSERT_EQ(client.Open("test.consumer"), BML_OK);
    Sample::ArrayStateValue arrays{};
    ASSERT_EQ(client.CallArrays(arrays), BML_OK);
    ASSERT_EQ(arrays.Names.size(), 2u);
    EXPECT_EQ(arrays.Names[0], "alpha.nmo");
    EXPECT_EQ(arrays.Values[1], 4);
    EXPECT_TRUE(g_LastRequest.empty());
    EXPECT_EQ(g_FutureReleases, 1);
}

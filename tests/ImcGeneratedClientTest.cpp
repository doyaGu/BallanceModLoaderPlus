#include "BML/Events.h"
#include "BML/Generated/bml_events_imc.hpp"
#include "BML/Generated/bml_gameplay_imc.hpp"
#include "BML/Generated/bml_scene_imc.hpp"
#include "BML/Generated/bml_speedrun_imc.hpp"
#include "BML/Generated/bml_runtime_imc.hpp"
#include "BML/Generated/bml_ui_imc.hpp"
#include "BML/Runtime.h"

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
std::string g_ProvidedMessage;
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
    g_ProvidedMessage.clear(); g_LastProviderUserdata = nullptr;
    g_RegisteredClient = nullptr; g_RegisteredHandler = nullptr;
    g_RegisteredUserdata = nullptr; g_RegisteredRpc = BML_IMC_INVALID_ID;
    g_RegisteredExecution = BML_IMC_EXECUTION_GAME_THREAD;
    g_TopicHandler = nullptr; g_TopicUserdata = nullptr;
}

int EncodeRuntimeResult(BML_ImcFuture_T &future) {
    namespace Runtime = BML::Imc::Generated::Bml::Runtime;
    Runtime::RuntimeStateValue value{};
    value.InGame = true; value.InLevel = true; value.Paused = false;
    value.Playing = true; value.CheatEnabled = false;
    future.Data.resize(Runtime::EncodedRuntimeStateSize(value));
    const int status = Runtime::EncodeRuntimeState(value, future.Data.data(), future.Data.size());
    future.Result = {};
    future.Result.Size = sizeof(BML_ImcMessage);
    future.Result.Data = future.Data.data();
    future.Result.DataSize = future.Data.size();
    future.Result.PayloadType = StableId(Runtime::RuntimeStatePayload);
    return status;
}

int EncodeEntityResult(BML_ImcFuture_T &future) {
    namespace Scene = BML::Imc::Generated::Bml::Scene;
    Scene::EntityTransformValue value{};
    value.Position = {1.0f, 2.0f, 3.0f}; value.Scale = {1.0f, 1.0f, 1.0f};
    value.Parent = {9, 8, 7}; value.ChildCount = 4;
    future.Data.resize(Scene::EncodedEntityTransformSize(value));
    const int status = Scene::EncodeEntityTransform(value, future.Data.data(), future.Data.size());
    future.Result = {}; future.Result.Size = sizeof(BML_ImcMessage);
    future.Result.Data = future.Data.data(); future.Result.DataSize = future.Data.size();
    future.Result.PayloadType = StableId(Scene::EntityTransformPayload); return status;
}

int EncodeCatalogResult(BML_ImcFuture_T &future) {
    namespace Gameplay = BML::Imc::Generated::Bml::Gameplay;
    Gameplay::CatalogResponseValue value{};
    value.Files = {"alpha.nmo", "beta.nmo"}; value.StartBalls = {"A", "B"};
    value.Skies = {"S", "T"}; value.Bonuses = {1, 3}; value.Music = {2, 4};
    future.Data.resize(Gameplay::EncodedCatalogResponseSize(value));
    const int status = Gameplay::EncodeCatalogResponse(value, future.Data.data(), future.Data.size());
    future.Result = {}; future.Result.Size = sizeof(BML_ImcMessage);
    future.Result.Data = future.Data.data(); future.Result.DataSize = future.Data.size();
    future.Result.PayloadType = StableId(Gameplay::CatalogResponsePayload); return status;
}

int EncodeSpeedrunStateResult(BML_ImcFuture_T &future) {
    namespace Speedrun = BML::Imc::Generated::Bml::Speedrun;
    Speedrun::TimerStateValue value{};
    value.ElapsedTime = 12.5f;
    future.Data.resize(Speedrun::EncodedTimerStateSize(value));
    const int status = Speedrun::EncodeTimerState(value, future.Data.data(), future.Data.size());
    future.Result = {}; future.Result.Size = sizeof(BML_ImcMessage);
    future.Result.Data = future.Data.data(); future.Result.DataSize = future.Data.size();
    future.Result.PayloadType = StableId(Speedrun::TimerStatePayload); return status;
}

int ProvideRuntimeState(BML::Imc::Generated::Bml::Runtime::RuntimeStateValue &out, void *userdata) {
    g_LastProviderUserdata = userdata;
    out.InGame = true; out.InLevel = false; out.Paused = true;
    out.Playing = false; out.CheatEnabled = true;
    return BML_OK;
}

int ProvideRuntimeClock(BML::Imc::Generated::Bml::Runtime::ClockStateValue &, void *) {
    return BML_OK;
}

int ProvideUiMessage(const BML::Imc::Generated::Bml::Ui::MessageInputValue &input, void *) {
    g_ProvidedMessage = input.Message;
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
    if (rpcId == StableId(BML::Imc::Generated::Bml::Runtime::StateRoute)) status = EncodeRuntimeResult(*future);
    if (rpcId == StableId(BML::Imc::Generated::Bml::Ui::MessageAddRoute)) status = BML_OK;
    if (rpcId == StableId(BML::Imc::Generated::Bml::Speedrun::StateRoute)) status = EncodeSpeedrunStateResult(*future);
    if (rpcId == StableId(BML::Imc::Generated::Bml::Scene::EntityRoute)) status = EncodeEntityResult(*future);
    if (rpcId == StableId(BML::Imc::Generated::Bml::Gameplay::CatalogRoute)) status = EncodeCatalogResult(*future);
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

TEST(ImcGeneratedClientTest, PublicFacadeInitializesClientOnce) {
    ResetMock();
    constexpr int ThreadCount = 8;
    std::atomic<bool> start{false};
    std::vector<std::thread> callers;
    callers.reserve(ThreadCount);
    std::array<int, ThreadCount> statuses{};
    for (int index = 0; index < ThreadCount; ++index) {
        callers.emplace_back([&, index] {
            while (!start.load(std::memory_order_acquire))
                std::this_thread::yield();
            statuses[index] = BML::Runtime::RequireApi();
        });
    }
    start.store(true, std::memory_order_release);
    for (auto &caller : callers) caller.join();
    for (const int status : statuses) EXPECT_EQ(status, BML_OK);
    EXPECT_EQ(g_OpenClients.load(std::memory_order_relaxed), 1);

}

TEST(ImcGeneratedClientTest, ClientCloseRetainsHandleUntilRuntimeTeardownSucceeds) {
    ResetMock();
    namespace Runtime = BML::Imc::Generated::Bml::Runtime;
    Runtime::Client client;
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
    namespace Events = BML::Imc::Generated::Bml::Events;
    Events::Client client;
    Events::AllSubscription subscription;
    ASSERT_EQ(client.Open("test.consumer"), BML_OK);
    ASSERT_EQ(client.SubscribeAll(subscription,
        [](int, Events::EventValue *, const BML_ImcMessage *, void *) noexcept {},
        nullptr, 2), BML_OK);
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
    namespace Runtime = BML::Imc::Generated::Bml::Runtime;
    Runtime::Provider provider;
    ASSERT_EQ(provider.Open("test.provider"), BML_OK);
    ASSERT_EQ(provider.RegisterState(&ProvideRuntimeState), BML_OK);
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
    namespace Runtime = BML::Imc::Generated::Bml::Runtime;
    Runtime::Provider provider;
    Runtime::Provider::Handlers handlers{};
    int ownerState = 42;
    handlers.Userdata = &ownerState;
    handlers.Execution = BML_IMC_EXECUTION_CALLER_THREAD;
    handlers.State = &ProvideRuntimeState;

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
    namespace Runtime = BML::Imc::Generated::Bml::Runtime;
    Runtime::Provider provider;
    Runtime::Provider::Handlers handlers{};
    EXPECT_EQ(provider.Start(handlers, "test.provider"), BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(g_OpenClients.load(std::memory_order_relaxed), 0);

    handlers.State = &ProvideRuntimeState;
    handlers.Execution = static_cast<BML_ImcExecution>(99);
    EXPECT_EQ(provider.Start(handlers, "test.provider"), BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(g_OpenClients.load(std::memory_order_relaxed), 0);

    handlers.Execution = BML_IMC_EXECUTION_GAME_THREAD;
    handlers.Clock = &ProvideRuntimeClock;
    EXPECT_EQ(provider.Start(handlers, "test.provider"), BML_ERROR_ALREADY_EXISTS);
    EXPECT_FALSE(provider.IsOpen());
    EXPECT_EQ(g_OpenClients.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(g_CloseClients.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(g_RegisteredHandler, nullptr);

    handlers.Clock = nullptr;
    ASSERT_EQ(provider.Start(handlers, "test.provider"), BML_OK);
    EXPECT_TRUE(provider.IsOpen());
    EXPECT_EQ(provider.Close(), BML_OK);
}

TEST(ImcGeneratedClientTest, GeneratedAvailabilityIsAnAdvisoryHandlerSnapshot) {
    ResetMock();
    namespace Runtime = BML::Imc::Generated::Bml::Runtime;
    Runtime::Client consumer;
    bool available = true;
    EXPECT_EQ(consumer.IsStateAvailable(available), BML_ERROR_INVALID_HANDLE);
    EXPECT_TRUE(available);

    Runtime::Provider provider;
    ASSERT_EQ(consumer.Open("test.consumer"), BML_OK);
    ASSERT_EQ(provider.Open("test.provider"), BML_OK);
    ASSERT_EQ(consumer.IsStateAvailable(available), BML_OK);
    EXPECT_FALSE(available);

    ASSERT_EQ(provider.RegisterState(&ProvideRuntimeState), BML_OK);
    ASSERT_EQ(consumer.IsStateAvailable(available), BML_OK);
    EXPECT_TRUE(available);

    ASSERT_EQ(provider.UnregisterState(), BML_OK);
    ASSERT_EQ(consumer.IsStateAvailable(available), BML_OK);
    EXPECT_FALSE(available);
}

TEST(ImcGeneratedClientTest, AdoptsInternallyOwnedClientWithoutReopeningIt) {
    ResetMock();
    namespace Runtime = BML::Imc::Generated::Bml::Runtime;
    Runtime::Client client;
    auto *raw = new BML_ImcClient_T;
    {
        std::lock_guard lock(g_LiveClientMutex);
        g_LiveClients.insert(raw);
    }
    ASSERT_EQ(client.Adopt(raw), BML_OK);
    EXPECT_EQ(client.Handle(), raw);
    EXPECT_EQ(g_OpenClients.load(std::memory_order_relaxed), 0);
    EXPECT_EQ(g_PayloadLookups.load(std::memory_order_relaxed), 3);
    EXPECT_EQ(g_RpcLookups.load(std::memory_order_relaxed), 3);
    EXPECT_EQ(client.Close(), BML_OK);
    EXPECT_EQ(g_CloseClients.load(std::memory_order_relaxed), 1);
}

TEST(ImcGeneratedClientTest, ResolvesIdsOnceAndReleasesEveryRpcFuture) {
    ResetMock();
    namespace Runtime = BML::Imc::Generated::Bml::Runtime;
    Runtime::Client client;
    ASSERT_EQ(client.Open("test.consumer"), BML_OK);
    EXPECT_EQ(g_RpcLookups.load(std::memory_order_relaxed), 3);
    EXPECT_EQ(g_PayloadLookups.load(std::memory_order_relaxed), 3);
    Runtime::RuntimeStateValue state{};
    ASSERT_EQ(client.CallState(state), BML_OK);
    EXPECT_TRUE(state.InGame);
    EXPECT_TRUE(state.Playing);
    EXPECT_EQ(g_RpcLookups.load(std::memory_order_relaxed), 3);
    EXPECT_EQ(g_PayloadLookups.load(std::memory_order_relaxed), 3);
    EXPECT_EQ(g_FutureReleases, 1);
}

TEST(ImcGeneratedClientTest, TypedAsyncRpcOwnsAndDecodesFutureWithoutRawCPlumbing) {
    ResetMock();
    namespace Runtime = BML::Imc::Generated::Bml::Runtime;
    Runtime::Client client;
    ASSERT_EQ(client.Open("test.consumer"), BML_OK);

    Runtime::Client::StateFuture future;
    EXPECT_FALSE(future.IsValid());
    ASSERT_EQ(client.BeginCallState(future), BML_OK);
    EXPECT_TRUE(future.IsValid());
    EXPECT_EQ(g_FutureReleases, 0);

    Runtime::RuntimeStateValue state{};
    ASSERT_EQ(future.AwaitResult(state, 0), BML_OK);
    EXPECT_TRUE(state.InGame);
    EXPECT_TRUE(state.Playing);

    EXPECT_EQ(client.BeginCallState(future), BML_ERROR_BUSY);
    EXPECT_EQ(g_FutureReleases, 0);
    EXPECT_EQ(future.Release(), BML_OK);
    EXPECT_FALSE(future.IsValid());
    EXPECT_EQ(g_FutureReleases, 1);
}

TEST(ImcGeneratedClientTest, EncodesTypedRequestForResponseLessRpc) {
    ResetMock();
    namespace Ui = BML::Imc::Generated::Bml::Ui;
    Ui::Client client;
    ASSERT_EQ(client.Open("test.consumer"), BML_OK);
    Ui::MessageInputValue input{}; input.Message = "hello IMC";
    ASSERT_EQ(client.CallMessageAdd(input), BML_OK);
    ASSERT_FALSE(g_LastRequest.empty());
    BML_ImcMessage request = BML_IMC_MESSAGE_INIT;
    request.Data = g_LastRequest.data(); request.DataSize = g_LastRequest.size();
    request.PayloadType = g_LastRequestPayload;
    Ui::MessageInputValue decoded{};
    ASSERT_EQ(Ui::DecodeMessageInput(request, decoded), BML_OK);
    EXPECT_EQ(decoded.Message, input.Message);
    EXPECT_EQ(g_FutureReleases, 1);
}

TEST(ImcGeneratedClientTest, ReadsSpeedrunStateFromItsOwnInterface) {
    ResetMock();
    namespace Speedrun = BML::Imc::Generated::Bml::Speedrun;
    Speedrun::Client client;
    ASSERT_EQ(client.Open("test.consumer"), BML_OK);
    Speedrun::TimerStateValue state{};
    ASSERT_EQ(client.CallState(state), BML_OK);
    EXPECT_FLOAT_EQ(state.ElapsedTime, 12.5f);
    EXPECT_EQ(g_FutureReleases, 1);
}

TEST(ImcGeneratedClientTest, PublishesTypedTopicPayload) {
    ResetMock();
    namespace Events = BML::Imc::Generated::Bml::Events;
    Events::Client client;
    ASSERT_EQ(client.Open("test.publisher"), BML_OK);
    Events::EventValue event{}; event.Kind = 42; event.HasCommandArgs = true;
    event.CommandArgs = {"one", "two"};
    std::size_t delivered = 0;
    ASSERT_EQ(client.PublishAll(event, &delivered), BML_OK);
    EXPECT_EQ(delivered, 1u);
    EXPECT_EQ(g_LastPublishPayload, StableId(Events::EventPayload));
    BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
    message.Data = g_LastPublish.data(); message.DataSize = g_LastPublish.size();
    message.PayloadType = g_LastPublishPayload;
    Events::EventValue decoded{};
    ASSERT_EQ(Events::DecodeEvent(message, decoded), BML_OK);
    EXPECT_EQ(decoded.Kind, 42);
    EXPECT_EQ(decoded.CommandArgs, event.CommandArgs);
}
TEST(ImcGeneratedClientTest, ProviderTrampolineEncodesTypedResponseDirectly) {
    ResetMock();
    namespace Runtime = BML::Imc::Generated::Bml::Runtime;
    Runtime::Provider provider;
    ASSERT_EQ(provider.Open("test.provider"), BML_OK);
    ASSERT_EQ(provider.RegisterState(&ProvideRuntimeState, nullptr, BML_IMC_EXECUTION_CALLER_THREAD), BML_OK);
    ASSERT_NE(g_RegisteredHandler, nullptr);

    BML_ImcMessage request = BML_IMC_MESSAGE_INIT;
    BML_ImcResponse_T response;
    ASSERT_EQ(g_RegisteredHandler(g_RegisteredRpc, &request, &response, g_RegisteredUserdata), BML_OK);
    BML_ImcMessage result = BML_IMC_MESSAGE_INIT;
    result.Data = response.Data.data(); result.DataSize = response.Data.size(); result.PayloadType = response.PayloadType;
    Runtime::RuntimeStateValue decoded{};
    ASSERT_EQ(Runtime::DecodeRuntimeState(result, decoded), BML_OK);
    EXPECT_TRUE(decoded.InGame);
    EXPECT_TRUE(decoded.Paused);
    EXPECT_TRUE(decoded.CheatEnabled);
    EXPECT_EQ(response.PayloadType, StableId(Runtime::RuntimeStatePayload));
}

TEST(ImcGeneratedClientTest, ResponseLessProviderDoesNotRequireOrWriteResponse) {
    ResetMock();
    namespace Ui = BML::Imc::Generated::Bml::Ui;
    Ui::Provider provider;
    ASSERT_EQ(provider.Open("test.provider"), BML_OK);
    ASSERT_EQ(provider.RegisterMessageAdd(
        &ProvideUiMessage, nullptr, BML_IMC_EXECUTION_CALLER_THREAD), BML_OK);

    Ui::MessageInputValue input{};
    input.Message = "provider call";
    std::vector<std::uint8_t> bytes(Ui::EncodedMessageInputSize(input));
    ASSERT_EQ(Ui::EncodeMessageInput(input, bytes.data(), bytes.size()), BML_OK);
    BML_ImcMessage request = BML_IMC_MESSAGE_INIT;
    request.Data = bytes.data();
    request.DataSize = bytes.size();
    request.PayloadType = StableId(Ui::MessageInputPayload);

    ASSERT_EQ(g_RegisteredHandler(
        g_RegisteredRpc, &request, nullptr, g_RegisteredUserdata), BML_OK);
    EXPECT_EQ(g_ProvidedMessage, input.Message);
}
TEST(ImcGeneratedClientTest, RpcUsesTypedObjectRequestSchema) {
    ResetMock();
    namespace Scene = BML::Imc::Generated::Bml::Scene;
    Scene::Client client;
    ASSERT_EQ(client.Open("test.consumer"), BML_OK);
    const BML_ObjectRef object{3, 20, 9};
    Scene::ObjectRequestValue input{}; input.Object = object;
    Scene::EntityTransformValue transform{};
    ASSERT_EQ(client.CallEntity(input, transform), BML_OK);
    EXPECT_FLOAT_EQ(transform.Position.y, 2.0f);
    EXPECT_EQ(transform.ChildCount, 4);
    BML_ImcMessage request = BML_IMC_MESSAGE_INIT;
    request.Data = g_LastRequest.data(); request.DataSize = g_LastRequest.size(); request.PayloadType = g_LastRequestPayload;
    Scene::ObjectRequestValue decoded{};
    ASSERT_EQ(Scene::DecodeObjectRequest(request, decoded), BML_OK);
    EXPECT_EQ(decoded.Object.Domain, object.Domain);
    EXPECT_EQ(decoded.Object.Slot, object.Slot);
    EXPECT_EQ(decoded.Object.Generation, object.Generation);
    EXPECT_EQ(request.PayloadType, StableId(Scene::ObjectRequestPayload));
}

TEST(ImcGeneratedClientTest, RpcResponseCarriesCountedArrays) {
    ResetMock();
    namespace Gameplay = BML::Imc::Generated::Bml::Gameplay;
    Gameplay::Client client;
    ASSERT_EQ(client.Open("test.consumer"), BML_OK);
    Gameplay::CatalogResponseValue catalog{};
    ASSERT_EQ(client.CallCatalog(catalog), BML_OK);
    ASSERT_EQ(catalog.Files.size(), 2u);
    EXPECT_EQ(catalog.Files[0], "alpha.nmo");
    EXPECT_EQ(catalog.Music[1], 4);
    EXPECT_TRUE(g_LastRequest.empty());
    EXPECT_EQ(g_FutureReleases, 1);
}
TEST(ImcGeneratedClientTest, PublicEventFacadePreservesTopicMetadataAndTypedPayload) {
    ResetMock();
    BML::Events::Stream stream;
    ASSERT_EQ(stream.Open(0), BML_OK);
    EXPECT_EQ(g_LastSubscribeCapacity.load(std::memory_order_relaxed), 256u);
    ASSERT_EQ(stream.Close(), BML_OK);
    ASSERT_EQ(stream.Open(1), BML_OK);
    EXPECT_EQ(g_LastSubscribeCapacity.load(std::memory_order_relaxed), 1u);
    ASSERT_EQ(stream.Close(), BML_OK);
    ASSERT_EQ(stream.Open(2), BML_OK);
    ASSERT_NE(g_TopicHandler, nullptr);
    namespace Events = BML::Imc::Generated::Bml::Events;
    Events::EventValue value{}; value.Kind = BML_EVENT_CHEAT_CHANGED;
    value.HasCheatEnabled = true; value.CheatEnabled = true;
    std::vector<std::uint8_t> data(Events::EncodedEventSize(value));
    ASSERT_EQ(Events::EncodeEvent(value, data.data(), data.size()), BML_OK);
    BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
    message.Data = data.data(); message.DataSize = data.size();
    message.PayloadType = StableId(Events::EventPayload); message.MessageId = 77; message.TimestampNs = 88;
    g_TopicHandler(StableId(Events::AllRoute), &message, g_TopicUserdata);
    BML::Events::Event event{};
    ASSERT_EQ(stream.Poll(event), BML_OK);
    EXPECT_EQ(event.Kind, BML_EVENT_CHEAT_CHANGED);
    EXPECT_EQ(event.Sequence, 77u);
    EXPECT_EQ(event.Timestamp, 88u);
    ASSERT_TRUE(event.CheatData.has_value());
    EXPECT_TRUE(event.CheatData->Enabled);
    int dropped = -1; ASSERT_EQ(stream.DroppedCount(dropped), BML_OK); EXPECT_EQ(dropped, 0);
    EXPECT_EQ(stream.Close(), BML_OK);
}

TEST(ImcGeneratedClientTest, PublicEventFacadeRejectsInvalidDomainPayloads) {
    ResetMock();
    BML::Events::Stream stream;
    ASSERT_EQ(stream.Open(2), BML_OK);
    ASSERT_NE(g_TopicHandler, nullptr);

    namespace Events = BML::Imc::Generated::Bml::Events;
    Events::EventValue value{};
    value.Kind = BML_EVENT_LOAD_OBJECT;
    std::vector<std::uint8_t> data(Events::EncodedEventSize(value));
    ASSERT_EQ(Events::EncodeEvent(value, data.data(), data.size()), BML_OK);
    BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
    message.Data = data.data();
    message.DataSize = data.size();
    message.PayloadType = StableId(Events::EventPayload);
    g_TopicHandler(StableId(Events::AllRoute), &message, g_TopicUserdata);

    BML::Events::Event event{};
    event.Kind = BML_EVENT_CHEAT_CHANGED;
    EXPECT_EQ(stream.Poll(event), BML_ERROR_MALFORMED_MESSAGE);
    EXPECT_EQ(event.Kind, 0);
    EXPECT_EQ(stream.Poll(event), BML_ERROR_NOT_FOUND);

    value = {};
    value.Kind = BML_EVENT_PHYSICALIZE;
    value.HasTarget = true;
    value.HasFixed = true;
    value.HasFriction = true;
    value.HasElasticity = true;
    value.HasMass = true;
    value.HasCollisionGroup = true;
    value.HasStartFrozen = true;
    value.HasEnableCollision = true;
    value.HasAutoCalculateMassCenter = true;
    value.HasLinearDamp = true;
    value.HasRotDamp = true;
    value.HasCollisionSurface = true;
    value.HasMassCenter = true;
    value.HasConvexMeshes = true;
    value.HasBallCenters = true;
    value.BallCenters.push_back({1.0f, 2.0f, 3.0f});
    value.HasBallRadii = true;
    value.HasConcaveMeshes = true;
    data.resize(Events::EncodedEventSize(value));
    ASSERT_EQ(Events::EncodeEvent(value, data.data(), data.size()), BML_OK);
    message.Data = data.data();
    message.DataSize = data.size();
    g_TopicHandler(StableId(Events::AllRoute), &message, g_TopicUserdata);

    EXPECT_EQ(stream.Poll(event), BML_ERROR_MALFORMED_MESSAGE);
    EXPECT_EQ(event.Kind, 0);
    EXPECT_EQ(stream.Poll(event), BML_ERROR_NOT_FOUND);
    int dropped = -1;
    EXPECT_EQ(stream.DroppedCount(dropped), BML_OK);
    EXPECT_EQ(dropped, 0);
    EXPECT_EQ(stream.Close(), BML_OK);
}

TEST(ImcGeneratedClientTest, PublicEventFacadeDistinguishesClosedEmptyAndReadyStates) {
    ResetMock();
    BML::Events::Stream stream;
    BML::Events::Event event{};
    event.Kind = BML_EVENT_CHEAT_CHANGED;

    EXPECT_EQ(stream.Poll(event), BML_ERROR_INVALID_HANDLE);
    EXPECT_EQ(event.Kind, 0);

    ASSERT_EQ(stream.Open(2), BML_OK);
    event.Kind = BML_EVENT_CHEAT_CHANGED;
    EXPECT_EQ(stream.Poll(event), BML_ERROR_NOT_FOUND);
    EXPECT_EQ(event.Kind, 0);

    ASSERT_NE(g_TopicHandler, nullptr);
    namespace Events = BML::Imc::Generated::Bml::Events;
    Events::EventValue value{};
    value.Kind = BML_EVENT_CHEAT_CHANGED;
    value.HasCheatEnabled = true;
    value.CheatEnabled = true;
    std::vector<std::uint8_t> data(Events::EncodedEventSize(value));
    ASSERT_EQ(Events::EncodeEvent(value, data.data(), data.size()), BML_OK);
    BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
    message.Data = data.data();
    message.DataSize = data.size();
    message.PayloadType = StableId(Events::EventPayload);
    g_TopicHandler(StableId(Events::AllRoute), &message, g_TopicUserdata);

    EXPECT_EQ(stream.Poll(event), BML_OK);
    EXPECT_EQ(event.Kind, BML_EVENT_CHEAT_CHANGED);
    EXPECT_EQ(stream.Poll(event), BML_ERROR_NOT_FOUND);
    EXPECT_EQ(event.Kind, 0);

    ASSERT_EQ(stream.Close(), BML_OK);
    EXPECT_EQ(stream.Poll(event), BML_ERROR_INVALID_HANDLE);
}

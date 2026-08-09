#include "BML/Generated/bml_runtime_imc.hpp"

#include "ImcRuntime.h"
#include "ModInvocationGate.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

BML::ImcRuntime *g_Runtime = nullptr;

BML::ImcRuntime *Runtime() noexcept {
    return g_Runtime;
}

namespace RuntimeApi = BML::Imc::Generated::Bml::Runtime;

struct RuntimeStateSource {
    int Status = BML_OK;
    std::uint64_t Calls = 0;
};

int ReadRuntimeState(RuntimeApi::RuntimeStateValue &out, void *userdata) {
    auto *source = static_cast<RuntimeStateSource *>(userdata);
    if (!source)
        return BML_ERROR_INVALID_PARAMETER;
    ++source->Calls;
    if (source->Status != BML_OK)
        return source->Status;
    out.InGame = true;
    out.InLevel = true;
    out.Paused = false;
    out.Playing = true;
    out.CheatEnabled = false;
    return BML_OK;
}

class ImcGeneratedRuntimeIntegrationTest : public testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(g_Runtime, nullptr);
        g_Runtime = &m_Runtime;
        ASSERT_EQ(m_Provider.Open("bml.core.test"), BML_OK);
        ASSERT_EQ(m_Provider.RegisterState(
                          &ReadRuntimeState, &m_Source,
                          BML_IMC_EXECUTION_CALLER_THREAD),
                  BML_OK);
        ASSERT_EQ(m_Client.Open("consumer.test"), BML_OK);
    }

    void TearDown() override {
        EXPECT_EQ(m_Client.Close(), BML_OK);
        EXPECT_EQ(m_Provider.Close(), BML_OK);
        g_Runtime = nullptr;
    }

    BML::ModInvocationGate m_InvocationGate;
    BML::ImcRuntime m_Runtime{&m_InvocationGate};
    RuntimeStateSource m_Source;
    RuntimeApi::Provider m_Provider;
    RuntimeApi::Client m_Client;
};

} // namespace

BML_EXPORT int BML_Imc_OpenClient(const char *ownerId,
                                  BML_ImcClient *outClient) {
    auto *runtime = Runtime();
    if (!runtime) {
        if (outClient)
            *outClient = nullptr;
        return BML_ERROR_FROZEN;
    }
    return runtime->OpenClient(ownerId ? ownerId : "", outClient);
}

BML_EXPORT int BML_Imc_CloseClient(BML_ImcClient client) {
    auto *runtime = Runtime();
    return runtime ? runtime->CloseClient(client) : BML_ERROR_FROZEN;
}

BML_EXPORT int BML_Imc_GetRpcId(BML_ImcClient client, const char *name,
                                BML_ImcRpcId *outId) {
    auto *runtime = Runtime();
    return runtime ? runtime->GetRpcId(client, name, outId) : BML_ERROR_FROZEN;
}

BML_EXPORT int BML_Imc_GetPayloadTypeId(BML_ImcClient client, const char *name,
                                        BML_ImcPayloadTypeId *outId) {
    auto *runtime = Runtime();
    return runtime ? runtime->GetPayloadTypeId(client, name, outId)
                   : BML_ERROR_FROZEN;
}

BML_EXPORT int BML_Imc_IsRpcAvailable(BML_ImcClient client,
                                      BML_ImcRpcId rpcId,
                                      int *outAvailable) {
    auto *runtime = Runtime();
    return runtime ? runtime->IsRpcAvailable(client, rpcId, outAvailable)
                   : BML_ERROR_FROZEN;
}

BML_EXPORT int BML_Imc_RegisterRpc(
        BML_ImcClient client, BML_ImcRpcId rpcId,
        const BML_ImcRpcRegistrationOptions *options,
        BML_ImcRpcHandler handler, void *userdata) {
    auto *runtime = Runtime();
    return runtime ? runtime->RegisterRpc(client, rpcId, options, handler, userdata)
                   : BML_ERROR_FROZEN;
}

BML_EXPORT int BML_Imc_CallRpc(BML_ImcClient client, BML_ImcRpcId rpcId,
                               const BML_ImcMessage *request,
                               const BML_ImcCallOptions *options,
                               BML_ImcFuture *outFuture) {
    auto *runtime = Runtime();
    return runtime ? runtime->CallRpc(client, rpcId, request, options, outFuture)
                   : BML_ERROR_FROZEN;
}

BML_EXPORT int BML_Imc_ResponseReserve(BML_ImcResponse *response, size_t size,
                                       void **outData) {
    return BML::ImcRuntime::ResponseReserve(response, size, outData);
}

BML_EXPORT int BML_Imc_ResponseCommit(BML_ImcResponse *response, size_t size,
                                      BML_ImcPayloadTypeId payloadType) {
    return BML::ImcRuntime::ResponseCommit(response, size, payloadType);
}

BML_EXPORT int BML_Imc_FutureAwait(BML_ImcFuture future, uint32_t timeoutMs) {
    auto *runtime = Runtime();
    return runtime ? runtime->FutureAwait(future, timeoutMs) : BML_ERROR_FROZEN;
}

BML_EXPORT int BML_Imc_FutureGetResult(BML_ImcFuture future,
                                       BML_ImcMessage *outMessage) {
    auto *runtime = Runtime();
    return runtime ? runtime->FutureGetResult(future, outMessage)
                   : BML_ERROR_FROZEN;
}

BML_EXPORT int BML_Imc_FutureRelease(BML_ImcFuture future) {
    auto *runtime = Runtime();
    return runtime ? runtime->FutureRelease(future) : BML_ERROR_FROZEN;
}

TEST_F(ImcGeneratedRuntimeIntegrationTest,
       GeneratedRuntimeInterfaceRoundTripsThroughActualTransport) {
    RuntimeApi::RuntimeStateValue state{};
    ASSERT_EQ(m_Client.CallState(state), BML_OK);
    EXPECT_TRUE(state.InGame);
    EXPECT_TRUE(state.InLevel);
    EXPECT_FALSE(state.Paused);
    EXPECT_TRUE(state.Playing);
    EXPECT_FALSE(state.CheatEnabled);
    EXPECT_EQ(m_Source.Calls, 1u);

    BML_ImcStats stats{sizeof(BML_ImcStats)};
    ASSERT_EQ(m_Runtime.GetStats(m_Client.Handle(), &stats), BML_OK);
    EXPECT_EQ(stats.RpcCalls, 1u);
    EXPECT_EQ(stats.RpcCompleted, 1u);
    EXPECT_EQ(stats.RpcFailed, 0u);
}

TEST_F(ImcGeneratedRuntimeIntegrationTest,
       GeneratedRuntimeInterfacePreservesProviderFailure) {
    m_Source.Status = BML_ERROR_IMC_UNSUPPORTED;
    RuntimeApi::RuntimeStateValue state{};
    EXPECT_EQ(m_Client.CallState(state), BML_ERROR_IMC_UNSUPPORTED);
    EXPECT_EQ(m_Source.Calls, 1u);

    BML_ImcStats stats{sizeof(BML_ImcStats)};
    ASSERT_EQ(m_Runtime.GetStats(m_Client.Handle(), &stats), BML_OK);
    EXPECT_EQ(stats.RpcCalls, 1u);
    EXPECT_EQ(stats.RpcCompleted, 0u);
    EXPECT_EQ(stats.RpcFailed, 1u);
}

TEST_F(ImcGeneratedRuntimeIntegrationTest,
       GeneratedRuntimeInterfaceObservesProviderUnload) {
    ASSERT_EQ(m_Provider.Close(), BML_OK);
    bool available = true;
    ASSERT_EQ(m_Client.IsStateAvailable(available), BML_OK);
    EXPECT_FALSE(available);

    RuntimeApi::RuntimeStateValue state{};
    EXPECT_EQ(m_Client.CallState(state), BML_ERROR_IMC_ENDPOINT_NOT_FOUND);
    EXPECT_EQ(m_Source.Calls, 0u);
}

TEST_F(ImcGeneratedRuntimeIntegrationTest,
       GeneratedRuntimeInterfaceMeetsReleaseThreshold) {
#ifndef NDEBUG
    GTEST_SKIP() << "Performance gate runs only in Release builds";
#else
    RuntimeApi::RuntimeStateValue state{};
    auto invoke = [&] { return m_Client.CallState(state) == BML_OK; };
    for (int index = 0; index < 10000; ++index)
        ASSERT_TRUE(invoke());

    constexpr int ThroughputIterations = 100000;
    const auto throughputStart = std::chrono::steady_clock::now();
    for (int index = 0; index < ThroughputIterations; ++index)
        ASSERT_TRUE(invoke());
    const double seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - throughputStart).count();
    const double callsPerSecond = ThroughputIterations / seconds;

    constexpr int LatencyIterations = 10000;
    std::vector<std::uint64_t> latencies;
    latencies.reserve(LatencyIterations);
    for (int index = 0; index < LatencyIterations; ++index) {
        const auto start = std::chrono::steady_clock::now();
        ASSERT_TRUE(invoke());
        latencies.push_back(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - start).count()));
    }
    const auto percentile = latencies.begin() +
        static_cast<std::ptrdiff_t>(latencies.size() * 99 / 100);
    std::nth_element(latencies.begin(), percentile, latencies.end());
    const std::uint64_t p99Ns = *percentile;

    RecordProperty("calls_per_second", callsPerSecond);
    RecordProperty("p99_nanoseconds", p99Ns);
    EXPECT_GE(callsPerSecond, 1000000.0);
    EXPECT_LE(p99Ns, 5000u);
#endif
}

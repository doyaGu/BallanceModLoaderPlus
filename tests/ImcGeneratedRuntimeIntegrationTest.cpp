#include "test_sample_imc.hpp"

#include "ImcRuntime.h"
#include "ModInvocationGate.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace {

BML::ImcRuntime *g_Runtime = nullptr;

BML::ImcRuntime *Runtime() noexcept {
    return g_Runtime;
}

namespace SampleApi = BML::Imc::Generated::Test::Sample;

struct ScalarStateSource {
    int Status = BML_OK;
    std::uint64_t Calls = 0;
};

int ReadScalarState(SampleApi::ScalarStateValue &out, void *userdata) {
    auto *source = static_cast<ScalarStateSource *>(userdata);
    if (!source)
        return BML_ERROR_INVALID_PARAMETER;
    ++source->Calls;
    if (source->Status != BML_OK)
        return source->Status;
    out.Flag = true;
    out.Count = 5;
    out.Ratio = 2.5f;
    return BML_OK;
}

struct SelfClosingScalarStateSource {
    std::unique_ptr<SampleApi::Provider> *Owner = nullptr;
    std::uint64_t Calls = 0;
};

int ReadScalarStateAndDestroyProvider(SampleApi::ScalarStateValue &out,
                                      void *userdata) {
    auto *source = static_cast<SelfClosingScalarStateSource *>(userdata);
    if (!source || !source->Owner)
        return BML_ERROR_INVALID_PARAMETER;
    ++source->Calls;
    out.Flag = true;
    out.Count = 1;
    out.Ratio = 1.0f;
    source->Owner->reset();
    return BML_OK;
}

class ImcGeneratedRuntimeIntegrationTest : public testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(g_Runtime, nullptr);
        g_Runtime = &m_Runtime;
        ASSERT_EQ(m_Provider.Open("bml.core.test"), BML_OK);
        ASSERT_EQ(m_Provider.RegisterState(
                          &ReadScalarState, &m_Source,
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
    ScalarStateSource m_Source;
    SampleApi::Provider m_Provider;
    SampleApi::Client m_Client;
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

BML_EXPORT int BML_Imc_GetTopicId(BML_ImcClient client, const char *name,
                                  BML_ImcTopicId *outId) {
    auto *runtime = Runtime();
    return runtime ? runtime->GetTopicId(client, name, outId) : BML_ERROR_FROZEN;
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
    SampleApi::ScalarStateValue state{};
    ASSERT_EQ(m_Client.CallState(state), BML_OK);
    EXPECT_TRUE(state.Flag);
    EXPECT_EQ(state.Count, 5);
    EXPECT_FLOAT_EQ(state.Ratio, 2.5f);
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
    SampleApi::ScalarStateValue state{};
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

    SampleApi::ScalarStateValue state{};
    EXPECT_EQ(m_Client.CallState(state), BML_ERROR_IMC_ENDPOINT_NOT_FOUND);
    EXPECT_EQ(m_Source.Calls, 0u);
}

TEST_F(ImcGeneratedRuntimeIntegrationTest,
       GeneratedProviderCanBeDestroyedByItsOwnHandler) {
    ASSERT_EQ(m_Provider.Close(), BML_OK);
    auto provider = std::make_unique<SampleApi::Provider>();
    SelfClosingScalarStateSource source{&provider};
    SampleApi::Provider::Handlers handlers{};
    handlers.Userdata = &source;
    handlers.State = &ReadScalarStateAndDestroyProvider;
    ASSERT_EQ(provider->Start(handlers, "self-closing.provider"), BML_OK);

    SampleApi::ScalarStateValue state{};
    EXPECT_EQ(m_Client.CallState(state), BML_OK);
    EXPECT_EQ(source.Calls, 1u);
    EXPECT_EQ(provider, nullptr);

    bool available = true;
    ASSERT_EQ(m_Client.IsStateAvailable(available), BML_OK);
    EXPECT_FALSE(available);
}

TEST_F(ImcGeneratedRuntimeIntegrationTest,
       GeneratedRuntimeInterfaceMeetsPerformanceBudget) {
#ifndef NDEBUG
    GTEST_SKIP() << "Performance gate runs only in Release builds";
#else
    SampleApi::ScalarStateValue state{};
    auto invokeGenerated = [&] { return m_Client.CallState(state) == BML_OK; };
    const BML_ImcCallOptions callOptions = BML_IMC_CALL_OPTIONS_INIT;
    auto invokeTransport = [&] {
        BML_ImcFuture future = nullptr;
        int status = BML_Imc_CallRpc(m_Client.Handle(), m_Client.StateRpcId(),
                                     nullptr, &callOptions, &future);
        if (status == BML_OK)
            status = BML_Imc_FutureAwait(future, callOptions.TimeoutMs);

        BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
        if (status == BML_OK)
            status = BML_Imc_FutureGetResult(future, &message);
        if (status == BML_OK &&
            message.PayloadType != m_Client.ScalarStatePayloadType())
            status = BML_ERROR_TYPE_MISMATCH;

        const int releaseStatus = future
            ? BML_Imc_FutureRelease(future)
            : BML_OK;
        return status == BML_OK && releaseStatus == BML_OK;
    };
    for (int index = 0; index < 10000; ++index)
        ASSERT_TRUE(invokeGenerated());
    for (int index = 0; index < 10000; ++index)
        ASSERT_TRUE(invokeTransport());

    struct ThroughputMeasurement {
        double CallsPerSecond;
        std::uint64_t Failures;
    };
    auto measureThroughput = [](auto &&invoke, int iterations) {
        std::uint64_t failures = 0;
        const auto start = std::chrono::steady_clock::now();
        for (int index = 0; index < iterations; ++index)
            failures += invoke() ? 0u : 1u;
        const double seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start).count();
        return ThroughputMeasurement{iterations / seconds, failures};
    };

    constexpr int ThroughputIterations = 1000000;
    const auto generated =
        measureThroughput(invokeGenerated, ThroughputIterations);
    const auto transport =
        measureThroughput(invokeTransport, ThroughputIterations);
    const double relativeThroughput =
        generated.CallsPerSecond / transport.CallsPerSecond;

    constexpr int LatencyIterations = 10000;
    std::uint64_t failedLatencyCalls = 0;
    std::vector<std::uint64_t> latencies;
    latencies.reserve(LatencyIterations);
    for (int index = 0; index < LatencyIterations; ++index) {
        const auto start = std::chrono::steady_clock::now();
        const bool succeeded = invokeGenerated();
        latencies.push_back(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - start).count()));
        failedLatencyCalls += succeeded ? 0u : 1u;
    }
    const auto percentile = latencies.begin() +
        static_cast<std::ptrdiff_t>(latencies.size() * 99 / 100);
    std::nth_element(latencies.begin(), percentile, latencies.end());
    const std::uint64_t p99Ns = *percentile;

    RecordProperty("calls_per_second", generated.CallsPerSecond);
    RecordProperty("transport_calls_per_second", transport.CallsPerSecond);
    RecordProperty("relative_throughput", relativeThroughput);
    RecordProperty("p99_nanoseconds", p99Ns);
    EXPECT_EQ(generated.Failures, 0u);
    EXPECT_EQ(transport.Failures, 0u);
    EXPECT_EQ(failedLatencyCalls, 0u);
    // Shared runners may not sustain the absolute target. On those machines,
    // bound the generated facade's overhead against the same-process transport.
    constexpr double AbsoluteThroughputTarget = 1000000.0;
    constexpr double MinimumRelativeThroughput = 0.8;
    EXPECT_TRUE(generated.CallsPerSecond >= AbsoluteThroughputTarget ||
                relativeThroughput >= MinimumRelativeThroughput)
        << "generated=" << generated.CallsPerSecond
        << " calls/s, transport=" << transport.CallsPerSecond
        << " calls/s, ratio=" << relativeThroughput;
    EXPECT_LE(p99Ns, 5000u);
#endif
}

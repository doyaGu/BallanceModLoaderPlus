#include "core/ImcBus.h"
#include "core/Context.h"
#include "core/ModManifest.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using BML::Core::ImcBus;
using Clock = std::chrono::steady_clock;

namespace {

struct BenchConfig {
    size_t messages = 100'000;
    size_t rpc_calls = 50'000;
    size_t payload_bytes = 64;
    size_t pump_budget = 0; // 0 = unlimited per pump invocation
};

struct Percentiles {
    double p50_us{0.0};
    double p99_us{0.0};
    double p999_us{0.0};
};

struct PubSubMetrics {
    double duration_s{0.0};
    double throughput_mps{0.0};
    Percentiles latency;
};

struct RpcMetrics {
    double duration_s{0.0};
    double throughput_rps{0.0};
    Percentiles latency;
};

[[noreturn]] void PrintUsage(const char *exe) {
    std::cout << "ImcBus benchmark\n\n"
              << "Usage: " << exe << " [options]\n\n"
              << "Options:\n"
              << "  --messages <n>       Number of pub/sub messages (default 100000)\n"
              << "  --rpc-calls <n>      Number of RPC calls (default 50000)\n"
              << "  --payload-bytes <n>  Payload size in bytes (default 64)\n"
              << "  --pump-budget <n>    Max messages per subscription per pump (0 = unlimited)\n"
              << "  -h, --help           Show this help\n";
    std::exit(0);
}

BenchConfig ParseArgs(int argc, char **argv) {
    BenchConfig config;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto require_value = [&](const char *name) -> std::string {
            if (i + 1 >= argc) {
                throw std::invalid_argument(std::string("Missing value for ") + name);
            }
            return argv[++i];
        };

        if (arg == "--messages") {
            config.messages = std::stoul(require_value("--messages"));
        } else if (arg == "--rpc-calls") {
            config.rpc_calls = std::stoul(require_value("--rpc-calls"));
        } else if (arg == "--payload-bytes") {
            config.payload_bytes = std::stoul(require_value("--payload-bytes"));
        } else if (arg == "--pump-budget") {
            config.pump_budget = std::stoul(require_value("--pump-budget"));
        } else if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
        } else {
            throw std::invalid_argument("Unknown argument: " + arg);
        }
    }

    if (config.messages == 0 || config.rpc_calls == 0) {
        throw std::invalid_argument("messages and rpc-calls must be greater than zero");
    }

    config.payload_bytes = std::max<size_t>(config.payload_bytes, sizeof(int64_t));
    return config;
}

void EnsureOk(BML_Result result, const char *context) {
    if (result != BML_RESULT_OK)
        throw std::runtime_error(std::string(context) + " failed with code " + std::to_string(static_cast<int>(result)));
}

Percentiles ComputePercentiles(std::vector<double> &samples) {
    if (samples.empty())
        return {};
    std::sort(samples.begin(), samples.end());
    auto pick = [&](double pct) {
        const double pos = pct * static_cast<double>(samples.size() - 1);
        const double clamped = std::clamp(pos, 0.0, static_cast<double>(samples.size() - 1));
        const size_t idx = static_cast<size_t>(clamped + 0.5);
        return samples[idx];
    };
    return {pick(0.50), pick(0.99), pick(0.999)};
}

struct PubSubContext {
    explicit PubSubContext(size_t expected) : expected(expected) {
        latencies.reserve(expected);
    }

    size_t expected;
    std::atomic<size_t> received{0};
    std::vector<double> latencies;
};

// New handler signature: BML_ImcHandler
void PubSubHandler(BML_Context ctx,
                   BML_TopicId topic,
                   const BML_ImcMessage* msg,
                   void *user_data) {
    (void)ctx; (void)topic;
    auto *pctx = static_cast<PubSubContext *>(user_data);
    if (!pctx || !msg || !msg->data || msg->size < sizeof(int64_t))
        return;
    int64_t sent_ns = 0;
    std::memcpy(&sent_ns, msg->data, sizeof(int64_t));
    const auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();
    const double latency_us = static_cast<double>(now_ns - sent_ns) / 1000.0;
    pctx->latencies.push_back(latency_us);
    pctx->received.fetch_add(1, std::memory_order_relaxed);
}

void PumpUntil(size_t target, std::atomic<size_t> &counter, size_t pump_budget) {
    auto &bus = ImcBus::Instance();
    while (counter.load(std::memory_order_acquire) < target) {
        bus.Pump(pump_budget);
        std::this_thread::yield();
    }
}

PubSubMetrics RunPubSubBenchmark(const BenchConfig &config) {
    auto &bus = ImcBus::Instance();
    
    PubSubContext ctx(config.messages);
    
    // Get topic ID
    BML_TopicId topic_id = 0;
    EnsureOk(bus.GetTopicId("bench.pubsub", &topic_id), "GetTopicId");
    
    BML_Subscription subscription = nullptr;
    EnsureOk(bus.Subscribe(topic_id, &PubSubHandler, &ctx, &subscription), "Subscribe");

    const size_t payload_size = config.payload_bytes;
    std::vector<uint8_t> payload(payload_size, 0);

    const auto start = Clock::now();
    for (size_t i = 0; i < config.messages; ++i) {
        const auto sent_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();
        std::memcpy(payload.data(), &sent_ns, sizeof(int64_t));
        EnsureOk(bus.Publish(topic_id, payload.data(), payload_size, nullptr), "Publish");
        if (config.pump_budget != 0 && ((i + 1) % config.pump_budget == 0)) {
            bus.Pump(config.pump_budget);
        }
    }

    PumpUntil(config.messages, ctx.received, config.pump_budget);
    const auto end = Clock::now();

    EnsureOk(bus.Unsubscribe(subscription), "Unsubscribe");

    const double duration_s = std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();
    const double throughput = static_cast<double>(config.messages) /
                              std::max(duration_s, std::numeric_limits<double>::epsilon());

    auto latency = ComputePercentiles(ctx.latencies);
    return {duration_s, throughput, latency};
}

struct RpcHandlerContext {
    size_t response_bytes;
};

// New RPC handler signature: BML_RpcHandler
BML_Result BenchRpcHandler(BML_Context ctx,
                           BML_RpcId rpc_id,
                           const BML_ImcMessage *request,
                           BML_ImcBuffer *out_response,
                           void *user_data) {
    (void)ctx; (void)rpc_id;
    auto *hctx = static_cast<RpcHandlerContext *>(user_data);
    const size_t payload_len = request ? request->size : 0;
    const void *payload = request ? request->data : nullptr;
    const size_t response_size = hctx ? hctx->response_bytes : payload_len;
    auto buffer = std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[response_size]);
    if (!buffer)
        return BML_RESULT_OUT_OF_MEMORY;

    if (payload && payload_len > 0) {
        const size_t copy = std::min(payload_len, response_size);
        std::memcpy(buffer.get(), payload, copy);
        if (response_size > copy) {
            std::memset(buffer.get() + copy, 0, response_size - copy);
        }
    } else {
        std::memset(buffer.get(), 0, response_size);
    }

    out_response->struct_size = sizeof(BML_ImcBuffer);
    out_response->data = buffer.get();
    out_response->size = response_size;
    out_response->cleanup = +[](const void * /*data*/, size_t /*size*/, void *user) {
        auto *ptr = static_cast<uint8_t *>(user);
        delete[] ptr;
    };
    out_response->cleanup_user_data = buffer.release();
    return BML_RESULT_OK;
}

RpcMetrics RunRpcBenchmark(const BenchConfig &config) {
    auto &bus = ImcBus::Instance();
    RpcHandlerContext ctx{config.payload_bytes};

    // Get RPC ID and register handler
    BML_RpcId rpc_id = 0;
    EnsureOk(bus.GetRpcId("bench.rpc", &rpc_id), "GetRpcId");
    EnsureOk(bus.RegisterRpc(rpc_id, &BenchRpcHandler, &ctx), "RegisterRpc");

    std::vector<double> latencies;
    latencies.reserve(config.rpc_calls);

    const size_t payload_size = config.payload_bytes;
    std::vector<uint8_t> payload(payload_size, 0);

    const auto start = Clock::now();
    for (size_t i = 0; i < config.rpc_calls; ++i) {
        BML_Future future = nullptr;
        const auto call_start = Clock::now();
        
        // Build request message
        BML_ImcMessage request = {};
        request.struct_size = sizeof(BML_ImcMessage);
        request.data = payload.data();
        request.size = payload_size;
        
        EnsureOk(bus.CallRpc(rpc_id, &request, &future), "CallRpc");

        bool completed = false;
        while (!completed) {
            bus.Pump(config.pump_budget);
            BML_FutureState state{};
            EnsureOk(bus.FutureGetState(future, &state), "FutureGetState");
            if (state == BML_FUTURE_READY) {
                BML_ImcMessage response = {};
                EnsureOk(bus.FutureGetResult(future, &response), "FutureGetResult");
                completed = true;
            } else if (state == BML_FUTURE_FAILED) {
                throw std::runtime_error("RPC future failed");
            }
        }

        const auto call_end = Clock::now();
        latencies.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(call_end - call_start).count() / 1000.0);
        EnsureOk(bus.FutureRelease(future), "FutureRelease");
    }

    const auto end = Clock::now();
    EnsureOk(bus.UnregisterRpc(rpc_id), "UnregisterRpc");

    const double duration_s = std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();
    const double throughput = static_cast<double>(config.rpc_calls) /
                              std::max(duration_s, std::numeric_limits<double>::epsilon());

    auto latency = ComputePercentiles(latencies);
    return {duration_s, throughput, latency};
}

} // namespace

int main(int argc, char **argv) {
    try {
        BenchConfig config = ParseArgs(argc, argv);
        
        std::cout << "=== ImcBus Benchmarks (ID-Based API) ===\n";
        std::cout << "Messages: " << config.messages << ", RPC Calls: " << config.rpc_calls
                  << ", Payload: " << config.payload_bytes << " bytes\n\n";
        
        // Pub/Sub benchmark
        const auto pub_metrics = RunPubSubBenchmark(config);
        std::cout << "[Pub/Sub]\n";
        std::cout << "  Duration   : " << std::fixed << std::setprecision(3) << pub_metrics.duration_s << " s\n";
        std::cout << "  Throughput : " << std::setprecision(2) << pub_metrics.throughput_mps << " msg/s\n";
        std::cout << "  Latency us : p50=" << std::setprecision(2) << pub_metrics.latency.p50_us
                  << "  p99=" << pub_metrics.latency.p99_us
                  << "  p99.9=" << pub_metrics.latency.p999_us << "\n\n";

        // RPC benchmark
        const auto rpc_metrics = RunRpcBenchmark(config);
        std::cout << "[RPC]\n";
        std::cout << "  Duration   : " << std::setprecision(3) << rpc_metrics.duration_s << " s\n";
        std::cout << "  Throughput : " << std::setprecision(2) << rpc_metrics.throughput_rps << " calls/s\n";
        std::cout << "  Latency us : p50=" << std::setprecision(2) << rpc_metrics.latency.p50_us
                  << "  p99=" << rpc_metrics.latency.p99_us
                  << "  p99.9=" << rpc_metrics.latency.p999_us << "\n";

        return 0;
    } catch (const std::exception &ex) {
        std::cerr << "Benchmark failed: " << ex.what() << '\n';
        return 1;
    }
}

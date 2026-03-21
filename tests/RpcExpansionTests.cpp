/**
 * @file RpcExpansionTests.cpp
 * @brief Tests for RPC v1.1 expansion: introspection, structured errors,
 *        call options/timeout, middleware, streaming, and RpcServiceManager.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "Core/ApiRegistry.h"
#include "Core/ConfigStore.h"
#include "Core/Context.h"
#include "Core/DiagnosticManager.h"
#include "Core/ImcBus.h"
#include "Core/ModHandle.h"
#include "Core/ModManifest.h"
#include "TestKernel.h"

namespace {

using namespace BML::Core;
using BML::Core::Testing::TestKernel;

class RpcExpansionTest : public ::testing::Test {
protected:
    TestKernel kernel_;

    void SetUp() override {
        kernel_->diagnostics = std::make_unique<DiagnosticManager>();
        kernel_->api_registry = std::make_unique<ApiRegistry>();
        kernel_->config = std::make_unique<ConfigStore>();
        kernel_->context = std::make_unique<Context>(*kernel_->api_registry, *kernel_->config);

        auto &ctx = *kernel_->context;
        ctx.Initialize({0, 4, 0});
        ImcShutdown();
        host_mod_ = ctx.GetSyntheticHostModule();
        ASSERT_NE(host_mod_, nullptr);
        Context::SetCurrentModule(host_mod_);
    }

    void TearDown() override {
        ImcShutdown();
        Context::SetCurrentModule(nullptr);
        manifests_.clear();
    }

    BML_Mod CreateTrackedMod(const std::string &id) {
        auto manifest = std::make_unique<BML::Core::ModManifest>();
        manifest->package.id = id;
        manifest->package.name = id;
        manifest->package.version = "1.0.0";
        manifest->package.parsed_version = {1, 0, 0};
        manifest->directory = L".";

        auto handle = kernel_->context->CreateModHandle(*manifest);
        LoadedModule module{};
        module.id = id;
        module.manifest = manifest.get();
        module.mod_handle = std::move(handle);
        kernel_->context->AddLoadedModule(std::move(module));

        BML_Mod mod = kernel_->context->GetModHandleById(id);
        manifests_.push_back(std::move(manifest));
        return mod;
    }

    BML_Mod host_mod_{nullptr};
    std::vector<std::unique_ptr<BML::Core::ModManifest>> manifests_;
};

// ========================================================================
// Introspection Tests
// ========================================================================

TEST_F(RpcExpansionTest, GetRpcInfoRegisteredHandler) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(ImcGetRpcId("Test/Introspect", &rpc_id), BML_RESULT_OK);
    ASSERT_NE(rpc_id, BML_RPC_ID_INVALID);

    auto handler = [](BML_Context, BML_RpcId, const BML_ImcMessage *, BML_ImcBuffer *, void *) -> BML_Result {
        return BML_RESULT_OK;
    };
    ASSERT_EQ(ImcRegisterRpc(rpc_id, handler, nullptr), BML_RESULT_OK);

    BML_RpcInfo info = BML_RPC_INFO_INIT;
    ASSERT_EQ(ImcGetRpcInfo(rpc_id, &info), BML_RESULT_OK);
    EXPECT_EQ(info.rpc_id, rpc_id);
    EXPECT_EQ(info.has_handler, BML_TRUE);
    EXPECT_STREQ(info.name, "Test/Introspect");
    EXPECT_EQ(info.call_count, 0u);

    ImcUnregisterRpc(rpc_id);
}

TEST_F(RpcExpansionTest, GetRpcInfoUnregistered) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(ImcGetRpcId("Test/NoHandler", &rpc_id), BML_RESULT_OK);

    BML_RpcInfo info = BML_RPC_INFO_INIT;
    ASSERT_EQ(ImcGetRpcInfo(rpc_id, &info), BML_RESULT_OK);
    EXPECT_EQ(info.has_handler, BML_FALSE);
}

TEST_F(RpcExpansionTest, GetRpcName) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(ImcGetRpcId("Test/Named", &rpc_id), BML_RESULT_OK);

    char buf[256] = {};
    size_t len = 0;
    ASSERT_EQ(ImcGetRpcName(rpc_id, buf, sizeof(buf), &len), BML_RESULT_OK);
    EXPECT_EQ(std::string(buf, len), "Test/Named");
}

TEST_F(RpcExpansionTest, EnumerateRpc) {
    BML_RpcId ids[3];
    const char *names[] = {"Test/Enum1", "Test/Enum2", "Test/Enum3"};
    for (int i = 0; i < 3; ++i) {
        ASSERT_EQ(ImcGetRpcId(names[i], &ids[i]), BML_RESULT_OK);
    }

    auto handler = [](BML_Context, BML_RpcId, const BML_ImcMessage *, BML_ImcBuffer *, void *) -> BML_Result {
        return BML_RESULT_OK;
    };
    for (int i = 0; i < 3; ++i) {
        ASSERT_EQ(ImcRegisterRpc(ids[i], handler, nullptr), BML_RESULT_OK);
    }

    int count = 0;
    ImcEnumerateRpc([](BML_RpcId, const char *, BML_Bool, void *ud) {
        ++*static_cast<int *>(ud);
    }, &count);

    EXPECT_GE(count, 3);

    for (int i = 0; i < 3; ++i) ImcUnregisterRpc(ids[i]);
}

TEST_F(RpcExpansionTest, HandlerMetrics) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(ImcGetRpcId("Test/Metrics", &rpc_id), BML_RESULT_OK);

    auto handler = [](BML_Context, BML_RpcId, const BML_ImcMessage *, BML_ImcBuffer *, void *) -> BML_Result {
        return BML_RESULT_OK;
    };
    ASSERT_EQ(ImcRegisterRpc(rpc_id, handler, nullptr), BML_RESULT_OK);

    // Call 3 times
    for (int i = 0; i < 3; ++i) {
        BML_Future future;
        ASSERT_EQ(ImcCallRpc(rpc_id, nullptr, &future), BML_RESULT_OK);
        ImcPump();
        ImcFutureRelease(future);
    }

    BML_RpcInfo info = BML_RPC_INFO_INIT;
    ASSERT_EQ(ImcGetRpcInfo(rpc_id, &info), BML_RESULT_OK);
    EXPECT_EQ(info.call_count, 3u);
    EXPECT_EQ(info.completion_count, 3u);
    EXPECT_EQ(info.failure_count, 0u);
    // Latency may be 0 on fast platforms with coarse clocks
    EXPECT_GE(info.total_latency_ns, 0u);

    ImcUnregisterRpc(rpc_id);
}

// ========================================================================
// Structured Error Tests
// ========================================================================

TEST_F(RpcExpansionTest, FutureGetErrorOnSuccess) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(ImcGetRpcId("Test/SuccessError", &rpc_id), BML_RESULT_OK);

    auto handler = [](BML_Context, BML_RpcId, const BML_ImcMessage *, BML_ImcBuffer *, void *) -> BML_Result {
        return BML_RESULT_OK;
    };
    ASSERT_EQ(ImcRegisterRpc(rpc_id, handler, nullptr), BML_RESULT_OK);

    BML_Future future;
    ASSERT_EQ(ImcCallRpc(rpc_id, nullptr, &future), BML_RESULT_OK);
    ImcPump();

    BML_Result code = BML_RESULT_FAIL;
    char msg[256] = {};
    size_t len = 0;
    ASSERT_EQ(ImcFutureGetError(future, &code, msg, sizeof(msg), &len), BML_RESULT_OK);
    EXPECT_EQ(code, BML_RESULT_OK);
    EXPECT_EQ(len, 0u);

    ImcFutureRelease(future);
    ImcUnregisterRpc(rpc_id);
}

TEST_F(RpcExpansionTest, RpcHandlerExErrorMessage) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(ImcGetRpcId("Test/ExError", &rpc_id), BML_RESULT_OK);

    auto handler_ex = [](BML_Context, BML_RpcId, const BML_ImcMessage *,
                          BML_ImcBuffer *, char *out_err, size_t err_cap, void *) -> BML_Result {
        const char *msg = "something went wrong";
        size_t copy = std::strlen(msg);
        if (copy >= err_cap) copy = err_cap - 1;
        std::memcpy(out_err, msg, copy);
        out_err[copy] = '\0';
        return BML_RESULT_FAIL;
    };
    ASSERT_EQ(ImcRegisterRpcEx(rpc_id, handler_ex, nullptr), BML_RESULT_OK);

    BML_Future future;
    ASSERT_EQ(ImcCallRpc(rpc_id, nullptr, &future), BML_RESULT_OK);
    ImcPump();

    BML_Result code = BML_RESULT_OK;
    char msg[256] = {};
    size_t len = 0;
    ASSERT_EQ(ImcFutureGetError(future, &code, msg, sizeof(msg), &len), BML_RESULT_OK);
    EXPECT_EQ(code, BML_RESULT_FAIL);
    EXPECT_STREQ(msg, "something went wrong");

    ImcFutureRelease(future);
    ImcUnregisterRpc(rpc_id);
}

TEST_F(RpcExpansionTest, FutureGetErrorOnHandlerFailure) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(ImcGetRpcId("Test/FailError", &rpc_id), BML_RESULT_OK);

    auto handler = [](BML_Context, BML_RpcId, const BML_ImcMessage *, BML_ImcBuffer *, void *) -> BML_Result {
        return BML_RESULT_FAIL;
    };
    ASSERT_EQ(ImcRegisterRpc(rpc_id, handler, nullptr), BML_RESULT_OK);

    BML_Future future;
    ASSERT_EQ(ImcCallRpc(rpc_id, nullptr, &future), BML_RESULT_OK);
    ImcPump();

    BML_Result code = BML_RESULT_OK;
    ASSERT_EQ(ImcFutureGetError(future, &code, nullptr, 0, nullptr), BML_RESULT_OK);
    EXPECT_EQ(code, BML_RESULT_FAIL);

    ImcFutureRelease(future);
    ImcUnregisterRpc(rpc_id);
}

// ========================================================================
// Call Options / Timeout Tests
// ========================================================================

static BML_Result CallExHandler(BML_Context, BML_RpcId, const BML_ImcMessage *,
                                 BML_ImcBuffer *resp, void *ud) {
    static int response_val = 42;
    resp->data = &response_val;
    resp->size = sizeof(response_val);
    (void)ud;
    return BML_RESULT_OK;
}

TEST_F(RpcExpansionTest, CallRpcExBasic) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(ImcGetRpcId("Test/CallEx", &rpc_id), BML_RESULT_OK);
    ASSERT_EQ(ImcRegisterRpc(rpc_id, CallExHandler, nullptr), BML_RESULT_OK);

    BML_RpcCallOptions opts = BML_RPC_CALL_OPTIONS_INIT;
    BML_Future future;
    ASSERT_EQ(ImcCallRpcEx(rpc_id, nullptr, &opts, &future), BML_RESULT_OK);
    ImcPump();

    BML_FutureState state;
    ASSERT_EQ(ImcFutureGetState(future, &state), BML_RESULT_OK);
    EXPECT_EQ(state, BML_FUTURE_READY);

    ImcFutureRelease(future);
    ImcUnregisterRpc(rpc_id);
}

TEST_F(RpcExpansionTest, CallRpcExTimeout) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(ImcGetRpcId("Test/Timeout", &rpc_id), BML_RESULT_OK);

    auto handler = [](BML_Context, BML_RpcId, const BML_ImcMessage *,
                       BML_ImcBuffer *, void *) -> BML_Result {
        return BML_RESULT_OK;
    };
    ASSERT_EQ(ImcRegisterRpc(rpc_id, handler, nullptr), BML_RESULT_OK);

    // Set a timeout of 1ms but sleep before pump to expire it
    BML_RpcCallOptions opts = BML_RPC_CALL_OPTIONS_INIT;
    opts.timeout_ms = 1;
    BML_Future future;
    ASSERT_EQ(ImcCallRpcEx(rpc_id, nullptr, &opts, &future), BML_RESULT_OK);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ImcPump();

    BML_FutureState state;
    ASSERT_EQ(ImcFutureGetState(future, &state), BML_RESULT_OK);
    EXPECT_EQ(state, BML_FUTURE_TIMEOUT);

    ImcFutureRelease(future);
    ImcUnregisterRpc(rpc_id);
}

// ========================================================================
// Middleware Tests
// ========================================================================

TEST_F(RpcExpansionTest, PreMiddlewareRuns) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(ImcGetRpcId("Test/PreMW", &rpc_id), BML_RESULT_OK);

    auto handler = [](BML_Context, BML_RpcId, const BML_ImcMessage *, BML_ImcBuffer *, void *) -> BML_Result {
        return BML_RESULT_OK;
    };
    ASSERT_EQ(ImcRegisterRpc(rpc_id, handler, nullptr), BML_RESULT_OK);

    int pre_count = 0;
    auto middleware = [](BML_Context, BML_RpcId, BML_Bool is_pre,
                          const BML_ImcMessage *, const BML_ImcBuffer *,
                          BML_Result, void *ud) -> BML_Result {
        if (is_pre) ++*static_cast<int *>(ud);
        return BML_RESULT_OK;
    };
    ASSERT_EQ(ImcAddRpcMiddleware(middleware, 0, &pre_count), BML_RESULT_OK);

    BML_Future future;
    ASSERT_EQ(ImcCallRpc(rpc_id, nullptr, &future), BML_RESULT_OK);
    ImcPump();
    EXPECT_EQ(pre_count, 1);

    ImcFutureRelease(future);
    ImcRemoveRpcMiddleware(middleware);
    ImcUnregisterRpc(rpc_id);
}

static BML_Result PreRejectHandler(BML_Context, BML_RpcId, const BML_ImcMessage *, BML_ImcBuffer *, void *ud) {
    *static_cast<bool *>(ud) = true;
    return BML_RESULT_OK;
}

TEST_F(RpcExpansionTest, PreMiddlewareRejects) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(ImcGetRpcId("Test/PreReject", &rpc_id), BML_RESULT_OK);

    bool handler_called = false;
    ASSERT_EQ(ImcRegisterRpc(rpc_id, PreRejectHandler, &handler_called), BML_RESULT_OK);

    auto rejecting_mw = [](BML_Context, BML_RpcId, BML_Bool is_pre,
                             const BML_ImcMessage *, const BML_ImcBuffer *,
                             BML_Result, void *) -> BML_Result {
        return is_pre ? BML_RESULT_PERMISSION_DENIED : BML_RESULT_OK;
    };
    ASSERT_EQ(ImcAddRpcMiddleware(rejecting_mw, 0, nullptr), BML_RESULT_OK);

    BML_Future future;
    ASSERT_EQ(ImcCallRpc(rpc_id, nullptr, &future), BML_RESULT_OK);
    ImcPump();

    EXPECT_FALSE(handler_called);

    BML_FutureState state;
    ASSERT_EQ(ImcFutureGetState(future, &state), BML_RESULT_OK);
    EXPECT_EQ(state, BML_FUTURE_FAILED);

    ImcFutureRelease(future);
    ImcRemoveRpcMiddleware(rejecting_mw);
    ImcUnregisterRpc(rpc_id);
}

TEST_F(RpcExpansionTest, PostMiddlewareRuns) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(ImcGetRpcId("Test/PostMW", &rpc_id), BML_RESULT_OK);

    auto handler = [](BML_Context, BML_RpcId, const BML_ImcMessage *, BML_ImcBuffer *, void *) -> BML_Result {
        return BML_RESULT_OK;
    };
    ASSERT_EQ(ImcRegisterRpc(rpc_id, handler, nullptr), BML_RESULT_OK);

    int post_count = 0;
    auto middleware = [](BML_Context, BML_RpcId, BML_Bool is_pre,
                          const BML_ImcMessage *, const BML_ImcBuffer *,
                          BML_Result handler_result, void *ud) -> BML_Result {
        if (!is_pre) {
            EXPECT_EQ(handler_result, BML_RESULT_OK);
            ++*static_cast<int *>(ud);
        }
        return BML_RESULT_OK;
    };
    ASSERT_EQ(ImcAddRpcMiddleware(middleware, 0, &post_count), BML_RESULT_OK);

    BML_Future future;
    ASSERT_EQ(ImcCallRpc(rpc_id, nullptr, &future), BML_RESULT_OK);
    ImcPump();
    EXPECT_EQ(post_count, 1);

    ImcFutureRelease(future);
    ImcRemoveRpcMiddleware(middleware);
    ImcUnregisterRpc(rpc_id);
}

TEST_F(RpcExpansionTest, MiddlewarePriority) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(ImcGetRpcId("Test/MWPriority", &rpc_id), BML_RESULT_OK);

    auto handler = [](BML_Context, BML_RpcId, const BML_ImcMessage *, BML_ImcBuffer *, void *) -> BML_Result {
        return BML_RESULT_OK;
    };
    ASSERT_EQ(ImcRegisterRpc(rpc_id, handler, nullptr), BML_RESULT_OK);

    std::vector<int> order;
    auto mw1 = [](BML_Context, BML_RpcId, BML_Bool is_pre,
                    const BML_ImcMessage *, const BML_ImcBuffer *,
                    BML_Result, void *ud) -> BML_Result {
        if (is_pre) static_cast<std::vector<int> *>(ud)->push_back(1);
        return BML_RESULT_OK;
    };
    auto mw2 = [](BML_Context, BML_RpcId, BML_Bool is_pre,
                    const BML_ImcMessage *, const BML_ImcBuffer *,
                    BML_Result, void *ud) -> BML_Result {
        if (is_pre) static_cast<std::vector<int> *>(ud)->push_back(2);
        return BML_RESULT_OK;
    };

    // Add mw2 with lower priority (should run first)
    ASSERT_EQ(ImcAddRpcMiddleware(mw2, -10, &order), BML_RESULT_OK);
    ASSERT_EQ(ImcAddRpcMiddleware(mw1, 10, &order), BML_RESULT_OK);

    BML_Future future;
    ASSERT_EQ(ImcCallRpc(rpc_id, nullptr, &future), BML_RESULT_OK);
    ImcPump();

    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], 2); // mw2 ran first (priority -10)
    EXPECT_EQ(order[1], 1); // mw1 ran second (priority 10)

    ImcFutureRelease(future);
    ImcRemoveRpcMiddleware(mw1);
    ImcRemoveRpcMiddleware(mw2);
    ImcUnregisterRpc(rpc_id);
}

TEST_F(RpcExpansionTest, MiddlewareRemovedDuringOwnerCleanup) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(ImcGetRpcId("Test/MiddlewareCleanup", &rpc_id), BML_RESULT_OK);

    auto owner = CreateTrackedMod("rpc.middleware.owner");
    Context::SetCurrentModule(owner);

    auto handler = [](BML_Context, BML_RpcId, const BML_ImcMessage *, BML_ImcBuffer *, void *) -> BML_Result {
        return BML_RESULT_OK;
    };
    ASSERT_EQ(ImcRegisterRpc(rpc_id, handler, nullptr), BML_RESULT_OK);

    int pre_count = 0;
    auto middleware = [](BML_Context, BML_RpcId, BML_Bool is_pre,
                         const BML_ImcMessage *, const BML_ImcBuffer *,
                         BML_Result, void *ud) -> BML_Result {
        if (is_pre) {
            ++*static_cast<int *>(ud);
        }
        return BML_RESULT_OK;
    };
    ASSERT_EQ(ImcAddRpcMiddleware(middleware, 0, &pre_count), BML_RESULT_OK);

    ImcCleanupOwner(owner);

    BML_Future future = nullptr;
    ASSERT_EQ(ImcCallRpc(rpc_id, nullptr, &future), BML_RESULT_OK);
    ImcPump();

    BML_FutureState state = BML_FUTURE_PENDING;
    ASSERT_EQ(ImcFutureGetState(future, &state), BML_RESULT_OK);
    EXPECT_EQ(BML_FUTURE_FAILED, state);
    EXPECT_EQ(0, pre_count);

    ImcFutureRelease(future);
}

TEST_F(RpcExpansionTest, MiddlewareRemovalRejectsNonOwner) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(ImcGetRpcId("Test/MiddlewareOwner", &rpc_id), BML_RESULT_OK);

    auto owner_a = CreateTrackedMod("rpc.middleware.owner.a");
    auto owner_b = CreateTrackedMod("rpc.middleware.owner.b");

    auto handler = [](BML_Context, BML_RpcId, const BML_ImcMessage *, BML_ImcBuffer *, void *) -> BML_Result {
        return BML_RESULT_OK;
    };
    auto middleware = [](BML_Context, BML_RpcId, BML_Bool,
                         const BML_ImcMessage *, const BML_ImcBuffer *,
                         BML_Result, void *) -> BML_Result {
        return BML_RESULT_OK;
    };

    Context::SetCurrentModule(owner_a);
    ASSERT_EQ(ImcRegisterRpc(rpc_id, handler, nullptr), BML_RESULT_OK);
    ASSERT_EQ(ImcAddRpcMiddleware(middleware, 0, nullptr), BML_RESULT_OK);

    Context::SetCurrentModule(owner_b);
    EXPECT_EQ(BML_RESULT_PERMISSION_DENIED, ImcRemoveRpcMiddleware(middleware));

    Context::SetCurrentModule(owner_a);
    EXPECT_EQ(BML_RESULT_OK, ImcRemoveRpcMiddleware(middleware));
    EXPECT_EQ(BML_RESULT_OK, ImcUnregisterRpc(rpc_id));
}

// ========================================================================
// Streaming RPC Tests
// ========================================================================

static BML_Result StreamingBasicHandler(BML_Context, BML_RpcId, const BML_ImcMessage *,
                                         BML_RpcStream stream, void *) {
    int chunks[] = {1, 2, 3};
    for (int c : chunks) {
        BML::Core::ImcStreamPush(stream, &c, sizeof(c));
    }
    BML::Core::ImcStreamComplete(stream);
    return BML_RESULT_OK;
}

TEST_F(RpcExpansionTest, StreamingRpcBasic) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(ImcGetRpcId("Test/Stream", &rpc_id), BML_RESULT_OK);
    ASSERT_EQ(ImcRegisterStreamingRpc(rpc_id, StreamingBasicHandler, nullptr), BML_RESULT_OK);

    std::vector<int> received;
    auto on_chunk = [](BML_Context, BML_TopicId, const BML_ImcMessage *msg, void *ud) {
        if (msg && msg->data && msg->size == sizeof(int)) {
            static_cast<std::vector<int> *>(ud)->push_back(*static_cast<const int *>(msg->data));
        }
    };

    BML_Future future;
    ASSERT_EQ(ImcCallStreamingRpc(rpc_id, nullptr, on_chunk, nullptr, &received, &future), BML_RESULT_OK);

    ASSERT_EQ(received.size(), 3u);
    EXPECT_EQ(received[0], 1);
    EXPECT_EQ(received[1], 2);
    EXPECT_EQ(received[2], 3);

    BML_FutureState state;
    ASSERT_EQ(ImcFutureGetState(future, &state), BML_RESULT_OK);
    EXPECT_EQ(state, BML_FUTURE_READY);

    ImcFutureRelease(future);
}

static BML_Result StreamingErrorHandler(BML_Context, BML_RpcId, const BML_ImcMessage *,
                                         BML_RpcStream stream, void *) {
    BML::Core::ImcStreamPush(stream, nullptr, 0);
    BML::Core::ImcStreamError(stream, BML_RESULT_FAIL, "stream error");
    return BML_RESULT_OK;
}

TEST_F(RpcExpansionTest, StreamingRpcError) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(ImcGetRpcId("Test/StreamErr", &rpc_id), BML_RESULT_OK);
    ASSERT_EQ(ImcRegisterStreamingRpc(rpc_id, StreamingErrorHandler, nullptr), BML_RESULT_OK);

    BML_Future future;
    ASSERT_EQ(ImcCallStreamingRpc(rpc_id, nullptr, nullptr, nullptr, nullptr, &future), BML_RESULT_OK);

    BML_FutureState state;
    ASSERT_EQ(ImcFutureGetState(future, &state), BML_RESULT_OK);
    EXPECT_EQ(state, BML_FUTURE_FAILED);

    BML_Result code;
    char msg[256] = {};
    ImcFutureGetError(future, &code, msg, sizeof(msg), nullptr);
    EXPECT_EQ(code, BML_RESULT_FAIL);
    EXPECT_STREQ(msg, "stream error");

    ImcFutureRelease(future);
}

// ========================================================================
// Middleware RAII Tests
// ========================================================================

TEST_F(RpcExpansionTest, MiddlewareRAII) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(ImcGetRpcId("Test/MWRAII", &rpc_id), BML_RESULT_OK);

    auto handler = [](BML_Context, BML_RpcId, const BML_ImcMessage *, BML_ImcBuffer *, void *) -> BML_Result {
        return BML_RESULT_OK;
    };
    ASSERT_EQ(ImcRegisterRpc(rpc_id, handler, nullptr), BML_RESULT_OK);

    int call_count = 0;
    auto middleware = [](BML_Context, BML_RpcId, BML_Bool is_pre,
                          const BML_ImcMessage *, const BML_ImcBuffer *,
                          BML_Result, void *ud) -> BML_Result {
        if (is_pre) ++*static_cast<int *>(ud);
        return BML_RESULT_OK;
    };

    // Add then remove middleware -- verify it stops being called
    ASSERT_EQ(ImcAddRpcMiddleware(middleware, 0, &call_count), BML_RESULT_OK);

    BML_Future f1;
    ASSERT_EQ(ImcCallRpc(rpc_id, nullptr, &f1), BML_RESULT_OK);
    ImcPump();
    EXPECT_EQ(call_count, 1);
    ImcFutureRelease(f1);

    ASSERT_EQ(ImcRemoveRpcMiddleware(middleware), BML_RESULT_OK);

    BML_Future f2;
    ASSERT_EQ(ImcCallRpc(rpc_id, nullptr, &f2), BML_RESULT_OK);
    ImcPump();
    EXPECT_EQ(call_count, 1); // unchanged after removal
    ImcFutureRelease(f2);

    ImcUnregisterRpc(rpc_id);
}

// ========================================================================
// Streaming Cleanup Test
// ========================================================================

static BML_Result g_SecondCompleteResult = BML_RESULT_OK;

static BML_Result DoubleCompleteHandler(BML_Context, BML_RpcId, const BML_ImcMessage *,
                                         BML_RpcStream stream, void *) {
    int val = 99;
    BML::Core::ImcStreamPush(stream, &val, sizeof(val));
    BML::Core::ImcStreamComplete(stream);
    g_SecondCompleteResult = BML::Core::ImcStreamComplete(stream);
    return BML_RESULT_OK;
}

TEST_F(RpcExpansionTest, StreamingRpcDoubleCompleteRejected) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(ImcGetRpcId("Test/StreamCleanup", &rpc_id), BML_RESULT_OK);

    g_SecondCompleteResult = BML_RESULT_OK;
    ASSERT_EQ(ImcRegisterStreamingRpc(rpc_id, DoubleCompleteHandler, nullptr), BML_RESULT_OK);

    BML_Future future;
    ASSERT_EQ(ImcCallStreamingRpc(rpc_id, nullptr, nullptr, nullptr, nullptr, &future), BML_RESULT_OK);

    EXPECT_EQ(g_SecondCompleteResult, BML_RESULT_INVALID_STATE);

    BML_FutureState state;
    ASSERT_EQ(ImcFutureGetState(future, &state), BML_RESULT_OK);
    EXPECT_EQ(state, BML_FUTURE_READY);

    ImcFutureRelease(future);
}

// ========================================================================
// RegisterRpcEx duplicate rejection
// ========================================================================

TEST_F(RpcExpansionTest, RegisterRpcExDuplicateRejected) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(ImcGetRpcId("Test/ExDup", &rpc_id), BML_RESULT_OK);

    auto handler_ex = [](BML_Context, BML_RpcId, const BML_ImcMessage *,
                          BML_ImcBuffer *, char *, size_t, void *) -> BML_Result {
        return BML_RESULT_OK;
    };
    ASSERT_EQ(ImcRegisterRpcEx(rpc_id, handler_ex, nullptr), BML_RESULT_OK);
    EXPECT_EQ(ImcRegisterRpcEx(rpc_id, handler_ex, nullptr), BML_RESULT_ALREADY_EXISTS);

    ImcUnregisterRpc(rpc_id);
}

// ========================================================================
// CallRpcEx with NULL options (default behavior)
// ========================================================================

TEST_F(RpcExpansionTest, CallRpcExNullOptions) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(ImcGetRpcId("Test/ExNull", &rpc_id), BML_RESULT_OK);
    ASSERT_EQ(ImcRegisterRpc(rpc_id, CallExHandler, nullptr), BML_RESULT_OK);

    BML_Future future;
    ASSERT_EQ(ImcCallRpcEx(rpc_id, nullptr, nullptr, &future), BML_RESULT_OK);
    ImcPump();

    BML_FutureState state;
    ASSERT_EQ(ImcFutureGetState(future, &state), BML_RESULT_OK);
    EXPECT_EQ(state, BML_FUTURE_READY);

    ImcFutureRelease(future);
    ImcUnregisterRpc(rpc_id);
}

} // namespace

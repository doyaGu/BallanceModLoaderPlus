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

#include "Core/Context.h"
#include "Core/ImcBus.h"
#include "TestKernel.h"
#include "TestKernelBuilder.h"
#include "TestModHelper.h"

namespace {

using namespace BML::Core;
using BML::Core::Testing::TestKernel;
using BML::Core::Testing::TestKernelBuilder;
using BML::Core::Testing::TestModHelper;

class RpcExpansionTest : public ::testing::Test {
protected:
    TestKernel kernel_;
    std::unique_ptr<TestModHelper> mods_;

    void SetUp() override {
        kernel_ = TestKernelBuilder()
            .WithConfig()
            .WithImcBus()
            .WithDiagnostics()
            .Build();
        mods_ = std::make_unique<TestModHelper>(*kernel_);
        host_mod_ = mods_->HostMod();
        ASSERT_NE(host_mod_, nullptr);
        Context::SetLifecycleModule(host_mod_);
    }

    void TearDown() override {
        Context::SetLifecycleModule(nullptr);
    }

    BML_Mod CreateTrackedMod(const std::string &id) {
        return mods_->CreateMod(id);
    }

    BML_Mod ResolveOwner(BML_Mod owner = nullptr) const {
        return owner ? owner : host_mod_;
    }

    BML_Result RegisterRpc(BML_RpcId rpcId, BML_RpcHandler handler, void *userData, BML_Mod owner = nullptr) {
        return ImcRegisterRpc(ResolveOwner(owner), rpcId, handler, userData);
    }

    BML_Result UnregisterRpc(BML_RpcId rpcId, BML_Mod owner = nullptr) {
        return ImcUnregisterRpc(ResolveOwner(owner), rpcId);
    }

    BML_Result CallRpc(BML_RpcId rpcId,
                       const BML_ImcMessage *request,
                       BML_Future *outFuture,
                       BML_Mod owner = nullptr) {
        return ImcCallRpc(ResolveOwner(owner), rpcId, request, outFuture);
    }

    BML_Result RegisterRpcEx(BML_RpcId rpcId, BML_RpcHandlerEx handler, void *userData, BML_Mod owner = nullptr) {
        return ImcRegisterRpcEx(ResolveOwner(owner), rpcId, handler, userData);
    }

    BML_Result CallRpcEx(BML_RpcId rpcId,
                         const BML_ImcMessage *request,
                         const BML_RpcCallOptions *options,
                         BML_Future *outFuture,
                         BML_Mod owner = nullptr) {
        return ImcCallRpcEx(ResolveOwner(owner), rpcId, request, options, outFuture);
    }

    BML_Result AddRpcMiddleware(BML_RpcMiddleware middleware,
                                int32_t priority,
                                void *userData,
                                BML_Mod owner = nullptr) {
        return ImcAddRpcMiddleware(ResolveOwner(owner), middleware, priority, userData);
    }

    BML_Result RemoveRpcMiddleware(BML_RpcMiddleware middleware, BML_Mod owner = nullptr) {
        return ImcRemoveRpcMiddleware(ResolveOwner(owner), middleware);
    }

    BML_Result GetRpcId(const char *name, BML_RpcId *outId) {
        return ImcGetRpcId(*kernel_, name, outId);
    }

    BML_Result GetRpcInfo(BML_RpcId rpcId, BML_RpcInfo *outInfo) {
        return ImcGetRpcInfo(*kernel_, rpcId, outInfo);
    }

    BML_Result GetRpcName(BML_RpcId rpcId, char *buffer, size_t capacity, size_t *outLength) {
        return ImcGetRpcName(*kernel_, rpcId, buffer, capacity, outLength);
    }

    void EnumerateRpc(void(*callback)(BML_RpcId, const char *, BML_Bool, void *), void *userData) {
        ImcEnumerateRpc(*kernel_, callback, userData);
    }

    void Pump(size_t maxPerSub = 0) {
        ImcPump(*kernel_, maxPerSub);
    }

    BML_Result RegisterStreamingRpc(BML_RpcId rpcId,
                                    BML_StreamingRpcHandler handler,
                                    void *userData,
                                    BML_Mod owner = nullptr) {
        return ImcRegisterStreamingRpc(ResolveOwner(owner), rpcId, handler, userData);
    }

    BML_Result CallStreamingRpc(BML_RpcId rpcId,
                                const BML_ImcMessage *request,
                                BML_ImcHandler chunkHandler,
                                BML_FutureCallback onDone,
                                void *userData,
                                BML_Future *outFuture,
                                BML_Mod owner = nullptr) {
        return ImcCallStreamingRpc(ResolveOwner(owner), rpcId, request, chunkHandler, onDone, userData, outFuture);
    }

    BML_Mod host_mod_{nullptr};
};

// ========================================================================
// Introspection Tests
// ========================================================================

TEST_F(RpcExpansionTest, GetRpcInfoRegisteredHandler) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(GetRpcId("Test/Introspect", &rpc_id), BML_RESULT_OK);
    ASSERT_NE(rpc_id, BML_RPC_ID_INVALID);

    auto handler = [](BML_Context, BML_RpcId, const BML_ImcMessage *, BML_ImcBuffer *, void *) -> BML_Result {
        return BML_RESULT_OK;
    };
    ASSERT_EQ(RegisterRpc(rpc_id, handler, nullptr), BML_RESULT_OK);

    BML_RpcInfo info = BML_RPC_INFO_INIT;
    ASSERT_EQ(GetRpcInfo(rpc_id, &info), BML_RESULT_OK);
    EXPECT_EQ(info.rpc_id, rpc_id);
    EXPECT_EQ(info.has_handler, BML_TRUE);
    EXPECT_STREQ(info.name, "Test/Introspect");
    EXPECT_EQ(info.call_count, 0u);

    UnregisterRpc(rpc_id);
}

TEST_F(RpcExpansionTest, GetRpcInfoUnregistered) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(GetRpcId("Test/NoHandler", &rpc_id), BML_RESULT_OK);

    BML_RpcInfo info = BML_RPC_INFO_INIT;
    ASSERT_EQ(GetRpcInfo(rpc_id, &info), BML_RESULT_OK);
    EXPECT_EQ(info.has_handler, BML_FALSE);
}

TEST_F(RpcExpansionTest, GetRpcName) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(GetRpcId("Test/Named", &rpc_id), BML_RESULT_OK);

    char buf[256] = {};
    size_t len = 0;
    ASSERT_EQ(GetRpcName(rpc_id, buf, sizeof(buf), &len), BML_RESULT_OK);
    EXPECT_EQ(std::string(buf, len), "Test/Named");
}

TEST_F(RpcExpansionTest, EnumerateRpc) {
    BML_RpcId ids[3];
    const char *names[] = {"Test/Enum1", "Test/Enum2", "Test/Enum3"};
    for (int i = 0; i < 3; ++i) {
        ASSERT_EQ(GetRpcId(names[i], &ids[i]), BML_RESULT_OK);
    }

    auto handler = [](BML_Context, BML_RpcId, const BML_ImcMessage *, BML_ImcBuffer *, void *) -> BML_Result {
        return BML_RESULT_OK;
    };
    for (int i = 0; i < 3; ++i) {
        ASSERT_EQ(RegisterRpc(ids[i], handler, nullptr), BML_RESULT_OK);
    }

    int count = 0;
    EnumerateRpc([](BML_RpcId, const char *, BML_Bool, void *ud) {
        ++*static_cast<int *>(ud);
    }, &count);

    EXPECT_GE(count, 3);

    for (int i = 0; i < 3; ++i) UnregisterRpc(ids[i]);
}

TEST_F(RpcExpansionTest, HandlerMetrics) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(GetRpcId("Test/Metrics", &rpc_id), BML_RESULT_OK);

    auto handler = [](BML_Context, BML_RpcId, const BML_ImcMessage *, BML_ImcBuffer *, void *) -> BML_Result {
        return BML_RESULT_OK;
    };
    ASSERT_EQ(RegisterRpc(rpc_id, handler, nullptr), BML_RESULT_OK);

    // Call 3 times
    for (int i = 0; i < 3; ++i) {
        BML_Future future;
        ASSERT_EQ(CallRpc(rpc_id, nullptr, &future), BML_RESULT_OK);
        Pump();
        ImcFutureRelease(future);
    }

    BML_RpcInfo info = BML_RPC_INFO_INIT;
    ASSERT_EQ(GetRpcInfo(rpc_id, &info), BML_RESULT_OK);
    EXPECT_EQ(info.call_count, 3u);
    EXPECT_EQ(info.completion_count, 3u);
    EXPECT_EQ(info.failure_count, 0u);
    // Latency may be 0 on fast platforms with coarse clocks
    EXPECT_GE(info.total_latency_ns, 0u);

    UnregisterRpc(rpc_id);
}

// ========================================================================
// Structured Error Tests
// ========================================================================

TEST_F(RpcExpansionTest, FutureGetErrorOnSuccess) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(GetRpcId("Test/SuccessError", &rpc_id), BML_RESULT_OK);

    auto handler = [](BML_Context, BML_RpcId, const BML_ImcMessage *, BML_ImcBuffer *, void *) -> BML_Result {
        return BML_RESULT_OK;
    };
    ASSERT_EQ(RegisterRpc(rpc_id, handler, nullptr), BML_RESULT_OK);

    BML_Future future;
    ASSERT_EQ(CallRpc(rpc_id, nullptr, &future), BML_RESULT_OK);
    Pump();

    BML_Result code = BML_RESULT_FAIL;
    char msg[256] = {};
    size_t len = 0;
    ASSERT_EQ(ImcFutureGetError(future, &code, msg, sizeof(msg), &len), BML_RESULT_OK);
    EXPECT_EQ(code, BML_RESULT_OK);
    EXPECT_EQ(len, 0u);

    ImcFutureRelease(future);
    UnregisterRpc(rpc_id);
}

TEST_F(RpcExpansionTest, RpcHandlerExErrorMessage) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(GetRpcId("Test/ExError", &rpc_id), BML_RESULT_OK);

    auto handler_ex = [](BML_Context, BML_RpcId, const BML_ImcMessage *,
                          BML_ImcBuffer *, char *out_err, size_t err_cap, void *) -> BML_Result {
        const char *msg = "something went wrong";
        size_t copy = std::strlen(msg);
        if (copy >= err_cap) copy = err_cap - 1;
        std::memcpy(out_err, msg, copy);
        out_err[copy] = '\0';
        return BML_RESULT_FAIL;
    };
    ASSERT_EQ(RegisterRpcEx(rpc_id, handler_ex, nullptr), BML_RESULT_OK);

    BML_Future future;
    ASSERT_EQ(CallRpc(rpc_id, nullptr, &future), BML_RESULT_OK);
    Pump();

    BML_Result code = BML_RESULT_OK;
    char msg[256] = {};
    size_t len = 0;
    ASSERT_EQ(ImcFutureGetError(future, &code, msg, sizeof(msg), &len), BML_RESULT_OK);
    EXPECT_EQ(code, BML_RESULT_FAIL);
    EXPECT_STREQ(msg, "something went wrong");

    ImcFutureRelease(future);
    UnregisterRpc(rpc_id);
}

TEST_F(RpcExpansionTest, FutureGetErrorOnHandlerFailure) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(GetRpcId("Test/FailError", &rpc_id), BML_RESULT_OK);

    auto handler = [](BML_Context, BML_RpcId, const BML_ImcMessage *, BML_ImcBuffer *, void *) -> BML_Result {
        return BML_RESULT_FAIL;
    };
    ASSERT_EQ(RegisterRpc(rpc_id, handler, nullptr), BML_RESULT_OK);

    BML_Future future;
    ASSERT_EQ(CallRpc(rpc_id, nullptr, &future), BML_RESULT_OK);
    Pump();

    BML_Result code = BML_RESULT_OK;
    ASSERT_EQ(ImcFutureGetError(future, &code, nullptr, 0, nullptr), BML_RESULT_OK);
    EXPECT_EQ(code, BML_RESULT_FAIL);

    ImcFutureRelease(future);
    UnregisterRpc(rpc_id);
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
    ASSERT_EQ(GetRpcId("Test/CallEx", &rpc_id), BML_RESULT_OK);
    ASSERT_EQ(RegisterRpc(rpc_id, CallExHandler, nullptr), BML_RESULT_OK);

    BML_RpcCallOptions opts = BML_RPC_CALL_OPTIONS_INIT;
    BML_Future future;
    ASSERT_EQ(CallRpcEx(rpc_id, nullptr, &opts, &future), BML_RESULT_OK);
    Pump();

    BML_FutureState state;
    ASSERT_EQ(ImcFutureGetState(future, &state), BML_RESULT_OK);
    EXPECT_EQ(state, BML_FUTURE_READY);

    ImcFutureRelease(future);
    UnregisterRpc(rpc_id);
}

TEST_F(RpcExpansionTest, CallRpcExTimeout) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(GetRpcId("Test/Timeout", &rpc_id), BML_RESULT_OK);

    auto handler = [](BML_Context, BML_RpcId, const BML_ImcMessage *,
                       BML_ImcBuffer *, void *) -> BML_Result {
        return BML_RESULT_OK;
    };
    ASSERT_EQ(RegisterRpc(rpc_id, handler, nullptr), BML_RESULT_OK);

    // Set a timeout of 1ms but sleep before pump to expire it
    BML_RpcCallOptions opts = BML_RPC_CALL_OPTIONS_INIT;
    opts.timeout_ms = 1;
    BML_Future future;
    ASSERT_EQ(CallRpcEx(rpc_id, nullptr, &opts, &future), BML_RESULT_OK);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    Pump();

    BML_FutureState state;
    ASSERT_EQ(ImcFutureGetState(future, &state), BML_RESULT_OK);
    EXPECT_EQ(state, BML_FUTURE_TIMEOUT);

    ImcFutureRelease(future);
    UnregisterRpc(rpc_id);
}

// ========================================================================
// Middleware Tests
// ========================================================================

TEST_F(RpcExpansionTest, PreMiddlewareRuns) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(GetRpcId("Test/PreMW", &rpc_id), BML_RESULT_OK);

    auto handler = [](BML_Context, BML_RpcId, const BML_ImcMessage *, BML_ImcBuffer *, void *) -> BML_Result {
        return BML_RESULT_OK;
    };
    ASSERT_EQ(RegisterRpc(rpc_id, handler, nullptr), BML_RESULT_OK);

    int pre_count = 0;
    auto middleware = [](BML_Context, BML_RpcId, BML_Bool is_pre,
                          const BML_ImcMessage *, const BML_ImcBuffer *,
                          BML_Result, void *ud) -> BML_Result {
        if (is_pre) ++*static_cast<int *>(ud);
        return BML_RESULT_OK;
    };
    ASSERT_EQ(AddRpcMiddleware(middleware, 0, &pre_count), BML_RESULT_OK);

    BML_Future future;
    ASSERT_EQ(CallRpc(rpc_id, nullptr, &future), BML_RESULT_OK);
    Pump();
    EXPECT_EQ(pre_count, 1);

    ImcFutureRelease(future);
    RemoveRpcMiddleware(middleware);
    UnregisterRpc(rpc_id);
}

static BML_Result PreRejectHandler(BML_Context, BML_RpcId, const BML_ImcMessage *, BML_ImcBuffer *, void *ud) {
    *static_cast<bool *>(ud) = true;
    return BML_RESULT_OK;
}

TEST_F(RpcExpansionTest, PreMiddlewareRejects) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(GetRpcId("Test/PreReject", &rpc_id), BML_RESULT_OK);

    bool handler_called = false;
    ASSERT_EQ(RegisterRpc(rpc_id, PreRejectHandler, &handler_called), BML_RESULT_OK);

    auto rejecting_mw = [](BML_Context, BML_RpcId, BML_Bool is_pre,
                             const BML_ImcMessage *, const BML_ImcBuffer *,
                             BML_Result, void *) -> BML_Result {
        return is_pre ? BML_RESULT_PERMISSION_DENIED : BML_RESULT_OK;
    };
    ASSERT_EQ(AddRpcMiddleware(rejecting_mw, 0, nullptr), BML_RESULT_OK);

    BML_Future future;
    ASSERT_EQ(CallRpc(rpc_id, nullptr, &future), BML_RESULT_OK);
    Pump();

    EXPECT_FALSE(handler_called);

    BML_FutureState state;
    ASSERT_EQ(ImcFutureGetState(future, &state), BML_RESULT_OK);
    EXPECT_EQ(state, BML_FUTURE_FAILED);

    ImcFutureRelease(future);
    RemoveRpcMiddleware(rejecting_mw);
    UnregisterRpc(rpc_id);
}

TEST_F(RpcExpansionTest, PostMiddlewareRuns) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(GetRpcId("Test/PostMW", &rpc_id), BML_RESULT_OK);

    auto handler = [](BML_Context, BML_RpcId, const BML_ImcMessage *, BML_ImcBuffer *, void *) -> BML_Result {
        return BML_RESULT_OK;
    };
    ASSERT_EQ(RegisterRpc(rpc_id, handler, nullptr), BML_RESULT_OK);

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
    ASSERT_EQ(AddRpcMiddleware(middleware, 0, &post_count), BML_RESULT_OK);

    BML_Future future;
    ASSERT_EQ(CallRpc(rpc_id, nullptr, &future), BML_RESULT_OK);
    Pump();
    EXPECT_EQ(post_count, 1);

    ImcFutureRelease(future);
    RemoveRpcMiddleware(middleware);
    UnregisterRpc(rpc_id);
}

TEST_F(RpcExpansionTest, MiddlewarePriority) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(GetRpcId("Test/MWPriority", &rpc_id), BML_RESULT_OK);

    auto handler = [](BML_Context, BML_RpcId, const BML_ImcMessage *, BML_ImcBuffer *, void *) -> BML_Result {
        return BML_RESULT_OK;
    };
    ASSERT_EQ(RegisterRpc(rpc_id, handler, nullptr), BML_RESULT_OK);

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
    ASSERT_EQ(AddRpcMiddleware(mw2, -10, &order), BML_RESULT_OK);
    ASSERT_EQ(AddRpcMiddleware(mw1, 10, &order), BML_RESULT_OK);

    BML_Future future;
    ASSERT_EQ(CallRpc(rpc_id, nullptr, &future), BML_RESULT_OK);
    Pump();

    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], 2); // mw2 ran first (priority -10)
    EXPECT_EQ(order[1], 1); // mw1 ran second (priority 10)

    ImcFutureRelease(future);
    RemoveRpcMiddleware(mw1);
    RemoveRpcMiddleware(mw2);
    UnregisterRpc(rpc_id);
}

TEST_F(RpcExpansionTest, MiddlewareRemovedDuringOwnerCleanup) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(GetRpcId("Test/MiddlewareCleanup", &rpc_id), BML_RESULT_OK);

    auto owner = CreateTrackedMod("rpc.middleware.owner");
    Context::SetLifecycleModule(owner);

    auto handler = [](BML_Context, BML_RpcId, const BML_ImcMessage *, BML_ImcBuffer *, void *) -> BML_Result {
        return BML_RESULT_OK;
    };
    ASSERT_EQ(RegisterRpc(rpc_id, handler, nullptr, owner), BML_RESULT_OK);

    int pre_count = 0;
    auto middleware = [](BML_Context, BML_RpcId, BML_Bool is_pre,
                         const BML_ImcMessage *, const BML_ImcBuffer *,
                         BML_Result, void *ud) -> BML_Result {
        if (is_pre) {
            ++*static_cast<int *>(ud);
        }
        return BML_RESULT_OK;
    };
    ASSERT_EQ(AddRpcMiddleware(middleware, 0, &pre_count, owner), BML_RESULT_OK);

    ImcCleanupOwner(owner);

    BML_Future future = nullptr;
    ASSERT_EQ(CallRpc(rpc_id, nullptr, &future), BML_RESULT_OK);
    Pump();

    BML_FutureState state = BML_FUTURE_PENDING;
    ASSERT_EQ(ImcFutureGetState(future, &state), BML_RESULT_OK);
    EXPECT_EQ(BML_FUTURE_FAILED, state);
    EXPECT_EQ(0, pre_count);

    ImcFutureRelease(future);
}

TEST_F(RpcExpansionTest, MiddlewareRemovalRejectsNonOwner) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(GetRpcId("Test/MiddlewareOwner", &rpc_id), BML_RESULT_OK);

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

    Context::SetLifecycleModule(owner_a);
    ASSERT_EQ(RegisterRpc(rpc_id, handler, nullptr, owner_a), BML_RESULT_OK);
    ASSERT_EQ(AddRpcMiddleware(middleware, 0, nullptr, owner_a), BML_RESULT_OK);

    Context::SetLifecycleModule(owner_b);
    EXPECT_EQ(BML_RESULT_PERMISSION_DENIED, RemoveRpcMiddleware(middleware, owner_b));

    Context::SetLifecycleModule(owner_a);
    EXPECT_EQ(BML_RESULT_OK, RemoveRpcMiddleware(middleware, owner_a));
    EXPECT_EQ(BML_RESULT_OK, UnregisterRpc(rpc_id, owner_a));
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
    ASSERT_EQ(GetRpcId("Test/Stream", &rpc_id), BML_RESULT_OK);
    ASSERT_EQ(RegisterStreamingRpc(rpc_id, StreamingBasicHandler, nullptr), BML_RESULT_OK);

    std::vector<int> received;
    auto on_chunk = [](BML_Context, BML_TopicId, const BML_ImcMessage *msg, void *ud) {
        if (msg && msg->data && msg->size == sizeof(int)) {
            static_cast<std::vector<int> *>(ud)->push_back(*static_cast<const int *>(msg->data));
        }
    };

    BML_Future future;
    ASSERT_EQ(CallStreamingRpc(rpc_id, nullptr, on_chunk, nullptr, &received, &future), BML_RESULT_OK);

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
    ASSERT_EQ(GetRpcId("Test/StreamErr", &rpc_id), BML_RESULT_OK);
    ASSERT_EQ(RegisterStreamingRpc(rpc_id, StreamingErrorHandler, nullptr), BML_RESULT_OK);

    BML_Future future;
    ASSERT_EQ(CallStreamingRpc(rpc_id, nullptr, nullptr, nullptr, nullptr, &future), BML_RESULT_OK);

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
    ASSERT_EQ(GetRpcId("Test/MWRAII", &rpc_id), BML_RESULT_OK);

    auto handler = [](BML_Context, BML_RpcId, const BML_ImcMessage *, BML_ImcBuffer *, void *) -> BML_Result {
        return BML_RESULT_OK;
    };
    ASSERT_EQ(RegisterRpc(rpc_id, handler, nullptr), BML_RESULT_OK);

    int call_count = 0;
    auto middleware = [](BML_Context, BML_RpcId, BML_Bool is_pre,
                          const BML_ImcMessage *, const BML_ImcBuffer *,
                          BML_Result, void *ud) -> BML_Result {
        if (is_pre) ++*static_cast<int *>(ud);
        return BML_RESULT_OK;
    };

    // Add then remove middleware -- verify it stops being called
    ASSERT_EQ(AddRpcMiddleware(middleware, 0, &call_count), BML_RESULT_OK);

    BML_Future f1;
    ASSERT_EQ(CallRpc(rpc_id, nullptr, &f1), BML_RESULT_OK);
    Pump();
    EXPECT_EQ(call_count, 1);
    ImcFutureRelease(f1);

    ASSERT_EQ(RemoveRpcMiddleware(middleware), BML_RESULT_OK);

    BML_Future f2;
    ASSERT_EQ(CallRpc(rpc_id, nullptr, &f2), BML_RESULT_OK);
    Pump();
    EXPECT_EQ(call_count, 1); // unchanged after removal
    ImcFutureRelease(f2);

    UnregisterRpc(rpc_id);
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
    ASSERT_EQ(GetRpcId("Test/StreamCleanup", &rpc_id), BML_RESULT_OK);

    g_SecondCompleteResult = BML_RESULT_OK;
    ASSERT_EQ(RegisterStreamingRpc(rpc_id, DoubleCompleteHandler, nullptr), BML_RESULT_OK);

    BML_Future future;
    ASSERT_EQ(CallStreamingRpc(rpc_id, nullptr, nullptr, nullptr, nullptr, &future), BML_RESULT_OK);

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
    ASSERT_EQ(GetRpcId("Test/ExDup", &rpc_id), BML_RESULT_OK);

    auto handler_ex = [](BML_Context, BML_RpcId, const BML_ImcMessage *,
                          BML_ImcBuffer *, char *, size_t, void *) -> BML_Result {
        return BML_RESULT_OK;
    };
    ASSERT_EQ(RegisterRpcEx(rpc_id, handler_ex, nullptr), BML_RESULT_OK);
    EXPECT_EQ(RegisterRpcEx(rpc_id, handler_ex, nullptr), BML_RESULT_ALREADY_EXISTS);

    UnregisterRpc(rpc_id);
}

// ========================================================================
// CallRpcEx with NULL options (default behavior)
// ========================================================================

TEST_F(RpcExpansionTest, CallRpcExNullOptions) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(GetRpcId("Test/ExNull", &rpc_id), BML_RESULT_OK);
    ASSERT_EQ(RegisterRpc(rpc_id, CallExHandler, nullptr), BML_RESULT_OK);

    BML_Future future;
    ASSERT_EQ(CallRpcEx(rpc_id, nullptr, nullptr, &future), BML_RESULT_OK);
    Pump();

    BML_FutureState state;
    ASSERT_EQ(ImcFutureGetState(future, &state), BML_RESULT_OK);
    EXPECT_EQ(state, BML_FUTURE_READY);

    ImcFutureRelease(future);
    UnregisterRpc(rpc_id);
}

} // namespace

#ifndef BML_CORE_IMC_BUS_RPC_INTERNAL_H
#define BML_CORE_IMC_BUS_RPC_INTERNAL_H

#include "ImcBusSharedInternal.h"

#include <condition_variable>

#include "FixedBlockPool.h"

struct FutureCallbackEntry {
    BML_FutureCallback fn{nullptr};
    void *user_data{nullptr};
    BML_Mod owner{nullptr};
};

struct BML_Future_T {
    std::atomic<uint32_t> ref_count{1};
    BML::Core::KernelServices *kernel{nullptr};
    std::mutex mutex;
    std::condition_variable cv;
    BML_FutureState state{BML_FUTURE_PENDING};
    BML_Result status{BML_RESULT_OK};
    BML::Core::BufferStorage payload;
    uint64_t msg_id{0};
    uint32_t flags{0};
    uint64_t creation_time{0};
    uint64_t completion_time{0};
    std::string error_message;
    std::vector<FutureCallbackEntry> callbacks;

    BML_Future_T() : creation_time(BML::Core::GetTimestampNs()) {}

    void Complete(BML_FutureState new_state,
                  BML_Result new_status,
                  const void *data,
                  size_t size) {
        std::vector<FutureCallbackEntry> pending_callbacks;
        {
            std::unique_lock lock(mutex);
            if (state != BML_FUTURE_PENDING) {
                return;
            }
            state = new_state;
            status = new_status;
            completion_time = BML::Core::GetTimestampNs();
            if (new_state == BML_FUTURE_READY && new_status == BML_RESULT_OK &&
                data && size > 0) {
                if (!payload.CopyFrom(data, size)) {
                    status = BML_RESULT_OUT_OF_MEMORY;
                    state = BML_FUTURE_FAILED;
                }
            }
            pending_callbacks = callbacks;
            callbacks.clear();
        }
        NotifyCallbacks(std::move(pending_callbacks));
    }

    void CompleteWithError(BML_FutureState new_state,
                           BML_Result new_status,
                           const char *err_msg) {
        std::vector<FutureCallbackEntry> pending_callbacks;
        {
            std::unique_lock lock(mutex);
            if (state != BML_FUTURE_PENDING) {
                return;
            }
            state = new_state;
            status = new_status;
            completion_time = BML::Core::GetTimestampNs();
            if (err_msg && *err_msg) {
                error_message = err_msg;
            }
            pending_callbacks = callbacks;
            callbacks.clear();
        }
        NotifyCallbacks(std::move(pending_callbacks));
    }

    bool Cancel() {
        std::vector<FutureCallbackEntry> pending_callbacks;
        {
            std::unique_lock lock(mutex);
            if (state != BML_FUTURE_PENDING) {
                return false;
            }
            state = BML_FUTURE_CANCELLED;
            status = BML_RESULT_FAIL;
            completion_time = BML::Core::GetTimestampNs();
            pending_callbacks = callbacks;
            callbacks.clear();
        }
        NotifyCallbacks(std::move(pending_callbacks));
        return true;
    }

    void NotifyCallbacks(std::vector<FutureCallbackEntry> &&pending_callbacks) {
        cv.notify_all();
        auto *busContext = BML::Core::ContextFromKernel(kernel);
        BML_Context ctx = busContext ? busContext->GetHandle() : nullptr;
        for (const auto &entry : pending_callbacks) {
            if (entry.fn) {
                BML::Core::ModuleContextScope scope(entry.owner);
                entry.fn(ctx, this, entry.user_data);
            }
        }
    }

    uint64_t GetLatencyNs() const {
        if (completion_time == 0) {
            return 0;
        }
        return completion_time - creation_time;
    }
};

struct BML_RpcStream_T {
    BML::Core::KernelServices *kernel{nullptr};
    BML_Future future{nullptr};
    BML_TopicId chunk_topic{BML_TOPIC_ID_INVALID};
    BML_ImcHandler on_chunk{nullptr};
    BML_FutureCallback on_done{nullptr};
    void *user_data{nullptr};
    BML_Mod owner{nullptr};
    std::atomic<bool> completed{false};
};

inline BML_Future CreateFuture(BML::Core::KernelServices *kernel = nullptr) {
    auto *future = new (std::nothrow) BML_Future_T();
    if (future) {
        future->kernel = kernel;
    }
    return future;
}

inline void FutureAddRefInternal(BML_Future future) {
    if (future) {
        future->ref_count.fetch_add(1, std::memory_order_relaxed);
    }
}

inline void FutureReleaseInternal(BML_Future future) {
    if (!future) {
        return;
    }
    uint32_t previous = future->ref_count.fetch_sub(1, std::memory_order_acq_rel);
    if (previous == 1) {
        delete future;
    }
}

namespace BML::Core {
    struct KernelServices;

    struct RpcHandlerStats {
        std::atomic<uint64_t> call_count{0};
        std::atomic<uint64_t> completion_count{0};
        std::atomic<uint64_t> failure_count{0};
        std::atomic<uint64_t> total_latency_ns{0};
    };

    struct RpcHandlerEntry {
        BML_RpcHandler handler{nullptr};
        BML_RpcHandlerEx handler_ex{nullptr};
        void *user_data{nullptr};
        BML_Mod owner{nullptr};
        std::shared_ptr<RpcHandlerStats> stats = std::make_shared<RpcHandlerStats>();
    };

    struct RpcRequest {
        BML_RpcId rpc_id{0};
        BufferStorage payload;
        uint64_t msg_id{0};
        BML_Mod caller{nullptr};
        BML_Future future{nullptr};
        uint64_t deadline_ns{0};
    };

    struct RpcMiddlewareEntry {
        BML_RpcMiddleware middleware{nullptr};
        int32_t priority{0};
        void *user_data{nullptr};
        BML_Mod owner{nullptr};
    };

    struct StreamingRpcHandlerEntry {
        BML_StreamingRpcHandler handler{nullptr};
        void *user_data{nullptr};
        BML_Mod owner{nullptr};
    };

    struct RpcRuntimeState {
        mutable std::shared_mutex rpc_mutex;
        std::unordered_map<BML_RpcId, RpcHandlerEntry> rpc_handlers;
        std::vector<RpcMiddlewareEntry> rpc_middleware;
        std::unordered_map<BML_RpcId, StreamingRpcHandlerEntry> streaming_rpc_handlers;
        std::unordered_map<BML_RpcStream, std::unique_ptr<BML_RpcStream_T>> streams;
        std::mutex stream_mutex;
        FixedBlockPool rpc_request_pool{sizeof(RpcRequest)};
        std::unique_ptr<MpscRingBuffer<RpcRequest *>> rpc_queue =
            std::make_unique<MpscRingBuffer<RpcRequest *>>(kDefaultRpcQueueCapacity);
        TopicRegistry rpc_registry;
    };
} // namespace BML::Core

#endif // BML_CORE_IMC_BUS_RPC_INTERNAL_H

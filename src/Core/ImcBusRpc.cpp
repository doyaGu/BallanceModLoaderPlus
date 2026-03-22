#include "ImcBusInternal.h"

namespace BML::Core {
    BML_Result ImcBusImpl::GetRpcId(const char *name, BML_RpcId *out_id) {
        if (!name || !*name || !out_id)
            return BML_RESULT_INVALID_ARGUMENT;
        *out_id = m_RpcRegistry.GetOrCreate(name);
        return (*out_id != BML_RPC_ID_INVALID) ? BML_RESULT_OK : BML_RESULT_FAIL;
    }

    BML_Result ImcBusImpl::RegisterRpc(BML_RpcId rpc_id,
                                       BML_RpcHandler handler,
                                       void *user_data) {
        return RegisterRpc(nullptr, rpc_id, handler, user_data);
    }

    BML_Result ImcBusImpl::RegisterRpc(BML_Mod owner,
                                       BML_RpcId rpc_id,
                                       BML_RpcHandler handler,
                                       void *user_data) {
        if (rpc_id == BML_RPC_ID_INVALID || !handler)
            return BML_RESULT_INVALID_ARGUMENT;

        BML_Mod resolved_owner = nullptr;
        BML_CHECK(ResolveRegistrationOwner(owner, &resolved_owner));

        std::unique_lock lock(m_RpcMutex);
        auto [it, inserted] = m_RpcHandlers.emplace(rpc_id, RpcHandlerEntry{});
        if (!inserted) {
            CoreLog(BML_LOG_WARN, kImcLogCategory,
                    "RPC handler already registered for ID 0x%08X", rpc_id);
            return BML_RESULT_ALREADY_EXISTS;
        }

        it->second.handler = handler;
        it->second.user_data = user_data;
        it->second.owner = resolved_owner;
        CoreLog(BML_LOG_DEBUG, kImcLogCategory,
                "Registered RPC handler for ID 0x%08X", rpc_id);
        return BML_RESULT_OK;
    }

    BML_Result ImcBusImpl::UnregisterRpc(BML_RpcId rpc_id) {
        return UnregisterRpc(nullptr, rpc_id);
    }

    BML_Result ImcBusImpl::UnregisterRpc(BML_Mod owner, BML_RpcId rpc_id) {
        if (rpc_id == BML_RPC_ID_INVALID)
            return BML_RESULT_INVALID_ARGUMENT;

        BML_Mod resolved_owner = nullptr;
        BML_CHECK(ResolveRegistrationOwner(owner, &resolved_owner));

        std::unique_lock lock(m_RpcMutex);
        auto it = m_RpcHandlers.find(rpc_id);
        if (it != m_RpcHandlers.end()) {
            if (it->second.owner != resolved_owner) {
                return BML_RESULT_PERMISSION_DENIED;
            }
            m_RpcHandlers.erase(it);
            CoreLog(BML_LOG_DEBUG, kImcLogCategory,
                    "Unregistered RPC handler for ID 0x%08X", rpc_id);
            return BML_RESULT_OK;
        }

        auto sit = m_StreamingRpcHandlers.find(rpc_id);
        if (sit != m_StreamingRpcHandlers.end()) {
            if (sit->second.owner != resolved_owner) {
                return BML_RESULT_PERMISSION_DENIED;
            }
            m_StreamingRpcHandlers.erase(sit);
            CoreLog(BML_LOG_DEBUG, kImcLogCategory,
                    "Unregistered streaming RPC handler for ID 0x%08X", rpc_id);
            return BML_RESULT_OK;
        }
        return BML_RESULT_NOT_FOUND;
    }

    BML_Result ImcBusImpl::CallRpc(BML_RpcId rpc_id,
                                   const BML_ImcMessage *request,
                                   BML_Future *out_future) {
        return CallRpc(nullptr, rpc_id, request, out_future);
    }

    BML_Result ImcBusImpl::CallRpc(BML_Mod owner,
                                   BML_RpcId rpc_id,
                                   const BML_ImcMessage *request,
                                   BML_Future *out_future) {
        if (rpc_id == BML_RPC_ID_INVALID || !out_future)
            return BML_RESULT_INVALID_ARGUMENT;

        BML_Mod resolved_owner = nullptr;
        BML_CHECK(ResolveRegistrationOwner(owner, &resolved_owner));

        BML_Future future = CreateFuture();
        if (!future)
            return BML_RESULT_OUT_OF_MEMORY;

        auto *req = m_RpcRequestPool.Construct<RpcRequest>();
        if (!req) {
            FutureReleaseInternal(future);
            return BML_RESULT_OUT_OF_MEMORY;
        }

        req->rpc_id = rpc_id;
        req->msg_id = request && request->msg_id
            ? request->msg_id
            : m_NextMessageId.fetch_add(1, std::memory_order_relaxed);
        req->caller = resolved_owner;
        req->future = future;

        if (request && request->data && request->size > 0) {
            if (!req->payload.CopyFrom(request->data, request->size)) {
                m_RpcRequestPool.Destroy(req);
                FutureReleaseInternal(future);
                return BML_RESULT_OUT_OF_MEMORY;
            }
        }

        FutureAddRefInternal(future);
        if (!m_RpcQueue->Enqueue(req)) {
            m_RpcRequestPool.Destroy(req);
            FutureReleaseInternal(future);
            FutureReleaseInternal(future);
            return BML_RESULT_WOULD_BLOCK;
        }

        m_Stats.total_rpc_calls.fetch_add(1, std::memory_order_relaxed);
        *out_future = future;
        return BML_RESULT_OK;
    }

    void ImcBusImpl::ProcessRpcRequest(RpcRequest *request) {
        if (!request)
            return;

        if (request->deadline_ns != 0 && GetTimestampNs() > request->deadline_ns) {
            if (request->future) {
                request->future->CompleteWithError(
                    BML_FUTURE_TIMEOUT, BML_RESULT_TIMEOUT, "RPC call timed out");
                FutureReleaseInternal(request->future);
            }
            m_Stats.total_rpc_failures.fetch_add(1, std::memory_order_relaxed);
            m_RpcRequestPool.Destroy(request);
            return;
        }

        RpcHandlerEntry entry;
        std::shared_ptr<RpcHandlerStats> handler_stats;
        {
            std::shared_lock lock(m_RpcMutex);
            auto it = m_RpcHandlers.find(request->rpc_id);
            if (it == m_RpcHandlers.end()) {
                if (request->future) {
                    request->future->Complete(
                        BML_FUTURE_FAILED, BML_RESULT_NOT_FOUND, nullptr, 0);
                    FutureReleaseInternal(request->future);
                }
                m_Stats.total_rpc_failures.fetch_add(1, std::memory_order_relaxed);
                m_RpcRequestPool.Destroy(request);
                return;
            }
            entry = it->second;
            handler_stats = entry.stats;
        }

        BML_ImcBuffer response = BML_IMC_BUFFER_INIT;
        BML_ImcMessage req_msg = {};
        req_msg.struct_size = sizeof(BML_ImcMessage);
        req_msg.data = request->payload.Data();
        req_msg.size = request->payload.Size();
        req_msg.msg_id = request->msg_id;
        req_msg.flags = 0;

        BML_Context ctx = g_BusContext->GetHandle();

        std::vector<RpcMiddlewareEntry> middleware_snapshot;
        {
            std::shared_lock lock(m_RpcMutex);
            middleware_snapshot = m_RpcMiddleware;
        }

        for (const auto &mw : middleware_snapshot) {
            BML_Result mw_result = BML_RESULT_OK;
            try {
                ModuleContextScope scope(mw.owner);
                mw_result = mw.middleware(ctx, request->rpc_id, BML_TRUE,
                                          &req_msg, nullptr, BML_RESULT_OK, mw.user_data);
            } catch (...) {
                mw_result = BML_RESULT_INTERNAL_ERROR;
            }
            if (mw_result != BML_RESULT_OK) {
                if (request->future) {
                    request->future->CompleteWithError(
                        BML_FUTURE_FAILED, mw_result, "Pre-middleware rejected RPC");
                    FutureReleaseInternal(request->future);
                }
                m_Stats.total_rpc_failures.fetch_add(1, std::memory_order_relaxed);
                m_RpcRequestPool.Destroy(request);
                return;
            }
        }

        auto start_time = GetTimestampNs();
        BML_Result result = BML_RESULT_INTERNAL_ERROR;
        char error_buf[512] = {};

        try {
            ModuleContextScope scope(entry.owner);
            if (entry.handler_ex) {
                result = entry.handler_ex(ctx, request->rpc_id, &req_msg, &response,
                                          error_buf, sizeof(error_buf), entry.user_data);
            } else if (entry.handler) {
                result = entry.handler(
                    ctx, request->rpc_id, &req_msg, &response, entry.user_data);
            }
        } catch (...) {
            result = BML_RESULT_INTERNAL_ERROR;
        }

        auto elapsed_ns = GetTimestampNs() - start_time;
        handler_stats->call_count.fetch_add(1, std::memory_order_relaxed);
        handler_stats->total_latency_ns.fetch_add(elapsed_ns, std::memory_order_relaxed);
        if (result == BML_RESULT_OK) {
            handler_stats->completion_count.fetch_add(1, std::memory_order_relaxed);
        } else {
            handler_stats->failure_count.fetch_add(1, std::memory_order_relaxed);
        }

        for (const auto &mw : middleware_snapshot) {
            try {
                ModuleContextScope scope(mw.owner);
                mw.middleware(ctx, request->rpc_id, BML_FALSE,
                              &req_msg, &response, result, mw.user_data);
            } catch (...) {
            }
        }

        if (request->future) {
            if (result == BML_RESULT_OK) {
                request->future->Complete(
                    BML_FUTURE_READY, BML_RESULT_OK, response.data, response.size);
                m_Stats.total_rpc_completions.fetch_add(1, std::memory_order_relaxed);
            } else {
                request->future->CompleteWithError(
                    BML_FUTURE_FAILED, result, error_buf[0] ? error_buf : nullptr);
                m_Stats.total_rpc_failures.fetch_add(1, std::memory_order_relaxed);
            }
            FutureReleaseInternal(request->future);
        }

        if (response.cleanup) {
            response.cleanup(response.data, response.size, response.cleanup_user_data);
        }

        m_RpcRequestPool.Destroy(request);
    }

    void ImcBusImpl::DrainRpcQueue(size_t budget) {
        size_t processed = 0;
        RpcRequest *req = nullptr;
        while ((budget == 0 || processed < budget) && m_RpcQueue->Dequeue(req)) {
            ProcessRpcRequest(req);
            ++processed;
        }
    }

    BML_Result ImcBusImpl::RegisterRpcEx(BML_RpcId rpc_id,
                                         BML_RpcHandlerEx handler,
                                         void *user_data) {
        return RegisterRpcEx(nullptr, rpc_id, handler, user_data);
    }

    BML_Result ImcBusImpl::RegisterRpcEx(BML_Mod owner,
                                         BML_RpcId rpc_id,
                                         BML_RpcHandlerEx handler,
                                         void *user_data) {
        if (rpc_id == BML_RPC_ID_INVALID || !handler)
            return BML_RESULT_INVALID_ARGUMENT;

        BML_Mod resolved_owner = nullptr;
        BML_CHECK(ResolveRegistrationOwner(owner, &resolved_owner));

        std::unique_lock lock(m_RpcMutex);
        auto [it, inserted] = m_RpcHandlers.emplace(rpc_id, RpcHandlerEntry{});
        if (!inserted) {
            return BML_RESULT_ALREADY_EXISTS;
        }
        it->second.handler_ex = handler;
        it->second.user_data = user_data;
        it->second.owner = resolved_owner;
        return BML_RESULT_OK;
    }

    BML_Result ImcBusImpl::CallRpcEx(BML_RpcId rpc_id,
                                     const BML_ImcMessage *request,
                                     const BML_RpcCallOptions *options,
                                     BML_Future *out_future) {
        return CallRpcEx(nullptr, rpc_id, request, options, out_future);
    }

    BML_Result ImcBusImpl::CallRpcEx(BML_Mod owner,
                                     BML_RpcId rpc_id,
                                     const BML_ImcMessage *request,
                                     const BML_RpcCallOptions *options,
                                     BML_Future *out_future) {
        if (rpc_id == BML_RPC_ID_INVALID || !out_future)
            return BML_RESULT_INVALID_ARGUMENT;

        BML_Mod resolved_owner = nullptr;
        BML_CHECK(ResolveRegistrationOwner(owner, &resolved_owner));

        BML_Future future = CreateFuture();
        if (!future)
            return BML_RESULT_OUT_OF_MEMORY;

        auto *req = m_RpcRequestPool.Construct<RpcRequest>();
        if (!req) {
            FutureReleaseInternal(future);
            return BML_RESULT_OUT_OF_MEMORY;
        }

        req->rpc_id = rpc_id;
        req->msg_id = request && request->msg_id
            ? request->msg_id
            : m_NextMessageId.fetch_add(1, std::memory_order_relaxed);
        req->caller = resolved_owner;
        req->future = future;

        if (options && options->timeout_ms > 0) {
            req->deadline_ns =
                GetTimestampNs() + static_cast<uint64_t>(options->timeout_ms) * 1000000ULL;
        }

        if (request && request->data && request->size > 0) {
            if (!req->payload.CopyFrom(request->data, request->size)) {
                m_RpcRequestPool.Destroy(req);
                FutureReleaseInternal(future);
                return BML_RESULT_OUT_OF_MEMORY;
            }
        }

        FutureAddRefInternal(future);
        if (!m_RpcQueue->Enqueue(req)) {
            m_RpcRequestPool.Destroy(req);
            FutureReleaseInternal(future);
            FutureReleaseInternal(future);
            return BML_RESULT_WOULD_BLOCK;
        }

        m_Stats.total_rpc_calls.fetch_add(1, std::memory_order_relaxed);
        *out_future = future;
        return BML_RESULT_OK;
    }

    BML_Result ImcBusImpl::FutureGetError(BML_Future future,
                                          BML_Result *out_code,
                                          char *msg,
                                          size_t cap,
                                          size_t *out_len) {
        if (!future)
            return BML_RESULT_INVALID_HANDLE;

        std::lock_guard lock(future->mutex);
        if (out_code)
            *out_code = future->status;
        if (msg && cap > 0) {
            size_t len = future->error_message.size();
            size_t copy_len = (len < cap - 1) ? len : cap - 1;
            if (copy_len > 0)
                std::memcpy(msg, future->error_message.c_str(), copy_len);
            msg[copy_len] = '\0';
            if (out_len)
                *out_len = len;
        } else if (out_len) {
            *out_len = future->error_message.size();
        }
        return BML_RESULT_OK;
    }

    BML_Result ImcBusImpl::GetRpcInfo(BML_RpcId rpc_id, BML_RpcInfo *out_info) {
        if (!out_info)
            return BML_RESULT_INVALID_ARGUMENT;

        std::memset(out_info, 0, sizeof(BML_RpcInfo));
        out_info->struct_size = sizeof(BML_RpcInfo);
        out_info->rpc_id = rpc_id;

        const std::string *name = m_RpcRegistry.GetName(rpc_id);
        if (name) {
            size_t copy_len = name->size() < 255 ? name->size() : 255;
            std::memcpy(out_info->name, name->c_str(), copy_len);
            out_info->name[copy_len] = '\0';
        }

        std::shared_lock lock(m_RpcMutex);
        auto it = m_RpcHandlers.find(rpc_id);
        if (it != m_RpcHandlers.end()) {
            out_info->has_handler =
                (it->second.handler || it->second.handler_ex) ? BML_TRUE : BML_FALSE;
            out_info->call_count =
                it->second.stats->call_count.load(std::memory_order_relaxed);
            out_info->completion_count =
                it->second.stats->completion_count.load(std::memory_order_relaxed);
            out_info->failure_count =
                it->second.stats->failure_count.load(std::memory_order_relaxed);
            out_info->total_latency_ns =
                it->second.stats->total_latency_ns.load(std::memory_order_relaxed);
        } else {
            out_info->has_handler = BML_FALSE;
        }
        return BML_RESULT_OK;
    }

    BML_Result ImcBusImpl::GetRpcName(BML_RpcId rpc_id,
                                      char *buf,
                                      size_t cap,
                                      size_t *out_len) {
        if (!buf || cap == 0)
            return BML_RESULT_INVALID_ARGUMENT;

        const std::string *name = m_RpcRegistry.GetName(rpc_id);
        if (!name) {
            buf[0] = '\0';
            if (out_len)
                *out_len = 0;
            return BML_RESULT_NOT_FOUND;
        }

        size_t len = name->size();
        if (len >= cap) {
            std::memcpy(buf, name->c_str(), cap - 1);
            buf[cap - 1] = '\0';
            if (out_len)
                *out_len = len;
            return BML_RESULT_BUFFER_TOO_SMALL;
        }

        std::memcpy(buf, name->c_str(), len);
        buf[len] = '\0';
        if (out_len)
            *out_len = len;
        return BML_RESULT_OK;
    }

    void ImcBusImpl::EnumerateRpc(void(*cb)(BML_RpcId, const char *, BML_Bool, void *),
                                  void *user_data) {
        if (!cb)
            return;

        struct SnapshotEntry {
            BML_RpcId id;
            BML_Bool has_handler;
        };

        std::vector<SnapshotEntry> snapshot;
        {
            std::shared_lock lock(m_RpcMutex);
            snapshot.reserve(m_RpcHandlers.size() + m_StreamingRpcHandlers.size());
            for (const auto &[id, entry] : m_RpcHandlers) {
                BML_Bool has =
                    (entry.handler || entry.handler_ex) ? BML_TRUE : BML_FALSE;
                snapshot.push_back({id, has});
            }
            for (const auto &[id, entry] : m_StreamingRpcHandlers) {
                snapshot.push_back({id, entry.handler ? BML_TRUE : BML_FALSE});
            }
        }

        auto &registry = m_RpcRegistry;
        for (const auto &e : snapshot) {
            const std::string *name = registry.GetName(e.id);
            cb(e.id, name ? name->c_str() : "", e.has_handler, user_data);
        }
    }

    BML_Result ImcBusImpl::AddRpcMiddleware(BML_RpcMiddleware middleware,
                                            int32_t priority,
                                            void *user_data) {
        return AddRpcMiddleware(nullptr, middleware, priority, user_data);
    }

    BML_Result ImcBusImpl::AddRpcMiddleware(BML_Mod owner,
                                            BML_RpcMiddleware middleware,
                                            int32_t priority,
                                            void *user_data) {
        if (!middleware)
            return BML_RESULT_INVALID_ARGUMENT;

        BML_Mod resolved_owner = nullptr;
        BML_CHECK(ResolveRegistrationOwner(owner, &resolved_owner));

        std::unique_lock lock(m_RpcMutex);
        RpcMiddlewareEntry mw;
        mw.middleware = middleware;
        mw.priority = priority;
        mw.user_data = user_data;
        mw.owner = resolved_owner;
        m_RpcMiddleware.push_back(mw);

        std::sort(m_RpcMiddleware.begin(), m_RpcMiddleware.end(),
                  [](const RpcMiddlewareEntry &a, const RpcMiddlewareEntry &b) {
                      return a.priority < b.priority;
                  });
        return BML_RESULT_OK;
    }

    BML_Result ImcBusImpl::RemoveRpcMiddleware(BML_RpcMiddleware middleware) {
        return RemoveRpcMiddleware(nullptr, middleware);
    }

    BML_Result ImcBusImpl::RemoveRpcMiddleware(BML_Mod owner,
                                               BML_RpcMiddleware middleware) {
        if (!middleware)
            return BML_RESULT_INVALID_ARGUMENT;

        BML_Mod resolved_owner = nullptr;
        BML_CHECK(ResolveRegistrationOwner(owner, &resolved_owner));

        std::unique_lock lock(m_RpcMutex);
        auto any_it = std::find_if(m_RpcMiddleware.begin(), m_RpcMiddleware.end(),
                                   [middleware](const RpcMiddlewareEntry &e) {
                                       return e.middleware == middleware;
                                   });
        if (any_it == m_RpcMiddleware.end()) {
            return BML_RESULT_NOT_FOUND;
        }
        if (any_it->owner != resolved_owner) {
            return BML_RESULT_PERMISSION_DENIED;
        }

        m_RpcMiddleware.erase(any_it);
        return BML_RESULT_OK;
    }

    BML_Result ImcBusImpl::RegisterStreamingRpc(BML_RpcId rpc_id,
                                                BML_StreamingRpcHandler handler,
                                                void *user_data) {
        return RegisterStreamingRpc(nullptr, rpc_id, handler, user_data);
    }

    BML_Result ImcBusImpl::RegisterStreamingRpc(BML_Mod owner,
                                                BML_RpcId rpc_id,
                                                BML_StreamingRpcHandler handler,
                                                void *user_data) {
        if (rpc_id == BML_RPC_ID_INVALID || !handler)
            return BML_RESULT_INVALID_ARGUMENT;

        BML_Mod resolved_owner = nullptr;
        BML_CHECK(ResolveRegistrationOwner(owner, &resolved_owner));

        std::unique_lock lock(m_RpcMutex);
        auto [it, inserted] =
            m_StreamingRpcHandlers.emplace(rpc_id, StreamingRpcHandlerEntry{});
        if (!inserted) {
            return BML_RESULT_ALREADY_EXISTS;
        }
        it->second.handler = handler;
        it->second.user_data = user_data;
        it->second.owner = resolved_owner;
        return BML_RESULT_OK;
    }

    BML_Result ImcBusImpl::StreamPush(BML_RpcStream stream, const void *data, size_t size) {
        if (!stream)
            return BML_RESULT_INVALID_HANDLE;

        auto *s = static_cast<BML_RpcStream_T *>(stream);
        if (s->completed.load(std::memory_order_acquire))
            return BML_RESULT_INVALID_STATE;

        if (s->on_chunk && s->chunk_topic != BML_TOPIC_ID_INVALID) {
            BML_ImcMessage msg = BML_IMC_MSG(data, size);
            BML_Context ctx = g_BusContext->GetHandle();
            ModuleContextScope scope(s->owner);
            s->on_chunk(ctx, s->chunk_topic, &msg, s->user_data);
        }
        return BML_RESULT_OK;
    }

    BML_Result ImcBusImpl::StreamComplete(BML_RpcStream stream) {
        if (!stream)
            return BML_RESULT_INVALID_HANDLE;

        auto *s = static_cast<BML_RpcStream_T *>(stream);
        bool expected = false;
        if (!s->completed.compare_exchange_strong(expected, true))
            return BML_RESULT_INVALID_STATE;

        if (s->future) {
            s->future->Complete(BML_FUTURE_READY, BML_RESULT_OK, nullptr, 0);
            FutureReleaseInternal(s->future);
            s->future = nullptr;
        }

        std::lock_guard lock(m_StreamMutex);
        m_Streams.erase(stream);
        return BML_RESULT_OK;
    }

    BML_Result ImcBusImpl::StreamError(BML_RpcStream stream,
                                       BML_Result error,
                                       const char *msg) {
        if (!stream)
            return BML_RESULT_INVALID_HANDLE;

        auto *s = static_cast<BML_RpcStream_T *>(stream);
        bool expected = false;
        if (!s->completed.compare_exchange_strong(expected, true))
            return BML_RESULT_INVALID_STATE;

        if (s->future) {
            s->future->CompleteWithError(BML_FUTURE_FAILED, error, msg);
            FutureReleaseInternal(s->future);
            s->future = nullptr;
        }

        std::lock_guard lock(m_StreamMutex);
        m_Streams.erase(stream);
        return BML_RESULT_OK;
    }

    BML_Result ImcBusImpl::CallStreamingRpc(BML_RpcId rpc_id,
                                            const BML_ImcMessage *request,
                                            BML_ImcHandler on_chunk,
                                            BML_FutureCallback on_done,
                                            void *user_data,
                                            BML_Future *out_future) {
        return CallStreamingRpc(nullptr, rpc_id, request, on_chunk, on_done, user_data,
                                out_future);
    }

    BML_Result ImcBusImpl::CallStreamingRpc(BML_Mod owner,
                                            BML_RpcId rpc_id,
                                            const BML_ImcMessage *request,
                                            BML_ImcHandler on_chunk,
                                            BML_FutureCallback on_done,
                                            void *user_data,
                                            BML_Future *out_future) {
        if (rpc_id == BML_RPC_ID_INVALID || !out_future)
            return BML_RESULT_INVALID_ARGUMENT;

        BML_Mod resolved_owner = nullptr;
        BML_CHECK(ResolveRegistrationOwner(owner, &resolved_owner));

        StreamingRpcHandlerEntry handler_entry;
        {
            std::shared_lock lock(m_RpcMutex);
            auto it = m_StreamingRpcHandlers.find(rpc_id);
            if (it == m_StreamingRpcHandlers.end())
                return BML_RESULT_NOT_FOUND;
            handler_entry = it->second;
        }

        BML_Future future = CreateFuture();
        if (!future)
            return BML_RESULT_OUT_OF_MEMORY;

        if (on_done) {
            future->callbacks.push_back({on_done, user_data, resolved_owner});
        }

        char topic_name[128];
        std::snprintf(topic_name, sizeof(topic_name), "BML/RpcStream/%u/%llu",
                      rpc_id,
                      static_cast<unsigned long long>(
                          m_NextMessageId.fetch_add(1, std::memory_order_relaxed)));
        BML_TopicId chunk_topic = BML_TOPIC_ID_INVALID;
        GetTopicId(topic_name, &chunk_topic);

        auto stream_state = std::make_unique<BML_RpcStream_T>();
        FutureAddRefInternal(future);
        stream_state->future = future;
        stream_state->chunk_topic = chunk_topic;
        stream_state->on_chunk = on_chunk;
        stream_state->on_done = on_done;
        stream_state->user_data = user_data;
        stream_state->owner = resolved_owner;

        BML_RpcStream stream = stream_state.get();
        {
            std::lock_guard lock(m_StreamMutex);
            m_Streams[stream] = std::move(stream_state);
        }

        BML_ImcMessage req_msg = {};
        req_msg.struct_size = sizeof(BML_ImcMessage);
        if (request) {
            req_msg.data = request->data;
            req_msg.size = request->size;
            req_msg.msg_id = request->msg_id;
        }

        BML_Context ctx = g_BusContext->GetHandle();
        BML_Result result = BML_RESULT_INTERNAL_ERROR;
        try {
            ModuleContextScope scope(handler_entry.owner);
            result = handler_entry.handler(ctx, rpc_id, &req_msg, stream, handler_entry.user_data);
        } catch (...) {
            result = BML_RESULT_INTERNAL_ERROR;
        }

        if (result != BML_RESULT_OK) {
            std::lock_guard slock(m_StreamMutex);
            if (m_Streams.count(stream)) {
                auto *s = static_cast<BML_RpcStream_T *>(stream);
                bool expected = false;
                if (s->completed.compare_exchange_strong(expected, true)) {
                    future->CompleteWithError(
                        BML_FUTURE_FAILED, result, "Streaming handler failed");
                }
                if (s->future) {
                    FutureReleaseInternal(s->future);
                    s->future = nullptr;
                }
                m_Streams.erase(stream);
            }
        }

        *out_future = future;
        return BML_RESULT_OK;
    }

    BML_Result ImcBusImpl::FutureAwait(BML_Future future, uint32_t timeout_ms) {
        if (!future)
            return BML_RESULT_INVALID_HANDLE;

        std::unique_lock lock(future->mutex);
        if (future->state != BML_FUTURE_PENDING)
            return BML_RESULT_OK;

        if (timeout_ms == 0) {
            future->cv.wait(lock, [future] { return future->state != BML_FUTURE_PENDING; });
        } else {
            auto deadline =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
            if (!future->cv.wait_until(
                    lock, deadline, [future] { return future->state != BML_FUTURE_PENDING; })) {
                return BML_RESULT_TIMEOUT;
            }
        }
        return BML_RESULT_OK;
    }

    BML_Result ImcBusImpl::FutureGetResult(BML_Future future, BML_ImcMessage *out_msg) {
        if (!future)
            return BML_RESULT_INVALID_HANDLE;
        if (!out_msg)
            return BML_RESULT_INVALID_ARGUMENT;

        std::lock_guard lock(future->mutex);
        if (future->state == BML_FUTURE_PENDING)
            return BML_RESULT_NOT_FOUND;
        if (future->state != BML_FUTURE_READY)
            return future->status;

        out_msg->struct_size = sizeof(BML_ImcMessage);
        out_msg->data = future->payload.Data();
        out_msg->size = future->payload.Size();
        out_msg->msg_id = future->msg_id;
        out_msg->flags = future->flags;
        return BML_RESULT_OK;
    }

    BML_Result ImcBusImpl::FutureGetState(BML_Future future, BML_FutureState *out_state) {
        if (!future)
            return BML_RESULT_INVALID_HANDLE;
        if (!out_state)
            return BML_RESULT_INVALID_ARGUMENT;

        std::lock_guard lock(future->mutex);
        *out_state = future->state;
        return BML_RESULT_OK;
    }

    BML_Result ImcBusImpl::FutureCancel(BML_Future future) {
        if (!future)
            return BML_RESULT_INVALID_HANDLE;
        return future->Cancel() ? BML_RESULT_OK : BML_RESULT_INVALID_STATE;
    }

    BML_Result ImcBusImpl::FutureOnComplete(BML_Future future,
                                            BML_FutureCallback callback,
                                            void *user_data) {
        return FutureOnComplete(nullptr, future, callback, user_data);
    }

    BML_Result ImcBusImpl::FutureOnComplete(BML_Mod owner,
                                            BML_Future future,
                                            BML_FutureCallback callback,
                                            void *user_data) {
        if (!future)
            return BML_RESULT_INVALID_HANDLE;
        if (!callback)
            return BML_RESULT_INVALID_ARGUMENT;

        BML_Mod resolved_owner = nullptr;
        BML_CHECK(ResolveRegistrationOwner(owner, &resolved_owner));
        bool invoke_now = false;
        {
            std::lock_guard lock(future->mutex);
            if (future->state == BML_FUTURE_PENDING) {
                future->callbacks.push_back({callback, user_data, resolved_owner});
            } else {
                invoke_now = true;
            }
        }

        if (invoke_now) {
            BML_Context ctx = g_BusContext->GetHandle();
            ModuleContextScope scope(resolved_owner);
            callback(ctx, future, user_data);
        }
        return BML_RESULT_OK;
    }

    BML_Result ImcBusImpl::FutureRelease(BML_Future future) {
        if (!future)
            return BML_RESULT_INVALID_HANDLE;
        FutureReleaseInternal(future);
        return BML_RESULT_OK;
    }
} // namespace BML::Core

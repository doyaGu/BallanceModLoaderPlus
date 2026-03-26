/**
 * @file bml_imc_rpc.hpp
 * @brief RPC (Remote Procedure Call) support for BML IMC
 *
 * Provides request/response patterns with async futures and typed handlers.
 */

#ifndef BML_IMC_RPC_HPP
#define BML_IMC_RPC_HPP

#include "bml_imc_fwd.hpp"
#include "bml_imc_message.hpp"
#include "bml_errors.h"
#include "bml_assert.hpp"

#include <optional>
#include <variant>

namespace bml {
namespace imc {

    // ========================================================================
    // RPC Endpoint
    // ========================================================================

    /**
     * @brief Represents an RPC endpoint
     *
     * Similar to Topic but for RPC calls.
     *
     * @code
     * Rpc getHealth("MyMod/GetHealth");
     *
     * if (getHealth) {
     *     auto future = getHealth.Call(playerId);
     *     future.Wait(1000);
     *     auto health = future.Result<int>();
     * }
     * @endcode
     */
    class Rpc {
    public:
        Rpc() : m_Id(InvalidRpcId) {}

        explicit Rpc(std::string_view name, const BML_ImcRpcInterface *rpc = nullptr)
            : m_Name(name), m_Id(InvalidRpcId), m_RpcIface(rpc) {
            Resolve();
        }

        explicit Rpc(RpcId id, std::string name = "", const BML_ImcRpcInterface *rpc = nullptr)
            : m_Name(std::move(name)), m_Id(id), m_RpcIface(rpc) {}

        // ====================================================================
        // Properties
        // ====================================================================

        RpcId Id() const noexcept { return m_Id; }
        const std::string &Name() const noexcept { return m_Name; }
        const BML_ImcRpcInterface *Iface() const noexcept { return m_RpcIface; }
        bool Valid() const noexcept { return m_Id != InvalidRpcId; }
        explicit operator bool() const noexcept { return Valid(); }
        operator RpcId() const noexcept { return m_Id; }

        // ====================================================================
        // Resolution
        // ====================================================================

        bool Resolve() {
            if (m_Id != InvalidRpcId) return true;
            if (m_Name.empty() || !m_RpcIface || !m_RpcIface->Context || !m_RpcIface->GetRpcId) {
                return false;
            }
            return m_RpcIface->GetRpcId(m_RpcIface->Context, m_Name.c_str(), &m_Id) ==
                BML_RESULT_OK;
        }

        static std::optional<Rpc> Create(std::string_view name, const BML_ImcRpcInterface *iface = nullptr) {
            Rpc endpoint(name, iface);
            if (endpoint.Valid()) return endpoint;
            return std::nullopt;
        }

    private:
        std::string m_Name;
        RpcId m_Id;
        const BML_ImcRpcInterface *m_RpcIface = nullptr;
    };

    // ========================================================================
    // Structured Error for RPC Handlers
    // ========================================================================

    struct RpcErrorResult {
        BML_Result code;
        std::string message;
    };

    using RpcHandlerWithError = std::function<
        std::variant<std::vector<uint8_t>, RpcErrorResult>(const Message &)>;

    struct WithErrorTag {};
    inline constexpr WithErrorTag with_error{};

    // ========================================================================
    // RPC Call Options (fluent builder)
    // ========================================================================

    class RpcCallOptions {
    public:
        RpcCallOptions() { m_Native.struct_size = sizeof(BML_RpcCallOptions); }

        RpcCallOptions &Timeout(uint32_t ms) { m_Native.timeout_ms = ms; return *this; }
        uint32_t GetTimeout() const noexcept { return m_Native.timeout_ms; }
        const BML_RpcCallOptions &Native() const noexcept { return m_Native; }

    private:
        BML_RpcCallOptions m_Native{};
    };

    // ========================================================================
    // RPC Future
    // ========================================================================

    /**
     * @brief RAII wrapper for async RPC result
     *
     * Represents a pending or completed RPC call result.
     *
     * @code
     * RpcFuture future = rpcClient.Call(request);
     *
     * // Wait with timeout
     * if (future.Wait(1000)) {
     *     auto result = future.Result<MyResponse>();
     *     if (result) {
     *         // Use result...
     *     }
     * }
     *
     * // Or use callback
     * future.OnComplete([](const Message& response) {
     *     // Handle response...
     * });
     * @endcode
     */
    class RpcFuture {
    public:
        RpcFuture() : m_Handle(nullptr) {}

        explicit RpcFuture(BML_Future handle, const BML_ImcRpcInterface *rpc = nullptr, BML_Mod owner = nullptr)
            : m_Handle(handle), m_RpcIface(rpc), m_Owner(owner) {
            BML_ASSERT(rpc);
        }

        RpcFuture(RpcFuture &&other) noexcept
            : m_Handle(other.m_Handle), m_RpcIface(other.m_RpcIface), m_Owner(other.m_Owner) {
            other.m_Handle = nullptr;
            other.m_RpcIface = nullptr;
            other.m_Owner = nullptr;
            m_ResultCache = std::move(other.m_ResultCache);
            other.m_ResultCache.reset();
        }

        RpcFuture &operator=(RpcFuture &&other) noexcept {
            if (this != &other) {
                Release();
                m_Handle = other.m_Handle;
                m_RpcIface = other.m_RpcIface;
                m_Owner = other.m_Owner;
                m_ResultCache = std::move(other.m_ResultCache);
                other.m_Handle = nullptr;
                other.m_RpcIface = nullptr;
                other.m_Owner = nullptr;
                other.m_ResultCache.reset();
            }
            return *this;
        }

        // Non-copyable
        RpcFuture(const RpcFuture &) = delete;
        RpcFuture &operator=(const RpcFuture &) = delete;

        ~RpcFuture() {
            Release();
        }

        // ====================================================================
        // State
        // ====================================================================

        bool Valid() const noexcept { return m_Handle != nullptr; }
        explicit operator bool() const noexcept { return Valid(); }

        /**
         * @brief Get current state
         */
        FutureState State() const {
            if (!m_Handle) return BML_FUTURE_FAILED;
            FutureState s;
            if (m_RpcIface->FutureGetState(m_Handle, &s) == BML_RESULT_OK) {
                return s;
            }
            return BML_FUTURE_FAILED;
        }

        bool IsPending() const { return State() == BML_FUTURE_PENDING; }
        bool IsReady() const { return State() == BML_FUTURE_READY; }
        bool IsCancelled() const { return State() == BML_FUTURE_CANCELLED; }
        bool IsTimedOut() const { return State() == BML_FUTURE_TIMEOUT; }
        bool IsFailed() const { return State() == BML_FUTURE_FAILED; }
        bool IsComplete() const { return !IsPending(); }

        // ====================================================================
        // Waiting
        // ====================================================================

        /**
         * @brief Wait for completion with timeout
         * @param timeoutMs Timeout in milliseconds (0 = no timeout)
         * @return true if completed (ready, failed, cancelled, etc.)
         * @note Pending futures cannot be awaited from the runtime main thread.
         */
        bool Wait(uint32_t timeoutMs = InfiniteTimeout) {
            if (!m_Handle) return false;
            return m_RpcIface->FutureAwait(m_Handle, timeoutMs) == BML_RESULT_OK;
        }

        // ====================================================================
        // Result
        // ====================================================================

        /**
         * @brief Get result as Message
         * @warning Each call invalidates the Message returned by the previous
         * call. Do not store the result and call ResultMessage() again -- the
         * earlier Message's data pointer will dangle.
         */
        std::optional<Message> ResultMessage() {
            if (!m_Handle) return std::nullopt;
            BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
            if (m_RpcIface->FutureGetResult(m_Handle, &msg) == BML_RESULT_OK) {
                m_ResultStorage.clear();
                if (msg.data && msg.size > 0) {
                    const auto *bytes = static_cast<const uint8_t *>(msg.data);
                    m_ResultStorage.assign(bytes, bytes + msg.size);
                    msg.data = m_ResultStorage.data();
                }
                m_ResultCache = msg;
                return Message(*m_ResultCache);
            }
            return std::nullopt;
        }

        /**
         * @brief Get typed result
         */
        template <typename T>
        std::optional<T> Result() {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            if (auto msg = ResultMessage()) {
                T value;
                if (msg->CopyTo(value)) {
                    return value;
                }
            }
            return std::nullopt;
        }

        /**
         * @brief Get error details from a failed future
         * @return pair of (error_code, error_message), or std::nullopt on success
         */
        std::optional<std::pair<BML_Result, std::string>> Error() const {
            if (!m_Handle || !BML_IFACE_HAS(m_RpcIface, BML_ImcRpcInterface, FutureGetError)) return std::nullopt;
            BML_Result code = BML_RESULT_OK;
            char buf[512] = {};
            size_t len = 0;
            if (m_RpcIface->FutureGetError(m_Handle, &code, buf, sizeof(buf), &len) == BML_RESULT_OK) {
                if (code != BML_RESULT_OK) {
                    // buf is null-terminated; use strlen to get the actual
                    // in-buffer length (len may exceed buffer capacity).
                    return std::pair<BML_Result, std::string>{code, std::string(buf)};
                }
            }
            return std::nullopt;
        }

        /**
         * @brief Get result as bytes
         */
        std::optional<std::vector<uint8_t>> ResultBytes() {
            if (auto msg = ResultMessage()) {
                auto span = msg->Bytes();
                return std::vector<uint8_t>(span.begin(), span.end());
            }
            return std::nullopt;
        }

        // ====================================================================
        // Callbacks
        // ====================================================================

        /**
         * @brief Set completion callback
         * @note Callback context must outlive the future
         */
        bool OnComplete(BML_FutureCallback callback, void *userData = nullptr) {
            if (!m_Handle) return false;
            return m_RpcIface->FutureOnComplete(m_Owner, m_Handle, callback, userData) ==
                BML_RESULT_OK;
        }

        // ====================================================================
        // Control
        // ====================================================================

        /**
         * @brief Cancel the pending operation
         */
        bool Cancel() {
            if (!m_Handle) return false;
            return m_RpcIface->FutureCancel(m_Handle) == BML_RESULT_OK;
        }

        /**
         * @brief Release the future handle
         */
        void Release() {
            if (m_Handle && m_RpcIface && m_RpcIface->FutureRelease) {
                m_RpcIface->FutureRelease(m_Handle);
                m_Handle = nullptr;
            }
            m_ResultCache.reset();
            m_ResultStorage.clear();
        }

        BML_Future Native() const noexcept { return m_Handle; }

    private:
        BML_Future m_Handle;
        const BML_ImcRpcInterface *m_RpcIface = nullptr;
        BML_Mod m_Owner = nullptr;
        std::optional<BML_ImcMessage> m_ResultCache;
        std::vector<uint8_t> m_ResultStorage;
    };

    // ========================================================================
    // RPC Server (Handler Registration)
    // ========================================================================

    namespace detail {

        struct RpcHandlerContext {
            RpcHandler handler;

            static BML_Result Invoke(
                BML_Context,
                BML_RpcId,
                const BML_ImcMessage *request,
                BML_ImcBuffer *response,
                void *userData
            ) {
                auto *ctx = static_cast<RpcHandlerContext *>(userData);
                if (!ctx || !ctx->handler || !request) {
                    return BML_RESULT_INTERNAL_ERROR;
                }

                try {
                    Message req(request);
                    auto result = ctx->handler(req);

                    if (!result.empty()) {
                        auto *owned = new (std::nothrow) std::vector<uint8_t>(std::move(result));
                        if (!owned) {
                            return BML_RESULT_OUT_OF_MEMORY;
                        }

                        response->data = owned->data();
                        response->size = owned->size();
                        response->cleanup = [](const void *, size_t, void *userData) {
                            delete static_cast<std::vector<uint8_t> *>(userData);
                        };
                        response->cleanup_user_data = owned;
                    }
                    return BML_RESULT_OK;
                } catch (...) {
                    return BML_RESULT_INTERNAL_ERROR;
                }
            }
        };

        struct RpcHandlerExContext {
            RpcHandlerWithError handler;

            static BML_Result Invoke(
                BML_Context,
                BML_RpcId,
                const BML_ImcMessage *request,
                BML_ImcBuffer *response,
                char *out_error_msg,
                size_t error_msg_capacity,
                void *userData
            ) {
                auto *ctx = static_cast<RpcHandlerExContext *>(userData);
                if (!ctx || !ctx->handler || !request)
                    return BML_RESULT_INTERNAL_ERROR;

                try {
                    Message req(request);
                    auto result = ctx->handler(req);

                    if (auto *data = std::get_if<std::vector<uint8_t>>(&result)) {
                        if (!data->empty()) {
                            auto *owned = new (std::nothrow) std::vector<uint8_t>(std::move(*data));
                            if (!owned)
                                return BML_RESULT_OUT_OF_MEMORY;
                            response->data = owned->data();
                            response->size = owned->size();
                            response->cleanup = [](const void *, size_t, void *ud) {
                                delete static_cast<std::vector<uint8_t> *>(ud);
                            };
                            response->cleanup_user_data = owned;
                        }
                        return BML_RESULT_OK;
                    } else if (auto *err = std::get_if<RpcErrorResult>(&result)) {
                        if (out_error_msg && error_msg_capacity > 0) {
                            size_t len = err->message.size();
                            size_t copy_len = len < error_msg_capacity - 1 ? len : error_msg_capacity - 1;
                            if (copy_len > 0)
                                std::memcpy(out_error_msg, err->message.c_str(), copy_len);
                            out_error_msg[copy_len] = '\0';
                        }
                        return err->code;
                    }
                    return BML_RESULT_INTERNAL_ERROR;
                } catch (...) {
                    return BML_RESULT_INTERNAL_ERROR;
                }
            }
        };

    } // namespace detail

    /**
     * @brief RAII RPC handler registration
     *
     * Registers an RPC handler and automatically unregisters on destruction.
     *
     * @code
     * RpcServer server("MyMod/GetHealth", [](const Message& req) {
     *     int playerId = *req.as<int>();
     *     int health = GetPlayerHealth(playerId);
     *
     *     std::vector<uint8_t> response(sizeof(health));
     *     std::memcpy(response.data(), &health, sizeof(health));
     *     return response;
     * });
     * @endcode
     */
    class RpcServer {
    public:
        RpcServer() : m_RpcId(InvalidRpcId) {}

        /**
         * @brief Register an RPC handler
         */
        RpcServer(std::string_view name, RpcHandler handler, const BML_ImcRpcInterface *rpc = nullptr,
                  BML_Mod owner = nullptr)
            : m_RpcId(InvalidRpcId), m_RpcIface(rpc), m_Owner(owner) {
            BML_ASSERT(rpc);
            BML_ASSERT(owner);

            std::string resolvedName(name);
            if (m_RpcIface->GetRpcId(m_RpcIface->Context, resolvedName.c_str(), &m_RpcId) !=
                BML_RESULT_OK) {
                m_RpcId = InvalidRpcId;
                return;
            }

            m_Context = std::make_unique<detail::RpcHandlerContext>();
            m_Context->handler = std::move(handler);

            const BML_Result result = m_RpcIface->RegisterRpc(
                m_Owner, m_RpcId, detail::RpcHandlerContext::Invoke, m_Context.get());
            if (result != BML_RESULT_OK) {
                m_RpcId = InvalidRpcId;
                m_Context.reset();
            }
        }

        /**
         * @brief Register an extended RPC handler with error detail support
         * @code
         * RpcServer server("MyRpc", with_error, [](const Message&) -> std::variant<...> { ... });
         * @endcode
         */
        RpcServer(std::string_view name, WithErrorTag, RpcHandlerWithError handler,
                  const BML_ImcRpcInterface *rpc = nullptr, BML_Mod owner = nullptr)
            : m_RpcId(InvalidRpcId), m_RpcIface(rpc), m_Owner(owner) {
            BML_ASSERT(rpc);
            BML_ASSERT(owner);

            std::string resolvedName(name);
            if (m_RpcIface->GetRpcId(m_RpcIface->Context, resolvedName.c_str(), &m_RpcId) !=
                BML_RESULT_OK) {
                m_RpcId = InvalidRpcId;
                return;
            }

            m_ExContext = std::make_unique<detail::RpcHandlerExContext>();
            m_ExContext->handler = std::move(handler);

            const BML_Result result = m_RpcIface->RegisterRpcEx(
                m_Owner, m_RpcId, detail::RpcHandlerExContext::Invoke, m_ExContext.get());
            if (result != BML_RESULT_OK) {
                m_RpcId = InvalidRpcId;
                m_ExContext.reset();
            }
        }

        /**
         * @brief Register a typed RPC handler
         */
        template <typename Req, typename Resp>
        RpcServer(
            std::string_view name,
            TypedRpcHandler<Req, Resp> handler,
            const BML_ImcRpcInterface *rpc = nullptr,
            BML_Mod owner = nullptr)
            : RpcServer(name, [h = std::move(handler)](const Message &req) -> std::vector<uint8_t> {
                  static_assert(std::is_trivially_copyable_v<Req>, "Req must be trivially copyable");
                  static_assert(std::is_trivially_copyable_v<Resp>, "Resp must be trivially copyable");

                  auto *reqData = req.As<Req>();
                  if (!reqData) return {};

                  Resp resp = h(*reqData);
                  std::vector<uint8_t> result(sizeof(Resp));
                  std::memcpy(result.data(), &resp, sizeof(Resp));
                  return result;
              }, rpc, owner) {}

        RpcServer(RpcServer &&other) noexcept
            : m_RpcId(other.m_RpcId),
              m_Context(std::move(other.m_Context)),
              m_ExContext(std::move(other.m_ExContext)),
              m_RpcIface(other.m_RpcIface),
              m_Owner(other.m_Owner) {
            other.m_RpcId = InvalidRpcId;
            other.m_RpcIface = nullptr;
            other.m_Owner = nullptr;
        }

        RpcServer &operator=(RpcServer &&other) noexcept {
            if (this != &other) {
                Unregister();
                m_RpcId = other.m_RpcId;
                m_Context = std::move(other.m_Context);
                m_ExContext = std::move(other.m_ExContext);
                m_RpcIface = other.m_RpcIface;
                m_Owner = other.m_Owner;
                other.m_RpcId = InvalidRpcId;
                other.m_RpcIface = nullptr;
                other.m_Owner = nullptr;
            }
            return *this;
        }

        // Non-copyable
        RpcServer(const RpcServer &) = delete;
        RpcServer &operator=(const RpcServer &) = delete;

        ~RpcServer() {
            Unregister();
        }

        void Unregister() {
            if (m_RpcId != InvalidRpcId && m_RpcIface && m_RpcIface->UnregisterRpc) {
                m_RpcIface->UnregisterRpc(m_Owner, m_RpcId);
                m_RpcId = InvalidRpcId;
            }
            m_Context.reset();
            m_ExContext.reset();
        }

        bool Valid() const noexcept { return m_RpcId != InvalidRpcId; }
        explicit operator bool() const noexcept { return Valid(); }

        RpcId GetRpcId() const noexcept { return m_RpcId; }

        std::optional<BML_RpcInfo> Info() const {
            if (m_RpcId == InvalidRpcId || !BML_IFACE_HAS(m_RpcIface, BML_ImcRpcInterface, GetRpcInfo)) return std::nullopt;
            BML_RpcInfo info = BML_RPC_INFO_INIT;
            if (m_RpcIface->GetRpcInfo(m_RpcIface->Context, m_RpcId, &info) == BML_RESULT_OK) {
                return info;
            }
            return std::nullopt;
        }

    private:
        RpcId m_RpcId;
        std::unique_ptr<detail::RpcHandlerContext> m_Context;
        std::unique_ptr<detail::RpcHandlerExContext> m_ExContext;
        const BML_ImcRpcInterface *m_RpcIface = nullptr;
        BML_Mod m_Owner = nullptr;
    };

    // ========================================================================
    // RPC Client
    // ========================================================================

    /**
     * @brief RPC caller for making remote procedure calls
     *
     * @code
     * RpcClient client("MyMod/GetHealth");
     *
     * // Async call
     * int playerId = 1;
     * auto future = client.Call(&playerId, sizeof(playerId));
     *
     * if (future.Wait(1000)) {
     *     auto health = future.Result<int>();
     * }
     *
     * // Typed call
     * auto healthFuture = client.call<int>(playerId);
     *
     * // Sync call (blocking)
     * auto health = client.callSync<int, int>(playerId, 1000);
     * @endcode
     */
    class RpcClient {
    public:
        RpcClient() : m_Endpoint() {}

        explicit RpcClient(std::string_view name, const BML_ImcRpcInterface *rpc = nullptr,
                           BML_Mod owner = nullptr)
            : m_Endpoint(name, rpc), m_RpcIface(rpc), m_Owner(owner) {}

        explicit RpcClient(const Rpc &endpoint, const BML_ImcRpcInterface *rpc = nullptr,
                           BML_Mod owner = nullptr)
            : m_Endpoint(endpoint.Valid() ? Rpc(endpoint.Id(), endpoint.Name(), rpc ? rpc : endpoint.Iface()) : Rpc()),
              m_RpcIface(rpc ? rpc : endpoint.Iface()),
              m_Owner(owner) {}

        // ====================================================================
        // Properties
        // ====================================================================

        bool Valid() const noexcept { return m_Endpoint.Valid(); }
        explicit operator bool() const noexcept { return Valid(); }

        RpcId GetRpcId() const noexcept { return m_Endpoint.Id(); }
        const Rpc &GetRpc() const noexcept { return m_Endpoint; }

        // ====================================================================
        // Async Calls
        // ====================================================================

        /**
         * @brief Call with raw data
         */
        RpcFuture Call(const void *data = nullptr, size_t size = 0) {
            if (!Valid()) return RpcFuture();

            BML_ImcMessage msg = BML_IMC_MSG(data, size);
            BML_Future handle;
            if (m_RpcIface->CallRpc(m_Owner, m_Endpoint.Id(), &msg, &handle) == BML_RESULT_OK) {
                return RpcFuture(handle, m_RpcIface, m_Owner);
            }
            return RpcFuture();
        }

        /**
         * @brief Call with typed request
         */
        template <typename T>
        RpcFuture Call(const T &request) {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            return Call(&request, sizeof(T));
        }

        /**
         * @brief Call with message
         */
        RpcFuture Call(const BML_ImcMessage &msg) {
            if (!Valid()) return RpcFuture();

            BML_Future handle;
            if (m_RpcIface->CallRpc(m_Owner, m_Endpoint.Id(), &msg, &handle) == BML_RESULT_OK) {
                return RpcFuture(handle, m_RpcIface, m_Owner);
            }
            return RpcFuture();
        }

        /**
         * @brief Call with message builder
         */
        RpcFuture Call(const MessageBuilder &builder) {
            return Call(builder.Build());
        }

        /**
         * @brief Call with raw data and options
         */
        RpcFuture Call(const void *data, size_t size, const RpcCallOptions &opts) {
            if (!Valid() || !BML_IFACE_HAS(m_RpcIface, BML_ImcRpcInterface, CallRpcEx))
                return RpcFuture();
            BML_ImcMessage msg = BML_IMC_MSG(data, size);
            BML_Future handle;
            if (m_RpcIface->CallRpcEx(m_Owner, m_Endpoint.Id(), &msg, &opts.Native(), &handle) ==
                BML_RESULT_OK) {
                return RpcFuture(handle, m_RpcIface, m_Owner);
            }
            return RpcFuture();
        }

        /**
         * @brief Call typed request with options
         */
        template <typename T>
        RpcFuture Call(const T &request, const RpcCallOptions &opts) {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            return Call(&request, sizeof(T), opts);
        }

        // ====================================================================
        // Sync Calls (Blocking)
        // ====================================================================

        /**
         * @brief Synchronous call with timeout
         * @return Result message if successful
         */
        std::optional<std::vector<uint8_t>> CallSync(
            const void *data,
            size_t size,
            uint32_t timeoutMs = 5000
        ) {
            auto future = Call(data, size);
            if (!future.Valid()) return std::nullopt;

            if (future.Wait(timeoutMs) && future.IsReady()) {
                return future.ResultBytes();
            }
            return std::nullopt;
        }

        /**
         * @brief Typed synchronous call
         */
        template <typename Resp, typename Req>
        std::optional<Resp> CallSync(const Req &request, uint32_t timeoutMs = 5000) {
            static_assert(std::is_trivially_copyable_v<Req>, "Req must be trivially copyable");
            static_assert(std::is_trivially_copyable_v<Resp>, "Resp must be trivially copyable");

            auto future = Call(request);
            if (!future.Valid()) return std::nullopt;

            if (future.Wait(timeoutMs) && future.IsReady()) {
                return future.Result<Resp>();
            }
            return std::nullopt;
        }

    private:
        Rpc m_Endpoint;
        const BML_ImcRpcInterface *m_RpcIface = nullptr;
        BML_Mod m_Owner = nullptr;
    };

    // ========================================================================
    // RPC Middleware Registration (RAII)
    // ========================================================================

    class RpcMiddlewareRegistration {
    public:
        RpcMiddlewareRegistration() = default;

        RpcMiddlewareRegistration(BML_RpcMiddleware mw, int32_t priority, void *ud,
                                  const BML_ImcRpcInterface *rpc, BML_Mod owner = nullptr)
            : m_Middleware(mw), m_RpcIface(rpc), m_Owner(owner) {
            if (m_Owner && BML_IFACE_HAS(m_RpcIface, BML_ImcRpcInterface, AddRpcMiddleware)) {
                const BML_Result result = m_RpcIface->AddRpcMiddleware(m_Owner, mw, priority, ud);
                if (result != BML_RESULT_OK) {
                    m_Middleware = nullptr;
                }
            } else {
                m_Middleware = nullptr;
            }
        }

        ~RpcMiddlewareRegistration() {
            if (m_Middleware && BML_IFACE_HAS(m_RpcIface, BML_ImcRpcInterface, RemoveRpcMiddleware)) {
                m_RpcIface->RemoveRpcMiddleware(m_Owner, m_Middleware);
            }
        }

        RpcMiddlewareRegistration(RpcMiddlewareRegistration &&other) noexcept
            : m_Middleware(other.m_Middleware), m_RpcIface(other.m_RpcIface), m_Owner(other.m_Owner) {
            other.m_Middleware = nullptr;
            other.m_RpcIface = nullptr;
            other.m_Owner = nullptr;
        }

        RpcMiddlewareRegistration &operator=(RpcMiddlewareRegistration &&other) noexcept {
            if (this != &other) {
                if (m_Middleware && BML_IFACE_HAS(m_RpcIface, BML_ImcRpcInterface, RemoveRpcMiddleware)) {
                    m_RpcIface->RemoveRpcMiddleware(m_Owner, m_Middleware);
                }
                m_Middleware = other.m_Middleware;
                m_RpcIface = other.m_RpcIface;
                m_Owner = other.m_Owner;
                other.m_Middleware = nullptr;
                other.m_RpcIface = nullptr;
                other.m_Owner = nullptr;
            }
            return *this;
        }

        RpcMiddlewareRegistration(const RpcMiddlewareRegistration &) = delete;
        RpcMiddlewareRegistration &operator=(const RpcMiddlewareRegistration &) = delete;

        bool Valid() const noexcept { return m_Middleware != nullptr; }
        explicit operator bool() const noexcept { return Valid(); }

    private:
        BML_RpcMiddleware m_Middleware = nullptr;
        const BML_ImcRpcInterface *m_RpcIface = nullptr;
        BML_Mod m_Owner = nullptr;
    };

    // ========================================================================
    // RPC Stream
    // ========================================================================

    class RpcStream {
    public:
        explicit RpcStream(BML_RpcStream handle, const BML_ImcRpcInterface *rpc = nullptr)
            : m_Handle(handle), m_RpcIface(rpc) {}

        bool Push(const void *data, size_t size) {
            if (!m_Handle || !BML_IFACE_HAS(m_RpcIface, BML_ImcRpcInterface, StreamPush)) return false;
            return m_RpcIface->StreamPush(m_Handle, data, size) == BML_RESULT_OK;
        }

        template <typename T>
        bool Push(const T &value) {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            return Push(&value, sizeof(T));
        }

        bool Complete() {
            if (!m_Handle || !BML_IFACE_HAS(m_RpcIface, BML_ImcRpcInterface, StreamComplete)) return false;
            return m_RpcIface->StreamComplete(m_Handle) == BML_RESULT_OK;
        }

        bool Error(BML_Result code, const char *msg) {
            if (!m_Handle || !BML_IFACE_HAS(m_RpcIface, BML_ImcRpcInterface, StreamError)) return false;
            return m_RpcIface->StreamError(m_Handle, code, msg) == BML_RESULT_OK;
        }

    private:
        BML_RpcStream m_Handle;
        const BML_ImcRpcInterface *m_RpcIface = nullptr;
    };

    // ========================================================================
    // Streaming RPC Server (RAII handler registration)
    // ========================================================================

    class StreamingRpcServer {
    public:
        StreamingRpcServer() : m_RpcId(InvalidRpcId) {}

        StreamingRpcServer(std::string_view name, BML_StreamingRpcHandler handler,
                           void *user_data, const BML_ImcRpcInterface *rpc = nullptr,
                           BML_Mod owner = nullptr)
            : m_RpcId(InvalidRpcId), m_RpcIface(rpc), m_Owner(owner) {
            if (!m_RpcIface || !m_Owner || !m_RpcIface->GetRpcId || !m_RpcIface->RegisterStreamingRpc)
                return;

            std::string resolvedName(name);
            if (m_RpcIface->GetRpcId(m_RpcIface->Context, resolvedName.c_str(), &m_RpcId) !=
                BML_RESULT_OK) {
                m_RpcId = InvalidRpcId;
                return;
            }

            const BML_Result result =
                m_RpcIface->RegisterStreamingRpc(m_Owner, m_RpcId, handler, user_data);
            if (result != BML_RESULT_OK) {
                m_RpcId = InvalidRpcId;
            }
        }

        ~StreamingRpcServer() {
            if (m_RpcId != InvalidRpcId && m_RpcIface && m_RpcIface->UnregisterRpc) {
                m_RpcIface->UnregisterRpc(m_Owner, m_RpcId);
            }
        }

        StreamingRpcServer(StreamingRpcServer &&other) noexcept
            : m_RpcId(other.m_RpcId), m_RpcIface(other.m_RpcIface), m_Owner(other.m_Owner) {
            other.m_RpcId = InvalidRpcId;
            other.m_RpcIface = nullptr;
            other.m_Owner = nullptr;
        }

        StreamingRpcServer &operator=(StreamingRpcServer &&other) noexcept {
            if (this != &other) {
                if (m_RpcId != InvalidRpcId && m_RpcIface && m_RpcIface->UnregisterRpc) {
                    m_RpcIface->UnregisterRpc(m_Owner, m_RpcId);
                }
                m_RpcId = other.m_RpcId;
                m_RpcIface = other.m_RpcIface;
                m_Owner = other.m_Owner;
                other.m_RpcId = InvalidRpcId;
                other.m_RpcIface = nullptr;
                other.m_Owner = nullptr;
            }
            return *this;
        }

        StreamingRpcServer(const StreamingRpcServer &) = delete;
        StreamingRpcServer &operator=(const StreamingRpcServer &) = delete;

        bool Valid() const noexcept { return m_RpcId != InvalidRpcId; }
        explicit operator bool() const noexcept { return Valid(); }
        RpcId GetRpcId() const noexcept { return m_RpcId; }

    private:
        RpcId m_RpcId;
        const BML_ImcRpcInterface *m_RpcIface = nullptr;
        BML_Mod m_Owner = nullptr;
    };

    // ========================================================================
    // RPC Service Manager (managed container)
    // ========================================================================

    class RpcServiceManager {
    public:
        explicit RpcServiceManager(const BML_ImcRpcInterface *rpc = nullptr, BML_Mod owner = nullptr)
            : m_RpcIface(rpc), m_Owner(owner) {}
        ~RpcServiceManager() = default;

        RpcServiceManager(RpcServiceManager &&) = default;
        RpcServiceManager &operator=(RpcServiceManager &&) = default;
        RpcServiceManager(const RpcServiceManager &) = delete;
        RpcServiceManager &operator=(const RpcServiceManager &) = delete;

        bool AddServer(std::string_view name, RpcHandler handler) {
            RpcServer server(name, std::move(handler), m_RpcIface, m_Owner);
            if (!server.Valid()) return false;
            m_Servers.push_back(std::move(server));
            return true;
        }

        bool AddServer(std::string_view name, WithErrorTag, RpcHandlerWithError handler) {
            RpcServer server(name, with_error, std::move(handler), m_RpcIface, m_Owner);
            if (!server.Valid()) return false;
            m_Servers.push_back(std::move(server));
            return true;
        }

        template <typename Req, typename Resp>
        bool AddServer(std::string_view name, TypedRpcHandler<Req, Resp> handler) {
            RpcServer server(name, std::move(handler), m_RpcIface, m_Owner);
            if (!server.Valid()) return false;
            m_Servers.push_back(std::move(server));
            return true;
        }

        bool AddStreamingServer(std::string_view name, BML_StreamingRpcHandler handler, void *user_data = nullptr) {
            StreamingRpcServer server(name, handler, user_data, m_RpcIface, m_Owner);
            if (!server.Valid()) return false;
            m_StreamingServers.push_back(std::move(server));
            return true;
        }

        RpcClient CreateClient(std::string_view name) const {
            return RpcClient(name, m_RpcIface, m_Owner);
        }

        bool AddMiddleware(BML_RpcMiddleware mw, int32_t priority, void *ud = nullptr) {
            RpcMiddlewareRegistration reg(mw, priority, ud, m_RpcIface, m_Owner);
            if (!reg.Valid()) return false;
            m_Middleware.push_back(std::move(reg));
            return true;
        }

        void Bind(const BML_ImcRpcInterface *rpc) { m_RpcIface = rpc; }
        void Clear() { m_Servers.clear(); m_StreamingServers.clear(); m_Middleware.clear(); }
        size_t Count() const noexcept { return m_Servers.size() + m_StreamingServers.size(); }
        bool Empty() const noexcept { return m_Servers.empty() && m_StreamingServers.empty(); }

        auto begin() { return m_Servers.begin(); }
        auto end() { return m_Servers.end(); }
        auto begin() const { return m_Servers.begin(); }
        auto end() const { return m_Servers.end(); }

    private:
        const BML_ImcRpcInterface *m_RpcIface = nullptr;
        BML_Mod m_Owner = nullptr;
        std::vector<RpcServer> m_Servers;
        std::vector<StreamingRpcServer> m_StreamingServers;
        std::vector<RpcMiddlewareRegistration> m_Middleware;
    };

} // namespace imc
} // namespace bml

#endif /* BML_IMC_RPC_HPP */


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

#include <optional>

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

        explicit Rpc(std::string_view name)
            : m_Name(name), m_Id(InvalidRpcId) {
            Resolve();
        }

        explicit Rpc(RpcId id, std::string name = "")
            : m_Name(std::move(name)), m_Id(id) {}

        // ====================================================================
        // Properties
        // ====================================================================

        RpcId Id() const noexcept { return m_Id; }
        const std::string &Name() const noexcept { return m_Name; }
        bool Valid() const noexcept { return m_Id != InvalidRpcId; }
        explicit operator bool() const noexcept { return Valid(); }
        operator RpcId() const noexcept { return m_Id; }

        // ====================================================================
        // Resolution
        // ====================================================================

        bool Resolve() {
            if (m_Id != InvalidRpcId) return true;
            if (m_Name.empty() || !bmlImcGetRpcId) return false;
            return bmlImcGetRpcId(m_Name.c_str(), &m_Id) == BML_RESULT_OK;
        }

        static std::optional<Rpc> Create(std::string_view name) {
            Rpc rpc(name);
            if (rpc.Valid()) return rpc;
            return std::nullopt;
        }

    private:
        std::string m_Name;
        RpcId m_Id;
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

        explicit RpcFuture(BML_Future handle) : m_Handle(handle) {}

        RpcFuture(RpcFuture &&other) noexcept : m_Handle(other.m_Handle) {
            other.m_Handle = nullptr;
        }

        RpcFuture &operator=(RpcFuture &&other) noexcept {
            if (this != &other) {
                Release();
                m_Handle = other.m_Handle;
                other.m_Handle = nullptr;
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
            if (!m_Handle || !bmlImcFutureGetState) return BML_FUTURE_FAILED;
            FutureState s;
            if (bmlImcFutureGetState(m_Handle, &s) == BML_RESULT_OK) {
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
         */
        bool Wait(uint32_t timeoutMs = InfiniteTimeout) {
            if (!m_Handle || !bmlImcFutureAwait) return false;
            return bmlImcFutureAwait(m_Handle, timeoutMs) == BML_RESULT_OK;
        }

        // ====================================================================
        // Result
        // ====================================================================

        /**
         * @brief Get result as Message
         */
        std::optional<Message> ResultMessage() {
            if (!m_Handle || !bmlImcFutureGetResult) return std::nullopt;
            BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
            if (bmlImcFutureGetResult(m_Handle, &msg) == BML_RESULT_OK) {
                return Message(&msg);
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
            if (!m_Handle || !bmlImcFutureOnComplete) return false;
            return bmlImcFutureOnComplete(m_Handle, callback, userData) == BML_RESULT_OK;
        }

        // ====================================================================
        // Control
        // ====================================================================

        /**
         * @brief Cancel the pending operation
         */
        bool Cancel() {
            if (!m_Handle || !bmlImcFutureCancel) return false;
            return bmlImcFutureCancel(m_Handle) == BML_RESULT_OK;
        }

        /**
         * @brief Release the future handle
         */
        void Release() {
            if (m_Handle && bmlImcFutureRelease) {
                bmlImcFutureRelease(m_Handle);
                m_Handle = nullptr;
            }
        }

        BML_Future Native() const noexcept { return m_Handle; }

    private:
        BML_Future m_Handle;
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

                    // Store result in response buffer
                    if (!result.empty()) {
                        // Note: This requires the caller to manage the response buffer
                        // In practice, the IMC system handles this
                        response->data = result.data();
                        response->size = result.size();
                    }
                    return BML_RESULT_OK;
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
        RpcServer(std::string_view name, RpcHandler handler)
            : m_RpcId(InvalidRpcId) {
            if (!bmlImcGetRpcId || !bmlImcRegisterRpc) return;

            if (bmlImcGetRpcId(name.data(), &m_RpcId) != BML_RESULT_OK) {
                m_RpcId = InvalidRpcId;
                return;
            }

            m_Context = std::make_unique<detail::RpcHandlerContext>();
            m_Context->handler = std::move(handler);

            if (bmlImcRegisterRpc(m_RpcId, detail::RpcHandlerContext::Invoke, m_Context.get()) != BML_RESULT_OK) {
                m_RpcId = InvalidRpcId;
                m_Context.reset();
            }
        }

        /**
         * @brief Register a typed RPC handler
         */
        template <typename Req, typename Resp>
        RpcServer(std::string_view name, TypedRpcHandler<Req, Resp> handler)
            : RpcServer(name, [h = std::move(handler)](const Message &req) -> std::vector<uint8_t> {
                  static_assert(std::is_trivially_copyable_v<Req>, "Req must be trivially copyable");
                  static_assert(std::is_trivially_copyable_v<Resp>, "Resp must be trivially copyable");

                  auto *reqData = req.As<Req>();
                  if (!reqData) return {};

                  Resp resp = h(*reqData);
                  std::vector<uint8_t> result(sizeof(Resp));
                  std::memcpy(result.data(), &resp, sizeof(Resp));
                  return result;
              }) {}

        RpcServer(RpcServer &&other) noexcept
            : m_RpcId(other.m_RpcId),
              m_Context(std::move(other.m_Context)) {
            other.m_RpcId = InvalidRpcId;
        }

        RpcServer &operator=(RpcServer &&other) noexcept {
            if (this != &other) {
                Unregister();
                m_RpcId = other.m_RpcId;
                m_Context = std::move(other.m_Context);
                other.m_RpcId = InvalidRpcId;
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
            if (m_RpcId != InvalidRpcId && bmlImcUnregisterRpc) {
                bmlImcUnregisterRpc(m_RpcId);
                m_RpcId = InvalidRpcId;
            }
            m_Context.reset();
        }

        bool Valid() const noexcept { return m_RpcId != InvalidRpcId; }
        explicit operator bool() const noexcept { return Valid(); }

        RpcId GetRpcId() const noexcept { return m_RpcId; }

    private:
        RpcId m_RpcId;
        std::unique_ptr<detail::RpcHandlerContext> m_Context;
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
        RpcClient() : m_Rpc() {}

        explicit RpcClient(std::string_view name) : m_Rpc(name) {}

        explicit RpcClient(const Rpc &rpc) : m_Rpc(rpc) {}

        // ====================================================================
        // Properties
        // ====================================================================

        bool Valid() const noexcept { return m_Rpc.Valid(); }
        explicit operator bool() const noexcept { return Valid(); }

        RpcId GetRpcId() const noexcept { return m_Rpc.Id(); }
        const Rpc &GetRpc() const noexcept { return m_Rpc; }

        // ====================================================================
        // Async Calls
        // ====================================================================

        /**
         * @brief Call with raw data
         */
        RpcFuture Call(const void *data = nullptr, size_t size = 0) {
            if (!Valid() || !bmlImcCallRpc) return RpcFuture();

            BML_ImcMessage msg = BML_IMC_MSG(data, size);
            BML_Future handle;
            if (bmlImcCallRpc(m_Rpc.Id(), &msg, &handle) == BML_RESULT_OK) {
                return RpcFuture(handle);
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
            if (!Valid() || !bmlImcCallRpc) return RpcFuture();

            BML_Future handle;
            if (bmlImcCallRpc(m_Rpc.Id(), &msg, &handle) == BML_RESULT_OK) {
                return RpcFuture(handle);
            }
            return RpcFuture();
        }

        /**
         * @brief Call with message builder
         */
        RpcFuture Call(const MessageBuilder &builder) {
            return Call(builder.Build());
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
        Rpc m_Rpc;
    };

} // namespace imc
} // namespace bml

#endif /* BML_IMC_RPC_HPP */


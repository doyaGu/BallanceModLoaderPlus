#ifndef BML_TIMER_HPP
#define BML_TIMER_HPP
#include "BML/Timer.h"
#include "BML/Mod.hpp"
#include <atomic>
#include <functional>

namespace BML::Timer {
struct Definition {
    std::string Name;
    uint32_t Type = BML_TIMER_ONCE, TimeBase = BML_TIMER_TIMEBASE_TIME;
    bool StartPaused = false;
    int32_t Priority = 0;
    double DelayMs = 0;
    uint64_t DelayTicks = 0, RepeatCount = 0;
};

struct Event {
    BML_TimerHandle Handle = 0;
    std::string_view Name;
    uint32_t State = 0, Type = 0, TimeBase = 0;
    uint64_t CompletedIterations = 0;
    int64_t RemainingIterations = 0;
    float Progress = 0;
};

struct Info {
    BML_TimerHandle Handle = 0;
    std::string Name;
    uint32_t State = 0, Type = 0, TimeBase = 0;
    uint64_t CompletedIterations = 0;
    int64_t RemainingIterations = 0;
    float Progress = 0;
};

using Callback = std::function<bool(const Mod::Context &, const Event &)>;
using OnceCallback = std::function<void(const Mod::Context &, const Event &)>;

class Registration {
public:
    Registration() noexcept = default;

    ~Registration() noexcept { Dispose(); }

    Registration(const Registration &) = delete;
    Registration &operator=(const Registration &) = delete;

    Registration(Registration &&other) noexcept { Move(other); }

    Registration &operator=(Registration &&other) noexcept {
        if (this != &other) {
            Dispose();
            Move(other);
        }
        return *this;
    }

    int Register(const Mod::Context &context, const Definition &definition,
                 Callback callback) noexcept {
        if (IsValid())
            return BML_ERROR_ALREADY_EXISTS;
        if (!callback || !BML::Detail::IsText(definition.Name))
            return BML_ERROR_INVALID_PARAMETER;
        return BML::Detail::Guard([&] {
            const auto *api = BML::Detail::Bind<BML_TimerInterface>(
                BML_TIMER_INTERFACE_ID, 1, BML_TIMER_INTERFACE_1_0_SIZE,
                +[](const BML_TimerInterface *table) {
                    return BML_IFACE_HAS(table, BML_TimerInterface, Register) &&
                           BML_IFACE_HAS(table, BML_TimerInterface, Read) &&
                           BML_IFACE_HAS(table, BML_TimerInterface, Pause) &&
                           BML_IFACE_HAS(table, BML_TimerInterface, Resume) &&
                           BML_IFACE_HAS(table, BML_TimerInterface, Cancel);
                });
            if (!api)
                return BML_ERROR_UNAVAILABLE;
            auto state = std::make_shared<State>();
            state->Function = std::move(callback);
            auto holder = std::make_unique<std::shared_ptr<State>>(state);
            const BML_TimerDefinition record{sizeof(BML_TimerDefinition),
                                             definition.Name.c_str(),
                                             definition.Type,
                                             definition.TimeBase,
                                             definition.StartPaused ? 1u : 0u,
                                             definition.Priority,
                                             definition.DelayMs,
                                             definition.DelayTicks,
                                             definition.RepeatCount};
            BML_TimerHandle handle = 0;
            const int code =
                api->Register(context.Handle(), &record, &Invoke, holder.get(), &Release, &handle);
            if (code != BML_OK)
                return code;
            holder.release(); // The host now owes exactly one Release.
            m_Context = context;
            m_Api = api;
            m_Handle = handle;
            m_State = std::move(state);
            return BML_OK;
        });
    }

    [[nodiscard]] bool IsValid() const noexcept {
        return m_Handle && m_State && !m_State->Released.load(std::memory_order_acquire);
    }
    [[nodiscard]] Result<Info> Read() const try {
        if (!IsValid())
            return Result<Info>::Failure(BML_ERROR_INVALID_HANDLE);
        Info value;
        const int code =
            m_Api->Read(m_Context.Handle(), m_Handle,
                        BML::Detail::Cdecl<[](void *data, const BML_TimerEvent *e) -> int {
                            return BML::Detail::Guard([&] {
                                *static_cast<Info *>(data) = {e->Handle,
                                                              e->Name ? e->Name : "",
                                                              e->State,
                                                              e->Type,
                                                              e->TimeBase,
                                                              e->CompletedIterations,
                                                              e->RemainingIterations,
                                                              e->Progress};
                                return BML_OK;
                            });
                        }>::Call,
                        &value);
        return code == BML_OK ? Result<Info>::Success(std::move(value))
                              : Result<Info>::Failure(code);
    } catch (const std::bad_alloc &) {
        return Result<Info>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Info>::Failure(BML_ERROR_FAIL);
    }

    int Pause() noexcept {
        return IsValid() ? m_Api->Pause(m_Context.Handle(), m_Handle) : BML_ERROR_INVALID_HANDLE;
    }

    int Resume() noexcept {
        return IsValid() ? m_Api->Resume(m_Context.Handle(), m_Handle) : BML_ERROR_INVALID_HANDLE;
    }

    int Cancel() noexcept {
        if (!IsValid()) {
            m_Handle = 0;
            m_State.reset();
            return BML_OK;
        }
        const int code = m_Api->Cancel(m_Context.Handle(), m_Handle);
        if (code == BML_OK) {
            m_Handle = 0;
            m_State.reset();
        }
        return code;
    }

private:
    struct State {
        Callback Function;
        std::atomic<bool> Abandoned{false}, Released{false};
    };

    static int BML_CDECL Invoke(void *data, BML_ModContextHandle h, const BML_TimerEvent *e,
                                uint32_t *keep) noexcept {
        if (!data || !e || !keep || e->StructSize < BML_TIMER_EVENT_1_0_SIZE)
            return BML_ERROR_INVALID_PARAMETER;
        const auto &state = *static_cast<std::shared_ptr<State> *>(data);
        if (state->Abandoned.load(std::memory_order_acquire)) {
            *keep = 0;
            return BML_OK;
        }
        return BML::Detail::Guard([&] {
            const Event event{e->Handle,
                              e->Name ? e->Name : "",
                              e->State,
                              e->Type,
                              e->TimeBase,
                              e->CompletedIterations,
                              e->RemainingIterations,
                              e->Progress};
            *keep = state->Function(Mod::Detail::ContextAccess::Make(h), event) ? 1u : 0u;
            return BML_OK;
        });
    }

    static void BML_CDECL Release(void *data) noexcept {
        auto *holder = static_cast<std::shared_ptr<State> *>(data);
        (*holder)->Released.store(true, std::memory_order_release);
        delete holder;
    }

    void Dispose() noexcept {
        if (Cancel() != BML_OK) {
            // We still hold a local reference, even if the host released its own
            // reference concurrently. Never delete transferred callback storage.
            if (m_State)
                m_State->Abandoned.store(true, std::memory_order_release);
            m_Context.LogError("Timer cancellation failed; callback abandoned until owner cleanup");
            m_Handle = 0;
            m_State.reset();
        }
    }

    void Move(Registration &other) noexcept {
        m_Context = other.m_Context;
        m_Api = other.m_Api;
        m_Handle = std::exchange(other.m_Handle, 0);
        m_State = std::move(other.m_State);
    }

    Mod::Context m_Context;
    const BML_TimerInterface *m_Api = nullptr;
    BML_TimerHandle m_Handle = 0;
    std::shared_ptr<State> m_State;
};

namespace Detail {
inline Result<Registration> Register(const Mod::Context &ctx, Definition definition,
                                     Callback callback) try {
    Registration value;
    const int code = value.Register(ctx, definition, std::move(callback));
    return code == BML_OK ? Result<Registration>::Success(std::move(value))
                          : Result<Registration>::Failure(code);
} catch (const std::bad_alloc &) {
    return Result<Registration>::Failure(BML_ERROR_OUT_OF_MEMORY);
} catch (...) {
    return Result<Registration>::Failure(BML_ERROR_FAIL);
}
} // namespace Detail
[[nodiscard]] inline Result<Registration> SetTimeout(const Mod::Context &ctx, double delayMs,
                                                     OnceCallback callback,
                                                     std::string_view name = {}) try {
    if (!callback || !BML::Detail::IsText(name))
        return Result<Registration>::Failure(BML_ERROR_INVALID_PARAMETER);
    Definition d;
    d.Name = name;
    d.DelayMs = delayMs;
    return Detail::Register(ctx, std::move(d),
                            [fn = std::move(callback)](const Mod::Context &c, const Event &e) {
                                fn(c, e);
                                return false;
                            });
} catch (const std::bad_alloc &) {
    return Result<Registration>::Failure(BML_ERROR_OUT_OF_MEMORY);
} catch (...) {
    return Result<Registration>::Failure(BML_ERROR_FAIL);
}
[[nodiscard]] inline Result<Registration> SetInterval(const Mod::Context &ctx, double delayMs,
                                                      Callback callback,
                                                      std::string_view name = {}) try {
    if (!BML::Detail::IsText(name))
        return Result<Registration>::Failure(BML_ERROR_INVALID_PARAMETER);
    Definition d;
    d.Name = name;
    d.Type = BML_TIMER_INTERVAL;
    d.DelayMs = delayMs;
    return Detail::Register(ctx, std::move(d), std::move(callback));
} catch (const std::bad_alloc &) {
    return Result<Registration>::Failure(BML_ERROR_OUT_OF_MEMORY);
} catch (...) {
    return Result<Registration>::Failure(BML_ERROR_FAIL);
}
[[nodiscard]] inline Result<Registration> SetTimeoutTicks(const Mod::Context &ctx, uint64_t ticks,
                                                          OnceCallback callback,
                                                          std::string_view name = {}) try {
    if (!callback || !BML::Detail::IsText(name))
        return Result<Registration>::Failure(BML_ERROR_INVALID_PARAMETER);
    Definition d;
    d.Name = name;
    d.TimeBase = BML_TIMER_TIMEBASE_TICK;
    d.DelayTicks = ticks;
    return Detail::Register(ctx, std::move(d),
                            [fn = std::move(callback)](const Mod::Context &c, const Event &e) {
                                fn(c, e);
                                return false;
                            });
} catch (const std::bad_alloc &) {
    return Result<Registration>::Failure(BML_ERROR_OUT_OF_MEMORY);
} catch (...) {
    return Result<Registration>::Failure(BML_ERROR_FAIL);
}
[[nodiscard]] inline Result<Registration> SetIntervalTicks(const Mod::Context &ctx, uint64_t ticks,
                                                           Callback callback,
                                                           std::string_view name = {}) try {
    if (!BML::Detail::IsText(name))
        return Result<Registration>::Failure(BML_ERROR_INVALID_PARAMETER);
    Definition d;
    d.Name = name;
    d.Type = BML_TIMER_INTERVAL;
    d.TimeBase = BML_TIMER_TIMEBASE_TICK;
    d.DelayTicks = ticks;
    return Detail::Register(ctx, std::move(d), std::move(callback));
} catch (const std::bad_alloc &) {
    return Result<Registration>::Failure(BML_ERROR_OUT_OF_MEMORY);
} catch (...) {
    return Result<Registration>::Failure(BML_ERROR_FAIL);
}

// A non-owning service view. Registrations still make cancellation ownership
// explicit; this view only binds Context and adapts ordinary author lambdas.
class View {
public:
    explicit View(const Mod::Context &context) noexcept : m_Context(context) {}
    [[nodiscard]] Result<Registration> Register(const Definition &definition,
                                                Callback callback) const try {
        return Detail::Register(m_Context, definition, std::move(callback));
    } catch (const std::bad_alloc &) {
        return Result<Registration>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Registration>::Failure(BML_ERROR_FAIL);
    }
    template <class Function>
    [[nodiscard]] Result<Registration> SetTimeout(double delayMs, Function &&function,
                                                  std::string_view name = {}) const try {
        if constexpr (requires { static_cast<bool>(function); }) {
            if (!static_cast<bool>(function))
                return Result<Registration>::Failure(BML_ERROR_INVALID_PARAMETER);
        }
        return Timer::SetTimeout(m_Context, delayMs, Once(std::forward<Function>(function)), name);
    } catch (const std::bad_alloc &) {
        return Result<Registration>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Registration>::Failure(BML_ERROR_FAIL);
    }
    template <class Function>
    [[nodiscard]] Result<Registration> SetTimeoutTicks(uint64_t ticks, Function &&function,
                                                       std::string_view name = {}) const try {
        if constexpr (requires { static_cast<bool>(function); }) {
            if (!static_cast<bool>(function))
                return Result<Registration>::Failure(BML_ERROR_INVALID_PARAMETER);
        }
        return Timer::SetTimeoutTicks(m_Context, ticks, Once(std::forward<Function>(function)),
                                      name);
    } catch (const std::bad_alloc &) {
        return Result<Registration>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Registration>::Failure(BML_ERROR_FAIL);
    }
    template <class Function>
    [[nodiscard]] Result<Registration> SetInterval(double intervalMs, Function &&function,
                                                   std::string_view name = {}) const try {
        if constexpr (requires { static_cast<bool>(function); }) {
            if (!static_cast<bool>(function))
                return Result<Registration>::Failure(BML_ERROR_INVALID_PARAMETER);
        }
        return Timer::SetInterval(m_Context, intervalMs, Loop(std::forward<Function>(function)),
                                  name);
    } catch (const std::bad_alloc &) {
        return Result<Registration>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Registration>::Failure(BML_ERROR_FAIL);
    }
    template <class Function>
    [[nodiscard]] Result<Registration> SetIntervalTicks(uint64_t ticks, Function &&function,
                                                        std::string_view name = {}) const try {
        if constexpr (requires { static_cast<bool>(function); }) {
            if (!static_cast<bool>(function))
                return Result<Registration>::Failure(BML_ERROR_INVALID_PARAMETER);
        }
        return Timer::SetIntervalTicks(m_Context, ticks, Loop(std::forward<Function>(function)),
                                       name);
    } catch (const std::bad_alloc &) {
        return Result<Registration>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Registration>::Failure(BML_ERROR_FAIL);
    }

private:
    template <class F>
    static decltype(auto) Invoke(F &function, const Mod::Context &context, const Event &event) {
        if constexpr (std::is_invocable_v<F &, const Mod::Context &, const Event &>)
            return std::invoke(function, context, event);
        else {
            static_assert(
                std::is_invocable_v<F &>,
                "A timer callback takes no arguments, or (const Context &, const Timer::Event &)");
            return std::invoke(function);
        }
    }

    template <class F> static OnceCallback Once(F &&function) {
        return [callback = std::forward<F>(function)](const Mod::Context &context,
                                                      const Event &event) mutable {
            (void)Invoke(callback, context, event);
        };
    }

    template <class F> static Callback Loop(F &&function) {
        return [callback = std::forward<F>(function)](const Mod::Context &context,
                                                      const Event &event) mutable {
            using Return = decltype(Invoke(callback, context, event));
            if constexpr (std::is_void_v<Return>) {
                Invoke(callback, context, event);
                return true;
            } else {
                static_assert(std::is_same_v<Return, bool>,
                              "An interval callback returns bool to continue/stop, or void to "
                              "continue until cancellation");
                return Invoke(callback, context, event);
            }
        };
    }

    Mod::Context m_Context;
};
} // namespace BML::Timer

inline BML::Timer::View BML::Mod::Context::Timers() const noexcept {
    return BML::Timer::View(*this);
}
#endif

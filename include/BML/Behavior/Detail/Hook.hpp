#ifndef BML_BEHAVIOR_DETAIL_HOOK_HPP
#define BML_BEHAVIOR_DETAIL_HOOK_HPP

#include "BML/Behavior/Block.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace BML::Behavior {

namespace Detail {

inline PlanInfo ReadPlanInfo(const BML_BehaviorPlanInfo &source) {
    PlanInfo info;
    info.State = static_cast<PlanState>(source.State);
    info.World = source.World;
    info.Matches = source.Matches;
    info.Installations = source.Installations;
    info.LastStatus = ReadStatus(source.Diagnostic);
    return info;
}

inline PatchInfo ReadPatchInfo(const BML_BehaviorPatchInfo &source) {
    PatchInfo info;
    info.State = static_cast<PatchState>(source.State);
    info.Conflicts = source.Conflicts;
    info.LastStatus = ReadStatus(source.Diagnostic);
    return info;
}

// Holds the caller reference to one author callback. The Loader takes a
// reference of its own while it accepts an edit, so this record only has to
// outlive the Edit that carries it.
struct HookHolder {
    HookHolder() = default;
    HookHolder(const HookHolder &) = delete;
    HookHolder &operator=(const HookHolder &) = delete;
    ~HookHolder() {
        if (Function.Release)
            Function.Release(Function.State);
    }

    BML_BehaviorHookFunction Function{};
};

// Resolves the return type of whichever call the callback supports, without
// forming the other one.
template <class Callable, bool TakesEvent, bool TakesNothing>
struct HookReturn {
    using Type = void;
};
template <class Callable, bool TakesNothing>
struct HookReturn<Callable, true, TakesNothing> {
    using Type = std::invoke_result_t<Callable &, const HookEvent &>;
};
template <class Callable>
struct HookReturn<Callable, false, true> {
    using Type = std::invoke_result_t<Callable &>;
};

template <class Function>
struct HookFunction {
    using Callable = std::decay_t<Function>;
    static constexpr bool TakesEvent =
        std::is_invocable_v<Callable &, const HookEvent &>;
    static_assert(TakesEvent || std::is_invocable_v<Callable &>,
                  "A Behavior Hook callback takes a HookEvent or nothing.");
    using Returned = typename HookReturn<
        Callable, TakesEvent, std::is_invocable_v<Callable &>>::Type;
    static_assert(std::is_void_v<Returned> ||
                  std::is_same_v<Returned, HookResult>,
                  "A Behavior Hook callback returns void or HookResult.");

    explicit HookFunction(Function &&function)
        : Value(std::forward<Function>(function)) {}

    static void BML_BEHAVIOR_CALL Retain(void *state) {
        ++static_cast<HookFunction *>(state)->References;
    }
    static void BML_BEHAVIOR_CALL Release(void *state) {
        auto *self = static_cast<HookFunction *>(state);
        if (self->References.fetch_sub(1) == 1)
            delete self;
    }
    static int BML_BEHAVIOR_CALL Invoke(
        void *state, const BML_BehaviorHookContext *source) noexcept {
        try {
            if (!state || !source || source->StructSize < sizeof(*source))
                return static_cast<int>(HookResult::Error);
            HookEvent event;
            event.DeltaTime = source->DeltaTime;
            event.Block = source->Block;
            event.Script = source->Script;
            event.Owner = source->Owner;
            auto &callable = static_cast<HookFunction *>(state)->Value;
            if constexpr (std::is_void_v<Returned>) {
                if constexpr (TakesEvent)
                    std::invoke(callable, event);
                else
                    std::invoke(callable);
                return BML_BEHAVIOR_HOOK_OK;
            } else if constexpr (TakesEvent) {
                return static_cast<int>(std::invoke(callable, event));
            } else {
                return static_cast<int>(std::invoke(callable));
            }
        } catch (...) {
            return static_cast<int>(HookResult::Fault);
        }
    }

    std::atomic<std::uint32_t> References{1};
    Callable Value;
};

} // namespace Detail

// One author callback, shareable across the steps of one Edit. It runs
// on the game thread inside the execution the game itself drives. It may close
// its Patch, Plan, Session, or Mod: callback admission closes immediately, the
// request never waits for this invocation, and native teardown plus Release
// occur later at a game-thread safe point.

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_DETAIL_HOOK_HPP

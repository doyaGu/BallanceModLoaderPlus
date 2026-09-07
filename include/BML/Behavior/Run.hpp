#ifndef BML_BEHAVIOR_RUN_HPP
#define BML_BEHAVIOR_RUN_HPP

#include "BML/Behavior/Detail/Wire.hpp"

#include <initializer_list>
#include <utility>

namespace BML::Behavior {

class Call {
public:
    Call(Call &&) noexcept = default;
    Call &operator=(Call &&) noexcept = default;
    Call(const Call &) = delete;
    Call &operator=(const Call &) = delete;

    [[nodiscard]] explicit operator bool() const noexcept {
        return static_cast<bool>(m_Run);
    }
    [[nodiscard]] Result<RunInfo> Info() const { return m_Run.Info(); }
    [[nodiscard]] Result<Behavior::Frames> TakeFrames() {
        return m_Run.TakeFrames();
    }
    [[nodiscard]] Result<void> TakeFrames(Behavior::Frames &frames) {
        return m_Run.TakeFrames(frames);
    }
    [[nodiscard]] Result<Behavior::Layout> Layout() const {
        return m_Run.Layout();
    }
    [[nodiscard]] Result<Graph> Inspect(View view = View::Logical) const {
        return m_Run.Inspect(view);
    }
    template <class T>
    [[nodiscard]] Result<std::uint64_t> Set(
        const Behavior::Slot &slot, T &&value) const {
        return m_Run.Set(slot, Behavior::Value(std::forward<T>(value)));
    }
    template <class T>
    [[nodiscard]] Result<std::uint64_t> Set(
        SlotKind kind, const Selector &slot, T &&value) const {
        return m_Run.Set(kind, slot,
                         Behavior::Value(std::forward<T>(value)));
    }
    [[nodiscard]] Result<std::uint64_t> Bind(
        const Behavior::Slot &slot, const Port &source,
        Relation relation = Relation::Direct) const {
        return m_Run.Bind(slot, source, relation);
    }
    [[nodiscard]] Result<std::uint64_t> Settings(
        std::initializer_list<SlotValue> values) const {
        return m_Run.Settings(values);
    }
    [[nodiscard]] Result<CloseState> Close() noexcept { return m_Run.Close(); }
    [[nodiscard]] Result<Task> Continue();

private:
    explicit Call(Detail::Run run) : m_Run(std::move(run)) {}
    Detail::Run m_Run;

    friend class Block;
};

class Task {
public:
    Task(Task &&) noexcept = default;
    Task &operator=(Task &&) noexcept = default;
    Task(const Task &) = delete;
    Task &operator=(const Task &) = delete;

    [[nodiscard]] explicit operator bool() const noexcept {
        return static_cast<bool>(m_Run);
    }
    [[nodiscard]] Result<RunInfo> Info() const { return m_Run.Info(); }
    [[nodiscard]] Result<Behavior::Frames> TakeFrames() {
        return m_Run.TakeFrames();
    }
    [[nodiscard]] Result<void> TakeFrames(Behavior::Frames &frames) {
        return m_Run.TakeFrames(frames);
    }
    [[nodiscard]] Result<Behavior::Layout> Layout() const {
        return m_Run.Layout();
    }
    [[nodiscard]] Result<Graph> Inspect(View view = View::Logical) const {
        return m_Run.Inspect(view);
    }
    [[nodiscard]] Result<PulseResult> Pulse(const Selector &input) const {
        return m_Run.Pulse(input);
    }
    [[nodiscard]] Result<PulseResult> Pulse(std::string_view input) const {
        return Pulse(Selector::Unique(input));
    }
    template <class T>
    [[nodiscard]] Result<std::uint64_t> Set(
        const Behavior::Slot &slot, T &&value) const {
        return m_Run.Set(slot, Behavior::Value(std::forward<T>(value)));
    }
    template <class T>
    [[nodiscard]] Result<std::uint64_t> Set(
        SlotKind kind, const Selector &slot, T &&value) const {
        return m_Run.Set(kind, slot,
                         Behavior::Value(std::forward<T>(value)));
    }
    [[nodiscard]] Result<std::uint64_t> Bind(
        const Behavior::Slot &slot, const Port &source,
        Relation relation = Relation::Direct) const {
        return m_Run.Bind(slot, source, relation);
    }
    [[nodiscard]] Result<std::uint64_t> Settings(
        std::initializer_list<SlotValue> values) const {
        return m_Run.Settings(values);
    }
    [[nodiscard]] Result<CloseState> Close() noexcept { return m_Run.Close(); }

private:
    explicit Task(Detail::Run run) : m_Run(std::move(run)) {}
    Detail::Run m_Run;

    friend class Block;
    friend class Call;
};

class Instance {
public:
    Instance(Instance &&) noexcept = default;
    Instance &operator=(Instance &&) noexcept = default;
    Instance(const Instance &) = delete;
    Instance &operator=(const Instance &) = delete;

    [[nodiscard]] explicit operator bool() const noexcept {
        return static_cast<bool>(m_Run);
    }
    [[nodiscard]] Result<RunInfo> Info() const { return m_Run.Info(); }
    [[nodiscard]] Result<Behavior::Frames> TakeFrames() {
        return m_Run.TakeFrames();
    }
    [[nodiscard]] Result<void> TakeFrames(Behavior::Frames &frames) {
        return m_Run.TakeFrames(frames);
    }
    [[nodiscard]] Result<Behavior::Layout> Layout() const {
        return m_Run.Layout();
    }
    [[nodiscard]] Result<Graph> Inspect(View view = View::Logical) const {
        return m_Run.Inspect(view);
    }
    [[nodiscard]] Result<PulseResult> Pulse(const Selector &input) const {
        return m_Run.Pulse(input);
    }
    [[nodiscard]] Result<PulseResult> Pulse(std::string_view input) const {
        return Pulse(Selector::Unique(input));
    }
    template <class T>
    [[nodiscard]] Result<std::uint64_t> Set(
        const Behavior::Slot &slot, T &&value) const {
        return m_Run.Set(slot, Behavior::Value(std::forward<T>(value)));
    }
    template <class T>
    [[nodiscard]] Result<std::uint64_t> Set(
        SlotKind kind, const Selector &slot, T &&value) const {
        return m_Run.Set(kind, slot,
                         Behavior::Value(std::forward<T>(value)));
    }
    [[nodiscard]] Result<std::uint64_t> Bind(
        const Behavior::Slot &slot, const Port &source,
        Relation relation = Relation::Direct) const {
        return m_Run.Bind(slot, source, relation);
    }
    [[nodiscard]] Result<std::uint64_t> Settings(
        std::initializer_list<SlotValue> values) const {
        return m_Run.Settings(values);
    }
    [[nodiscard]] Result<CloseState> Close() noexcept { return m_Run.Close(); }

private:
    explicit Instance(Detail::Run run) : m_Run(std::move(run)) {}
    Detail::Run m_Run;

    friend class Block;
};


} // namespace BML::Behavior

#endif // BML_BEHAVIOR_RUN_HPP

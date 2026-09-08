#ifndef BML_BEHAVIOR_BLOCK_HPP
#define BML_BEHAVIOR_BLOCK_HPP

#include "BML/Behavior/Run.hpp"

#include <initializer_list>
#include <iterator>
#include <memory>
#include <string_view>
#include <vector>

namespace BML::Behavior {

class Block {
public:
    Block(const Block &) noexcept = default;
    Block &operator=(const Block &) noexcept = default;
    Block(Block &&) noexcept = default;
    Block &operator=(Block &&) noexcept = default;

    Block &TargetOwner() {
        Detail::BlockSpec &spec = Change();
        spec.TargetKind = BML_BEHAVIOR_TARGET_OWNER;
        spec.TargetType = CKGUID(0, 0);
        spec.TargetObject = {};
        return *this;
    }
    Block &Target(CKGUID type, ObjectRef object) {
        Detail::BlockSpec &spec = Change();
        spec.TargetKind = BML_BEHAVIOR_TARGET_OBJECT;
        spec.TargetType = type;
        spec.TargetObject = object;
        return *this;
    }
    Block &NullTarget(CKGUID type) {
        Detail::BlockSpec &spec = Change();
        spec.TargetKind = BML_BEHAVIOR_TARGET_NULL;
        spec.TargetType = type;
        spec.TargetObject = {};
        return *this;
    }
    Block &Settings(std::initializer_list<SlotValue> values) {
        if (!values.size())
            return *this;
        Detail::BlockSpec &spec = Change();
        spec.Settings.emplace_back(values);
        return *this;
    }
    Block &Settings(std::vector<SlotValue> values) {
        if (values.empty())
            return *this;
        Change().Settings.push_back(std::move(values));
        return *this;
    }
    Block &Pins(std::initializer_list<SlotValue> values) {
        if (!values.size())
            return *this;
        Detail::BlockSpec &spec = Change();
        spec.Pins.insert(spec.Pins.end(), values.begin(), values.end());
        return *this;
    }
    Block &Pins(std::vector<SlotValue> values) {
        if (values.empty())
            return *this;
        Detail::BlockSpec &spec = Change();
        spec.Pins.insert(spec.Pins.end(),
                         std::make_move_iterator(values.begin()),
                         std::make_move_iterator(values.end()));
        return *this;
    }
    Block &Locals(std::initializer_list<SlotValue> values) {
        if (!values.size())
            return *this;
        Detail::BlockSpec &spec = Change();
        spec.Locals.insert(spec.Locals.end(), values.begin(), values.end());
        return *this;
    }
    Block &Locals(std::vector<SlotValue> values) {
        if (values.empty())
            return *this;
        Detail::BlockSpec &spec = Change();
        spec.Locals.insert(spec.Locals.end(),
                           std::make_move_iterator(values.begin()),
                           std::make_move_iterator(values.end()));
        return *this;
    }
    [[nodiscard]] Result<void> Validate() const;
    [[nodiscard]] Result<Behavior::Call> Call(
        const Selector &input = Selector::Only(),
        FramePolicy frames = Signals()) const;
    [[nodiscard]] Result<Behavior::Call> Call(FramePolicy frames) const {
        return Call(Selector::Only(), frames);
    }
    [[nodiscard]] Result<Behavior::Call> Call(
        std::string_view input,
        FramePolicy frames = Signals()) const {
        return Call(Selector::Unique(input), frames);
    }
    [[nodiscard]] Result<Behavior::Call> Call(
        ObjectRef owner, const Selector &input = Selector::Only(),
        FramePolicy frames = Signals()) const;
    [[nodiscard]] Result<Behavior::Call> Call(
        ObjectRef owner, FramePolicy frames) const {
        return Call(owner, Selector::Only(), frames);
    }
    [[nodiscard]] Result<Behavior::Call> Call(
        ObjectRef owner, std::string_view input,
        FramePolicy frames = Signals()) const {
        return Call(owner, Selector::Unique(input), frames);
    }
    [[nodiscard]] Result<Task> Start(
        const Selector &input = Selector::Only(),
        FramePolicy frames = Signals()) const;
    [[nodiscard]] Result<Task> Start(FramePolicy frames) const {
        return Start(Selector::Only(), frames);
    }
    [[nodiscard]] Result<Task> Start(
        std::string_view input,
        FramePolicy frames = Signals()) const {
        return Start(Selector::Unique(input), frames);
    }
    [[nodiscard]] Result<Task> Start(
        ObjectRef owner, const Selector &input = Selector::Only(),
        FramePolicy frames = Signals()) const;
    [[nodiscard]] Result<Task> Start(
        ObjectRef owner, FramePolicy frames) const {
        return Start(owner, Selector::Only(), frames);
    }
    [[nodiscard]] Result<Task> Start(
        ObjectRef owner, std::string_view input,
        FramePolicy frames = Signals()) const {
        return Start(owner, Selector::Unique(input), frames);
    }
    [[nodiscard]] Result<Instance> Spawn(
        FramePolicy frames = Signals()) const;
    [[nodiscard]] Result<Instance> Spawn(
        ObjectRef owner, FramePolicy frames = Signals()) const;
    // Parks this Block inside a live graph and keeps driving it. The graph
    // does not activate a parked Block, so the returned Instance is what sets
    // its Pins and pulses it. Closing the Instance removes the Block again.
    [[nodiscard]] Result<Instance> SpawnIn(
        ObjectRef graph, FramePolicy frames = Signals()) const;
    [[nodiscard]] Result<Instance> SpawnIn(
        const Graph &graph, FramePolicy frames = Signals()) const;

private:
    Block(std::shared_ptr<Detail::SessionState> session,
          Prototype prototype)
        : m_Session(std::move(session)),
          m_State(std::make_shared<Detail::BlockState>(
              Detail::BlockSpec(prototype))) {}

    template <class Handle, class Function>
    Result<Handle> Open(Function function, RunKind kind, ObjectRef owner,
                        const Selector *input, FramePolicy frames) const;
    [[nodiscard]] Result<std::shared_ptr<const Detail::CompiledBlock>>
    Compile(bool requireDeclared = false) const;
    [[nodiscard]] Status Accept(const BML_BehaviorRunInfo &info,
                                RunKind kind) const;
    Detail::BlockSpec &Change();

    std::shared_ptr<Detail::SessionState> m_Session;
    std::shared_ptr<Detail::BlockState> m_State;

    friend class Session;
    friend class Edit;
};


} // namespace BML::Behavior

#endif // BML_BEHAVIOR_BLOCK_HPP

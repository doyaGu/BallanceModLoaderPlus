#ifndef BML_BEHAVIOR_BLOCK_HPP
#define BML_BEHAVIOR_BLOCK_HPP

#include "BML/Behavior/Run.hpp"

#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
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
        Detail::BlockDefinition &definition = Change();
        definition.TargetKind = BML_BEHAVIOR_TARGET_OWNER;
        definition.TargetType = CKGUID(0, 0);
        definition.TargetObject = {};
        return *this;
    }
    Block &Target(CKGUID type, ObjectRef object) {
        Detail::BlockDefinition &definition = Change();
        definition.TargetKind = BML_BEHAVIOR_TARGET_OBJECT;
        definition.TargetType = type;
        definition.TargetObject = object;
        return *this;
    }
    Block &NullTarget(CKGUID type) {
        Detail::BlockDefinition &definition = Change();
        definition.TargetKind = BML_BEHAVIOR_TARGET_NULL;
        definition.TargetType = type;
        definition.TargetObject = {};
        return *this;
    }
    Block &Settings(std::initializer_list<SlotValue> values) {
        if (!values.size())
            return *this;
        Detail::BlockDefinition &definition = Change();
        definition.Settings.emplace_back(values);
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
        Detail::BlockDefinition &definition = Change();
        definition.Pins.insert(definition.Pins.end(), values.begin(),
                               values.end());
        return *this;
    }
    Block &Pins(std::vector<SlotValue> values) {
        if (values.empty())
            return *this;
        Detail::BlockDefinition &definition = Change();
        definition.Pins.insert(definition.Pins.end(),
                               std::make_move_iterator(values.begin()),
                               std::make_move_iterator(values.end()));
        return *this;
    }
    Block &Locals(std::initializer_list<SlotValue> values) {
        if (!values.size())
            return *this;
        Detail::BlockDefinition &definition = Change();
        definition.Locals.insert(definition.Locals.end(), values.begin(),
                                 values.end());
        return *this;
    }
    Block &Locals(std::vector<SlotValue> values) {
        if (values.empty())
            return *this;
        Detail::BlockDefinition &definition = Change();
        definition.Locals.insert(definition.Locals.end(),
                                 std::make_move_iterator(values.begin()),
                                 std::make_move_iterator(values.end()));
        return *this;
    }
    Block &Frames(FramePolicy policy) {
        Change().Frames = policy;
        return *this;
    }

    [[nodiscard]] Result<void> Validate() const;
    [[nodiscard]] Result<Behavior::Call> Call(
        const Selector &input = Selector::Only(),
        std::optional<FramePolicy> frames = std::nullopt) const;
    [[nodiscard]] Result<Behavior::Call> Call(
        std::string_view input,
        std::optional<FramePolicy> frames = std::nullopt) const {
        return Call(Selector::Unique(input), frames);
    }
    [[nodiscard]] Result<Behavior::Call> Call(
        ObjectRef owner, const Selector &input = Selector::Only(),
        std::optional<FramePolicy> frames = std::nullopt) const;
    [[nodiscard]] Result<Behavior::Call> Call(
        ObjectRef owner, std::string_view input,
        std::optional<FramePolicy> frames = std::nullopt) const {
        return Call(owner, Selector::Unique(input), frames);
    }
    [[nodiscard]] Result<Task> Start(
        const Selector &input = Selector::Only(),
        std::optional<FramePolicy> frames = std::nullopt) const;
    [[nodiscard]] Result<Task> Start(
        std::string_view input,
        std::optional<FramePolicy> frames = std::nullopt) const {
        return Start(Selector::Unique(input), frames);
    }
    [[nodiscard]] Result<Task> Start(
        ObjectRef owner, const Selector &input = Selector::Only(),
        std::optional<FramePolicy> frames = std::nullopt) const;
    [[nodiscard]] Result<Task> Start(
        ObjectRef owner, std::string_view input,
        std::optional<FramePolicy> frames = std::nullopt) const {
        return Start(owner, Selector::Unique(input), frames);
    }
    [[nodiscard]] Result<Instance> Spawn(
        std::optional<FramePolicy> frames = std::nullopt) const;
    [[nodiscard]] Result<Instance> Spawn(
        ObjectRef owner,
        std::optional<FramePolicy> frames = std::nullopt) const;
    // Parks this Block inside a live graph and keeps driving it. The graph
    // does not activate a parked Block, so the returned Instance is what sets
    // its Pins and pulses it. Closing the Instance removes the Block again.
    [[nodiscard]] Result<Instance> SpawnIn(
        ObjectRef graph,
        std::optional<FramePolicy> frames = std::nullopt) const;
    [[nodiscard]] Result<Instance> SpawnIn(
        const Graph &graph,
        std::optional<FramePolicy> frames = std::nullopt) const;

private:
    Block(std::shared_ptr<Detail::SessionState> session,
          Prototype prototype)
        : m_Session(std::move(session)),
          m_State(std::make_shared<Detail::BlockState>(
              Detail::BlockDefinition(prototype))) {}

    template <class Handle, class Function>
    Result<Handle> Open(Function function, RunKind kind, ObjectRef owner,
                        const Selector *input,
                        std::optional<FramePolicy> frames) const;
    [[nodiscard]] Result<std::shared_ptr<const Detail::CompiledBlock>>
    Compile(bool requireDeclared = false) const;
    [[nodiscard]] Status Accept(const BML_BehaviorRunInfo &info,
                                RunKind kind) const;
    Detail::BlockDefinition &Change();

    std::shared_ptr<Detail::SessionState> m_Session;
    std::shared_ptr<Detail::BlockState> m_State;

    friend class Session;
    friend class Edit;
};


} // namespace BML::Behavior

#endif // BML_BEHAVIOR_BLOCK_HPP

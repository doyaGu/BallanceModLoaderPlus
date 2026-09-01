#include "Behavior/Edit.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include <gtest/gtest.h>

namespace {

using namespace BML::Behavior;

NativeRef Native(std::uint64_t id) {
    return {id, reinterpret_cast<const void *>(
                    static_cast<std::uintptr_t>(id))};
}

SlotInfo SlotOf(SlotKind kind, int index, std::string name,
                CKGUID type = CKGUID()) {
    return {kind, index, index, std::move(name), type, 4};
}

Layout Shape(CKDWORD flags = 0) {
    Layout layout;
    layout.BehaviorFlags = flags;
    layout.Slots = {
        SlotOf(SlotKind::Input, 0, "In"),
        SlotOf(SlotKind::Output, 0, "Out"),
        SlotOf(SlotKind::InputParameter, 0, "Value", CKPGUID_INT),
        SlotOf(SlotKind::OutputParameter, 0, "Result", CKPGUID_INT),
        SlotOf(SlotKind::Local, 0, "State", CKPGUID_INT),
    };
    return layout;
}

Layout RootShape() {
    Layout layout = Shape();
    layout.Slots[0].Name = "Start";
    layout.Slots[1].Name = "Done";
    return layout;
}

GraphModel Base(bool cycle = false) {
    GraphModel graph;
    graph.Nodes = {
        {100, {}, 0},
        {101, {}, 100},
        {102, {}, 100},
        {103, {}, 100},
    };
    if (cycle) {
        graph.Links = {
            {1, {}, {101, SlotKind::Output, 0},
                    {102, SlotKind::Input, 0}, 0},
            {2, {}, {102, SlotKind::Output, 0},
                    {101, SlotKind::Input, 0}, 0},
        };
    }
    return graph;
}

Edit MakeEdit() {
    return Edit({"mod", "edit"}, Native(100), RootShape());
}

std::shared_ptr<HookBlock::Binding> FakeTap() {
    return {reinterpret_cast<HookBlock::Binding *>(
                static_cast<std::uintptr_t>(1)),
            [](HookBlock::Binding *) {}};
}

TEST(BehaviorEdit, ResolvesAdditiveControlAndDataRelationsTogether) {
    Edit edit = MakeEdit();
    const Node a = edit.Use(Native(101), Shape());
    const Node b = edit.Use(Native(102), Shape());

    edit.Flow(edit.Entry("Start"), a.In());
    edit.Flow(a.Out(), b.In(), 2);
    edit.Flow(b.Out(), edit.Exit("Done"));
    edit.Bind(a.Pin("Value"), Value{});
    edit.Bind(b.Pin(), a.Pout());
    edit.Push(b.Pout(), a.Local());
    edit.Tap(a.Out(), FakeTap());

    CheckedEdit checked;
    ASSERT_TRUE(edit.Validate(Base(), checked));
    EXPECT_EQ(checked.Flows.size(), 3u);
    EXPECT_EQ(checked.Flows[1].Delay, 2);
    EXPECT_EQ(checked.Binds.size(), 2u);
    EXPECT_EQ(checked.Binds[0].Kind, BindKind::Literal);
    EXPECT_EQ(checked.Binds[1].Kind, BindKind::Direct);
    EXPECT_EQ(checked.Pushes.size(), 1u);
    EXPECT_EQ(checked.Taps.size(), 1u);
}

TEST(BehaviorEdit, RequiresEveryDeltaLinkInANewSameFrameCycle) {
    Edit rejected = MakeEdit();
    const Node a = rejected.Use(Native(101), Shape());
    const Node b = rejected.Use(Native(102), Shape());
    rejected.Flow(a.Out(), b.In(), 0, Cycle::Confirmed);
    rejected.Flow(b.Out(), a.In());

    CheckedEdit checked;
    Status status = rejected.Validate(Base(), checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::UnconfirmedSameFrameCycle);
    EXPECT_NE(status.Message.find("Flow #2"), std::string::npos);

    Edit accepted = MakeEdit();
    const Node acceptedA = accepted.Use(Native(101), Shape());
    const Node acceptedB = accepted.Use(Native(102), Shape());
    accepted.Flow(acceptedA.Out(), acceptedB.In(), 0, Cycle::Confirmed);
    accepted.Flow(acceptedB.Out(), acceptedA.In(), 0, Cycle::Confirmed);
    EXPECT_TRUE(accepted.Validate(Base(), checked));
}

TEST(BehaviorEdit, DelayBreaksSameFrameCycle) {
    Edit edit = MakeEdit();
    const Node a = edit.Use(Native(101), Shape());
    const Node b = edit.Use(Native(102), Shape());
    edit.Flow(a.Out(), b.In());
    edit.Flow(b.Out(), a.In(), 1);

    CheckedEdit checked;
    EXPECT_TRUE(edit.Validate(Base(), checked));
}

TEST(BehaviorEdit, ExistingCycleDoesNotBlockAnUnrelatedFlow) {
    Edit edit = MakeEdit();
    const Node c = edit.Use(Native(103), Shape());
    edit.Flow(edit.Entry(), c.In());

    CheckedEdit checked;
    EXPECT_TRUE(edit.Validate(Base(true), checked));
}

TEST(BehaviorEdit, ChecksEachDynamicInterfaceKindIndependently) {
    constexpr CKDWORD flags = CKBEHAVIOR_VARIABLEINPUTS |
                              CKBEHAVIOR_VARIABLEPARAMETEROUTPUTS |
                              CKBEHAVIOR_INTERNALLYCREATEDOUTPUTS |
                              CKBEHAVIOR_INTERNALLYCREATEDINPUTPARAMS;
    Edit accepted = MakeEdit();
    const Node node = accepted.Use(Native(101), Shape(flags));
    const Port input = accepted.AppendIn(node, "Again");
    const Port output = accepted.AppendPout(node, "Other", CKPGUID_INT);
    ASSERT_TRUE(input);
    accepted.Flow(accepted.Entry(), node.In("Again"));
    accepted.Push(node.Pout("Other"), node.Local());

    CheckedEdit checked;
    EXPECT_TRUE(accepted.Validate(Base(), checked));

    Edit internalOnly = MakeEdit();
    const Node fixed = internalOnly.Use(Native(101), Shape(flags));
    internalOnly.AppendOut(fixed, "Again");
    Status status = internalOnly.Validate(Base(), checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::InterfaceUnsupported);

    Edit pinInternalOnly = MakeEdit();
    const Node fixedPin = pinInternalOnly.Use(Native(101), Shape(flags));
    pinInternalOnly.AppendPin(fixedPin, "Again", CKPGUID_INT);
    status = pinInternalOnly.Validate(Base(), checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::InterfaceUnsupported);

    Edit local = MakeEdit();
    const Node noLocalFlag = local.Use(Native(101), Shape(0xffffffffu));
    local.AppendLocal(noLocalFlag, "Again", CKPGUID_INT);
    status = local.Validate(Base(), checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::InterfaceUnsupported);
}

TEST(BehaviorEdit, RejectsSharedAndPushCyclesBeforeMutation) {
    Edit shared = MakeEdit();
    const Node a = shared.Use(Native(101), Shape());
    const Node b = shared.Use(Native(102), Shape());
    shared.Share(a.Pin(), b.Pin());
    shared.Share(b.Pin(), a.Pin());
    CheckedEdit checked;
    Status status = shared.Validate(Base(), checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::SharedSourceCycle);

    Edit push = MakeEdit();
    const Node pushA = push.Use(Native(101), Shape());
    const Node pushB = push.Use(Native(102), Shape());
    push.Push(pushA.Pout(), pushB.Pout());
    push.Push(pushB.Pout(), pushA.Pout());
    status = push.Validate(Base(), checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::PushCycle);
}

TEST(BehaviorEdit, RejectsForeignNodesAndWrongEndpointDomains) {
    Edit foreign = MakeEdit();
    const Node outside = foreign.Use(Native(999), Shape());
    foreign.Flow(foreign.Entry(), outside.In());
    CheckedEdit checked;
    Status status = foreign.Validate(Base(), checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::InvalidGraphLocality);

    Edit wrong = MakeEdit();
    const Node node = wrong.Use(Native(101), Shape());
    wrong.Flow(node.In(), node.Out());
    status = wrong.Validate(Base(), checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::TypeMismatch);
}

TEST(BehaviorEdit, LeavesVirtoolsTypeCompatibilityToTheCKAdapter) {
    Edit edit = MakeEdit();
    Layout sourceShape = Shape();
    sourceShape.Slots[3].Type = CKPGUID_FLOAT;
    const Node source = edit.Use(Native(101), std::move(sourceShape));
    const Node target = edit.Use(Native(102), Shape());

    edit.Bind(target.Pin(), source.Pout());
    edit.Push(source.Pout(), target.Local());

    CheckedEdit checked;
    EXPECT_TRUE(edit.Validate(Base(), checked));
}

TEST(BehaviorEdit, RejectsCompetingDataRelationsWithinOneEdit) {
    Edit edit = MakeEdit();
    const Node a = edit.Use(Native(101), Shape());
    const Node b = edit.Use(Native(102), Shape());
    const Node c = edit.Use(Native(103), Shape());
    edit.Bind(a.Pin(), b.Pout());
    edit.Share(a.Pin(), c.Pin());

    CheckedEdit checked;
    Status status = edit.Validate(Base(), checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::InvalidState);

    Edit duplicatePush = MakeEdit();
    const Node source = duplicatePush.Use(Native(101), Shape());
    const Node destination = duplicatePush.Use(Native(102), Shape());
    duplicatePush.Push(source.Pout(), destination.Local());
    duplicatePush.Push(source.Pout(), destination.Local());
    status = duplicatePush.Validate(Base(), checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::InvalidState);
}

} // namespace

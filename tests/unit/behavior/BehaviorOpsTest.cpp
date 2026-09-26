#include "Behavior/Edit/Ops.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include <gtest/gtest.h>

namespace {

using namespace BML::Behavior::Internal;

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
        SlotOf(SlotKind::Setting, 1, "Mode", CKPGUID_INT),
    };
    return layout;
}

Layout RootShape() {
    Layout layout = Shape();
    layout.Kind = BehaviorKind::Graph;
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

Ops MakeEdit() {
    return Ops({"mod", "edit"}, Native(100), RootShape());
}

std::shared_ptr<HookBlock::Binding> FakeTap() {
    return {reinterpret_cast<HookBlock::Binding *>(
                static_cast<std::uintptr_t>(1)),
            [](HookBlock::Binding *) {}};
}

TEST(BehaviorOps, ResolvesAdditiveControlAndDataRelationsTogether) {
    Ops edit = MakeEdit();
    const Node a = edit.Use(Native(101), Shape());
    const Node b = edit.Use(Native(102), Shape());

    edit.Flow(edit.Entry("Start"), a.In());
    edit.Flow(a.Out(), b.In(), 2);
    edit.Flow(b.Out(), edit.Exit("Done"));
    edit.Bind(a.Pin("Value"), Value{});
    edit.Bind(b.Pin(), a.Pout());
    edit.Push(b.Pout(), a.Local());
    edit.Tap(a.Out(), FakeTap());

    CheckedOps checked;
    ASSERT_TRUE(edit.Validate(Base(), checked));
    EXPECT_EQ(checked.Flows.size(), 3u);
    EXPECT_EQ(checked.Flows[1].Delay, 2);
    EXPECT_EQ(checked.Binds.size(), 2u);
    EXPECT_EQ(checked.Binds[0].Kind, BindKind::Literal);
    EXPECT_EQ(checked.Binds[1].Kind, BindKind::Direct);
    EXPECT_EQ(checked.Pushes.size(), 1u);
    EXPECT_EQ(checked.Taps.size(), 1u);
}

TEST(BehaviorOps, UsesConfiguredParameterTypesOnAnAuthoredBlock) {
    Ops edit = MakeEdit();
    BlockSpec block(CKGUID(1, 2));
    block.PinType(Slot::At(SlotKind::InputParameter, 0), CKPGUID_BOOL)
        .PoutType(Slot::At(SlotKind::OutputParameter, 0), CKPGUID_BOOL);
    const Node identity = edit.Add(std::move(block), Shape());
    edit.Bind(identity.Pin(0), Value::From(CKPGUID_BOOL, true));

    CheckedOps checked;
    const Status status = edit.Validate(Base(), checked);
    ASSERT_TRUE(status) << status.Message;
    ASSERT_EQ(checked.Binds.size(), 1u);
    EXPECT_EQ(checked.Binds[0].Target.Slot.Type, CKPGUID_BOOL);
}

TEST(BehaviorOps, KeepsOneTypePerVariableParameterSelector) {
    BlockSpec block(CKGUID(1, 2));
    block.PinType(Slot::At(SlotKind::InputParameter, 0), CKPGUID_BOOL)
        .PinType(Slot::At(SlotKind::InputParameter, 0), CKPGUID_FLOAT)
        .PoutType(Slot::Named(SlotKind::OutputParameter, "Result"),
                  CKPGUID_BOOL);

    ASSERT_EQ(block.PinTypes().size(), 1u);
    EXPECT_EQ(block.PinTypes().front().Type, CKPGUID_FLOAT);
    ASSERT_EQ(block.PoutTypes().size(), 1u);
    EXPECT_EQ(block.PoutTypes().front().Target.Name, "Result");
}

TEST(BehaviorOps, RequiresEveryDeltaLinkInANewSameFrameCycle) {
    Ops rejected = MakeEdit();
    const Node a = rejected.Use(Native(101), Shape());
    const Node b = rejected.Use(Native(102), Shape());
    rejected.Flow(a.Out(), b.In(), 0, Cycle::Confirmed);
    rejected.Flow(b.Out(), a.In());

    CheckedOps checked;
    Status status = rejected.Validate(Base(), checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::UnconfirmedSameFrameCycle);
    EXPECT_NE(status.Message.find("Flow #2"), std::string::npos);

    Ops accepted = MakeEdit();
    const Node acceptedA = accepted.Use(Native(101), Shape());
    const Node acceptedB = accepted.Use(Native(102), Shape());
    accepted.Flow(acceptedA.Out(), acceptedB.In(), 0, Cycle::Confirmed);
    accepted.Flow(acceptedB.Out(), acceptedA.In(), 0, Cycle::Confirmed);
    EXPECT_TRUE(accepted.Validate(Base(), checked));
}

TEST(BehaviorOps, DelayBreaksSameFrameCycle) {
    Ops edit = MakeEdit();
    const Node a = edit.Use(Native(101), Shape());
    const Node b = edit.Use(Native(102), Shape());
    edit.Flow(a.Out(), b.In());
    edit.Flow(b.Out(), a.In(), 1);

    CheckedOps checked;
    EXPECT_TRUE(edit.Validate(Base(), checked));
}

TEST(BehaviorOps, ExistingCycleDoesNotBlockAnUnrelatedFlow) {
    Ops edit = MakeEdit();
    const Node c = edit.Use(Native(103), Shape());
    edit.Flow(edit.Entry(), c.In());

    CheckedOps checked;
    EXPECT_TRUE(edit.Validate(Base(true), checked));
}

TEST(BehaviorOps, RemovesAnIdleChildWhileUnrelatedGraphWorkIsActive) {
    Ops edit = MakeEdit();
    const Node removed = edit.Use(Native(101), Shape());
    edit.Remove(removed);

    CheckedOps checked;
    ASSERT_TRUE(edit.Validate(Base(), checked));
    ASSERT_EQ(checked.Removals.size(), 1u);
    EXPECT_EQ(checked.Removals[0].Target, removed);

    GraphModel activeGraph = Base();
    activeGraph.Nodes.front().Active = true;
    EXPECT_TRUE(edit.Validate(activeGraph, checked));

    GraphModel activeNode = Base();
    activeNode.Nodes[1].Active = true;
    EXPECT_EQ(edit.Validate(activeNode, checked).Code, Error::Busy);

    GraphModel activePort = Base();
    activePort.Nodes[1].Ports.push_back(
        {SlotKind::Input, 0, 0, 0, CKGUID(), false, "In", true});
    EXPECT_EQ(edit.Validate(activePort, checked).Code, Error::Busy);
}

TEST(BehaviorOps, RejectsRemovalOfRootOwnedOrReusedNodes) {
    CheckedOps checked;

    Ops root = MakeEdit();
    root.Remove(root.Graph());
    EXPECT_EQ(root.Validate(Base(), checked).Code, Error::InvalidState);

    Ops authored = MakeEdit();
    const Node added = authored.Add(BlockSpec(CKGUID(1, 2)), Shape());
    authored.Remove(added);
    EXPECT_EQ(authored.Validate(Base(), checked).Code, Error::InvalidState);

    Ops reused = MakeEdit();
    const Node removed = reused.Use(Native(101), Shape());
    reused.Remove(removed);
    reused.Flow(removed.Out(), reused.Exit());
    EXPECT_EQ(reused.Validate(Base(), checked).Code, Error::InvalidState);
}

TEST(BehaviorOps, AllowsAdjacentReplaceAndRemoveInOneEdit) {
    GraphModel base = Base();
    base.Links = {
        {1, {}, {101, SlotKind::Output, 0},
                {102, SlotKind::Input, 0}, 0},
    };
    Ops edit = MakeEdit();
    const Node removed = edit.Use(Native(101), Shape());
    const Node original = edit.Use(Native(102), Shape());
    const Node replacement = edit.Add(BlockSpec(CKGUID(1, 2)), Shape());
    edit.Replace(original, replacement);
    edit.Remove(removed);

    CheckedOps checked;
    const Status status = edit.Validate(base, checked);
    ASSERT_TRUE(status) << status.Message;
    EXPECT_EQ(checked.Replacements.size(), 1u);
    EXPECT_EQ(checked.Removals.size(), 1u);
}

TEST(BehaviorOps, DetectsACycleClosedThroughExistingSameFrameLinks) {
    GraphModel base = Base();
    base.Links = {
        {1, {}, {101, SlotKind::Output, 0}, {102, SlotKind::Input, 0}, 0},
        {2, {}, {102, SlotKind::Output, 0}, {103, SlotKind::Input, 0}, 0},
    };

    Ops rejected = MakeEdit();
    const Node a = rejected.Use(Native(101), Shape());
    const Node c = rejected.Use(Native(103), Shape());
    rejected.Flow(c.Out(), a.In());

    CheckedOps checked;
    Status status = rejected.Validate(base, checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::UnconfirmedSameFrameCycle);

    Ops delayed = MakeEdit();
    const Node delayedA = delayed.Use(Native(101), Shape());
    const Node delayedC = delayed.Use(Native(103), Shape());
    delayed.Flow(delayedC.Out(), delayedA.In(), 1);
    EXPECT_TRUE(delayed.Validate(base, checked));

    Ops confirmed = MakeEdit();
    const Node confirmedA = confirmed.Use(Native(101), Shape());
    const Node confirmedC = confirmed.Use(Native(103), Shape());
    confirmed.Flow(confirmedC.Out(), confirmedA.In(), 0, Cycle::Confirmed);
    EXPECT_TRUE(confirmed.Validate(base, checked));
}

TEST(BehaviorOps, RejectsATapOnAGraphEntryBeforeMutation) {
    Ops edit = MakeEdit();
    edit.Tap(edit.Entry("Start"), FakeTap());

    CheckedOps checked;
    Status status = edit.Validate(Base(), checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::TypeMismatch);
}

TEST(BehaviorOps, AppendsPortsWithoutTreatingVariableFlagsAsPermissions) {
    Ops accepted = MakeEdit();
    const Node node = accepted.Use(
        Native(101),
        Shape(CKBEHAVIOR_VARIABLEINPUTS |
              CKBEHAVIOR_VARIABLEOUTPUTS |
              CKBEHAVIOR_VARIABLEPARAMETERINPUTS |
              CKBEHAVIOR_VARIABLEPARAMETEROUTPUTS));
    const Port input = accepted.AppendIn(node, "Again");
    const Port output = accepted.AppendPout(node, "Other", CKPGUID_INT);
    ASSERT_TRUE(input);
    ASSERT_TRUE(output);
    accepted.AppendOut(node, "Done");
    accepted.AppendPin(node, "Extra", CKPGUID_INT);
    accepted.Flow(accepted.Entry(), node.In("Again"));
    accepted.Push(node.Pout("Other"), node.Local());

    CheckedOps checked;
    EXPECT_TRUE(accepted.Validate(Base(), checked));

    Ops fixed = MakeEdit();
    const Node fixedNode = fixed.Use(Native(101), Shape());
    fixed.AppendOut(fixedNode, "Done");
    Status status = fixed.Validate(Base(), checked);
    EXPECT_TRUE(status) << status.Message;

    Ops managedAlone = MakeEdit();
    const Node managedNode = managedAlone.Use(
        Native(101),
        Shape(CKBEHAVIOR_VARIABLEOUTPUTS |
              CKBEHAVIOR_INTERNALLYCREATEDINPUTPARAMS));
    managedAlone.AppendPin(managedNode, "Other", CKPGUID_INT);
    status = managedAlone.Validate(Base(), checked);
    EXPECT_TRUE(status) << status.Message;

    Ops paired = MakeEdit();
    const Node pairedNode = paired.Use(
        Native(101),
        Shape(CKBEHAVIOR_VARIABLEOUTPUTS |
              CKBEHAVIOR_INTERNALLYCREATEDINPUTPARAMS));
    paired.AppendOut(pairedNode, "Done");
    paired.AppendPin(pairedNode, "Other", CKPGUID_INT);
    status = paired.Validate(Base(), checked);
    EXPECT_TRUE(status) << status.Message;

    Ops local = MakeEdit();
    const Node everyFlag = local.Use(Native(101), Shape(0xffffffffu));
    local.AppendLocal(everyFlag, "Again", CKPGUID_INT);
    status = local.Validate(Base(), checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::InterfaceUnsupported);
}

TEST(BehaviorOps, DefersCallbackOwnedPortsUntilInterfaceReconciliation) {
    Ops edit = MakeEdit();
    const Node node = edit.Use(
        Native(101),
        Shape(CKBEHAVIOR_VARIABLEOUTPUTS |
              CKBEHAVIOR_INTERNALLYCREATEDINPUTPARAMS));
    (void) edit.AppendOut(node, "Out 2");
    const Port pin{
        node.Value,
        Slot::Named(SlotKind::InputParameter, "Pin 2", CKPGUID_INT)};
    edit.Bind(pin, Value::From(CKPGUID_INT, 7));

    CheckedOps checked;
    const Status status = edit.Validate(Base(), checked);
    ASSERT_TRUE(status) << status.Message;
    ASSERT_EQ(checked.Binds.size(), 1u);
    EXPECT_TRUE(checked.Binds[0].Target.Appended);
    EXPECT_TRUE(checked.Binds[0].Target.Deferred);
    EXPECT_EQ(checked.Binds[0].Target.Interface, 0u);
    EXPECT_EQ(checked.Binds[0].Target.Selector.Name, "Pin 2");
}

TEST(BehaviorOps, DistinguishesExistingAndAppendedDynamicPorts) {
    Layout dynamic = Shape(CKBEHAVIOR_VARIABLEPARAMETERINPUTS);
    dynamic.Slots[2].Dynamic = true;

    Ops edit = MakeEdit();
    const Node target = edit.Use(Native(101), std::move(dynamic));
    const Node source = edit.Use(Native(102), Shape());
    const Port appended = edit.AppendPin(target, "Other", CKPGUID_INT);
    const Port another = edit.AppendPin(target, "Other", CKPGUID_INT);
    edit.Share(target.Pin("Value"), source.Pin());
    edit.Share(appended, source.Pin());
    edit.Share(another, source.Pin());

    CheckedOps checked;
    ASSERT_TRUE(edit.Validate(Base(), checked));
    ASSERT_EQ(checked.Binds.size(), 3u);
    EXPECT_FALSE(checked.Binds[0].Target.Appended);
    EXPECT_TRUE(checked.Binds[1].Target.Appended);
    EXPECT_TRUE(checked.Binds[2].Target.Appended);
    EXPECT_NE(appended.Interface, 0u);
    EXPECT_NE(another.Interface, 0u);
    EXPECT_NE(appended.Interface, another.Interface);
    EXPECT_EQ(checked.Binds[1].Target.Interface, appended.Interface);
    EXPECT_EQ(checked.Binds[2].Target.Interface, another.Interface);
}

TEST(BehaviorOps, WritesAValueIntoALocalButNeverIntoASetting) {
    // CK2 keeps a written value in the parameter itself, which is how a Local
    // holds its state, so a Local takes one. A Setting is refused: editing one
    // can rebuild the layout of the block, so it belongs to how the Block was
    // created and not to a later write.
    Ops local = MakeEdit();
    const Node node = local.Use(Native(101), Shape());
    local.Bind(node.Local("State"), Value{});
    CheckedOps checked;
    ASSERT_TRUE(local.Validate(Base(), checked));
    ASSERT_EQ(checked.Binds.size(), 1u);
    EXPECT_EQ(checked.Binds[0].Kind, BindKind::Literal);
    EXPECT_EQ(checked.Binds[0].Target.Slot.Kind, SlotKind::Local);

    Ops setting = MakeEdit();
    const Node target = setting.Use(Native(101), Shape());
    // The internal Node has no Setting accessor, because a Setting is never a
    // destination a later write can name. This test asks for one anyway to
    // prove the validator refuses it.
    setting.Bind(Port{target.Value, Slot::Named(SlotKind::Setting, "Mode")},
                 Value{});
    Status status = setting.Validate(Base(), checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::TypeMismatch);

    // A relation still needs somewhere to read from, so only a Pin or the
    // Target accepts one.
    Ops relation = MakeEdit();
    const Node source = relation.Use(Native(101), Shape());
    const Node destination = relation.Use(Native(102), Shape());
    relation.Bind(destination.Local("State"), source.Pout());
    status = relation.Validate(Base(), checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::TypeMismatch);
}

TEST(BehaviorOps, SetsTheValueReadByAnExistingParameter) {
    Ops edit = MakeEdit();
    const Node node = edit.Use(Native(101), Shape());
    edit.Set(node.Pin("Value"), Value::From(CKPGUID_INT, 1));
    edit.Set(node.Pout("Result"), Value::From(CKPGUID_INT, 2));
    edit.Set(node.Local("State"), Value::From(CKPGUID_INT, 3));

    CheckedOps checked;
    ASSERT_TRUE(edit.Validate(Base(), checked));
    ASSERT_EQ(checked.Sets.size(), 3u);
    EXPECT_TRUE(checked.Binds.empty());
    EXPECT_EQ(checked.Sets[0].Target.Slot.Kind, SlotKind::InputParameter);
    EXPECT_EQ(checked.Sets[1].Target.Slot.Kind, SlotKind::OutputParameter);
    EXPECT_EQ(checked.Sets[2].Target.Slot.Kind, SlotKind::Local);

    Ops conflicting = MakeEdit();
    const Node target = conflicting.Use(Native(101), Shape());
    conflicting.Set(target.Pin("Value"), Value::From(CKPGUID_INT, 1));
    conflicting.Bind(target.Pin("Value"), Value::From(CKPGUID_INT, 2));
    const Status status = conflicting.Validate(Base(), checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::InvalidState);
}

TEST(BehaviorOps, RefusesAWrittenValueAndAPushOnOneParameter) {
    Ops edit = MakeEdit();
    const Node source = edit.Use(Native(101), Shape());
    const Node destination = edit.Use(Native(102), Shape());
    edit.Bind(destination.Local("State"), Value{});
    edit.Push(source.Pout(), destination.Local("State"));

    CheckedOps checked;
    const Status status = edit.Validate(Base(), checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::InvalidState);
}

TEST(BehaviorOps, RejectsSharedAndPushCyclesBeforeMutation) {
    Ops shared = MakeEdit();
    const Node a = shared.Use(Native(101), Shape());
    const Node b = shared.Use(Native(102), Shape());
    shared.Share(a.Pin(), b.Pin());
    shared.Share(b.Pin(), a.Pin());
    CheckedOps checked;
    Status status = shared.Validate(Base(), checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::SharedSourceCycle);

    Ops push = MakeEdit();
    const Node pushA = push.Use(Native(101), Shape());
    const Node pushB = push.Use(Native(102), Shape());
    push.Push(pushA.Pout(), pushB.Pout());
    push.Push(pushB.Pout(), pushA.Pout());
    status = push.Validate(Base(), checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::PushCycle);
}

TEST(BehaviorOps, RejectsForeignNodesAndWrongEndpointDomains) {
    Ops foreign = MakeEdit();
    const Node outside = foreign.Use(Native(999), Shape());
    foreign.Flow(foreign.Entry(), outside.In());
    CheckedOps checked;
    Status status = foreign.Validate(Base(), checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::InvalidGraphLocality);

    Ops wrong = MakeEdit();
    const Node node = wrong.Use(Native(101), Shape());
    wrong.Flow(node.In(), node.Out());
    status = wrong.Validate(Base(), checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::TypeMismatch);
}

TEST(BehaviorOps, LeavesVirtoolsTypeCompatibilityToTheCKAdapter) {
    Ops edit = MakeEdit();
    Layout sourceShape = Shape();
    sourceShape.Slots[3].Type = CKPGUID_FLOAT;
    const Node source = edit.Use(Native(101), std::move(sourceShape));
    const Node target = edit.Use(Native(102), Shape());

    edit.Bind(target.Pin(), source.Pout());
    edit.Push(source.Pout(), target.Local());

    CheckedOps checked;
    EXPECT_TRUE(edit.Validate(Base(), checked));
}

TEST(BehaviorOps, RejectsCompetingDataRelationsWithinOneEdit) {
    Ops edit = MakeEdit();
    const Node a = edit.Use(Native(101), Shape());
    const Node b = edit.Use(Native(102), Shape());
    const Node c = edit.Use(Native(103), Shape());
    edit.Bind(a.Pin(), b.Pout());
    edit.Share(a.Pin(), c.Pin());

    CheckedOps checked;
    Status status = edit.Validate(Base(), checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::InvalidState);

    Ops duplicatePush = MakeEdit();
    const Node source = duplicatePush.Use(Native(101), Shape());
    const Node destination = duplicatePush.Use(Native(102), Shape());
    duplicatePush.Push(source.Pout(), destination.Local());
    duplicatePush.Push(source.Pout(), destination.Local());
    status = duplicatePush.Validate(Base(), checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::InvalidState);
}

TEST(BehaviorOps, SelectsAnExactLinkForSpliceWithoutEndpointGuessing) {
    GraphModel graph = Base();
    graph.Links = {
        {1, {7, 31, 9}, {101, SlotKind::Output, 0},
         {102, SlotKind::Input, 0}, 1},
        {2, {7, 32, 9}, {101, SlotKind::Output, 0},
         {102, SlotKind::Input, 0}, 3},
    };

    Ops edit = MakeEdit();
    const Node block = edit.Use(Native(103), Shape());
    const Link target = edit.Use(ObjectRef{7, 32, 9});
    edit.Splice(target, block,
                {{OrderKind::After, {"other", "patch"}}});

    CheckedOps checked;
    ASSERT_TRUE(edit.Validate(graph, checked));
    ASSERT_EQ(checked.Splices.size(), 1u);
    EXPECT_EQ(checked.Splices[0].Target.Anchor, (ObjectRef{7, 32, 9}));
    EXPECT_EQ(checked.Splices[0].Target.Delay, 3);
    EXPECT_EQ(checked.Splices[0].Input.Owner, block);
    EXPECT_EQ(checked.Splices[0].Output.Owner, block);
}

TEST(BehaviorOps, RejectsAStaleLinkAndMismatchedSplicePorts) {
    GraphModel graph = Base();
    graph.Links = {
        {1, {7, 31, 9}, {101, SlotKind::Output, 0},
         {102, SlotKind::Input, 0}, 0},
    };
    Ops stale = MakeEdit();
    const Node node = stale.Use(Native(103), Shape());
    stale.Splice(stale.Use(ObjectRef{7, 99, 9}), node);
    CheckedOps checked;
    Status status = stale.Validate(graph, checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::LinkNotFound);

    Ops mismatch = MakeEdit();
    const Node first = mismatch.Use(Native(101), Shape());
    const Node second = mismatch.Use(Native(102), Shape());
    mismatch.Splice(mismatch.Use(ObjectRef{7, 31, 9}), first.In(),
                    second.Out());
    status = mismatch.Validate(graph, checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::TypeMismatch);
}

TEST(BehaviorOps, KeepsSamePatchSplicesInDeclarationOrder) {
    GraphModel graph = Base();
    graph.Links = {
        {1, {7, 31, 9}, {101, SlotKind::Output, 0},
         {102, SlotKind::Input, 0}, 0},
    };
    Ops edit = MakeEdit();
    const Node first = edit.Use(Native(101), Shape());
    const Node second = edit.Use(Native(102), Shape());
    const Link target = edit.Use(ObjectRef{7, 31, 9});
    edit.Splice(target, first);
    edit.Splice(target, second);

    CheckedOps checked;
    ASSERT_TRUE(edit.Validate(graph, checked));
    ASSERT_EQ(checked.Splices.size(), 2u);
    EXPECT_LT(checked.Splices[0].Ordinal, checked.Splices[1].Ordinal);
}

TEST(BehaviorOps, RejectsConflictingOrSelfReferentialSpliceOrder) {
    GraphModel graph = Base();
    graph.Links = {
        {1, {7, 31, 9}, {101, SlotKind::Output, 0},
         {102, SlotKind::Input, 0}, 0},
    };
    Ops mismatch = MakeEdit();
    const Node first = mismatch.Use(Native(101), Shape());
    const Node second = mismatch.Use(Native(102), Shape());
    const Link target = mismatch.Use(ObjectRef{7, 31, 9});
    mismatch.Splice(target, first,
                    {{OrderKind::Before, {"other", "patch"}}});
    mismatch.Splice(target, second,
                    {{OrderKind::After, {"other", "patch"}}});
    CheckedOps checked;
    Status status = mismatch.Validate(graph, checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::InvalidState);

    Ops self = MakeEdit();
    const Node node = self.Use(Native(101), Shape());
    self.Splice(self.Use(ObjectRef{7, 31, 9}), node,
                {{OrderKind::Before, {"mod", "edit"}}});
    status = self.Validate(graph, checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::OverlayOrderCycle);
}

TEST(BehaviorOps, SendsAnExactLinkToANodeInOrAGraphExit) {
    GraphModel graph = Base();
    graph.Links = {
        {1, {7, 31, 9}, {101, SlotKind::Output, 0},
         {102, SlotKind::Input, 0}, 1},
        {2, {7, 32, 9}, {101, SlotKind::Output, 0},
         {102, SlotKind::Input, 0}, 3},
    };

    Ops edit = MakeEdit();
    const Node block = edit.Use(Native(103), Shape());
    edit.Redirect(edit.Use(ObjectRef{7, 32, 9}), block.In(),
                  {{OrderKind::After, {"other", "patch"}}});

    CheckedOps checked;
    ASSERT_TRUE(edit.Validate(graph, checked));
    ASSERT_EQ(checked.Redirects.size(), 1u);
    EXPECT_EQ(checked.Redirects[0].Target.Anchor, (ObjectRef{7, 32, 9}));
    EXPECT_EQ(checked.Redirects[0].Target.Delay, 3);
    EXPECT_EQ(checked.Redirects[0].Sink.Owner, block);
    ASSERT_EQ(checked.Redirects[0].Ordering.size(), 1u);

    Ops boundary = MakeEdit();
    boundary.Redirect(boundary.Use(ObjectRef{7, 31, 9}), boundary.Exit("Done"));
    ASSERT_TRUE(boundary.Validate(graph, checked));
    ASSERT_EQ(checked.Redirects.size(), 1u);
    EXPECT_EQ(checked.Redirects[0].Sink.Owner, boundary.Graph());
}

TEST(BehaviorOps, RejectsARedirectToAnOutAndARedirectDeclaredTwice) {
    GraphModel graph = Base();
    graph.Links = {
        {1, {7, 31, 9}, {101, SlotKind::Output, 0},
         {102, SlotKind::Input, 0}, 0},
    };

    Ops wrongWay = MakeEdit();
    const Node node = wrongWay.Use(Native(103), Shape());
    wrongWay.Redirect(wrongWay.Use(ObjectRef{7, 31, 9}), node.Out());
    CheckedOps checked;
    Status status = wrongWay.Validate(graph, checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::TypeMismatch);

    Ops twice = MakeEdit();
    const Node first = twice.Use(Native(101), Shape());
    const Node second = twice.Use(Native(102), Shape());
    const Link target = twice.Use(ObjectRef{7, 31, 9});
    twice.Redirect(target, first.In());
    twice.Redirect(target, second.In());
    status = twice.Validate(graph, checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::RedirectConflict);

    Ops stale = MakeEdit();
    const Node missing = stale.Use(Native(103), Shape());
    stale.Redirect(stale.Use(ObjectRef{7, 99, 9}), missing.In());
    status = stale.Validate(graph, checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::LinkNotFound);
}

TEST(BehaviorOps, MakesASpliceAndARedirectOnOneLinkShareTheOrdering) {
    GraphModel graph = Base();
    graph.Links = {
        {1, {7, 31, 9}, {101, SlotKind::Output, 0},
         {102, SlotKind::Input, 0}, 0},
    };

    Ops shared = MakeEdit();
    const Node block = shared.Use(Native(101), Shape());
    const Node moved = shared.Use(Native(102), Shape());
    const Link target = shared.Use(ObjectRef{7, 31, 9});
    shared.Splice(target, block, {{OrderKind::After, {"other", "patch"}}});
    shared.Redirect(target, moved.In(), {{OrderKind::After, {"other", "patch"}}});
    CheckedOps checked;
    ASSERT_TRUE(shared.Validate(graph, checked));
    EXPECT_EQ(checked.Splices.size(), 1u);
    ASSERT_EQ(checked.Redirects.size(), 1u);
    EXPECT_EQ(checked.Redirects[0].Ordering, checked.Splices[0].Ordering);

    Ops conflicting = MakeEdit();
    const Node other = conflicting.Use(Native(101), Shape());
    const Node destination = conflicting.Use(Native(102), Shape());
    const Link link = conflicting.Use(ObjectRef{7, 31, 9});
    conflicting.Splice(link, other, {{OrderKind::Before, {"other", "patch"}}});
    conflicting.Redirect(link, destination.In(),
                         {{OrderKind::After, {"other", "patch"}}});
    const Status status = conflicting.Validate(graph, checked);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::InvalidState);
}

TEST(BehaviorOps, ReconnectsOneExactLinkWithoutChangingItsDelay) {
    GraphModel graph = Base();
    graph.Links = {
        {31, {7, 31, 9}, {101, SlotKind::Output, 0},
         {102, SlotKind::Input, 0}, 3, 2},
    };
    Ops edit = MakeEdit();
    const Node source = edit.Use(Native(103), Shape());
    const Link link = edit.Use(ObjectRef{7, 31, 9});
    edit.Reconnect(link, source.Out(), edit.Exit("Done"));

    CheckedOps checked;
    const Status status = edit.Validate(graph, checked);
    ASSERT_TRUE(status) << status.Message;
    ASSERT_EQ(checked.Reconnections.size(), 1u);
    EXPECT_EQ(checked.Reconnections[0].Target.Anchor,
              (ObjectRef{7, 31, 9}));
    EXPECT_EQ(checked.Reconnections[0].Target.Delay, 3);
    EXPECT_EQ(checked.Reconnections[0].Source.Owner, source);
    EXPECT_EQ(checked.Reconnections[0].Sink.Owner, edit.Graph());
}

TEST(BehaviorOps, RejectsAmbiguousOrIllTypedReconnects) {
    GraphModel graph = Base();
    graph.Links = {
        {31, {7, 31, 9}, {101, SlotKind::Output, 0},
         {102, SlotKind::Input, 0}, 0},
    };
    CheckedOps checked;

    Ops twice = MakeEdit();
    const Node source = twice.Use(Native(103), Shape());
    const Link link = twice.Use(ObjectRef{7, 31, 9});
    twice.Reconnect(link, source.Out(), twice.Exit());
    twice.Reconnect(link, source.Out(), twice.Exit());
    EXPECT_EQ(twice.Validate(graph, checked).Code, Error::RedirectConflict);

    Ops wrongWay = MakeEdit();
    const Node node = wrongWay.Use(Native(103), Shape());
    wrongWay.Reconnect(wrongWay.Use(ObjectRef{7, 31, 9}),
                       node.In(), node.Out());
    EXPECT_EQ(wrongWay.Validate(graph, checked).Code, Error::TypeMismatch);

    Ops mixed = MakeEdit();
    const Node destination = mixed.Use(Native(103), Shape());
    const Link mixedLink = mixed.Use(ObjectRef{7, 31, 9});
    mixed.Redirect(mixedLink, destination.In());
    mixed.Reconnect(mixedLink, destination.Out(), mixed.Exit());
    EXPECT_EQ(mixed.Validate(graph, checked).Code, Error::RedirectConflict);
}

TEST(BehaviorOps, RequiresConfirmationForACycleCreatedByReconnect) {
    GraphModel graph = Base();
    graph.Links = {
        {31, {7, 31, 9}, {101, SlotKind::Output, 0},
         {102, SlotKind::Input, 0}, 0},
        {32, {7, 32, 9}, {103, SlotKind::Output, 0},
         {101, SlotKind::Input, 0}, 0},
    };

    Ops rejected = MakeEdit();
    const Node b = rejected.Use(Native(102), Shape());
    rejected.Reconnect(rejected.Use(ObjectRef{7, 32, 9}),
                       b.Out(), rejected.Use(Native(101), Shape()).In());
    CheckedOps checked;
    EXPECT_EQ(rejected.Validate(graph, checked).Code,
              Error::UnconfirmedSameFrameCycle);

    Ops confirmed = MakeEdit();
    const Node confirmedB = confirmed.Use(Native(102), Shape());
    const Node confirmedA = confirmed.Use(Native(101), Shape());
    confirmed.Reconnect(confirmed.Use(ObjectRef{7, 32, 9}),
                        confirmedB.Out(), confirmedA.In(),
                        Cycle::Confirmed);
    const Status status = confirmed.Validate(graph, checked);
    EXPECT_TRUE(status) << status.Message;
}

} // namespace

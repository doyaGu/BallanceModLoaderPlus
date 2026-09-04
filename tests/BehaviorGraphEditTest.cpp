#include "Behavior/GraphEdit.h"

#include <cstdint>
#include <cstring>
#include <map>
#include <utility>

#include <gtest/gtest.h>

namespace {

using namespace BML::Behavior;

ObjectRef Ref(std::uint32_t slot, std::uint32_t generation = 1) {
    return {3, slot, generation};
}

NativeRef Native(std::uint64_t id) {
    return {id, reinterpret_cast<const void *>(
                    static_cast<std::uintptr_t>(id))};
}

GraphPort In(int index = 0, std::string name = "In") {
    return {SlotKind::Input, index, 0, std::move(name)};
}

GraphPort Out(int index = 0, std::string name = "Out") {
    return {SlotKind::Output, index, 0, std::move(name)};
}

SlotInfo SlotOf(SlotKind kind, int index, std::string name) {
    return {kind, index, index, std::move(name), CKGUID(), 0};
}

GraphNode NodeOf(std::uint64_t id, ObjectRef object, std::uint64_t parent,
                 CKGUID prototype, std::string name,
                 std::vector<GraphPort> ports) {
    GraphNode node;
    node.Id = id;
    node.Object = object;
    node.Parent = parent;
    node.Prototype = prototype;
    node.Name = std::move(name);
    node.Ports = std::move(ports);
    return node;
}

Layout Shape(std::string input = "In", std::string output = "Out",
             CKDWORD flags = 0) {
    Layout layout;
    layout.BehaviorFlags = flags;
    layout.Slots = {
        SlotOf(SlotKind::Input, 0, std::move(input)),
        SlotOf(SlotKind::Output, 0, std::move(output)),
        SlotOf(SlotKind::InputParameter, 0, "Value"),
        SlotOf(SlotKind::OutputParameter, 0, "Result"),
        SlotOf(SlotKind::Local, 0, "State"),
    };
    layout.Slots[2].Type = CKPGUID_INT;
    layout.Slots[3].Type = CKPGUID_INT;
    layout.Slots[4].Type = CKPGUID_INT;
    return layout;
}

int Noop(const CKBehaviorContext *, void *) {
    return CKBR_OK;
}

std::shared_ptr<HookBlock::Binding> FakeTap() {
    return {reinterpret_cast<HookBlock::Binding *>(
                static_cast<std::uintptr_t>(1)),
            [](HookBlock::Binding *) {}};
}

GraphModel Model(std::uint32_t generation = 1,
                 std::uint64_t offset = 0) {
    const std::uint64_t root = 100 + offset;
    const std::uint64_t wait = 101 + offset;
    const std::uint64_t sink = 102 + offset;
    GraphModel graph;
    graph.Root = Ref(static_cast<std::uint32_t>(root), generation);
    graph.Generation = generation;
    graph.Fingerprint = 700 + offset;
    graph.Nodes = {
        NodeOf(root, graph.Root, 0, CKGUID(), "Gameplay_Events",
               {In(0, "Start"), Out(0, "Done")}),
        NodeOf(wait, Ref(static_cast<std::uint32_t>(wait), generation), root,
               CKGUID(0x1111, 1), "Wait Message", {In(), Out()}),
        NodeOf(sink, Ref(static_cast<std::uint32_t>(sink), generation), root,
               CKGUID(0x2222, 2), "set Resetpoint", {In(), Out()}),
    };
    graph.Links = {
        {1, Ref(static_cast<std::uint32_t>(201 + offset), generation),
         {wait, SlotKind::Output, 0}, {sink, SlotKind::Input, 0}, 0},
    };
    return graph;
}

class FakeCompiler final : public GraphEdit::Compiler {
public:
    explicit FakeCompiler(GraphModel model) : Base(std::move(model)) {}

    Status Begin(const PatchKey &patch, const ObjectRef &graph,
                 Edit &out, GraphModel &base) override {
        ++Begins;
        if (graph != Base.Root)
            return {Error::InvalidGraphLocality, CKERR_INVALIDOBJECT,
                    CKBR_PARAMETERERROR, "wrong graph"};
        const GraphNode &root = Base.Nodes.front();
        out = Edit(patch, Native(root.Id), Shape("Start", "Done"));
        base = Base;
        return {};
    }

    Status UseNode(Edit &edit, const ObjectRef &node, Node &out) override {
        UsedNodes.push_back(node);
        out = edit.Use(Native(node.Slot), Shape("In", "Out", NodeFlags));
        return {};
    }

    Status UseLink(Edit &edit, const ObjectRef &link, Link &out) override {
        UsedLinks.push_back(link);
        out = edit.Use(link);
        return {};
    }

    Status Add(Edit &edit, CKGUID prototype,
               const GraphEdit::SettingStages &settings,
               Node &out) override {
        ++Adds;
        AddedPrototypes.push_back(prototype);
        // Spec is a world-bound value, so this world-free fake only records
        // what the intent asked for.
        AddedSettings = settings;
        out = edit.Add(Spec(prototype), Shape("In", "Out", AddedFlags));
        return {};
    }

    Status Tap(Edit &edit, Port source,
               const HookBlock::Hook &hook) override {
        if (!hook)
            return {Error::CallbackFailed, CKERR_INVALIDPARAMETER,
                    CKBR_PARAMETERERROR, "missing hook"};
        ++Taps;
        edit.Tap(std::move(source), FakeTap());
        return {};
    }

    Status Interpose(Edit &edit, Link link,
                     const HookBlock::Hook &hook) override {
        if (!hook)
            return {Error::CallbackFailed, CKERR_INVALIDPARAMETER,
                    CKBR_PARAMETERERROR, "missing hook"};
        ++Afters;
        InterposedLinks.push_back(link);
        Node block = edit.Add(
            Spec(CKGUID(0x19038c0, 0x663902da)), Shape());
        edit.Splice(link, block);
        return {};
    }

    GraphModel Base;
    GraphEdit::SettingStages AddedSettings;
    int Begins = 0;
    int Adds = 0;
    int Taps = 0;
    int Afters = 0;
    CKDWORD NodeFlags = 0;
    CKDWORD AddedFlags = 0;
    std::vector<ObjectRef> UsedNodes;
    std::vector<ObjectRef> UsedLinks;
    std::vector<Link> InterposedLinks;
    std::vector<CKGUID> AddedPrototypes;
};

GraphEdit SpliceEdit(std::optional<int> delay = std::nullopt) {
    GraphEdit edit;
    const Node wait = edit.RequireOne(
        {"Wait Message", CKGUID(0x1111, 1)});
    const Node sink = edit.RequireOne(
        {"set Resetpoint", CKGUID(0x2222, 2)});
    const Link edge = edit.RequireOne(wait.Out(), sink.In(), delay);
    const Node hook = edit.Add(CKGUID(0x3333, 3));
    edit.Splice(edge, hook);
    return edit;
}

TEST(BehaviorGraphEdit, ResolvesSemanticNodesAndAnExactLinkBeforeAdding) {
    FakeCompiler compiler(Model());
    GraphEdit plan = SpliceEdit();
    Edit edit;
    ASSERT_TRUE(plan.Compile(
        {"mod", "checkpoint"}, compiler.Base.Root, compiler, edit));
    EXPECT_EQ(compiler.UsedNodes,
              (std::vector<ObjectRef>{Ref(101), Ref(102)}));
    EXPECT_EQ(compiler.UsedLinks,
              (std::vector<ObjectRef>{Ref(201)}));
    ASSERT_EQ(compiler.AddedPrototypes.size(), 1u);
    EXPECT_EQ(compiler.AddedPrototypes.front(), CKGUID(0x3333, 3));

    CheckedEdit checked;
    ASSERT_TRUE(edit.Validate(compiler.Base, checked));
    ASSERT_EQ(checked.Splices.size(), 1u);
    EXPECT_EQ(checked.Splices.front().Target.Anchor, Ref(201));
}

TEST(BehaviorGraphEdit, AcceptsTheOnlyPortSelectorFromThePublicDsl) {
    FakeCompiler compiler(Model());
    GraphEdit edit;
    const Node wait = edit.RequireOne({"Wait Message"});
    const Node sink = edit.RequireOne({"set Resetpoint"});
    const Port onlyOut{wait.Value, Slot::Only(SlotKind::Output)};
    const Port onlyIn{sink.Value, Slot::Only(SlotKind::Input)};
    const Link edge = edit.RequireOne(onlyOut, onlyIn);
    edit.Splice(edge, edit.Add(CKGUID(0x3333, 3)));

    Edit resolved;
    ASSERT_TRUE(edit.Compile(
        {"mod", "only-ports"}, compiler.Base.Root, compiler, resolved));
    EXPECT_EQ(compiler.UsedLinks,
              (std::vector<ObjectRef>{Ref(201)}));
}

TEST(BehaviorGraphEdit, CompilesTheOnlyPortSelectorInActions) {
    FakeCompiler compiler(Model());
    GraphEdit edit;
    const Node wait = edit.RequireOne({"Wait Message"});
    const Node added = edit.Add(CKGUID(0x3333, 3));
    const Port onlyOut{wait.Value, Slot::Only(SlotKind::Output)};
    const Port onlyIn{added.Value, Slot::Only(SlotKind::Input)};
    edit.Flow(onlyOut, onlyIn, 1);
    edit.Bind({added.Value, Slot::Only(SlotKind::InputParameter)},
              Value::From(CKPGUID_INT, 7));
    edit.Tap(onlyOut, HookBlock::Hook(Noop));

    Edit resolved;
    const Status status = edit.Compile(
        {"mod", "only-actions"}, compiler.Base.Root, compiler, resolved);
    ASSERT_TRUE(status) << status.Message;
    EXPECT_EQ(compiler.Adds, 1);
}

TEST(BehaviorGraphEdit, RejectsAnAmbiguousNameBeforeResolvingLinksOrBlocks) {
    GraphModel graph = Model();
    GraphNode duplicate = graph.Nodes[1];
    duplicate.Id = 103;
    duplicate.Object = Ref(103);
    graph.Nodes.push_back(std::move(duplicate));
    FakeCompiler compiler(std::move(graph));

    GraphEdit plan;
    const Node wait = plan.RequireOne({"Wait Message"});
    const Node sink = plan.RequireOne({"set Resetpoint"});
    const Link edge = plan.RequireOne(wait.Out(), sink.In());
    plan.Splice(edge, plan.Add(CKGUID(0x3333, 3)));

    Edit edit;
    const Status status = plan.Compile(
        {"mod", "checkpoint"}, compiler.Base.Root, compiler, edit);
    EXPECT_EQ(status.Code, Error::QueryAmbiguous);
    EXPECT_TRUE(compiler.UsedNodes.empty());
    EXPECT_TRUE(compiler.UsedLinks.empty());
    EXPECT_EQ(compiler.Adds, 0);
}

TEST(BehaviorGraphEdit, RequiresAUniqueParallelLink) {
    GraphModel graph = Model();
    GraphLink parallel = graph.Links.front();
    parallel.Id = 2;
    parallel.Object = Ref(202);
    parallel.InitialDelay = 2;
    graph.Links.push_back(parallel);

    FakeCompiler ambiguous(graph);
    Edit edit;
    Status status = SpliceEdit().Compile(
        {"mod", "checkpoint"}, ambiguous.Base.Root, ambiguous, edit);
    EXPECT_EQ(status.Code, Error::QueryAmbiguous);
    EXPECT_EQ(ambiguous.Adds, 0);

    FakeCompiler exact(std::move(graph));
    ASSERT_TRUE(SpliceEdit(2).Compile(
        {"mod", "checkpoint"}, exact.Base.Root, exact, edit));
    EXPECT_EQ(exact.UsedLinks,
              (std::vector<ObjectRef>{Ref(202)}));
}

TEST(BehaviorGraphEdit, RejectsNonGraphLinksAndInvalidDelayBeforeCK) {
    FakeCompiler compiler(Model());
    GraphEdit addedLink;
    const Node existing = addedLink.RequireOne({"Wait Message"});
    const Node added = addedLink.Add(CKGUID(0x3333, 3));
    (void) addedLink.RequireOne(added.Out(), existing.In());
    EXPECT_EQ(addedLink.Validate().Code, Error::InvalidState);

    GraphEdit invalidDelay;
    const Node wait = invalidDelay.RequireOne({"Wait Message"});
    const Node sink = invalidDelay.RequireOne({"set Resetpoint"});
    (void) invalidDelay.RequireOne(wait.Out(), sink.In(), -1);
    EXPECT_EQ(invalidDelay.Validate().Code, Error::InvalidDelay);

    invalidDelay.Flow(wait.Out(), sink.In(), 32765);
    Edit edit;
    const Status status = invalidDelay.Compile(
        {"mod", "invalid"}, compiler.Base.Root, compiler, edit);
    EXPECT_EQ(status.Code, Error::InvalidDelay);
    EXPECT_EQ(compiler.Begins, 0);
}

TEST(BehaviorGraphEdit, ResolvesTheSameIntentAgainForANewWorld) {
    GraphEdit plan = SpliceEdit();
    FakeCompiler first(Model(1, 0));
    FakeCompiler second(Model(2, 1000));
    Edit firstEdit;
    Edit secondEdit;
    ASSERT_TRUE(plan.Compile(
        {"mod", "checkpoint"}, first.Base.Root, first, firstEdit));
    ASSERT_TRUE(plan.Compile(
        {"mod", "checkpoint"}, second.Base.Root, second, secondEdit));
    EXPECT_EQ(first.UsedNodes.front(), Ref(101, 1));
    EXPECT_EQ(second.UsedNodes.front(), Ref(1101, 2));
    EXPECT_EQ(first.UsedLinks.front(), Ref(201, 1));
    EXPECT_EQ(second.UsedLinks.front(), Ref(1201, 2));
}

TEST(BehaviorGraphEdit, PinsTheResolvedLogicalGraphUntilApply) {
    FakeCompiler compiler(Model());
    Edit edit;
    ASSERT_TRUE(SpliceEdit().Compile(
        {"mod", "checkpoint"}, compiler.Base.Root, compiler, edit));
    GraphModel changed = compiler.Base;
    ++changed.Fingerprint;
    CheckedEdit checked;
    const Status status = edit.Validate(changed, checked);
    EXPECT_EQ(status.Code, Error::GraphChanged);
}

TEST(BehaviorGraphEdit, ReplaysRootAndNodeFlowWithSymbolicHandles) {
    FakeCompiler compiler(Model());
    GraphEdit plan;
    const Node wait = plan.RequireOne(
        {"Wait Message", CKGUID(0x1111, 1)});
    plan.Flow(plan.Entry("Start"), wait.In());
    plan.Flow(wait.Out(), plan.Exit("Done"), 2);

    Edit edit;
    ASSERT_TRUE(plan.Compile(
        {"mod", "flow"}, compiler.Base.Root, compiler, edit));
    CheckedEdit checked;
    ASSERT_TRUE(edit.Validate(compiler.Base, checked));
    ASSERT_EQ(checked.Flows.size(), 2u);
    EXPECT_EQ(checked.Flows[1].Delay, 2);
}

TEST(BehaviorGraphEdit, KeepsControlDataAndSpliceDeclarationOrder) {
    FakeCompiler compiler(Model());
    GraphEdit plan;
    const Node wait = plan.RequireOne(
        {"Wait Message", CKGUID(0x1111, 1)});
    const Node sink = plan.RequireOne(
        {"set Resetpoint", CKGUID(0x2222, 2)});
    const Link edge = plan.RequireOne(wait.Out(), sink.In());
    const Node hook = plan.Add(CKGUID(0x3333, 3));

    const int value = 42;
    plan.Bind(wait.Pin("Value"), Value::From(CKPGUID_INT, value));
    plan.Flow(plan.Entry("Start"), wait.In());
    plan.Bind(hook.Pin("Value"), wait.Pout("Result"));
    plan.Share(sink.Pin("Value"), wait.Pin("Value"));
    plan.Push(wait.Pout("Result"), sink.Local("State"));
    plan.Splice(edge, hook);

    Edit edit;
    ASSERT_TRUE(plan.Compile(
        {"mod", "data"}, compiler.Base.Root, compiler, edit));
    CheckedEdit checked;
    ASSERT_TRUE(edit.Validate(compiler.Base, checked));
    ASSERT_EQ(checked.Binds.size(), 3u);
    ASSERT_EQ(checked.Flows.size(), 1u);
    ASSERT_EQ(checked.Pushes.size(), 1u);
    ASSERT_EQ(checked.Splices.size(), 1u);
    EXPECT_EQ(checked.Binds[0].Kind, BindKind::Literal);
    EXPECT_EQ(checked.Binds[1].Kind, BindKind::Direct);
    EXPECT_EQ(checked.Binds[2].Kind, BindKind::Shared);
    EXPECT_LT(checked.Binds[0].Ordinal, checked.Flows[0].Ordinal);
    EXPECT_LT(checked.Flows[0].Ordinal, checked.Binds[1].Ordinal);
    EXPECT_LT(checked.Binds[1].Ordinal, checked.Binds[2].Ordinal);
    EXPECT_LT(checked.Binds[2].Ordinal, checked.Pushes[0].Ordinal);
    EXPECT_LT(checked.Pushes[0].Ordinal, checked.Splices[0].Ordinal);
}

TEST(BehaviorGraphEdit, ResolvesDynamicPortsForTheCurrentLayout) {
    FakeCompiler compiler(Model());
    compiler.NodeFlags = CKBEHAVIOR_VARIABLEINPUTS |
                         CKBEHAVIOR_VARIABLEOUTPUTS |
                         CKBEHAVIOR_VARIABLEPARAMETERINPUTS |
                         CKBEHAVIOR_VARIABLEPARAMETEROUTPUTS;

    GraphEdit plan;
    const Node wait = plan.RequireOne(
        {"Wait Message", CKGUID(0x1111, 1)});
    const Node sink = plan.RequireOne(
        {"set Resetpoint", CKGUID(0x2222, 2)});
    const Port input = plan.AppendIn(wait, "Again");
    const Port output = plan.AppendOut(wait, "Finished");
    const Port pin = plan.AppendPin(wait, "Other", CKPGUID_INT);
    const Port pout = plan.AppendPout(wait, "Produced", CKPGUID_INT);
    plan.Flow(plan.Entry("Start"), input);
    plan.Flow(output, plan.Exit("Done"));
    const int value = 7;
    plan.Bind(pin, Value::From(CKPGUID_INT, value));
    plan.Push(pout, sink.Local("State"));

    Edit edit;
    ASSERT_TRUE(plan.Compile(
        {"mod", "interface"}, compiler.Base.Root, compiler, edit));
    CheckedEdit checked;
    ASSERT_TRUE(edit.Validate(compiler.Base, checked));
    ASSERT_EQ(checked.Flows.size(), 2u);
    ASSERT_EQ(checked.Binds.size(), 1u);
    ASSERT_EQ(checked.Pushes.size(), 1u);
    EXPECT_TRUE(checked.Flows[0].Sink.Appended);
    EXPECT_TRUE(checked.Flows[1].Source.Appended);
    EXPECT_TRUE(checked.Binds[0].Target.Appended);
    EXPECT_TRUE(checked.Pushes[0].Source.Appended);
}

TEST(BehaviorGraphEdit, KeepsTypedNullAsADurableLiteral) {
    FakeCompiler compiler(Model());
    GraphEdit plan;
    const Node wait = plan.RequireOne(
        {"Wait Message", CKGUID(0x1111, 1)});
    plan.Bind(wait.Pin("Value"), Value::Null(CKPGUID_OBJECT));

    EXPECT_TRUE(plan.Validate());
    Edit edit;
    ASSERT_TRUE(plan.Compile(
        {"mod", "null"}, compiler.Base.Root, compiler, edit));
    CheckedEdit checked;
    ASSERT_TRUE(edit.Validate(compiler.Base, checked));
    ASSERT_EQ(checked.Binds.size(), 1u);
    EXPECT_EQ(checked.Binds[0].Literal.Kind(), ValueKind::Null);
    EXPECT_TRUE(checked.Binds[0].Literal.IsNull());
}

TEST(BehaviorGraphEdit, CarriesASettingWithTheBlockThatDeclaresIt) {
    FakeCompiler compiler(Model());
    GraphEdit plan;
    const Node added = plan.Add(CKGUID(0x3333, 3));
    ASSERT_TRUE(plan.Setting(
        added, Slot::Named(SlotKind::Setting, "Mode"),
        Value::From(CKPGUID_INT, 2)));

    ASSERT_TRUE(plan.Validate());
    Edit edit;
    ASSERT_TRUE(plan.Compile(
        {"mod", "setting"}, compiler.Base.Root, compiler, edit));
    EXPECT_EQ(compiler.Adds, 1);
    ASSERT_EQ(compiler.AddedSettings.size(), 1u);
    ASSERT_EQ(compiler.AddedSettings[0].size(), 1u);
    EXPECT_EQ(compiler.AddedSettings[0][0].first.Kind, SlotKind::Setting);
    const Value &declared = compiler.AddedSettings[0][0].second;
    EXPECT_EQ(declared.Type(), CKPGUID_INT);
    ASSERT_EQ(declared.Bytes().size(), sizeof(int));
    int mode = 0;
    std::memcpy(&mode, declared.Bytes().data(), sizeof(mode));
    EXPECT_EQ(mode, 2);
    // A Setting travels with the Block, so it is not an action the Edit
    // replays afterwards.
    CheckedEdit checked;
    ASSERT_TRUE(edit.Validate(compiler.Base, checked));
    EXPECT_TRUE(checked.Binds.empty());
}

TEST(BehaviorGraphEdit, RefusesASettingOnABlockItDidNotCreate) {
    GraphEdit plan;
    const Node wait = plan.RequireOne(
        {"Wait Message", CKGUID(0x1111, 1)});
    Status status = plan.Setting(
        wait, Slot::Named(SlotKind::Setting, "Mode"),
        Value::From(CKPGUID_INT, 2));
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::InterfaceUnsupported);

    GraphEdit wrongSlot;
    const Node added = wrongSlot.Add(CKGUID(0x3333, 3));
    ASSERT_TRUE(wrongSlot.Setting(
        added, Slot::Named(SlotKind::Local, "Mode"),
        Value::From(CKPGUID_INT, 2)));
    status = wrongSlot.Validate();
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::TypeMismatch);
}

TEST(BehaviorGraphEdit, PreservesSettingStageBoundaries) {
    FakeCompiler compiler(Model());
    GraphEdit edit;
    const Node added = edit.Add(CKGUID(0x3333, 3));
    ASSERT_TRUE(edit.Setting(
        added, Slot::Named(SlotKind::Setting, "Mode"),
        Value::From(CKPGUID_INT, 2)));
    ASSERT_TRUE(edit.Setting(
        added, Slot::Named(SlotKind::Setting, "Created Later"),
        Value::From(CKPGUID_INT, 9), true));

    Edit compiled;
    ASSERT_TRUE(edit.Compile(
        {"mod", "setting-stages"}, compiler.Base.Root, compiler, compiled));
    ASSERT_EQ(compiler.AddedSettings.size(), 2u);
    ASSERT_EQ(compiler.AddedSettings[0].size(), 1u);
    ASSERT_EQ(compiler.AddedSettings[1].size(), 1u);
    EXPECT_EQ(compiler.AddedSettings[0][0].first.Name, "Mode");
    EXPECT_EQ(compiler.AddedSettings[1][0].first.Name, "Created Later");
}

TEST(BehaviorGraphEdit, CompletesAPathAgainAndTapsItsFinalOut) {
    FakeCompiler compiler(Model());
    GraphEdit plan;
    const Node wait = plan.RequireOne(
        {"Wait Message", CKGUID(0x1111, 1)});
    (void) plan.RequireOne(
        {"set Resetpoint", CKGUID(0x2222, 2)});
    const PathRef path = plan.Follow(wait.Out());
    plan.After(path, HookBlock::Hook(Noop));

    Edit edit;
    ASSERT_TRUE(plan.Compile(
        {"mod", "after"}, compiler.Base.Root, compiler, edit));
    EXPECT_EQ(compiler.Taps, 1);
    EXPECT_EQ(compiler.Afters, 0);
    EXPECT_EQ(compiler.UsedLinks,
              (std::vector<ObjectRef>{Ref(201)}));
    EXPECT_EQ(compiler.UsedNodes,
              (std::vector<ObjectRef>{Ref(101), Ref(102)}));

    CheckedEdit checked;
    ASSERT_TRUE(edit.Validate(compiler.Base, checked));
    ASSERT_EQ(checked.Taps.size(), 1u);
    EXPECT_EQ(checked.Taps.front().Source.Slot.Kind, SlotKind::Output);
    EXPECT_EQ(checked.Taps.front().Source.Owner.Value, 3u);
}

TEST(BehaviorGraphEdit, PlacesAfterBeforeAGraphExit) {
    GraphModel graph = Model();
    graph.Links.push_back({
        2, Ref(202),
        {102, SlotKind::Output, 0},
        {100, SlotKind::Output, 0}, 0});
    FakeCompiler compiler(std::move(graph));
    GraphEdit plan;
    const Node wait = plan.RequireOne({"Wait Message"});
    plan.After(plan.Follow(wait.Out()), HookBlock::Hook(Noop));

    Edit edit;
    ASSERT_TRUE(plan.Compile(
        {"mod", "exit"}, compiler.Base.Root, compiler, edit));
    EXPECT_EQ(compiler.Taps, 0);
    EXPECT_EQ(compiler.Afters, 1);
    EXPECT_EQ(compiler.UsedLinks,
              (std::vector<ObjectRef>{Ref(201), Ref(202)}));

    CheckedEdit checked;
    ASSERT_TRUE(edit.Validate(compiler.Base, checked));
    ASSERT_EQ(checked.Splices.size(), 1u);
    EXPECT_EQ(checked.Splices.front().Target.Anchor, Ref(202));
}

TEST(BehaviorGraphEdit, RejectsAnAmbiguousPathBeforeInstallingAHook) {
    GraphModel graph = Model();
    graph.Links.push_back({
        2, Ref(202),
        {101, SlotKind::Output, 0},
        {100, SlotKind::Output, 0}, 0});
    FakeCompiler compiler(std::move(graph));
    GraphEdit plan;
    const Node wait = plan.RequireOne({"Wait Message"});
    plan.After(plan.Follow(wait.Out()), HookBlock::Hook(Noop));

    Edit edit;
    const Status status = plan.Compile(
        {"mod", "ambiguous-path"}, compiler.Base.Root, compiler, edit);
    EXPECT_EQ(status.Code, Error::PathAmbiguous);
    EXPECT_EQ(compiler.Taps, 0);
    EXPECT_EQ(compiler.Afters, 0);
}


TEST(BehaviorGraphEdit, NamesANodeAndALinkTheAuthorAlreadyHolds) {
    FakeCompiler compiler(Model());
    GraphEdit plan;
    const Node wait = plan.UseNode(Ref(101));
    const Link edge = plan.UseLink(Ref(201));
    const Node hook = plan.Add(CKGUID(0x3333, 3));
    plan.Splice(edge, hook);
    EXPECT_TRUE(plan.UsesIdentity());

    Edit edit;
    std::map<std::uint32_t, Node> handles;
    const Status status = plan.Compile(
        {"mod", "by-reference"}, compiler.Base.Root, compiler, edit, &handles);
    ASSERT_TRUE(status) << status.Message;
    EXPECT_EQ(compiler.UsedNodes, (std::vector<ObjectRef>{Ref(101)}));
    EXPECT_EQ(compiler.UsedLinks, (std::vector<ObjectRef>{Ref(201)}));
    EXPECT_EQ(compiler.Adds, 1);

    // Every named Node is reported back, including the graph itself.
    EXPECT_EQ(handles.count(wait.Value), 1u);
    EXPECT_EQ(handles.count(hook.Value), 1u);
    EXPECT_EQ(handles.count(plan.Graph().Value), 1u);

    CheckedEdit checked;
    ASSERT_TRUE(edit.Validate(compiler.Base, checked));
    ASSERT_EQ(checked.Splices.size(), 1u);
    EXPECT_EQ(checked.Splices.front().Target.Anchor, Ref(201));
}

TEST(BehaviorGraphEdit, ReportsAQueriedNodeUnderItsOwnHandle) {
    FakeCompiler compiler(Model());
    GraphEdit plan;
    const Node wait = plan.RequireOne({"Wait Message", CKGUID(0x1111, 1)});
    EXPECT_FALSE(plan.UsesIdentity());

    Edit edit;
    std::map<std::uint32_t, Node> handles;
    ASSERT_TRUE(plan.Compile(
        {"mod", "queried"}, compiler.Base.Root, compiler, edit, &handles));
    EXPECT_EQ(handles.count(wait.Value), 1u);
}

TEST(BehaviorGraphEdit, RejectsANodeReferenceOutsideTheTargetGraph) {
    FakeCompiler compiler(Model());
    GraphEdit plan;
    (void) plan.UseNode(Ref(999));

    Edit edit;
    const Status status = plan.Compile(
        {"mod", "foreign-node"}, compiler.Base.Root, compiler, edit);
    EXPECT_EQ(status.Code, Error::InvalidGraphLocality);
    EXPECT_TRUE(compiler.UsedNodes.empty());
}

TEST(BehaviorGraphEdit, RejectsALinkReferenceOutsideTheTargetGraph) {
    FakeCompiler compiler(Model());
    GraphEdit plan;
    (void) plan.UseLink(Ref(999));

    Edit edit;
    const Status status = plan.Compile(
        {"mod", "foreign-link"}, compiler.Base.Root, compiler, edit);
    EXPECT_EQ(status.Code, Error::LinkNotFound);
    EXPECT_TRUE(compiler.UsedLinks.empty());
}

TEST(BehaviorGraphEdit, ComposesIdentityNodesWithQueriedLinks) {
    FakeCompiler compiler(Model());
    GraphEdit plan;
    const Node added = plan.Add(CKGUID(0x3333, 3));
    (void) added;
    const Node wait = plan.UseNode(Ref(101));
    const Node sink = plan.RequireOne({"set Resetpoint"});
    // An endpoint query cannot be mixed with a Link named by identity, so the
    // two ways of naming the same edge stay separate.
    const Link queried = plan.RequireOne(wait.Out(), sink.In());
    (void) queried;

    Edit edit;
    ASSERT_TRUE(plan.Compile(
        {"mod", "mixed"}, compiler.Base.Root, compiler, edit))
        << "identity nodes and queried links compose";
}

TEST(BehaviorGraphEdit, PutsACallbackOnALinkTheAuthorNamed) {
    FakeCompiler compiler(Model());
    GraphEdit plan;
    const Link edge = plan.UseLink(Ref(201));
    plan.Before(edge, HookBlock::Hook(Noop));

    Edit edit;
    const Status status = plan.Compile(
        {"mod", "before"}, compiler.Base.Root, compiler, edit);
    ASSERT_TRUE(status) << status.Message;
    EXPECT_EQ(compiler.UsedLinks, (std::vector<ObjectRef>{Ref(201)}));
    EXPECT_EQ(compiler.Taps, 0);
    EXPECT_EQ(compiler.Afters, 1);
    ASSERT_EQ(compiler.InterposedLinks.size(), 1u);
}

TEST(BehaviorGraphEdit, PutsACallbackOnAQueriedLink) {
    FakeCompiler compiler(Model());
    GraphEdit plan;
    const Node wait = plan.RequireOne({"Wait Message"});
    const Node sink = plan.RequireOne({"set Resetpoint"});
    plan.Before(plan.RequireOne(wait.Out(), sink.In()),
                HookBlock::Hook(Noop));

    Edit edit;
    const Status status = plan.Compile(
        {"mod", "before-query"}, compiler.Base.Root, compiler, edit);
    ASSERT_TRUE(status) << status.Message;
    EXPECT_EQ(compiler.Afters, 1);
}

TEST(BehaviorGraphEdit, RejectsABeforeWithoutALinkOrACallback) {
    FakeCompiler compiler(Model());
    GraphEdit missingHook;
    missingHook.Before(missingHook.UseLink(Ref(201)), HookBlock::Hook());
    Edit edit;
    Status status = missingHook.Compile(
        {"mod", "no-hook"}, compiler.Base.Root, compiler, edit);
    EXPECT_EQ(status.Code, Error::InvalidState);

    GraphEdit foreignLink;
    foreignLink.Before(Link{}, HookBlock::Hook(Noop));
    status = foreignLink.Compile(
        {"mod", "no-link"}, compiler.Base.Root, compiler, edit);
    EXPECT_EQ(status.Code, Error::InvalidState);
    EXPECT_EQ(compiler.Afters, 0);
}

TEST(BehaviorGraphEdit, CompilesARedirectOntoTheExactLiveLink) {
    FakeCompiler compiler(Model());
    GraphEdit plan;
    const Node wait = plan.RequireOne({"Wait Message", CKGUID(0x1111, 1)});
    const Node sink = plan.RequireOne({"set Resetpoint", CKGUID(0x2222, 2)});
    const Link edge = plan.RequireOne(wait.Out(), sink.In());
    const Node detour = plan.Add(CKGUID(0x3333, 3));
    plan.Redirect(edge, detour.In());
    ASSERT_TRUE(plan.Validate());

    Edit edit;
    ASSERT_TRUE(plan.Compile(
        {"mod", "detour"}, compiler.Base.Root, compiler, edit));
    EXPECT_EQ(compiler.UsedLinks, (std::vector<ObjectRef>{Ref(201)}));

    CheckedEdit checked;
    ASSERT_TRUE(edit.Validate(compiler.Base, checked));
    EXPECT_TRUE(checked.Splices.empty());
    ASSERT_EQ(checked.Redirects.size(), 1u);
    EXPECT_EQ(checked.Redirects.front().Target.Anchor, Ref(201));
}

TEST(BehaviorGraphEdit, RefusesARedirectThatNamesNothing) {
    GraphEdit plan;
    const Node wait = plan.RequireOne({"Wait Message"});
    const Node sink = plan.RequireOne({"set Resetpoint"});
    const Port stranger{9999, Slot::Only(SlotKind::Input)};
    plan.Redirect(plan.RequireOne(wait.Out(), sink.In()), stranger);
    const Status status = plan.Validate();
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::InvalidState);
}

} // namespace

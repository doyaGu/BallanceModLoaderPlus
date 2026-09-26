#include "Behavior/Edit/Program.h"

#include <cstdint>
#include <cstring>
#include <map>
#include <tuple>
#include <utility>

#include <gtest/gtest.h>

namespace {

using namespace BML::Behavior::Internal;

ObjectRef Ref(std::uint32_t slot, std::uint32_t generation = 1) {
    return {3, slot, generation};
}

NativeRef Native(std::uint64_t id) {
    return {id, reinterpret_cast<const void *>(
                    static_cast<std::uintptr_t>(id))};
}

GraphPort In(int index = 0, std::string name = "In") {
    return {SlotKind::Input, 0, index, 0, CKGUID(), false,
            std::move(name), false};
}

GraphPort Out(int index = 0, std::string name = "Out") {
    return {SlotKind::Output, 0, index, 0, CKGUID(), false,
            std::move(name), false};
}

GraphPort Pin(int index = 0, std::string name = "Value",
              CKGUID type = CKPGUID_INT) {
    return {SlotKind::InputParameter, 0, index, 0, type, false,
            std::move(name), false};
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

class FakeResolver final : public Program::Resolver {
public:
    explicit FakeResolver(GraphModel model) : Base(std::move(model)) {}

    Status Begin(const PatchKey &patch, const ObjectRef &graph,
                 Ops &out, GraphModel &base) override {
        ++Begins;
        if (graph != Base.Root)
            return {Error::InvalidGraphLocality, CKERR_INVALIDOBJECT,
                    CKBR_PARAMETERERROR, "wrong graph"};
        const GraphNode &root = Base.Nodes.front();
        out = Ops(patch, Native(root.Id), Shape("Start", "Done"));
        base = Base;
        return {};
    }

    Status UseNode(Ops &edit, const ObjectRef &node, Node &out) override {
        UsedNodes.push_back(node);
        out = edit.Use(Native(node.Slot), Shape("In", "Out", NodeFlags));
        return {};
    }

    Status UseLink(Ops &edit, const ObjectRef &link, Link &out) override {
        UsedLinks.push_back(link);
        out = edit.Use(link);
        return {};
    }

    Status ReadPatternValue(const GraphNode &node, const Slot &slot,
                            GraphValue &out) override {
        ++ValueReads;
        const auto found = Values.find({node.Id, slot.Kind, slot.Index});
        if (found == Values.end()) {
            out = {};
            out.State = ValueState::Indeterminate;
            return {};
        }
        out = found->second;
        return {};
    }

    Status Add(Ops &edit, BlockSpec block, Node &out) override {
        ++Adds;
        AddedPrototypes.push_back(
            {block.Prototype(), block.PrototypeGeneration()});
        AddedSettings = block.Settings();
        Layout shape = AddedShape;
        shape.BehaviorFlags = AddedFlags;
        out = edit.Add(std::move(block), std::move(shape));
        return {};
    }

    Status AddGraph(Ops &edit, std::string name, int priority,
                    Node &out) override {
        ++Adds;
        AddedGraphName = std::move(name);
        AddedGraphPriority = priority;
        out = edit.AddGraph(AddedGraphName, priority);
        return {};
    }

    Status Tap(Ops &edit, Port source,
               const HookBlock::Hook &hook) override {
        if (!hook)
            return {Error::CallbackFailed, CKERR_INVALIDPARAMETER,
                    CKBR_PARAMETERERROR, "missing hook"};
        ++Taps;
        edit.Tap(std::move(source), FakeTap());
        return {};
    }

    Status Interpose(Ops &edit, Link link,
                     const HookBlock::Hook &hook) override {
        if (!hook)
            return {Error::CallbackFailed, CKERR_INVALIDPARAMETER,
                    CKBR_PARAMETERERROR, "missing hook"};
        ++Afters;
        InterposedLinks.push_back(link);
        Node block = edit.Add(
            BlockSpec(CKGUID(0x19038c0, 0x663902da)), Shape());
        edit.Splice(link, block);
        return {};
    }

    Status Interpose(Ops &edit, Port source, Port sink,
                     const HookBlock::Hook &hook) override {
        if (!hook)
            return {Error::CallbackFailed, CKERR_INVALIDPARAMETER,
                    CKBR_PARAMETERERROR, "missing hook"};
        ++HookFlows;
        Node block = edit.Add(
            BlockSpec(CKGUID(0x19038c0, 0x663902da)), Shape(),
            NodeRole::Infrastructure);
        edit.Flow(std::move(source), block.In());
        edit.Flow(block.Out(), std::move(sink));
        return {};
    }

    GraphModel Base;
    std::vector<std::vector<BlockSpec::Binding>> AddedSettings;
    int Begins = 0;
    int Adds = 0;
    int Taps = 0;
    int Afters = 0;
    int HookFlows = 0;
    int ValueReads = 0;
    std::map<std::tuple<std::uint64_t, SlotKind, int>, GraphValue> Values;
    CKDWORD NodeFlags = 0;
    CKDWORD AddedFlags = 0;
    Layout AddedShape = Shape();
    std::vector<ObjectRef> UsedNodes;
    std::vector<ObjectRef> UsedLinks;
    std::vector<Link> InterposedLinks;
    std::vector<PrototypeRef> AddedPrototypes;
    std::string AddedGraphName;
    int AddedGraphPriority = 0;
};

Program SpliceEdit(std::optional<int> delay = std::nullopt) {
    Program edit;
    const Node wait = edit.RequireOne(
        {"Wait Message", CKGUID(0x1111, 1)});
    const Node sink = edit.RequireOne(
        {"set Resetpoint", CKGUID(0x2222, 2)});
    const Link edge = edit.RequireOne(wait.Out(), sink.In(), delay);
    const Node hook = edit.Add(CKGUID(0x3333, 3));
    edit.Splice(edge, hook);
    return edit;
}

TEST(BehaviorProgram, ResolvesSemanticNodesAndAnExactLinkBeforeAdding) {
    FakeResolver resolver(Model());
    Program plan = SpliceEdit();
    Ops edit;
    ASSERT_TRUE(plan.Resolve(
        {"mod", "checkpoint"}, resolver.Base.Root, resolver, edit));
    EXPECT_EQ(resolver.UsedNodes,
              (std::vector<ObjectRef>{Ref(101), Ref(102)}));
    EXPECT_EQ(resolver.UsedLinks,
              (std::vector<ObjectRef>{Ref(201)}));
    ASSERT_EQ(resolver.AddedPrototypes.size(), 1u);
    EXPECT_EQ(resolver.AddedPrototypes.front().Guid, CKGUID(0x3333, 3));
    EXPECT_EQ(resolver.AddedPrototypes.front().Generation, 0u);

    CheckedOps checked;
    ASSERT_TRUE(edit.Validate(resolver.Base, checked));
    ASSERT_EQ(checked.Splices.size(), 1u);
    EXPECT_EQ(checked.Splices.front().Target.Anchor, Ref(201));
}

TEST(BehaviorProgram, ReusesOneLiveNodeForRepeatedStructuralRequirements) {
    FakeResolver resolver(Model());
    Program plan;
    const NodePattern wait{"Wait Message", CKGUID(0x1111, 1)};
    (void) plan.RequireOne(wait);
    (void) plan.RequireOne(wait);

    Ops edit;
    const Status status = plan.Resolve(
        {"mod", "repeated-require"}, resolver.Base.Root, resolver, edit);
    ASSERT_TRUE(status) << status.Message;
    EXPECT_EQ(resolver.UsedNodes, (std::vector<ObjectRef>{Ref(101)}));
}

TEST(BehaviorProgram, ResolvesNodePatternsByKindAndPortCounts) {
    GraphModel graph = Model();
    graph.Nodes[1].Ports = {In(), Out(), Pin()};
    graph.Nodes.push_back(NodeOf(
        103, Ref(103), 100, CKGUID(0x1111, 1), "Wait Message",
        {In(), Out(), Out(1, "Timeout"), Pin()}));
    graph.Nodes.back().Kind = BehaviorKind::Graph;
    FakeResolver resolver(std::move(graph));

    NodePattern pattern{"Wait Message", CKGUID(0x1111, 1)};
    pattern.ExpectedKind = BehaviorKind::Function;
    pattern.PortCounts = {
        {SlotKind::Input, 1},
        {SlotKind::Output, 1},
        {SlotKind::InputParameter, 1},
        {SlotKind::OutputParameter, 0},
    };
    Program edit;
    (void) edit.RequireOne(std::move(pattern));

    Ops resolved;
    const Status status = edit.Resolve(
        {"mod", "pattern-shape"}, resolver.Base.Root, resolver, resolved);
    ASSERT_TRUE(status) << status.Message;
    EXPECT_EQ(resolver.UsedNodes, (std::vector<ObjectRef>{Ref(101)}));
    EXPECT_EQ(resolver.ValueReads, 0);
}

TEST(BehaviorProgram, ResolvesNodePatternsByObservedPortValue) {
    GraphModel graph = Model();
    graph.Nodes[1].Ports.push_back(Pin(0, "Message"));
    graph.Nodes.push_back(NodeOf(
        103, Ref(103), 100, CKGUID(0x1111, 1), "Wait Message",
        {In(), Out(), Pin(0, "Message")}));
    FakeResolver resolver(std::move(graph));
    resolver.Values[{101, SlotKind::InputParameter, 0}] = {
        ValueState::Available, ValueRelation::Stored, CKPGUID_INT,
        Parameter::Form::Int32, std::int32_t{7}};
    resolver.Values[{103, SlotKind::InputParameter, 0}] = {
        ValueState::Available, ValueRelation::Stored, CKPGUID_INT,
        Parameter::Form::Int32, std::int32_t{11}};

    NodePattern pattern{"Wait Message", CKGUID(0x1111, 1)};
    pattern.PortValues.push_back({
        Slot::Named(SlotKind::InputParameter, "Message", CKPGUID_INT),
        Value::From(CKPGUID_INT, std::int32_t{11})});
    Program edit;
    (void) edit.RequireOne(std::move(pattern));

    Ops resolved;
    const Status status = edit.Resolve(
        {"mod", "pattern-value"}, resolver.Base.Root, resolver, resolved);
    ASSERT_TRUE(status) << status.Message;
    EXPECT_EQ(resolver.UsedNodes, (std::vector<ObjectRef>{Ref(103)}));
    EXPECT_EQ(resolver.ValueReads, 2);
}

TEST(BehaviorProgram, RepeatsActionsForEveryPatternMatchInChildOrder) {
    GraphModel graph = Model();
    graph.Nodes.push_back(NodeOf(
        103, Ref(103), 100, CKGUID(0x3333, 3), "Activate Script",
        {In(), Out()}));
    graph.Nodes.back().Index = 4;
    graph.Nodes.push_back(NodeOf(
        104, Ref(104), 100, CKGUID(0x3333, 3), "Activate Script",
        {In(), Out()}));
    graph.Nodes.back().Index = 2;
    FakeResolver resolver(std::move(graph));

    Program plan;
    const Node activators = plan.Each({"Activate Script"});
    plan.Flow(activators.Out(), plan.Exit("Done"));

    Ops edit;
    const Status status = plan.Resolve(
        {"mod", "each"}, resolver.Base.Root, resolver, edit);
    ASSERT_TRUE(status) << status.Message;
    EXPECT_EQ(resolver.UsedNodes,
              (std::vector<ObjectRef>{Ref(104), Ref(103)}));

    CheckedOps checked;
    ASSERT_TRUE(edit.Validate(resolver.Base, checked));
    ASSERT_EQ(checked.Flows.size(), 2u);
    EXPECT_NE(checked.Flows[0].Source.Owner,
              checked.Flows[1].Source.Owner);
    EXPECT_EQ(checked.Flows[0].Sink.Owner, edit.Graph());
    EXPECT_EQ(checked.Flows[1].Sink.Owner, edit.Graph());
}

TEST(BehaviorProgram, RequiresAtLeastOneNodeForEach) {
    FakeResolver resolver(Model());
    Program plan;
    (void) plan.Each({"Activate Script"});
    Ops edit;
    const Status status = plan.Resolve(
        {"mod", "each-missing"}, resolver.Base.Root, resolver, edit);
    EXPECT_EQ(status.Code, Error::QueryNotFound);
    EXPECT_TRUE(resolver.UsedNodes.empty());
}

TEST(BehaviorProgram, RejectsInvalidOrUnresolvedNodePatterns) {
    FakeResolver resolver(Model());
    Program duplicateCounts;
    NodePattern duplicate{"Wait Message"};
    duplicate.PortCounts = {
        {SlotKind::Input, 1}, {SlotKind::Input, 1}};
    (void) duplicateCounts.RequireOne(std::move(duplicate));
    EXPECT_EQ(duplicateCounts.Validate().Code, Error::InvalidArgument);

    Program controlValue;
    NodePattern control{"Wait Message"};
    control.PortValues.push_back({
        Slot::At(SlotKind::Output, 0),
        Value::From(CKPGUID_INT, std::int32_t{1})});
    (void) controlValue.RequireOne(std::move(control));
    EXPECT_EQ(controlValue.Validate().Code, Error::TypeMismatch);

    resolver.Base.Nodes[1].Ports.push_back(Pin());
    NodePattern unavailable{"Wait Message"};
    unavailable.PortValues.push_back({
        Slot::At(SlotKind::InputParameter, 0, CKPGUID_INT),
        Value::From(CKPGUID_INT, std::int32_t{1})});
    Program unresolved;
    (void) unresolved.RequireOne(std::move(unavailable));
    Ops resolved;
    const Status status = unresolved.Resolve(
        {"mod", "pattern-unresolved"}, resolver.Base.Root, resolver,
        resolved);
    EXPECT_EQ(status.Code, Error::QueryNotFound);
    EXPECT_EQ(resolver.UsedNodes.size(), 0u);
}

TEST(BehaviorProgram, KeepsTheSelectedProviderForEveryInstallation) {
    FakeResolver resolver(Model());
    Program plan;
    (void) plan.Add(PrototypeRef{CKGUID(0x3333, 3), 91});

    Ops edit;
    ASSERT_TRUE(plan.Resolve(
        {"mod", "provider"}, resolver.Base.Root, resolver, edit));
    ASSERT_EQ(resolver.AddedPrototypes.size(), 1u);
    EXPECT_EQ(resolver.AddedPrototypes.front().Guid, CKGUID(0x3333, 3));
    EXPECT_EQ(resolver.AddedPrototypes.front().Generation, 91u);
}

TEST(BehaviorProgram, AcceptsTheOnlyPortSelectorFromThePublicDsl) {
    FakeResolver resolver(Model());
    Program edit;
    const Node wait = edit.RequireOne({"Wait Message"});
    const Node sink = edit.RequireOne({"set Resetpoint"});
    const Port onlyOut{wait.Value, Slot::Only(SlotKind::Output)};
    const Port onlyIn{sink.Value, Slot::Only(SlotKind::Input)};
    const Link edge = edit.RequireOne(onlyOut, onlyIn);
    edit.Splice(edge, edit.Add(CKGUID(0x3333, 3)));

    Ops resolved;
    ASSERT_TRUE(edit.Resolve(
        {"mod", "only-ports"}, resolver.Base.Root, resolver, resolved));
    EXPECT_EQ(resolver.UsedLinks,
              (std::vector<ObjectRef>{Ref(201)}));
}

TEST(BehaviorProgram, CompilesTheOnlyPortSelectorInActions) {
    FakeResolver resolver(Model());
    Program edit;
    const Node wait = edit.RequireOne({"Wait Message"});
    const Node added = edit.Add(CKGUID(0x3333, 3));
    const Port onlyOut{wait.Value, Slot::Only(SlotKind::Output)};
    const Port onlyIn{added.Value, Slot::Only(SlotKind::Input)};
    edit.Flow(onlyOut, onlyIn, 1);
    edit.Bind({added.Value, Slot::Only(SlotKind::InputParameter)},
              Value::From(CKPGUID_INT, 7));
    edit.Tap(onlyOut, HookBlock::Hook(Noop));

    Ops resolved;
    const Status status = edit.Resolve(
        {"mod", "only-actions"}, resolver.Base.Root, resolver, resolved);
    ASSERT_TRUE(status) << status.Message;
    EXPECT_EQ(resolver.Adds, 1);
}

TEST(BehaviorProgram, RejectsAnAmbiguousNameBeforeResolvingLinksOrBlocks) {
    GraphModel graph = Model();
    GraphNode duplicate = graph.Nodes[1];
    duplicate.Id = 103;
    duplicate.Object = Ref(103);
    graph.Nodes.push_back(std::move(duplicate));
    FakeResolver resolver(std::move(graph));

    Program plan;
    const Node wait = plan.RequireOne({"Wait Message"});
    const Node sink = plan.RequireOne({"set Resetpoint"});
    const Link edge = plan.RequireOne(wait.Out(), sink.In());
    plan.Splice(edge, plan.Add(CKGUID(0x3333, 3)));

    Ops edit;
    const Status status = plan.Resolve(
        {"mod", "checkpoint"}, resolver.Base.Root, resolver, edit);
    EXPECT_EQ(status.Code, Error::QueryAmbiguous);
    EXPECT_TRUE(resolver.UsedNodes.empty());
    EXPECT_TRUE(resolver.UsedLinks.empty());
    EXPECT_EQ(resolver.Adds, 0);
}

TEST(BehaviorProgram, RequiresAUniqueParallelLink) {
    GraphModel graph = Model();
    GraphLink parallel = graph.Links.front();
    parallel.Id = 2;
    parallel.Object = Ref(202);
    parallel.InitialDelay = 2;
    graph.Links.push_back(parallel);

    FakeResolver ambiguous(graph);
    Ops edit;
    Status status = SpliceEdit().Resolve(
        {"mod", "checkpoint"}, ambiguous.Base.Root, ambiguous, edit);
    EXPECT_EQ(status.Code, Error::QueryAmbiguous);
    EXPECT_EQ(ambiguous.Adds, 0);

    FakeResolver exact(std::move(graph));
    ASSERT_TRUE(SpliceEdit(2).Resolve(
        {"mod", "checkpoint"}, exact.Base.Root, exact, edit));
    EXPECT_EQ(exact.UsedLinks,
              (std::vector<ObjectRef>{Ref(202)}));
}

TEST(BehaviorProgram, ResolvesNodesAndLinksByTopology) {
    FakeResolver resolver(Model());
    Program plan;
    const Node wait = plan.RequireOne({"Wait Message"});
    const Node sink = plan.Next(wait.Out());
    const Node back = plan.Previous(sink.In());
    (void) plan.Leaving(wait.Out());
    (void) plan.Entering(sink.In());
    (void) plan.To(wait.Out(), sink);

    Ops edit;
    Program::ResolvedSymbols symbols;
    const Status status = plan.Resolve(
        {"mod", "topology"}, resolver.Base.Root, resolver, edit, &symbols);
    ASSERT_TRUE(status) << status.Message;
    const auto &nodes = symbols.Nodes;
    ASSERT_TRUE(nodes.contains(wait.Value));
    ASSERT_TRUE(nodes.contains(sink.Value));
    ASSERT_TRUE(nodes.contains(back.Value));
    EXPECT_EQ(nodes.at(wait.Value), nodes.at(back.Value));
    EXPECT_NE(nodes.at(wait.Value), nodes.at(sink.Value));
    EXPECT_EQ(resolver.UsedNodes,
              (std::vector<ObjectRef>{Ref(101), Ref(102)}));
    EXPECT_EQ(resolver.UsedLinks,
              (std::vector<ObjectRef>{Ref(201), Ref(201), Ref(201)}));
}

TEST(BehaviorProgram, ConstrainsRelatedNodesWithoutGlobalNameUniqueness) {
    GraphModel graph = Model();
    graph.Nodes.push_back(NodeOf(
        103, Ref(103), 100, CKGUID(0x3333, 3), "set Resetpoint",
        {In(), Out()}));
    graph.Nodes.back().Index = 3;
    graph.Nodes.push_back(NodeOf(
        104, Ref(104), 100, CKGUID(0x4444, 4), "Show",
        {In(), Out()}));
    graph.Nodes.back().Index = 4;
    graph.Links.push_back(
        {2, Ref(202), {101, SlotKind::Output, 0},
         {104, SlotKind::Input, 0}, 0});
    FakeResolver resolver(std::move(graph));

    Program plan;
    const Node wait = plan.RequireOne({"Wait Message"});
    const Node sink = plan.Next(wait.Out(), {"set Resetpoint"});
    (void) plan.To(wait.Out(), sink);

    Ops edit;
    Program::ResolvedSymbols symbols;
    const Status status = plan.Resolve(
        {"mod", "related-pattern"}, resolver.Base.Root, resolver, edit,
        &symbols);
    ASSERT_TRUE(status) << status.Message;
    const auto &nodes = symbols.Nodes;
    ASSERT_TRUE(nodes.contains(sink.Value));
    EXPECT_EQ(resolver.UsedNodes,
              (std::vector<ObjectRef>{Ref(101), Ref(102)}));
}

TEST(BehaviorProgram, RejectsARelatedNodeThatDoesNotMatchItsPattern) {
    FakeResolver resolver(Model());
    Program plan;
    const Node wait = plan.RequireOne({"Wait Message"});
    (void) plan.Next(wait.Out(), {"Send Message"});

    Ops edit;
    const Status status = plan.Resolve(
        {"mod", "wrong-related-pattern"}, resolver.Base.Root, resolver,
        edit);
    EXPECT_EQ(status.Code, Error::QueryNotFound);
    EXPECT_EQ(resolver.UsedNodes, (std::vector<ObjectRef>{Ref(101)}));
}

TEST(BehaviorProgram, ResolvesTopologyAtTheGraphEntryAndExit) {
    GraphModel graph = Model();
    graph.Links.insert(
        graph.Links.begin(),
        {2, Ref(202), {100, SlotKind::Input, 0},
         {101, SlotKind::Input, 0}, 0});
    graph.Links.push_back(
        {3, Ref(203), {102, SlotKind::Output, 0},
         {100, SlotKind::Output, 0}, 0});
    FakeResolver resolver(std::move(graph));
    Program plan;
    const Node first = plan.Next(plan.Entry("Start"));
    const Node last = plan.Previous(plan.Exit("Done"));
    (void) plan.Leaving(plan.Entry("Start"));
    (void) plan.Entering(plan.Exit("Done"));

    Ops edit;
    Program::ResolvedSymbols symbols;
    const Status status = plan.Resolve(
        {"mod", "graph-ends"}, resolver.Base.Root, resolver, edit, &symbols);
    ASSERT_TRUE(status) << status.Message;
    const auto &nodes = symbols.Nodes;
    EXPECT_NE(nodes.at(first.Value), nodes.at(last.Value));
    EXPECT_EQ(resolver.UsedNodes,
              (std::vector<ObjectRef>{Ref(101), Ref(102)}));
    EXPECT_EQ(resolver.UsedLinks,
              (std::vector<ObjectRef>{Ref(202), Ref(203)}));
}

TEST(BehaviorProgram, RedirectsToTheDestinationOfAnotherLink) {
    GraphModel graph = Model();
    graph.Links.insert(
        graph.Links.begin(),
        {2, Ref(202), {100, SlotKind::Input, 0},
         {101, SlotKind::Input, 0}, 0});
    FakeResolver resolver(std::move(graph));
    Program plan;
    const Node wait = plan.RequireOne({"Wait Message"});
    const Link entering = plan.Entering(wait.In());
    const Link leaving = plan.Leaving(wait.Out());
    plan.Redirect(entering, leaving);

    Ops edit;
    const Status status = plan.Resolve(
        {"mod", "bypass"}, resolver.Base.Root, resolver, edit);
    ASSERT_TRUE(status) << status.Message;
    EXPECT_EQ(resolver.UsedLinks,
              (std::vector<ObjectRef>{Ref(202), Ref(201)}));
    EXPECT_EQ(resolver.UsedNodes,
              (std::vector<ObjectRef>{Ref(101), Ref(102)}));

    CheckedOps checked;
    ASSERT_TRUE(edit.Validate(resolver.Base, checked));
    ASSERT_EQ(checked.Redirects.size(), 1u);
    EXPECT_EQ(checked.Redirects.front().Target.Anchor, Ref(202));
    EXPECT_EQ(checked.Redirects.front().Sink.Slot.Kind, SlotKind::Input);
    EXPECT_EQ(checked.Redirects.front().Sink.Slot.Index, 0);
}

TEST(BehaviorProgram, RejectsMissingAmbiguousAndBackwardTopology) {
    Program wrongDirection;
    const Node wait = wrongDirection.RequireOne({"Wait Message"});
    (void) wrongDirection.Next(wait.In());
    EXPECT_EQ(wrongDirection.Validate().Code, Error::InvalidState);

    GraphModel parallel = Model();
    GraphLink duplicate = parallel.Links.front();
    duplicate.Id = 2;
    duplicate.Object = Ref(202);
    parallel.Links.push_back(duplicate);
    FakeResolver ambiguous(std::move(parallel));
    Program branching;
    const Node source = branching.RequireOne({"Wait Message"});
    (void) branching.Next(source.Out());
    Ops edit;
    Status status = branching.Resolve(
        {"mod", "branching"}, ambiguous.Base.Root, ambiguous, edit);
    EXPECT_EQ(status.Code, Error::QueryAmbiguous);

    FakeResolver missing(Model());
    Program disconnected;
    const Node target = disconnected.RequireOne({"set Resetpoint"});
    (void) disconnected.Next(target.Out());
    status = disconnected.Resolve(
        {"mod", "disconnected"}, missing.Base.Root, missing, edit);
    EXPECT_EQ(status.Code, Error::LinkNotFound);
}

TEST(BehaviorProgram, RejectsNonGraphLinksAndInvalidDelayBeforeCK) {
    FakeResolver resolver(Model());
    Program addedLink;
    const Node existing = addedLink.RequireOne({"Wait Message"});
    const Node added = addedLink.Add(CKGUID(0x3333, 3));
    (void) addedLink.RequireOne(added.Out(), existing.In());
    EXPECT_EQ(addedLink.Validate().Code, Error::InvalidState);

    Program invalidDelay;
    const Node wait = invalidDelay.RequireOne({"Wait Message"});
    const Node sink = invalidDelay.RequireOne({"set Resetpoint"});
    (void) invalidDelay.RequireOne(wait.Out(), sink.In(), -1);
    EXPECT_EQ(invalidDelay.Validate().Code, Error::InvalidDelay);

    invalidDelay.Flow(wait.Out(), sink.In(), 32765);
    Ops edit;
    const Status status = invalidDelay.Resolve(
        {"mod", "invalid"}, resolver.Base.Root, resolver, edit);
    EXPECT_EQ(status.Code, Error::InvalidDelay);
    EXPECT_EQ(resolver.Begins, 0);
}

TEST(BehaviorProgram, ResolvesTheSameIntentAgainForANewWorld) {
    Program plan = SpliceEdit();
    FakeResolver first(Model(1, 0));
    FakeResolver second(Model(2, 1000));
    Ops firstEdit;
    Ops secondEdit;
    ASSERT_TRUE(plan.Resolve(
        {"mod", "checkpoint"}, first.Base.Root, first, firstEdit));
    ASSERT_TRUE(plan.Resolve(
        {"mod", "checkpoint"}, second.Base.Root, second, secondEdit));
    EXPECT_EQ(first.UsedNodes.front(), Ref(101, 1));
    EXPECT_EQ(second.UsedNodes.front(), Ref(1101, 2));
    EXPECT_EQ(first.UsedLinks.front(), Ref(201, 1));
    EXPECT_EQ(second.UsedLinks.front(), Ref(1201, 2));
}

TEST(BehaviorProgram, PinsTheResolvedLogicalGraphUntilApply) {
    FakeResolver resolver(Model());
    Ops edit;
    ASSERT_TRUE(SpliceEdit().Resolve(
        {"mod", "checkpoint"}, resolver.Base.Root, resolver, edit));
    GraphModel changed = resolver.Base;
    ++changed.Fingerprint;
    CheckedOps checked;
    const Status status = edit.Validate(changed, checked);
    EXPECT_EQ(status.Code, Error::GraphChanged);
}

TEST(BehaviorProgram, ReplaysRootAndNodeFlowWithSymbolicHandles) {
    FakeResolver resolver(Model());
    Program plan;
    const Node wait = plan.RequireOne(
        {"Wait Message", CKGUID(0x1111, 1)});
    plan.Flow(plan.Entry("Start"), wait.In());
    plan.Flow(wait.Out(), plan.Exit("Done"), 2);

    Ops edit;
    ASSERT_TRUE(plan.Resolve(
        {"mod", "flow"}, resolver.Base.Root, resolver, edit));
    CheckedOps checked;
    ASSERT_TRUE(edit.Validate(resolver.Base, checked));
    ASSERT_EQ(checked.Flows.size(), 2u);
    EXPECT_EQ(checked.Flows[1].Delay, 2);
}

TEST(BehaviorProgram, PlacesACallbackInsideANewFlow) {
    FakeResolver resolver(Model());
    Program plan;
    const Node wait = plan.RequireOne({"Wait Message"});
    const Node sink = plan.RequireOne({"set Resetpoint"});
    plan.Flow(wait.Out(), HookBlock::Hook(Noop), sink.In());

    Ops edit;
    ASSERT_TRUE(plan.Resolve(
        {"mod", "callback-flow"}, resolver.Base.Root, resolver, edit));
    EXPECT_EQ(resolver.HookFlows, 1);

    CheckedOps checked;
    ASSERT_TRUE(edit.Validate(resolver.Base, checked));
    ASSERT_EQ(checked.Flows.size(), 2u);
    EXPECT_EQ(checked.Flows[0].Source.Owner.Value, 2u);
    EXPECT_EQ(checked.Flows[1].Sink.Owner.Value, 3u);
}

TEST(BehaviorProgram, CompilesAStoredValueUpdate) {
    FakeResolver resolver(Model());
    Program plan;
    const Node wait = plan.RequireOne({"Wait Message"});
    plan.Set(wait.Pin("Value"), Value::From(CKPGUID_INT, 42));

    Ops edit;
    ASSERT_TRUE(plan.Resolve(
        {"mod", "set-value"}, resolver.Base.Root, resolver, edit));
    CheckedOps checked;
    ASSERT_TRUE(edit.Validate(resolver.Base, checked));
    ASSERT_EQ(checked.Sets.size(), 1u);
    EXPECT_EQ(checked.Sets[0].Target.Owner.Value, 2u);
    EXPECT_EQ(checked.Sets[0].Target.Slot.Kind, SlotKind::InputParameter);
    EXPECT_EQ(checked.Sets[0].Value.Literal(),
              Value::From(CKPGUID_INT, 42));
}

TEST(BehaviorProgram, KeepsControlDataAndSpliceDeclarationOrder) {
    FakeResolver resolver(Model());
    Program plan;
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

    Ops edit;
    ASSERT_TRUE(plan.Resolve(
        {"mod", "data"}, resolver.Base.Root, resolver, edit));
    CheckedOps checked;
    ASSERT_TRUE(edit.Validate(resolver.Base, checked));
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

TEST(BehaviorProgram, ResolvesDynamicPortsForTheCurrentLayout) {
    FakeResolver resolver(Model());
    resolver.NodeFlags = CKBEHAVIOR_VARIABLEINPUTS |
                         CKBEHAVIOR_VARIABLEOUTPUTS |
                         CKBEHAVIOR_VARIABLEPARAMETERINPUTS |
                         CKBEHAVIOR_VARIABLEPARAMETEROUTPUTS;

    Program plan;
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

    Ops edit;
    ASSERT_TRUE(plan.Resolve(
        {"mod", "interface"}, resolver.Base.Root, resolver, edit));
    CheckedOps checked;
    ASSERT_TRUE(edit.Validate(resolver.Base, checked));
    ASSERT_EQ(checked.Flows.size(), 2u);
    ASSERT_EQ(checked.Binds.size(), 1u);
    ASSERT_EQ(checked.Pushes.size(), 1u);
    EXPECT_TRUE(checked.Flows[0].Sink.Appended);
    EXPECT_TRUE(checked.Flows[1].Source.Appended);
    EXPECT_TRUE(checked.Binds[0].Target.Appended);
    EXPECT_TRUE(checked.Pushes[0].Source.Appended);
}

TEST(BehaviorProgram, AppendsPrivateStateToTheGraphOrAnAddedBlock) {
    FakeResolver resolver(Model());
    Program plan;
    const Node added = plan.Add(CKGUID(0x3333, 3));
    const Port state = plan.AppendLocal(added, "Scratch", CKPGUID_INT);
    plan.Bind(state, Value::From(CKPGUID_INT, 7));

    ASSERT_TRUE(plan.Validate());
    Ops edit;
    ASSERT_TRUE(plan.Resolve(
        {"mod", "local"}, resolver.Base.Root, resolver, edit));
    CheckedOps checked;
    ASSERT_TRUE(edit.Validate(resolver.Base, checked));
    ASSERT_EQ(checked.Binds.size(), 1u);
    EXPECT_EQ(checked.Binds[0].Target.Slot.Kind, SlotKind::Local);
    EXPECT_TRUE(checked.Binds[0].Target.Appended);

    Program rootState;
    const Port enabled = rootState.AppendLocal(
        rootState.Graph(), "Enabled", CKPGUID_BOOL);
    rootState.Bind(enabled, Value::From(CKPGUID_BOOL, TRUE));
    ASSERT_TRUE(rootState.Validate());
    Ops rootEdit;
    Program::ResolvedSymbols rootSymbols;
    ASSERT_TRUE(rootState.Resolve(
        {"mod", "root-local"}, resolver.Base.Root, resolver, rootEdit,
        &rootSymbols));
    ASSERT_TRUE(rootEdit.Validate(resolver.Base, checked));
    ASSERT_EQ(checked.Binds.size(), 1u);
    EXPECT_EQ(checked.Binds[0].Target.Owner, rootEdit.Graph());
    EXPECT_EQ(checked.Binds[0].Target.Slot.Kind, SlotKind::Local);
    EXPECT_EQ(rootSymbols.Nodes.at(rootState.Graph().Value), rootEdit.Graph());
    ASSERT_EQ(rootSymbols.Ports.size(), 1u);
    EXPECT_EQ(rootSymbols.Ports.begin()->second.Owner, rootEdit.Graph().Value);
    EXPECT_EQ(rootSymbols.Ports.begin()->second.Selector.Kind,
              SlotKind::Local);

    Program borrowed;
    const Node existing = borrowed.RequireOne({"Wait Message"});
    (void) borrowed.AppendLocal(existing, "Scratch", CKPGUID_INT);
    const Status rejected = borrowed.Validate();
    EXPECT_EQ(rejected.Code, Error::InterfaceUnsupported);
}

TEST(BehaviorProgram, ComposesNativeParameterOperationsWithGraphPorts) {
    FakeResolver resolver(Model());
    Program graph;
    const Node sink = graph.RequireOne({"set Resetpoint"});
    const Port base = graph.AppendLocal(graph.Graph(), "Base", CKPGUID_INT);
    const ParameterOperation sum = graph.AddOperation(
        CKGUID(0x12345678, 0x87654321), CKPGUID_INT,
        CKPGUID_INT, CKPGUID_INT);
    graph.Bind(base, Value::From(CKPGUID_INT, 40));
    graph.Bind(sum.Input(0), base);
    graph.Bind(sum.Input(1), Value::From(CKPGUID_INT, 2));
    graph.Bind(sink.Pin("Value"), sum.Result());

    Ops resolved;
    ASSERT_TRUE(graph.Resolve(
        {"mod", "operation"}, resolver.Base.Root, resolver, resolved));
    CheckedOps checked;
    const Status status = resolved.Validate(resolver.Base, checked);
    ASSERT_TRUE(status) << status.Message;
    ASSERT_EQ(checked.Binds.size(), 4u);
    EXPECT_TRUE(checked.Binds[1].Target.Operation);
    EXPECT_TRUE(checked.Binds[2].Target.Operation);
    EXPECT_TRUE(checked.Binds[3].Source.Operation);
}

TEST(BehaviorProgram, ReplacesAnIdleNodeThroughItsPublicInterface) {
    FakeResolver resolver(Model());
    Program graph;
    const Node original = graph.RequireOne(
        {"Wait Message", CKGUID(0x1111, 1)});
    BlockSpec block(CKGUID(0x3333, 3));
    const Node replacement = graph.Replace(original, std::move(block));
    graph.Flow(replacement.Out(), graph.Exit("Done"));

    Ops resolved;
    const Status compiled = graph.Resolve(
        {"mod", "replace"}, resolver.Base.Root, resolver, resolved);
    ASSERT_TRUE(compiled) << compiled.Message;
    CheckedOps checked;
    const Status status = resolved.Validate(resolver.Base, checked);
    ASSERT_TRUE(status) << status.Message;
    ASSERT_EQ(checked.Replacements.size(), 1u);
    EXPECT_EQ(resolver.Adds, 1);
    EXPECT_EQ(checked.Flows.size(), 1u);
}

TEST(BehaviorProgram, RejectsReplacementInterfaceDriftAndParkedNodeUse) {
    FakeResolver mismatch(Model());
    mismatch.AddedShape.Slots.erase(
        std::remove_if(mismatch.AddedShape.Slots.begin(),
                       mismatch.AddedShape.Slots.end(),
                       [](const SlotInfo &slot) {
                           return slot.Kind == SlotKind::OutputParameter;
                       }),
        mismatch.AddedShape.Slots.end());
    Program changed;
    const Node original = changed.RequireOne({"Wait Message"});
    (void) changed.Replace(original, BlockSpec(CKGUID(0x3333, 3)));
    Ops resolved;
    const Status compiled = changed.Resolve(
        {"mod", "replace-shape"}, mismatch.Base.Root, mismatch, resolved);
    ASSERT_TRUE(compiled) << compiled.Message;
    CheckedOps checked;
    EXPECT_EQ(resolved.Validate(mismatch.Base, checked).Code,
              Error::InterfaceUnsupported);

    Program reused;
    const Node parked = reused.RequireOne({"Wait Message"});
    (void) reused.Replace(parked, BlockSpec(CKGUID(0x3333, 3)));
    reused.Flow(parked.Out(), reused.Exit("Done"));
    EXPECT_EQ(reused.Validate().Code, Error::InvalidState);
}

TEST(BehaviorProgram, ResolvesRemovalOfAnIdleChildNode) {
    FakeResolver resolver(Model());
    Program graph;
    const Node removed = graph.RequireOne({"Wait Message"});
    graph.Remove(removed);

    ASSERT_TRUE(graph.Validate());
    Ops resolved;
    const Status compiled = graph.Resolve(
        {"mod", "remove"}, resolver.Base.Root, resolver, resolved);
    ASSERT_TRUE(compiled) << compiled.Message;
    CheckedOps checked;
    const Status status = resolved.Validate(resolver.Base, checked);
    ASSERT_TRUE(status) << status.Message;
    ASSERT_EQ(checked.Removals.size(), 1u);
    EXPECT_EQ(resolver.Adds, 0);
}

TEST(BehaviorProgram, RejectsUnsafeOrContradictoryNodeRemoval) {
    Program reused;
    const Node removed = reused.RequireOne({"Wait Message"});
    reused.Remove(removed);
    reused.Flow(removed.Out(), reused.Exit("Done"));
    EXPECT_EQ(reused.Validate().Code, Error::InvalidState);

    Program duplicate;
    const Node twice = duplicate.RequireOne({"Wait Message"});
    duplicate.Remove(twice);
    duplicate.Remove(twice);
    EXPECT_EQ(duplicate.Validate().Code, Error::InvalidState);

    FakeResolver resolver(Model());
    resolver.Base.Nodes.front().Active = true;
    Program active;
    const Node target = active.RequireOne({"Wait Message"});
    active.Remove(target);
    Ops resolved;
    ASSERT_TRUE(active.Resolve(
        {"mod", "active-remove"}, resolver.Base.Root, resolver, resolved));
    CheckedOps checked;
    EXPECT_TRUE(resolved.Validate(resolver.Base, checked));
}

TEST(BehaviorProgram, CompilesAdjacentReplaceAndRemoveTogether) {
    FakeResolver resolver(Model());
    Program graph;
    const Node removed = graph.RequireOne({"Wait Message"});
    const Node original = graph.RequireOne({"set Resetpoint"});
    (void) graph.Replace(original, BlockSpec(CKGUID(0x3333, 3)));
    graph.Remove(removed);

    ASSERT_TRUE(graph.Validate());
    Ops resolved;
    const Status compiled = graph.Resolve(
        {"mod", "replace-remove"}, resolver.Base.Root, resolver, resolved);
    ASSERT_TRUE(compiled) << compiled.Message;
    CheckedOps checked;
    const Status checkedStatus = resolved.Validate(resolver.Base, checked);
    ASSERT_TRUE(checkedStatus) << checkedStatus.Message;
    EXPECT_EQ(checked.Replacements.size(), 1u);
    EXPECT_EQ(checked.Removals.size(), 1u);
}

TEST(BehaviorProgram, RejectsIncompleteAndCyclicParameterOperations) {
    FakeResolver resolver(Model());
    Program incomplete;
    const ParameterOperation missing = incomplete.AddOperation(
        CKGUID(0x12345678, 1), CKPGUID_INT, CKPGUID_INT, CKPGUID_NONE);
    (void) missing;
    Ops resolved;
    ASSERT_TRUE(incomplete.Resolve(
        {"mod", "missing-operation-input"}, resolver.Base.Root,
        resolver, resolved));
    CheckedOps checked;
    EXPECT_EQ(resolved.Validate(resolver.Base, checked).Code,
              Error::OperationInvalid);

    Program cyclic;
    const ParameterOperation first = cyclic.AddOperation(
        CKGUID(0x12345678, 1), CKPGUID_INT, CKPGUID_INT, CKPGUID_NONE);
    const ParameterOperation second = cyclic.AddOperation(
        CKGUID(0x12345678, 1), CKPGUID_INT, CKPGUID_INT, CKPGUID_NONE);
    cyclic.Bind(first.Input(0), second.Result());
    cyclic.Bind(second.Input(0), first.Result());
    ASSERT_TRUE(cyclic.Resolve(
        {"mod", "operation-cycle"}, resolver.Base.Root,
        resolver, resolved));
    EXPECT_EQ(resolved.Validate(resolver.Base, checked).Code,
              Error::OperationInvalid);
}

TEST(BehaviorProgram, KeepsTypedNullInAPlan) {
    FakeResolver resolver(Model());
    Program plan;
    const Node wait = plan.RequireOne(
        {"Wait Message", CKGUID(0x1111, 1)});
    plan.Bind(wait.Pin("Value"), Value::Null(CKPGUID_OBJECT));

    EXPECT_TRUE(plan.Validate());
    Ops edit;
    ASSERT_TRUE(plan.Resolve(
        {"mod", "null"}, resolver.Base.Root, resolver, edit));
    CheckedOps checked;
    ASSERT_TRUE(edit.Validate(resolver.Base, checked));
    ASSERT_EQ(checked.Binds.size(), 1u);
    EXPECT_EQ(checked.Binds[0].Value.Kind(), Parameter::BindingKind::Value);
    EXPECT_EQ(checked.Binds[0].Value.Literal().Kind(), ValueKind::Null);
    EXPECT_TRUE(checked.Binds[0].Value.Literal().IsNull());
}

TEST(BehaviorProgram, CarriesASettingWithTheBlockThatDeclaresIt) {
    FakeResolver resolver(Model());
    Program plan;
    BlockSpec block(CKGUID(0x3333, 3));
    block.Setting(Slot::Named(SlotKind::Setting, "Mode"),
                  Value::From(CKPGUID_INT, 2));
    (void) plan.Add(std::move(block));

    ASSERT_TRUE(plan.Validate());
    Ops edit;
    ASSERT_TRUE(plan.Resolve(
        {"mod", "setting"}, resolver.Base.Root, resolver, edit));
    EXPECT_EQ(resolver.Adds, 1);
    ASSERT_EQ(resolver.AddedSettings.size(), 1u);
    ASSERT_EQ(resolver.AddedSettings[0].size(), 1u);
    EXPECT_EQ(resolver.AddedSettings[0][0].Target.Kind, SlotKind::Setting);
    const Value &declared = resolver.AddedSettings[0][0].Source.Literal();
    EXPECT_EQ(declared.Type(), CKPGUID_INT);
    ASSERT_EQ(declared.Bytes().size(), sizeof(int));
    int mode = 0;
    std::memcpy(&mode, declared.Bytes().data(), sizeof(mode));
    EXPECT_EQ(mode, 2);
    // A Setting travels with the Block, so it is not an action the Edit
    // replays afterwards.
    CheckedOps checked;
    ASSERT_TRUE(edit.Validate(resolver.Base, checked));
    EXPECT_TRUE(checked.Binds.empty());
}

TEST(BehaviorProgram, PreservesSettingStageBoundaries) {
    FakeResolver resolver(Model());
    Program edit;
    BlockSpec block(CKGUID(0x3333, 3));
    block.Setting(Slot::Named(SlotKind::Setting, "Mode"),
                  Value::From(CKPGUID_INT, 2));
    block.NextSettingStage().Setting(
        Slot::Named(SlotKind::Setting, "Created Later"),
        Value::From(CKPGUID_INT, 9));
    (void) edit.Add(std::move(block));

    Ops compiled;
    ASSERT_TRUE(edit.Resolve(
        {"mod", "setting-stages"}, resolver.Base.Root, resolver, compiled));
    ASSERT_EQ(resolver.AddedSettings.size(), 2u);
    ASSERT_EQ(resolver.AddedSettings[0].size(), 1u);
    ASSERT_EQ(resolver.AddedSettings[1].size(), 1u);
    EXPECT_EQ(resolver.AddedSettings[0][0].Target.Name, "Mode");
    EXPECT_EQ(resolver.AddedSettings[1][0].Target.Name, "Created Later");
}

TEST(BehaviorProgram, CompletesAPathAgainAndTapsItsFinalOut) {
    FakeResolver resolver(Model());
    Program plan;
    const Node wait = plan.RequireOne(
        {"Wait Message", CKGUID(0x1111, 1)});
    (void) plan.RequireOne(
        {"set Resetpoint", CKGUID(0x2222, 2)});
    const PathRef path = plan.Follow(wait.Out());
    plan.After(path, HookBlock::Hook(Noop));

    Ops edit;
    ASSERT_TRUE(plan.Resolve(
        {"mod", "after"}, resolver.Base.Root, resolver, edit));
    EXPECT_EQ(resolver.Taps, 1);
    EXPECT_EQ(resolver.Afters, 0);
    EXPECT_EQ(resolver.UsedLinks,
              (std::vector<ObjectRef>{Ref(201)}));
    EXPECT_EQ(resolver.UsedNodes,
              (std::vector<ObjectRef>{Ref(101), Ref(102)}));

    CheckedOps checked;
    ASSERT_TRUE(edit.Validate(resolver.Base, checked));
    ASSERT_EQ(checked.Taps.size(), 1u);
    EXPECT_EQ(checked.Taps.front().Source.Slot.Kind, SlotKind::Output);
    EXPECT_EQ(checked.Taps.front().Source.Owner.Value, 3u);
}

TEST(BehaviorProgram, PlacesAfterBeforeAGraphExit) {
    GraphModel graph = Model();
    graph.Links.push_back({
        2, Ref(202),
        {102, SlotKind::Output, 0},
        {100, SlotKind::Output, 0}, 0});
    FakeResolver resolver(std::move(graph));
    Program plan;
    const Node wait = plan.RequireOne({"Wait Message"});
    plan.After(plan.Follow(wait.Out()), HookBlock::Hook(Noop));

    Ops edit;
    ASSERT_TRUE(plan.Resolve(
        {"mod", "exit"}, resolver.Base.Root, resolver, edit));
    EXPECT_EQ(resolver.Taps, 0);
    EXPECT_EQ(resolver.Afters, 1);
    EXPECT_EQ(resolver.UsedLinks,
              (std::vector<ObjectRef>{Ref(201), Ref(202)}));

    CheckedOps checked;
    ASSERT_TRUE(edit.Validate(resolver.Base, checked));
    ASSERT_EQ(checked.Splices.size(), 1u);
    EXPECT_EQ(checked.Splices.front().Target.Anchor, Ref(202));
}

TEST(BehaviorProgram, RejectsAnAmbiguousPathBeforeInstallingAHook) {
    GraphModel graph = Model();
    graph.Links.push_back({
        2, Ref(202),
        {101, SlotKind::Output, 0},
        {100, SlotKind::Output, 0}, 0});
    FakeResolver resolver(std::move(graph));
    Program plan;
    const Node wait = plan.RequireOne({"Wait Message"});
    plan.After(plan.Follow(wait.Out()), HookBlock::Hook(Noop));

    Ops edit;
    const Status status = plan.Resolve(
        {"mod", "ambiguous-path"}, resolver.Base.Root, resolver, edit);
    EXPECT_EQ(status.Code, Error::PathAmbiguous);
    EXPECT_EQ(resolver.Taps, 0);
    EXPECT_EQ(resolver.Afters, 0);
}


TEST(BehaviorProgram, NamesANodeAndALinkTheAuthorAlreadyHolds) {
    FakeResolver resolver(Model());
    Program plan;
    const Node wait = plan.UseNode(Ref(101));
    const Link edge = plan.UseLink(Ref(201));
    const Node hook = plan.Add(CKGUID(0x3333, 3));
    plan.Splice(edge, hook);
    EXPECT_TRUE(plan.UsesIdentity());

    Ops edit;
    Program::ResolvedSymbols symbols;
    const Status status = plan.Resolve(
        {"mod", "by-reference"}, resolver.Base.Root, resolver, edit, &symbols);
    ASSERT_TRUE(status) << status.Message;
    EXPECT_EQ(resolver.UsedNodes, (std::vector<ObjectRef>{Ref(101)}));
    EXPECT_EQ(resolver.UsedLinks, (std::vector<ObjectRef>{Ref(201)}));
    EXPECT_EQ(resolver.Adds, 1);

    // Every named Node is reported back, including the graph itself.
    EXPECT_EQ(symbols.Nodes.count(wait.Value), 1u);
    EXPECT_EQ(symbols.Nodes.count(hook.Value), 1u);
    EXPECT_EQ(symbols.Nodes.count(plan.Graph().Value), 1u);

    CheckedOps checked;
    ASSERT_TRUE(edit.Validate(resolver.Base, checked));
    ASSERT_EQ(checked.Splices.size(), 1u);
    EXPECT_EQ(checked.Splices.front().Target.Anchor, Ref(201));
}

TEST(BehaviorProgram, ReportsAQueriedNodeUnderItsOwnHandle) {
    FakeResolver resolver(Model());
    Program plan;
    const Node wait = plan.RequireOne({"Wait Message", CKGUID(0x1111, 1)});
    EXPECT_FALSE(plan.UsesIdentity());

    Ops edit;
    Program::ResolvedSymbols symbols;
    ASSERT_TRUE(plan.Resolve(
        {"mod", "queried"}, resolver.Base.Root, resolver, edit, &symbols));
    EXPECT_EQ(symbols.Nodes.count(wait.Value), 1u);
}

TEST(BehaviorProgram, RejectsANodeReferenceOutsideTheTargetGraph) {
    FakeResolver resolver(Model());
    Program plan;
    (void) plan.UseNode(Ref(999));

    Ops edit;
    const Status status = plan.Resolve(
        {"mod", "foreign-node"}, resolver.Base.Root, resolver, edit);
    EXPECT_EQ(status.Code, Error::InvalidGraphLocality);
    EXPECT_TRUE(resolver.UsedNodes.empty());
}

TEST(BehaviorProgram, RejectsALinkReferenceOutsideTheTargetGraph) {
    FakeResolver resolver(Model());
    Program plan;
    (void) plan.UseLink(Ref(999));

    Ops edit;
    const Status status = plan.Resolve(
        {"mod", "foreign-link"}, resolver.Base.Root, resolver, edit);
    EXPECT_EQ(status.Code, Error::LinkNotFound);
    EXPECT_TRUE(resolver.UsedLinks.empty());
}

TEST(BehaviorProgram, ComposesIdentityNodesWithQueriedLinks) {
    FakeResolver resolver(Model());
    Program plan;
    const Node added = plan.Add(CKGUID(0x3333, 3));
    (void) added;
    const Node wait = plan.UseNode(Ref(101));
    const Node sink = plan.RequireOne({"set Resetpoint"});
    // An endpoint query cannot be mixed with a Link named by identity, so the
    // two ways of naming the same edge stay separate.
    const Link queried = plan.RequireOne(wait.Out(), sink.In());
    (void) queried;

    Ops edit;
    ASSERT_TRUE(plan.Resolve(
        {"mod", "mixed"}, resolver.Base.Root, resolver, edit))
        << "identity nodes and queried links compose";
}

TEST(BehaviorProgram, PutsACallbackOnALinkTheAuthorNamed) {
    FakeResolver resolver(Model());
    Program plan;
    const Link edge = plan.UseLink(Ref(201));
    plan.Before(edge, HookBlock::Hook(Noop));

    Ops edit;
    const Status status = plan.Resolve(
        {"mod", "before"}, resolver.Base.Root, resolver, edit);
    ASSERT_TRUE(status) << status.Message;
    EXPECT_EQ(resolver.UsedLinks, (std::vector<ObjectRef>{Ref(201)}));
    EXPECT_EQ(resolver.Taps, 0);
    EXPECT_EQ(resolver.Afters, 1);
    ASSERT_EQ(resolver.InterposedLinks.size(), 1u);
}

TEST(BehaviorProgram, PutsACallbackOnAQueriedLink) {
    FakeResolver resolver(Model());
    Program plan;
    const Node wait = plan.RequireOne({"Wait Message"});
    const Node sink = plan.RequireOne({"set Resetpoint"});
    plan.Before(plan.RequireOne(wait.Out(), sink.In()),
                HookBlock::Hook(Noop));

    Ops edit;
    const Status status = plan.Resolve(
        {"mod", "before-query"}, resolver.Base.Root, resolver, edit);
    ASSERT_TRUE(status) << status.Message;
    EXPECT_EQ(resolver.Afters, 1);
}

TEST(BehaviorProgram, RejectsABeforeWithoutALinkOrACallback) {
    FakeResolver resolver(Model());
    Program missingHook;
    missingHook.Before(missingHook.UseLink(Ref(201)), HookBlock::Hook());
    Ops edit;
    Status status = missingHook.Resolve(
        {"mod", "no-hook"}, resolver.Base.Root, resolver, edit);
    EXPECT_EQ(status.Code, Error::InvalidState);

    Program foreignLink;
    foreignLink.Before(Link{}, HookBlock::Hook(Noop));
    status = foreignLink.Resolve(
        {"mod", "no-link"}, resolver.Base.Root, resolver, edit);
    EXPECT_EQ(status.Code, Error::InvalidState);
    EXPECT_EQ(resolver.Afters, 0);
}

TEST(BehaviorProgram, CompilesARedirectOntoTheExactLiveLink) {
    FakeResolver resolver(Model());
    Program plan;
    const Node wait = plan.RequireOne({"Wait Message", CKGUID(0x1111, 1)});
    const Node sink = plan.RequireOne({"set Resetpoint", CKGUID(0x2222, 2)});
    const Link edge = plan.RequireOne(wait.Out(), sink.In());
    const Node detour = plan.Add(CKGUID(0x3333, 3));
    plan.Redirect(edge, detour.In());
    ASSERT_TRUE(plan.Validate());

    Ops edit;
    ASSERT_TRUE(plan.Resolve(
        {"mod", "detour"}, resolver.Base.Root, resolver, edit));
    EXPECT_EQ(resolver.UsedLinks, (std::vector<ObjectRef>{Ref(201)}));

    CheckedOps checked;
    ASSERT_TRUE(edit.Validate(resolver.Base, checked));
    EXPECT_TRUE(checked.Splices.empty());
    ASSERT_EQ(checked.Redirects.size(), 1u);
    EXPECT_EQ(checked.Redirects.front().Target.Anchor, Ref(201));
}

TEST(BehaviorProgram, CompilesReconnectOntoTheExactLiveLink) {
    FakeResolver resolver(Model());
    Program plan;
    const Node wait = plan.RequireOne({"Wait Message", CKGUID(0x1111, 1)});
    const Node sink = plan.RequireOne(
        {"set Resetpoint", CKGUID(0x2222, 2)});
    const Link edge = plan.RequireOne(wait.Out(), sink.In());
    const Node source = plan.Add(CKGUID(0x3333, 3));
    plan.Reconnect(edge, source.Out(), plan.Exit("Done"), Cycle::Confirmed);
    ASSERT_TRUE(plan.Validate());

    Ops edit;
    ASSERT_TRUE(plan.Resolve(
        {"mod", "reconnect"}, resolver.Base.Root, resolver, edit));
    EXPECT_EQ(resolver.UsedLinks, (std::vector<ObjectRef>{Ref(201)}));

    CheckedOps checked;
    const Status status = edit.Validate(resolver.Base, checked);
    ASSERT_TRUE(status) << status.Message;
    ASSERT_EQ(checked.Reconnections.size(), 1u);
    EXPECT_EQ(checked.Reconnections.front().Target.Anchor, Ref(201));
    EXPECT_EQ(checked.Reconnections.front().Source.Owner.Value, 4u);
    EXPECT_EQ(checked.Reconnections.front().Sink.Owner, edit.Graph());
    EXPECT_EQ(checked.Reconnections.front().SameFrameCycle,
              Cycle::Confirmed);
}

TEST(BehaviorProgram, CompilesAConfiguredVariableParameterBlock) {
    FakeResolver resolver(Model());
    Program plan;
    BlockSpec block(CKGUID(0x3333, 3));
    block.PinType(Slot::At(SlotKind::InputParameter, 0), CKPGUID_BOOL)
        .PoutType(Slot::At(SlotKind::OutputParameter, 0), CKPGUID_BOOL);
    const Node identity = plan.Add(std::move(block));
    plan.Bind(identity.Pin(0), Value::From(CKPGUID_BOOL, true));
    ASSERT_TRUE(plan.Validate());

    Ops edit;
    ASSERT_TRUE(plan.Resolve(
        {"mod", "parameter-types"}, resolver.Base.Root, resolver, edit));
    CheckedOps checked;
    const Status status = edit.Validate(resolver.Base, checked);
    ASSERT_TRUE(status) << status.Message;
    ASSERT_EQ(checked.Binds.size(), 1u);
    EXPECT_EQ(checked.Binds[0].Target.Slot.Type, CKPGUID_BOOL);
}

TEST(BehaviorProgram, RefusesARedirectThatNamesNothing) {
    Program plan;
    const Node wait = plan.RequireOne({"Wait Message"});
    const Node sink = plan.RequireOne({"set Resetpoint"});
    const Port stranger{9999, Slot::Only(SlotKind::Input)};
    plan.Redirect(plan.RequireOne(wait.Out(), sink.In()), stranger);
    const Status status = plan.Validate();
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::InvalidState);
}

TEST(BehaviorProgram, CompilesAPlainGraphNodeWithoutABlockPrototype) {
    FakeResolver resolver(Model());
    Program plan;
    const Node graph = plan.AddGraph("Nested", 23);
    plan.AppendIn(graph, "Start");
    plan.AppendOut(graph, "Done");

    Ops edit;
    const Status status = plan.Resolve(
        {"mod", "add-graph"}, resolver.Base.Root, resolver, edit);
    ASSERT_TRUE(status) << status.Message;
    EXPECT_EQ(resolver.Adds, 1);
    EXPECT_EQ(resolver.AddedGraphName, "Nested");
    EXPECT_EQ(resolver.AddedGraphPriority, 23);

    CheckedOps checked;
    ASSERT_TRUE(edit.Validate(resolver.Base, checked));
}

TEST(BehaviorProgram, KeepsNestedGraphBodiesInTheirOwnScope) {
    Program plan;
    const Node nestedNode = plan.AddGraph("Nested");
    Program &nested = plan.Enter(nestedNode, 7);
    const Node child = nested.Add(CKGUID(0x3333, 3));
    nested.Flow(nested.Entry(), child.In());

    ASSERT_TRUE(plan.Validate());
    ASSERT_EQ(plan.NestedGraphs().size(), 1u);
    EXPECT_EQ(plan.NestedGraphs()[0].Parent, nestedNode);
    EXPECT_EQ(plan.NestedGraphs()[0].Scope, 7u);
    ASSERT_NE(plan.NestedGraphs()[0].Body, nullptr);
    EXPECT_TRUE(plan.NestedGraphs()[0].Body->Validate());
}

TEST(BehaviorProgram, PublishesNestedPublicPortsWithTheParentNode) {
    FakeResolver resolver(Model());
    Program plan;
    const Node nestedNode = plan.AddGraph("Nested");
    Program &nested = plan.Enter(nestedNode, 7);
    const Port done = nested.AppendOut(nested.Graph(), "Done");
    const Node child = nested.Add(CKGUID(0x3333, 3));
    nested.Flow(child.Out(), done);

    // The parent Graph may immediately address the public output declared by
    // the nested scope. Its CK port must therefore be part of the parent
    // transaction, before the nested Graph's internal flow is published.
    plan.Flow(nestedNode.Out("Done"), plan.Exit("Done"));

    Ops parent;
    const Status parentStatus = plan.Resolve(
        {"mod", "nested-interface"}, resolver.Base.Root, resolver, parent);
    ASSERT_TRUE(parentStatus) << parentStatus.Message;
    CheckedOps parentChecked;
    const Status parentValidation = parent.Validate(
        resolver.Base, parentChecked);
    ASSERT_TRUE(parentValidation) << parentValidation.Message;
    ASSERT_EQ(parentChecked.Flows.size(), 1u);

    GraphModel nestedModel;
    nestedModel.Root = Ref(301);
    nestedModel.Fingerprint = 901;
    nestedModel.Nodes = {
        NodeOf(301, nestedModel.Root, 0, CKGUID(), "Nested",
               {In(0, "Start"), Out(0, "Done")}),
    };
    FakeResolver nestedCompiler(std::move(nestedModel));
    Ops body;
    const Status bodyStatus = nested.Resolve(
        {"mod", "nested-interface/7"}, nestedCompiler.Base.Root,
        nestedCompiler, body, nullptr, true);
    ASSERT_TRUE(bodyStatus) << bodyStatus.Message;
    CheckedOps bodyChecked;
    const Status bodyValidation = body.Validate(
        nestedCompiler.Base, bodyChecked);
    ASSERT_TRUE(bodyValidation) << bodyValidation.Message;
    ASSERT_EQ(bodyChecked.Flows.size(), 1u);
}

TEST(BehaviorProgram, KeepsNestedGraphLocalsOutOfTheParentInterface) {
    FakeResolver resolver(Model());
    Program plan;
    const Node nestedNode = plan.RequireOne(
        NodePattern{"Wait Message", CKGUID(0x1111, 1)});
    Program &nested = plan.Enter(nestedNode, 7);
    nested.AppendLocal(nested.Graph(), "Scratch", CKPGUID_INT);

    Ops parent;
    const Status parentStatus = plan.Resolve(
        {"mod", "nested-local"}, resolver.Base.Root, resolver, parent);
    ASSERT_TRUE(parentStatus) << parentStatus.Message;
    CheckedOps parentChecked;
    const Status parentValidation = parent.Validate(
        resolver.Base, parentChecked);
    ASSERT_TRUE(parentValidation) << parentValidation.Message;

    GraphModel nestedModel;
    nestedModel.Root = Ref(301);
    nestedModel.Fingerprint = 902;
    nestedModel.Nodes = {
        NodeOf(301, nestedModel.Root, 0, CKGUID(), "Nested",
               {In(0, "Start"), Out(0, "Done")}),
    };
    FakeResolver nestedCompiler(std::move(nestedModel));
    Ops body;
    const Status bodyStatus = nested.Resolve(
        {"mod", "nested-local/7"}, nestedCompiler.Base.Root,
        nestedCompiler, body, nullptr, true);
    ASSERT_TRUE(bodyStatus) << bodyStatus.Message;
    CheckedOps bodyChecked;
    const Status bodyValidation = body.Validate(
        nestedCompiler.Base, bodyChecked);
    ASSERT_TRUE(bodyValidation) << bodyValidation.Message;
}

TEST(BehaviorProgram, ComparesAuthoredDefinitionsAcrossOwnedCopies) {
    int callbackState = 0;
    const auto define = [&](int value) {
        Program edit;
        const Node existing = edit.RequireOne(
            NodePattern{"Counter", CKGUID(0x1111, 1)});
        BlockSpec block(CKGUID(0x3333, 3));
        block.PrototypeGeneration(7)
            .Pin("Value", CKGUID(0x4444, 4), value);
        const Node added = edit.Add(std::move(block));
        edit.Flow(existing.Out(), added.In());
        edit.Tap(added.Out(), HookBlock::Hook(Noop, &callbackState));
        const Node nestedNode = edit.AddGraph("Nested", 5);
        Program &nested = edit.Enter(nestedNode, 12);
        const Node child = nested.Add(CKGUID(0x5555, 5));
        nested.Flow(nested.Entry(), child.In());
        return edit;
    };

    Program first = define(9);
    Program same = define(9);
    Program changed = define(10);
    EXPECT_TRUE(first.SameAs(same));
    EXPECT_TRUE(same.SameAs(first));
    EXPECT_FALSE(first.SameAs(changed));
}

} // namespace

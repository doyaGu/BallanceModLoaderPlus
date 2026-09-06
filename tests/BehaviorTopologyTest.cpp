#include "Behavior/Topology.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace {

using namespace BML::Behavior::Internal;

GraphEndpoint Endpoint(std::uint64_t node, SlotKind kind, int index) {
    return {node, kind, index};
}

LinkBase Base(std::uint32_t slot, std::uint64_t source, std::uint64_t sink,
              int delay = 0) {
    return {{7, slot, 11},
            Endpoint(source, SlotKind::Output, 0),
            Endpoint(sink, SlotKind::Input, 0),
            delay};
}

LinkId Identify(Topology &topology, const LinkBase &base) {
    LinkId id;
    EXPECT_TRUE(topology.Identify(base, id));
    EXPECT_TRUE(id);
    return id;
}

GraphNode GraphNodeWithPorts(std::uint64_t id, std::uint64_t parent,
                             std::string name) {
    GraphNode node;
    node.Id = id;
    node.Parent = parent;
    node.Name = std::move(name);
    node.Ports = {
        {SlotKind::Input, 0, 0, 0, CKGUID(), false, "In", false},
        {SlotKind::Output, 0, 0, 0, CKGUID(), false, "Out", false},
    };
    return node;
}

PatchLayer OverlayPatch(std::string owner, std::string name, int priority, LinkId link,
                        std::vector<Overlay> items, std::vector<Order> order = {}) {
    PatchLayer patch;
    patch.Patch = {std::move(owner), std::move(name)};
    patch.Priority = priority;
    patch.Links.push_back({link, std::move(order), std::move(items)});
    return patch;
}

PatchLayer TapPatch(std::string owner, std::string name, int priority,
                    GraphEndpoint out, std::vector<OutTap> items) {
    PatchLayer patch;
    patch.Patch = {std::move(owner), std::move(name)};
    patch.Priority = priority;
    patch.Outs.push_back({out, std::move(items)});
    return patch;
}

std::vector<std::string> OverlayNames(const Topology &topology, LinkId id) {
    const LogicalLink *link = topology.Find(id);
    EXPECT_NE(link, nullptr);
    std::vector<std::string> names;
    if (!link)
        return names;
    for (const OrderedOverlay &overlay : link->Overlays) {
        names.push_back(overlay.Patch.Owner + ":" + overlay.Patch.Name + "#" +
                        std::to_string(overlay.Ordinal));
    }
    return names;
}

TEST(BehaviorTopology, LogicalIdentityUsesTheAnchorNotEndpointGuessing) {
    Topology topology;
    const LinkBase first = Base(31, 100, 200, 2);
    LinkId firstId = Identify(topology, first);
    LinkId repeated;
    ASSERT_TRUE(topology.Identify(first, repeated));
    EXPECT_EQ(repeated, firstId);

    LinkBase parallel = first;
    parallel.Anchor.Slot = 32;
    LinkId parallelId = Identify(topology, parallel);
    EXPECT_NE(parallelId, firstId);

    LinkBase changed = first;
    changed.Sink.Index = 1;
    LinkId rejected{999};
    const Status status = topology.Identify(changed, rejected);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::GraphChanged);
    EXPECT_FALSE(rejected);
    EXPECT_EQ(topology.Find(firstId)->Base, first);
}

TEST(BehaviorTopology, AcceptsBoundaryLinksAndRejectsInvalidBases) {
    Topology topology;
    LinkBase boundary = Base(1, 10, 20, 1);
    boundary.Source.Kind = SlotKind::Input;
    boundary.Sink.Kind = SlotKind::Output;
    EXPECT_TRUE(Identify(topology, boundary));

    LinkId id;
    LinkBase invalid = Base(2, 10, 20, -1);
    EXPECT_FALSE(topology.Identify(invalid, id));
    invalid = Base(2, 10, 20);
    invalid.Anchor = {};
    EXPECT_FALSE(topology.Identify(invalid, id));
}

TEST(BehaviorTopology, StableOrderIgnoresInstallationOrder) {
    const std::array<std::array<int, 3>, 6> permutations{{
        {{0, 1, 2}},
        {{0, 2, 1}},
        {{1, 0, 2}},
        {{1, 2, 0}},
        {{2, 0, 1}},
        {{2, 1, 0}},
    }};
    std::uint64_t expectedFingerprint = 0;
    for (const auto &order : permutations) {
        Topology topology;
        const LinkId link = Identify(topology, Base(1, 100, 200));
        std::array<PatchLayer, 3> patches{
            OverlayPatch("mod", "alpha", 10, link,
                         {{OverlayKind::Tap, 4, 104}, {OverlayKind::Splice, 1, 101}}),
            OverlayPatch("mod", "beta", 0, link, {{OverlayKind::Splice, 2, 202}}),
            OverlayPatch("mod", "charlie", -5, link, {{OverlayKind::Tap, 3, 303}},
                         {{OrderKind::After, {"mod", "alpha"}}}),
        };
        for (const int index : order)
            ASSERT_TRUE(topology.Set(patches[static_cast<std::size_t>(index)]));

        EXPECT_EQ(OverlayNames(topology, link),
                  (std::vector<std::string>{"mod:beta#2", "mod:alpha#1", "mod:alpha#4",
                                            "mod:charlie#3"}));
        if (!expectedFingerprint)
            expectedFingerprint = topology.Fingerprint();
        EXPECT_EQ(topology.Fingerprint(), expectedFingerprint);
    }
}

TEST(BehaviorTopology, ReordersWhenAMissingTargetAppearsAndDisappears) {
    Topology topology;
    const LinkId link = Identify(topology, Base(1, 100, 200));
    ASSERT_TRUE(topology.Set(OverlayPatch("mod", "alpha", 20, link,
                                          {{OverlayKind::Splice, 1, 1}},
                                          {{OrderKind::Before, {"mod", "target"}}})));
    ASSERT_TRUE(
        topology.Set(OverlayPatch("mod", "beta", 0, link, {{OverlayKind::Tap, 2, 2}})));
    EXPECT_EQ(OverlayNames(topology, link),
              (std::vector<std::string>{"mod:beta#2", "mod:alpha#1"}));

    ASSERT_TRUE(topology.Set(
        OverlayPatch("mod", "target", -10, link, {{OverlayKind::Splice, 3, 3}})));
    EXPECT_EQ(OverlayNames(topology, link),
              (std::vector<std::string>{"mod:beta#2", "mod:alpha#1", "mod:target#3"}));

    EXPECT_TRUE(topology.Remove({"mod", "target"}));
    EXPECT_EQ(OverlayNames(topology, link),
              (std::vector<std::string>{"mod:beta#2", "mod:alpha#1"}));
}

TEST(BehaviorTopology, RejectsOrderingAcrossLogicalLinksOrOutTaps) {
    Topology topology;
    const LinkId first = Identify(topology, Base(1, 100, 200));
    const LinkId second = Identify(topology, Base(2, 300, 400));
    ASSERT_TRUE(topology.Set(
        OverlayPatch("target", "patch", 0, second, {{OverlayKind::Splice, 1, 1}})));
    const std::uint64_t before = topology.Fingerprint();

    Status status = topology.Set(
        OverlayPatch("source", "patch", 0, first, {{OverlayKind::Splice, 2, 2}},
                     {{OrderKind::Before, {"target", "patch"}}}));
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::OrderingTargetMismatch);
    EXPECT_EQ(topology.Fingerprint(), before);
    EXPECT_TRUE(OverlayNames(topology, first).empty());

    Topology tapsOnly;
    const LinkId link = Identify(tapsOnly, Base(3, 100, 200));
    ASSERT_TRUE(tapsOnly.Set(
        TapPatch("target", "tap", 0, Endpoint(100, SlotKind::Output, 0), {{1, 11}})));
    status = tapsOnly.Set(OverlayPatch("source", "patch", 0, link,
                                       {{OverlayKind::Tap, 2, 22}},
                                       {{OrderKind::After, {"target", "tap"}}}));
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::OrderingTargetMismatch);
}

TEST(BehaviorTopology, RejectsOrderCyclesWithoutPublishingTheCandidate) {
    Topology topology;
    const LinkId link = Identify(topology, Base(1, 100, 200));
    ASSERT_TRUE(topology.Set(OverlayPatch("mod", "alpha", 0, link,
                                          {{OverlayKind::Splice, 1, 1}},
                                          {{OrderKind::Before, {"mod", "beta"}}})));
    const std::uint64_t before = topology.Fingerprint();

    const Status status =
        topology.Set(OverlayPatch("mod", "beta", 0, link, {{OverlayKind::Tap, 2, 2}},
                                  {{OrderKind::Before, {"mod", "alpha"}}}));
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::OverlayOrderCycle);
    EXPECT_NE(status.Message.find("mod:alpha -> mod:beta -> mod:alpha"),
              std::string::npos);
    EXPECT_EQ(topology.Fingerprint(), before);
    EXPECT_EQ(OverlayNames(topology, link), (std::vector<std::string>{"mod:alpha#1"}));
}

TEST(BehaviorTopology, KeepsOutTapsSeparateFromLinkOverlays) {
    Topology topology;
    const LinkId link = Identify(topology, Base(1, 100, 200));
    const GraphEndpoint out = Endpoint(100, SlotKind::Output, 0);
    ASSERT_TRUE(topology.Set(
        OverlayPatch("flow", "link-tap", 0, link, {{OverlayKind::Tap, 1, 101}})));
    const std::uint64_t linkFingerprint = topology.Find(link)->Fingerprint;
    const std::uint64_t withoutOutTap = topology.Fingerprint();

    ASSERT_TRUE(topology.Set(TapPatch("flow", "out-tap", 0, out, {{2, 202}})));
    ASSERT_NE(topology.Taps(out), nullptr);
    ASSERT_EQ(topology.Taps(out)->size(), 1u);
    EXPECT_EQ(topology.Find(link)->Fingerprint, linkFingerprint);
    EXPECT_NE(topology.Fingerprint(), withoutOutTap);

    EXPECT_TRUE(topology.Remove({"flow", "out-tap"}));
    EXPECT_EQ(topology.Taps(out), nullptr);
    EXPECT_EQ(topology.Find(link)->Fingerprint, linkFingerprint);
    EXPECT_EQ(topology.Fingerprint(), withoutOutTap);
    EXPECT_EQ(OverlayNames(topology, link),
              (std::vector<std::string>{"flow:link-tap#1"}));
}

TEST(BehaviorTopology, PatchReplacementIsAtomicAcrossLinksAndTaps) {
    Topology topology;
    const LinkId first = Identify(topology, Base(1, 100, 200));
    const LinkId second = Identify(topology, Base(2, 300, 400));
    PatchLayer target =
        OverlayPatch("mod", "target", 0, first, {{OverlayKind::Splice, 1, 11}});
    target.Links.push_back({second, {}, {{OverlayKind::Splice, 2, 22}}});
    ASSERT_TRUE(topology.Set(std::move(target)));
    ASSERT_TRUE(topology.Set(OverlayPatch("mod", "dependent", 0, first,
                                          {{OverlayKind::Tap, 3, 33}},
                                          {{OrderKind::After, {"mod", "target"}}})));
    const std::uint64_t before = topology.Fingerprint();

    PatchLayer replacement =
        OverlayPatch("mod", "target", 0, second, {{OverlayKind::Splice, 2, 222}});
    replacement.Outs.push_back({Endpoint(300, SlotKind::Output, 0), {{4, 44}}});
    const Status status = topology.Set(std::move(replacement));
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::OrderingTargetMismatch);
    EXPECT_EQ(topology.Fingerprint(), before);
    EXPECT_EQ(OverlayNames(topology, first),
              (std::vector<std::string>{"mod:target#1", "mod:dependent#3"}));
    EXPECT_EQ(topology.Taps(Endpoint(300, SlotKind::Output, 0)), nullptr);
}

TEST(BehaviorTopology, CompletesAUniquePathWithExactLinkAnchors) {
    GraphModel graph;
    graph.Nodes = {
        GraphNodeWithPorts(100, 0, "Graph"),
        GraphNodeWithPorts(101, 100, "First"),
        GraphNodeWithPorts(102, 100, "Last"),
    };
    graph.Links = {
        {1, {9, 1, 4}, Endpoint(100, SlotKind::Input, 0),
         Endpoint(101, SlotKind::Input, 0), 0},
        {2, {9, 2, 4}, Endpoint(101, SlotKind::Output, 0),
         Endpoint(102, SlotKind::Input, 0), 2},
    };

    Path path;
    ASSERT_TRUE(CompletePath(
        graph, Endpoint(100, SlotKind::Input, 0), path));
    ASSERT_EQ(path.Links.size(), 2u);
    EXPECT_EQ(path.Links[0].Anchor, (ObjectRef{9, 1, 4}));
    EXPECT_EQ(path.Links[1].Anchor, (ObjectRef{9, 2, 4}));
    EXPECT_EQ(path.Links[1].Delay, 2);
    EXPECT_EQ(path.End, Endpoint(102, SlotKind::Output, 0));
}

TEST(BehaviorTopology, RejectsBranchesParallelLinksAndCyclesDuringPathCompletion) {
    GraphModel graph;
    graph.Nodes = {
        GraphNodeWithPorts(100, 0, "Graph"),
        GraphNodeWithPorts(101, 100, "Node"),
    };
    graph.Links = {
        {1, {9, 1, 4}, Endpoint(100, SlotKind::Input, 0),
         Endpoint(101, SlotKind::Input, 0), 0},
        {2, {9, 2, 4}, Endpoint(100, SlotKind::Input, 0),
         Endpoint(101, SlotKind::Input, 0), 1},
    };
    Path path;
    Status status = CompletePath(
        graph, Endpoint(100, SlotKind::Input, 0), path);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::PathAmbiguous);
    EXPECT_TRUE(path.Links.empty());

    graph.Links = {
        {1, {9, 1, 4}, Endpoint(100, SlotKind::Input, 0),
         Endpoint(101, SlotKind::Input, 0), 0},
        {2, {9, 2, 4}, Endpoint(101, SlotKind::Output, 0),
         Endpoint(101, SlotKind::Input, 0), 0},
    };
    status = CompletePath(graph, Endpoint(100, SlotKind::Input, 0), path);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::PathCycle);
}

TEST(BehaviorTopology, RejectsAnUnlinkedSiblingOut) {
    GraphModel graph;
    graph.Nodes = {
        GraphNodeWithPorts(100, 0, "Graph"),
        GraphNodeWithPorts(101, 100, "Branch"),
        GraphNodeWithPorts(102, 100, "Sink"),
    };
    graph.Nodes[1].Ports.push_back(
        {SlotKind::Output, 0, 1, 0, CKGUID(), false, "Other", false});
    graph.Links = {
        {1, {9, 1, 4}, Endpoint(100, SlotKind::Input, 0),
         Endpoint(101, SlotKind::Input, 0), 0},
        {2, {9, 2, 4}, Endpoint(101, SlotKind::Output, 0),
         Endpoint(102, SlotKind::Input, 0), 0},
    };

    Path path;
    const Status status = CompletePath(
        graph, Endpoint(100, SlotKind::Input, 0), path);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::PathAmbiguous);
    EXPECT_TRUE(path.Links.empty());
}

TEST(BehaviorTopology, SendsARedirectedLinkToItsNewSinkAndKeepsSplicesInvisible) {
    Topology topology;
    const LinkId link = Identify(topology, Base(1, 100, 200));
    const GraphEndpoint moved = Endpoint(300, SlotKind::Input, 1);

    ASSERT_TRUE(topology.Set(
        OverlayPatch("mod", "alpha", 0, link, {{OverlayKind::Splice, 1, 101}})));
    EXPECT_EQ(EffectiveSink(*topology.Find(link)), Endpoint(200, SlotKind::Input, 0));

    ASSERT_TRUE(topology.Set(
        OverlayPatch("mod", "beta", 0, link,
                     {{OverlayKind::Redirect, 2, 202, moved}})));
    EXPECT_EQ(EffectiveSink(*topology.Find(link)), moved);
    EXPECT_EQ(topology.Find(link)->Base.Sink, Endpoint(200, SlotKind::Input, 0));

    EXPECT_TRUE(topology.Remove({"mod", "beta"}));
    EXPECT_EQ(EffectiveSink(*topology.Find(link)), Endpoint(200, SlotKind::Input, 0));
}

TEST(BehaviorTopology, AdmitsOnlyOneRedirectPerLink) {
    Topology topology;
    const LinkId link = Identify(topology, Base(1, 100, 200));
    const GraphEndpoint moved = Endpoint(300, SlotKind::Input, 0);
    const GraphEndpoint other = Endpoint(400, SlotKind::Input, 0);

    Status status = topology.Set(
        OverlayPatch("mod", "twice", 0, link,
                     {{OverlayKind::Redirect, 1, 101, moved},
                      {OverlayKind::Redirect, 2, 202, other}}));
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::RedirectConflict);

    ASSERT_TRUE(topology.Set(
        OverlayPatch("mod", "alpha", 0, link,
                     {{OverlayKind::Redirect, 1, 101, moved}})));
    status = topology.Set(
        OverlayPatch("mod", "beta", 0, link,
                     {{OverlayKind::Redirect, 2, 202, other}}));
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::RedirectConflict);
    EXPECT_EQ(EffectiveSink(*topology.Find(link)), moved);
}

TEST(BehaviorTopology, RejectsARedirectWithoutAControlDestination) {
    Topology topology;
    const LinkId link = Identify(topology, Base(1, 100, 200));
    const Status status = topology.Set(
        OverlayPatch("mod", "alpha", 0, link,
                     {{OverlayKind::Redirect, 1, 101,
                       Endpoint(300, SlotKind::InputParameter, 0)}}));
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::InvalidState);
}

} // namespace

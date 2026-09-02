#include "Behavior/Relations.h"

#include <array>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

using namespace BML::Behavior;

GraphEndpoint Pin(std::uint64_t node, int index = 0) {
    return {node, SlotKind::InputParameter, index};
}

RelationLayer Layer(std::string owner, std::string name,
                    GraphEndpoint pin, RelationKind kind,
                    std::uint32_t ordinal, int priority = 0,
                    std::vector<Order> ordering = {}) {
    RelationLayer layer;
    layer.Patch = {std::move(owner), std::move(name)};
    layer.Priority = priority;
    layer.Pins.push_back(
        {pin, std::move(ordering), {{kind, ordinal, ordinal * 101u}}});
    return layer;
}

std::vector<std::string> Names(const Relations &relations,
                               GraphEndpoint pin) {
    const LogicalPin *source = relations.Find(pin);
    if (!source)
        return {};
    std::vector<std::string> names;
    for (const OrderedRelation &relation : source->Relations) {
        names.push_back(relation.Patch.Owner + ":" + relation.Patch.Name +
                        "#" + std::to_string(relation.Ordinal));
    }
    return names;
}

TEST(BehaviorRelations, BindExclusivelyOwnsAPinSource) {
    Relations relations;
    ASSERT_TRUE(relations.Set(
        Layer("alpha", "bind", Pin(10), RelationKind::Bind, 1)));
    const std::uint64_t before = relations.Fingerprint();

    Status status = relations.Set(
        Layer("beta", "bind", Pin(10), RelationKind::Bind, 2));
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::SourceConflict);
    EXPECT_EQ(relations.Fingerprint(), before);
    EXPECT_EQ(Names(relations, Pin(10)),
              (std::vector<std::string>{"alpha:bind#1"}));

    status = relations.Set(
        Layer("beta", "transform", Pin(10), RelationKind::Transform, 3));
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::SourceConflict);
    EXPECT_EQ(relations.Fingerprint(), before);
}

TEST(BehaviorRelations, TransformsAreStableAcrossInstallationOrder) {
    const std::array<std::array<int, 3>, 6> permutations{{
        {{0, 1, 2}}, {{0, 2, 1}}, {{1, 0, 2}},
        {{1, 2, 0}}, {{2, 0, 1}}, {{2, 1, 0}},
    }};
    std::uint64_t fingerprint = 0;
    for (const auto &permutation : permutations) {
        Relations relations;
        std::array<RelationLayer, 3> layers{
            Layer("mod", "alpha", Pin(10), RelationKind::Transform, 1, 10),
            Layer("mod", "beta", Pin(10), RelationKind::Transform, 2, 0),
            Layer("mod", "charlie", Pin(10), RelationKind::Transform, 3, -10,
                  {{OrderKind::After, {"mod", "alpha"}}}),
        };
        for (const int index : permutation)
            ASSERT_TRUE(relations.Set(layers[static_cast<std::size_t>(index)]));
        EXPECT_EQ(Names(relations, Pin(10)),
                  (std::vector<std::string>{
                      "mod:beta#2", "mod:alpha#1", "mod:charlie#3"}));
        if (!fingerprint)
            fingerprint = relations.Fingerprint();
        EXPECT_EQ(relations.Fingerprint(), fingerprint);
    }
}

TEST(BehaviorRelations, ReplacementAcrossPinsIsAtomic) {
    Relations relations;
    RelationLayer target = Layer(
        "mod", "target", Pin(10), RelationKind::Bind, 1);
    target.Pins.push_back(
        {Pin(20), {}, {{RelationKind::Bind, 2, 202}}});
    ASSERT_TRUE(relations.Set(std::move(target)));
    ASSERT_TRUE(relations.Set(
        Layer("other", "owner", Pin(30), RelationKind::Bind, 3)));
    const std::uint64_t before = relations.Fingerprint();

    RelationLayer replacement = Layer(
        "mod", "target", Pin(40), RelationKind::Bind, 4);
    replacement.Pins.push_back(
        {Pin(30), {}, {{RelationKind::Bind, 5, 505}}});
    const Status status = relations.Set(std::move(replacement));
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::SourceConflict);
    EXPECT_EQ(relations.Fingerprint(), before);
    EXPECT_NE(relations.Find(Pin(10)), nullptr);
    EXPECT_NE(relations.Find(Pin(20)), nullptr);
    EXPECT_EQ(relations.Find(Pin(40)), nullptr);
}

TEST(BehaviorRelations, TransformOrderRejectsWrongPinAndCycles) {
    Relations relations;
    ASSERT_TRUE(relations.Set(
        Layer("mod", "target", Pin(20), RelationKind::Transform, 1)));
    Status status = relations.Set(
        Layer("mod", "source", Pin(10), RelationKind::Transform, 2, 0,
              {{OrderKind::Before, {"mod", "target"}}}));
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::OrderingTargetMismatch);

    Relations cycle;
    ASSERT_TRUE(cycle.Set(
        Layer("mod", "alpha", Pin(10), RelationKind::Transform, 1, 0,
              {{OrderKind::Before, {"mod", "beta"}}})));
    status = cycle.Set(
        Layer("mod", "beta", Pin(10), RelationKind::Transform, 2, 0,
              {{OrderKind::Before, {"mod", "alpha"}}}));
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::SourceOrderCycle);
    EXPECT_NE(status.Message.find("mod:alpha -> mod:beta -> mod:alpha"),
              std::string::npos);
}

TEST(BehaviorRelations, RemovingABindReleasesItsPin) {
    Relations relations;
    ASSERT_TRUE(relations.Set(
        Layer("alpha", "bind", Pin(10), RelationKind::Bind, 1)));
    EXPECT_TRUE(relations.Remove({"alpha", "bind"}));
    EXPECT_EQ(relations.Find(Pin(10)), nullptr);
    EXPECT_TRUE(relations.Set(
        Layer("beta", "bind", Pin(10), RelationKind::Bind, 2)));
}

} // namespace

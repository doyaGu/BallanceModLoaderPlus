// The loader's own event queues, driven through the same bml.events interface a
// Mod sees. The thunks below stand in for the ones in Interfaces.cpp, which add
// nothing but the game-thread and mods-loaded checks, so none of this needs a
// running loader or a CKContext.
#include "BML/Events.h"

#include "EventSnapshot.h"
#include "EventStreams.h"
#include "InterfaceRegistry.h"

#include <gtest/gtest.h>

#include <iterator>
#include <string>

namespace {

int EventsOpenStream(int capacity, BML_EventStream *out) {
    return BML::OpenEventStream(capacity, *out);
}

int EventsCloseStream(BML_EventStream stream) { return BML::CloseEventStream(stream); }

int EventsReadDroppedCount(BML_EventStream stream, int *out) {
    return BML::ReadEventStreamDroppedCount(stream, *out);
}

int EventsPoll(BML_EventStream stream, BML_EventInfo *out) {
    return BML::PollEventStream(stream, *out);
}

int EventsReadLoad(BML_EventStream stream, BML_EventLoad *out) {
    return BML::ReadEventLoad(stream, *out);
}

int EventsReadLoadObject(BML_EventStream stream, std::size_t index, BML_ObjectRef *out) {
    return BML::ReadEventLoadObject(stream, index, *out);
}

int EventsReadPhysics(BML_EventStream stream, BML_EventPhysics *out) {
    return BML::ReadEventPhysics(stream, *out);
}

int EventsReadPhysicsConvexMesh(BML_EventStream stream, std::size_t index, BML_ObjectRef *out) {
    return BML::ReadEventPhysicsConvexMesh(stream, index, *out);
}

int EventsReadPhysicsBall(BML_EventStream stream, std::size_t index, BML_Vec3 *outCenter,
                          float *outRadius) {
    return BML::ReadEventPhysicsBall(stream, index, *outCenter, *outRadius);
}

int EventsReadPhysicsConcaveMesh(BML_EventStream stream, std::size_t index, BML_ObjectRef *out) {
    return BML::ReadEventPhysicsConcaveMesh(stream, index, *out);
}

int EventsReadCommand(BML_EventStream stream, BML_EventCommand *out) {
    return BML::ReadEventCommand(stream, *out);
}

int EventsReadCommandArgument(BML_EventStream stream, std::size_t index, BML_EventText *out) {
    return BML::ReadEventCommandArgument(stream, index, *out);
}

int EventsReadConfig(BML_EventStream stream, BML_EventConfig *out) {
    return BML::ReadEventConfig(stream, *out);
}

int EventsReadCheat(BML_EventStream stream, BML_EventCheat *out) {
    return BML::ReadEventCheat(stream, *out);
}

const BML_EventsInterface kEventsInterface = {
    BML_IFACE_HEADER(BML_EventsInterface, BML_EVENTS_INTERFACE_ID, BML_EVENTS_INTERFACE_MAJOR,
                     BML_EVENTS_INTERFACE_MINOR),
    &EventsOpenStream,
    &EventsCloseStream,
    &EventsReadDroppedCount,
    &EventsPoll,
    &EventsReadLoad,
    &EventsReadLoadObject,
    &EventsReadPhysics,
    &EventsReadPhysicsConvexMesh,
    &EventsReadPhysicsBall,
    &EventsReadPhysicsConcaveMesh,
    &EventsReadCommand,
    &EventsReadCommandArgument,
    &EventsReadConfig,
    &EventsReadCheat,
};

const BML::InterfaceEntry kInterfaces[] = {
    {BML_EVENTS_INTERFACE_ID, BML_EVENTS_INTERFACE_MAJOR, &kEventsInterface},
};

BML_ObjectRef Ref(std::uint32_t slot) { return BML_ObjectRef{1u, slot, slot + 100u}; }

BML::EventSnapshot CheatSnapshot(bool enabled) {
    BML::EventSnapshot snapshot;
    snapshot.Kind = BML_EVENT_CHEAT_CHANGED;
    snapshot.CheatEnabled = enabled;
    return snapshot;
}

// Every open stream is closed between tests, so the queues each test sees are the
// ones it opened itself.
class EventStreamTest : public ::testing::Test {
protected:
    void SetUp() override { BML::CloseAllEventStreams(); }
    void TearDown() override { BML::CloseAllEventStreams(); }
};

} // namespace

int BML_GetInterface(const char *interfaceId, uint16_t majorVersion, const void **out) {
    return BML::FindInterface(kInterfaces, std::size(kInterfaces), interfaceId, majorVersion, out);
}

TEST_F(EventStreamTest, InterfaceIsReachableThroughTheStreamClass) {
    EXPECT_EQ(BML::Events::RequireApi(), BML_OK);
}

TEST_F(EventStreamTest, PublishReachesEveryOpenStream) {
    BML::Events::Stream first;
    BML::Events::Stream second;
    EXPECT_FALSE(BML::HasEventConsumers());
    ASSERT_EQ(first.Open(4), BML_OK);
    ASSERT_EQ(second.Open(4), BML_OK);
    EXPECT_TRUE(BML::HasEventConsumers());

    BML::PublishEventSnapshot(CheatSnapshot(true));

    BML::Events::Event firstEvent{};
    BML::Events::Event secondEvent{};
    ASSERT_EQ(first.Poll(firstEvent), BML_OK);
    ASSERT_EQ(second.Poll(secondEvent), BML_OK);
    EXPECT_EQ(firstEvent.Kind, BML_EVENT_CHEAT_CHANGED);
    ASSERT_TRUE(firstEvent.CheatData.has_value());
    EXPECT_TRUE(firstEvent.CheatData->Enabled);
    // One published event is one event, so both streams see the same number for it.
    EXPECT_EQ(firstEvent.Sequence, secondEvent.Sequence);
    EXPECT_EQ(firstEvent.Timestamp, secondEvent.Timestamp);
    EXPECT_NE(firstEvent.Timestamp, 0u);
}

TEST_F(EventStreamTest, SequenceCountsPublishedEventsSoAGapIsWhatWasDropped) {
    BML::Events::Stream stream;
    ASSERT_EQ(stream.Open(1), BML_OK);

    BML::PublishEventSnapshot(CheatSnapshot(true));
    BML::Events::Event first{};
    ASSERT_EQ(stream.Poll(first), BML_OK);

    BML::PublishEventSnapshot(CheatSnapshot(false));
    BML::Events::Event second{};
    ASSERT_EQ(stream.Poll(second), BML_OK);
    EXPECT_EQ(second.Sequence, first.Sequence + 1u);
}

TEST_F(EventStreamTest, FullStreamDropsTheOldestEvent) {
    BML::Events::Stream stream;
    ASSERT_EQ(stream.Open(1), BML_OK);

    BML::PublishEventSnapshot(CheatSnapshot(true));
    BML::PublishEventSnapshot(CheatSnapshot(false));

    int dropped = -1;
    ASSERT_EQ(stream.DroppedCount(dropped), BML_OK);
    EXPECT_EQ(dropped, 1);

    BML::Events::Event event{};
    ASSERT_EQ(stream.Poll(event), BML_OK);
    ASSERT_TRUE(event.CheatData.has_value());
    EXPECT_FALSE(event.CheatData->Enabled);
    EXPECT_EQ(stream.Poll(event), BML_ERROR_NOT_FOUND);
}

TEST_F(EventStreamTest, LoadEventCarriesItsObjectList) {
    BML::Events::Stream stream;
    ASSERT_EQ(stream.Open(2), BML_OK);

    BML::EventSnapshot snapshot;
    snapshot.Kind = BML_EVENT_LOAD_OBJECT;
    snapshot.Filename = "Level_01.nmo";
    snapshot.MasterName = "Level";
    snapshot.IsMap = true;
    snapshot.FilterClass = 42;
    snapshot.AddToScene = true;
    snapshot.ReuseMeshes = true;
    snapshot.ReuseMaterials = false;
    snapshot.IsDynamic = true;
    snapshot.ObjectIds = {Ref(1), Ref(2), Ref(3)};
    snapshot.MasterObject = Ref(4);
    BML::PublishEventSnapshot(snapshot);

    BML::Events::Event event{};
    ASSERT_EQ(stream.Poll(event), BML_OK);
    ASSERT_TRUE(event.LoadData.has_value());
    EXPECT_EQ(event.LoadData->Filename, "Level_01.nmo");
    EXPECT_EQ(event.LoadData->MasterName, "Level");
    EXPECT_TRUE(event.LoadData->IsMap);
    EXPECT_EQ(event.LoadData->FilterClass, 42);
    EXPECT_TRUE(event.LoadData->AddToScene);
    EXPECT_TRUE(event.LoadData->ReuseMeshes);
    EXPECT_FALSE(event.LoadData->ReuseMaterials);
    EXPECT_TRUE(event.LoadData->Dynamic);
    ASSERT_EQ(event.LoadData->Objects.size(), 3u);
    EXPECT_EQ(event.LoadData->Objects[2].Slot, 3u);
    EXPECT_EQ(event.LoadData->MasterObject.Slot, 4u);
    EXPECT_FALSE(event.PhysicsData.has_value());
}

TEST_F(EventStreamTest, PhysicsEventPairsBallCentersWithRadii) {
    BML::Events::Stream stream;
    ASSERT_EQ(stream.Open(2), BML_OK);

    BML::EventSnapshot snapshot;
    snapshot.Kind = BML_EVENT_PHYSICALIZE;
    snapshot.Target = Ref(7);
    snapshot.Fixed = true;
    snapshot.Friction = 0.5f;
    snapshot.Elasticity = 0.25f;
    snapshot.Mass = 2.0f;
    snapshot.CollisionGroup = "Ball";
    snapshot.CollisionSurface = "Wood";
    snapshot.MassCenter = BML_Vec3{1.0f, 2.0f, 3.0f};
    snapshot.ConvexMeshes = {Ref(8)};
    snapshot.BallCenters = {BML_Vec3{1.0f, 0.0f, 0.0f}, BML_Vec3{0.0f, 1.0f, 0.0f}};
    snapshot.BallRadii = {0.5f, 1.5f};
    snapshot.ConcaveMeshes = {Ref(9), Ref(10)};
    BML::PublishEventSnapshot(snapshot);

    BML::Events::Event event{};
    ASSERT_EQ(stream.Poll(event), BML_OK);
    ASSERT_TRUE(event.PhysicsData.has_value());
    EXPECT_EQ(event.PhysicsData->Target.Slot, 7u);
    EXPECT_TRUE(event.PhysicsData->Fixed);
    EXPECT_FLOAT_EQ(event.PhysicsData->Mass, 2.0f);
    EXPECT_EQ(event.PhysicsData->CollisionGroup, "Ball");
    EXPECT_EQ(event.PhysicsData->CollisionSurface, "Wood");
    EXPECT_FLOAT_EQ(event.PhysicsData->MassCenter.z, 3.0f);
    ASSERT_EQ(event.PhysicsData->ConvexMeshes.size(), 1u);
    ASSERT_EQ(event.PhysicsData->BallCenters.size(), 2u);
    ASSERT_EQ(event.PhysicsData->BallRadii.size(), 2u);
    EXPECT_FLOAT_EQ(event.PhysicsData->BallCenters[1].y, 1.0f);
    EXPECT_FLOAT_EQ(event.PhysicsData->BallRadii[1], 1.5f);
    ASSERT_EQ(event.PhysicsData->ConcaveMeshes.size(), 2u);
}

// The two ball lists are read as one, so a snapshot with more centers than radii
// hands back only the rows that have both rather than reading a missing radius.
TEST_F(EventStreamTest, PhysicsBallCountIsTheShorterOfTheTwoLists) {
    BML::Events::Stream stream;
    ASSERT_EQ(stream.Open(2), BML_OK);

    BML::EventSnapshot snapshot;
    snapshot.Kind = BML_EVENT_PHYSICALIZE;
    snapshot.BallCenters = {BML_Vec3{1.0f, 0.0f, 0.0f}, BML_Vec3{0.0f, 1.0f, 0.0f}};
    snapshot.BallRadii = {0.5f};
    BML::PublishEventSnapshot(snapshot);

    BML::Events::Event event{};
    ASSERT_EQ(stream.Poll(event), BML_OK);
    ASSERT_TRUE(event.PhysicsData.has_value());
    EXPECT_EQ(event.PhysicsData->BallCenters.size(), 1u);
    EXPECT_EQ(event.PhysicsData->BallRadii.size(), 1u);
}

TEST_F(EventStreamTest, CommandEventCarriesItsArguments) {
    BML::Events::Stream stream;
    ASSERT_EQ(stream.Open(2), BML_OK);

    BML::EventSnapshot snapshot;
    snapshot.Kind = BML_EVENT_COMMAND_PRE;
    snapshot.Command = "cheat";
    snapshot.CommandArgs = {"on"};
    BML::PublishEventSnapshot(snapshot);

    BML::Events::Event event{};
    ASSERT_EQ(stream.Poll(event), BML_OK);
    ASSERT_TRUE(event.CommandData.has_value());
    EXPECT_EQ(event.CommandData->Name, "cheat");
    ASSERT_EQ(event.CommandData->Arguments.size(), 1u);
    EXPECT_EQ(event.CommandData->Arguments[0], "on");
}

TEST_F(EventStreamTest, ConfigEventCarriesTheRenderedValue) {
    BML::Events::Stream stream;
    ASSERT_EQ(stream.Open(2), BML_OK);

    BML::EventSnapshot snapshot;
    snapshot.Kind = BML_EVENT_CONFIG_MODIFIED;
    snapshot.ConfigCategory = "Misc";
    snapshot.ConfigKey = "ShowFPS";
    snapshot.ConfigType = 3;
    snapshot.ConfigValue = "true";
    BML::PublishEventSnapshot(snapshot);

    BML::Events::Event event{};
    ASSERT_EQ(stream.Poll(event), BML_OK);
    ASSERT_TRUE(event.ConfigData.has_value());
    EXPECT_EQ(event.ConfigData->Category, "Misc");
    EXPECT_EQ(event.ConfigData->Key, "ShowFPS");
    EXPECT_EQ(event.ConfigData->Type, 3);
    EXPECT_EQ(event.ConfigData->Value, "true");
}

// The cursor is what says which payload is there, so asking for another kind's
// payload, or for a row past the end of a list, is BML_ERROR_NOT_FOUND rather
// than a zeroed struct.
TEST_F(EventStreamTest, ReadsRefuseAPayloadTheCurrentEventDoesNotCarry) {
    BML_EventStream stream = nullptr;
    ASSERT_EQ(BML::OpenEventStream(2, stream), BML_OK);

    BML::EventSnapshot snapshot;
    snapshot.Kind = BML_EVENT_COMMAND_POST;
    snapshot.Command = "exit";
    BML::PublishEventSnapshot(snapshot);

    BML_EventLoad load = {};
    BML_EventPhysics physics = {};
    BML_EventConfig config = {};
    BML_EventCheat cheat = {};
    BML_EventCommand command = {};
    BML_EventText argument = {};

    // Nothing has been polled yet, so there is no current event at all.
    EXPECT_EQ(BML::ReadEventCommand(stream, command), BML_ERROR_NOT_FOUND);

    BML_EventInfo info = {};
    ASSERT_EQ(BML::PollEventStream(stream, info), BML_OK);
    EXPECT_EQ(info.Kind, BML_EVENT_COMMAND_POST);
    ASSERT_EQ(BML::ReadEventCommand(stream, command), BML_OK);
    EXPECT_EQ(command.ArgumentCount, 0);
    EXPECT_EQ(BML::ReadEventCommandArgument(stream, 0, argument), BML_ERROR_NOT_FOUND);
    EXPECT_EQ(BML::ReadEventLoad(stream, load), BML_ERROR_NOT_FOUND);
    EXPECT_EQ(BML::ReadEventPhysics(stream, physics), BML_ERROR_NOT_FOUND);
    EXPECT_EQ(BML::ReadEventConfig(stream, config), BML_ERROR_NOT_FOUND);
    EXPECT_EQ(BML::ReadEventCheat(stream, cheat), BML_ERROR_NOT_FOUND);

    EXPECT_EQ(BML::CloseEventStream(stream), BML_OK);
}

TEST_F(EventStreamTest, ClosedAndEmptyAndReadyAreDistinct) {
    BML::Events::Stream stream;
    BML::Events::Event event{};

    EXPECT_FALSE(stream.IsOpen());
    EXPECT_EQ(stream.Poll(event), BML_ERROR_INVALID_HANDLE);
    int dropped = -1;
    EXPECT_EQ(stream.DroppedCount(dropped), BML_ERROR_INVALID_HANDLE);
    EXPECT_EQ(stream.Close(), BML_OK);

    ASSERT_EQ(stream.Open(2), BML_OK);
    EXPECT_TRUE(stream.IsOpen());
    EXPECT_EQ(stream.Poll(event), BML_ERROR_NOT_FOUND);

    BML::PublishEventSnapshot(CheatSnapshot(true));
    EXPECT_EQ(stream.Poll(event), BML_OK);
    EXPECT_EQ(stream.Poll(event), BML_ERROR_NOT_FOUND);

    ASSERT_EQ(stream.Close(), BML_OK);
    EXPECT_FALSE(stream.IsOpen());
    EXPECT_EQ(stream.Poll(event), BML_ERROR_INVALID_HANDLE);
}

TEST_F(EventStreamTest, NegativeCapacityIsRejectedAndZeroMeansTheDefault) {
    BML::Events::Stream stream;
    EXPECT_EQ(stream.Open(-1), BML_ERROR_INVALID_PARAMETER);
    EXPECT_FALSE(stream.IsOpen());

    ASSERT_EQ(stream.Open(0), BML_OK);
    for (int index = 0; index < BML_EVENT_DEFAULT_CAPACITY; ++index)
        BML::PublishEventSnapshot(CheatSnapshot(true));

    int dropped = -1;
    ASSERT_EQ(stream.DroppedCount(dropped), BML_OK);
    EXPECT_EQ(dropped, 0);

    BML::PublishEventSnapshot(CheatSnapshot(true));
    ASSERT_EQ(stream.DroppedCount(dropped), BML_OK);
    EXPECT_EQ(dropped, 1);
}

// Reopening closes the old queue first, so the events the Mod never drained are
// gone rather than leaking a second queue that nothing polls.
TEST_F(EventStreamTest, ReopeningReplacesTheQueue) {
    BML::Events::Stream stream;
    ASSERT_EQ(stream.Open(4), BML_OK);
    BML::PublishEventSnapshot(CheatSnapshot(true));

    ASSERT_EQ(stream.Open(4), BML_OK);
    BML::Events::Event event{};
    EXPECT_EQ(stream.Poll(event), BML_ERROR_NOT_FOUND);
}

TEST_F(EventStreamTest, TextLongerThanTheBufferIsTruncatedButItsLengthIsWhole) {
    BML_EventStream stream = nullptr;
    ASSERT_EQ(BML::OpenEventStream(2, stream), BML_OK);
    BML::Events::Stream valueStream;
    ASSERT_EQ(valueStream.Open(2), BML_OK);

    const std::string name(600u, 'x');
    BML::EventSnapshot snapshot;
    snapshot.Kind = BML_EVENT_COMMAND_PRE;
    snapshot.Command = name;
    BML::PublishEventSnapshot(snapshot);

    BML_EventInfo info = {};
    ASSERT_EQ(BML::PollEventStream(stream, info), BML_OK);
    BML_EventCommand command = {};
    ASSERT_EQ(BML::ReadEventCommand(stream, command), BML_OK);
    EXPECT_EQ(command.Name.Length, 600);
    EXPECT_EQ(std::string(command.Name.Value).size(), BML_EVENT_TEXT_CAPACITY - 1u);

    BML::Events::Event event{};
    ASSERT_EQ(valueStream.Poll(event), BML_OK);
    ASSERT_TRUE(event.CommandData.has_value());
    EXPECT_TRUE(event.TextTruncated);
    EXPECT_EQ(event.CommandData->Name.size(), BML_EVENT_TEXT_CAPACITY - 1u);

    EXPECT_EQ(BML::CloseEventStream(stream), BML_OK);
}

TEST_F(EventStreamTest, PublishingWithNoStreamOpenIsIgnored) {
    EXPECT_FALSE(BML::HasEventConsumers());
    BML::PublishEventSnapshot(CheatSnapshot(true));

    BML::Events::Stream stream;
    ASSERT_EQ(stream.Open(2), BML_OK);
    BML::Events::Event event{};
    EXPECT_EQ(stream.Poll(event), BML_ERROR_NOT_FOUND);
}

// UnloadMods drops every queue, so a handle a Mod forgot to close names nothing
// afterwards instead of freed memory.
TEST_F(EventStreamTest, ClosingEveryStreamInvalidatesTheHandlesLeftBehind) {
    BML_EventStream stream = nullptr;
    ASSERT_EQ(BML::OpenEventStream(2, stream), BML_OK);
    ASSERT_NE(stream, nullptr);

    BML::CloseAllEventStreams();
    EXPECT_FALSE(BML::HasEventConsumers());

    BML_EventInfo info = {};
    int dropped = 0;
    EXPECT_EQ(BML::PollEventStream(stream, info), BML_ERROR_INVALID_HANDLE);
    EXPECT_EQ(BML::ReadEventStreamDroppedCount(stream, dropped), BML_ERROR_INVALID_HANDLE);
    EXPECT_EQ(BML::CloseEventStream(stream), BML_ERROR_INVALID_HANDLE);
}

TEST_F(EventStreamTest, ANullHandleIsRefusedRatherThanDereferenced) {
    BML_EventInfo info = {};
    int dropped = 0;
    EXPECT_EQ(BML::PollEventStream(nullptr, info), BML_ERROR_INVALID_HANDLE);
    EXPECT_EQ(BML::ReadEventStreamDroppedCount(nullptr, dropped), BML_ERROR_INVALID_HANDLE);
    EXPECT_EQ(BML::CloseEventStream(nullptr), BML_ERROR_INVALID_HANDLE);
}

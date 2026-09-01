#include "BML/Behavior.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

using namespace BML::Behavior;

struct FakeState {
    std::string Owner;
    std::string Input;
    std::string SettingName;
    std::string Text;
    std::uint32_t SelectorKind = 0;
    std::uint32_t FrameKind = 0;
    std::uint32_t FrameLimit = 0;
    std::uint64_t Generation = 0;
    int SessionCloses = 0;
    int RunCloses = 0;
    bool FramesAvailable = true;
};

FakeState g_State;

template <class T>
void Init(T *value) {
    if (!value)
        return;
    *value = {};
    value->StructSize = sizeof(*value);
}

void Success(BML_BehaviorStatus *status) {
    Init(status);
}

int BML_BEHAVIOR_CALL OpenSession(BML_BehaviorString owner,
                                  BML_BehaviorSession *session,
                                  BML_BehaviorStatus *status) {
    g_State.Owner.assign(owner.Data, owner.Length);
    *session = reinterpret_cast<BML_BehaviorSession>(1);
    Success(status);
    return BML_OK;
}

int BML_BEHAVIOR_CALL CloseSession(BML_BehaviorSession) {
    ++g_State.SessionCloses;
    return BML_OK;
}

int OpenRun(const BML_BehaviorBlock *block,
            const BML_BehaviorSelector *input,
            BML_BehaviorRun *run,
            BML_BehaviorRunInfo *info,
            BML_BehaviorStatus *status,
            std::uint32_t kind) {
    g_State.Generation = block->PrototypeGeneration;
    g_State.FrameKind = block->Frames.Kind;
    g_State.FrameLimit = block->Frames.Limit;
    if (input) {
        g_State.SelectorKind = input->Kind;
        g_State.Input.assign(input->Name.Data, input->Name.Length);
    }
    if (block->SettingStageCount && block->SettingStages[0].SettingCount) {
        const BML_BehaviorBinding &binding =
            block->SettingStages[0].Settings[0];
        g_State.SettingName.assign(binding.Slot.Name.Data,
                                   binding.Slot.Name.Length);
        g_State.Text.assign(binding.Value.Data.Utf8.Data,
                            binding.Value.Data.Utf8.Length);
    }
    *run = reinterpret_cast<BML_BehaviorRun>(2);
    Init(info);
    info->Kind = kind;
    info->State = kind == BML_BEHAVIOR_RUN_CALL
        ? BML_BEHAVIOR_RUN_PENDING
        : BML_BEHAVIOR_RUN_COMPLETED;
    Init(&info->Status);
    Success(status);
    return BML_OK;
}

int BML_BEHAVIOR_CALL CallRun(BML_BehaviorSession, BML_ObjectRef,
                              const BML_BehaviorBlock *block,
                              const BML_BehaviorSelector *input,
                              BML_BehaviorRun *run,
                              BML_BehaviorRunInfo *info,
                              BML_BehaviorStatus *status) {
    return OpenRun(block, input, run, info, status, BML_BEHAVIOR_RUN_CALL);
}

int BML_BEHAVIOR_CALL StartRun(BML_BehaviorSession, BML_ObjectRef,
                               const BML_BehaviorBlock *block,
                               const BML_BehaviorSelector *input,
                               BML_BehaviorRun *run,
                               BML_BehaviorRunInfo *info,
                               BML_BehaviorStatus *status) {
    return OpenRun(block, input, run, info, status, BML_BEHAVIOR_RUN_TASK);
}

int BML_BEHAVIOR_CALL SpawnRun(BML_BehaviorSession, BML_ObjectRef,
                               const BML_BehaviorBlock *block,
                               BML_BehaviorRun *run,
                               BML_BehaviorRunInfo *info,
                               BML_BehaviorStatus *status) {
    return OpenRun(block, nullptr, run, info, status,
                   BML_BEHAVIOR_RUN_INSTANCE);
}

int BML_BEHAVIOR_CALL ContinueRun(BML_BehaviorRun,
                                  BML_BehaviorRunInfo *info,
                                  BML_BehaviorStatus *status) {
    Init(info);
    info->Kind = BML_BEHAVIOR_RUN_TASK;
    info->State = BML_BEHAVIOR_RUN_PENDING;
    Init(&info->Status);
    Success(status);
    return BML_OK;
}

int BML_BEHAVIOR_CALL PulseRun(BML_BehaviorRun,
                               const BML_BehaviorSelector *input,
                               std::uint32_t *admission,
                               BML_BehaviorRunInfo *info,
                               BML_BehaviorStatus *status) {
    g_State.Input.assign(input->Name.Data, input->Name.Length);
    *admission = BML_BEHAVIOR_ADMISSION_EXECUTED;
    Init(info);
    info->Kind = BML_BEHAVIOR_RUN_INSTANCE;
    info->State = BML_BEHAVIOR_RUN_COMPLETED;
    Init(&info->Status);
    Success(status);
    return BML_OK;
}

int BML_BEHAVIOR_CALL ReadRun(BML_BehaviorRun,
                              BML_BehaviorRunInfo *info,
                              BML_BehaviorStatus *status) {
    Init(info);
    info->Kind = BML_BEHAVIOR_RUN_TASK;
    info->State = BML_BEHAVIOR_RUN_PENDING;
    Init(&info->Status);
    Success(status);
    return BML_OK;
}

std::vector<std::uint8_t> FramePayload() {
    std::vector<std::uint8_t> payload(73, 0);
    BML_BehaviorOutRecord out{};
    out.StructSize = sizeof(out);
    out.NameOffset = 20;
    out.NameLength = 4;
    std::memcpy(payload.data(), &out, sizeof(out));
    std::memcpy(payload.data() + 20, "Done", 4);

    BML_BehaviorPoutRecord pout{};
    pout.StructSize = sizeof(pout);
    pout.Type = {CKPGUID_INT.d1, CKPGUID_INT.d2};
    pout.Kind = BML_BEHAVIOR_VALUE_INT32;
    pout.NameOffset = 64;
    pout.NameLength = 5;
    pout.ValueOffset = 69;
    pout.ValueSize = 4;
    std::memcpy(payload.data() + 24, &pout, sizeof(pout));
    std::memcpy(payload.data() + 64, "Value", 5);
    payload[69] = 42;
    return payload;
}

int BML_BEHAVIOR_CALL TakeFrames(BML_BehaviorRun,
                                 BML_BehaviorRunFrame *frames,
                                 std::uint32_t frameCapacity,
                                 std::uint32_t,
                                 void *payload,
                                 std::uint32_t payloadCapacity,
                                 std::uint32_t *frameCount,
                                 std::uint32_t *payloadSize,
                                 BML_BehaviorStatus *status) {
    Success(status);
    if (!g_State.FramesAvailable) {
        *frameCount = 0;
        *payloadSize = 0;
        return BML_OK;
    }
    const std::vector<std::uint8_t> bytes = FramePayload();
    *frameCount = 1;
    *payloadSize = static_cast<std::uint32_t>(bytes.size());
    if (frameCapacity < 1 || payloadCapacity < bytes.size())
        return BML_ERROR_BUFFER_TOO_SMALL;
    *frames = {};
    frames->StructSize = sizeof(*frames);
    frames->Sequence = 7;
    frames->Frame = 91;
    frames->Terminal = 1;
    frames->OutOffset = 0;
    frames->OutCount = 1;
    frames->PoutOffset = 24;
    frames->PoutCount = 1;
    std::memcpy(payload, bytes.data(), bytes.size());
    g_State.FramesAvailable = false;
    return BML_OK;
}

int BML_BEHAVIOR_CALL CloseRun(BML_BehaviorRun) {
    ++g_State.RunCloses;
    return BML_OK;
}

int BML_BEHAVIOR_CALL FindPrototypes(
    BML_BehaviorSession, const BML_BehaviorPrototypeQuery *,
    BML_BehaviorPrototypeInfo *, std::uint32_t, std::uint32_t, void *,
    std::uint32_t, std::uint32_t *, std::uint32_t *, BML_BehaviorStatus *) {
    return BML_ERROR_NOT_IMPLEMENTED;
}

int BML_BEHAVIOR_CALL ReadDeclaredLayout(
    BML_BehaviorSession, const BML_BehaviorPrototypeRef *,
    BML_BehaviorLayout *, void *, std::uint32_t, std::uint32_t *,
    BML_BehaviorStatus *) {
    return BML_ERROR_NOT_IMPLEMENTED;
}

int BML_BEHAVIOR_CALL ReadLiveLayout(
    BML_BehaviorRun, BML_BehaviorLayout *, void *, std::uint32_t,
    std::uint32_t *, BML_BehaviorStatus *) {
    return BML_ERROR_NOT_IMPLEMENTED;
}

BML_BehaviorInterface g_Interface = {
    BML_IFACE_HEADER(BML_BehaviorInterface, BML_BEHAVIOR_INTERFACE_ID,
                     BML_BEHAVIOR_INTERFACE_MAJOR,
                     BML_BEHAVIOR_INTERFACE_MINOR),
    &OpenSession,
    &CloseSession,
    &CallRun,
    &StartRun,
    &SpawnRun,
    &ContinueRun,
    &PulseRun,
    &ReadRun,
    &TakeFrames,
    &CloseRun,
    &FindPrototypes,
    &ReadDeclaredLayout,
    &ReadLiveLayout,
};

} // namespace

extern "C" int BML_GetInterface(const char *id, std::uint16_t major,
                                const void **out) {
    if (!id || !out)
        return BML_ERROR_INVALID_PARAMETER;
    if (std::string_view(id) != BML_BEHAVIOR_INTERFACE_ID)
        return BML_ERROR_NOT_FOUND;
    if (major != BML_BEHAVIOR_INTERFACE_MAJOR)
        return BML_ERROR_VERSION_MISMATCH;
    *out = &g_Interface;
    return BML_OK;
}

TEST(BehaviorAuthoring, OwnsBlockTextAndUsesDomainSelectors) {
    g_State = {};
    auto opened = Session::Open("test.mod");
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    std::string settingName = "Caption";
    std::string text = "owned text";
    Block block = session.Use(Prototype(CKGUID(11, 22), 37))
        .Settings(setting(settingName, text))
        .Frames(eachFrame(8));
    settingName.assign("changed");
    text.assign("changed");

    auto called = block.Call();
    ASSERT_TRUE(called) << called.Code();
    EXPECT_EQ(g_State.Owner, "test.mod");
    EXPECT_EQ(g_State.SelectorKind, BML_BEHAVIOR_SELECTOR_ONLY);
    EXPECT_EQ(g_State.SettingName, "Caption");
    EXPECT_EQ(g_State.Text, "owned text");
    EXPECT_EQ(g_State.FrameKind, BML_BEHAVIOR_FRAMES_EACH_FRAME);
    EXPECT_EQ(g_State.FrameLimit, 8u);
    EXPECT_EQ(g_State.Generation, 37u);
}

TEST(BehaviorAuthoring, TakesOwnedFramesAndContinuesTheSameRun) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto called = session.Use(CKGUID(1, 2)).Call(unique("Run"));
    ASSERT_TRUE(called);
    Call call = std::move(called).Value();

    auto taken = call.Take();
    ASSERT_TRUE(taken);
    ASSERT_EQ(taken.Value().size(), 1u);
    const Frame &frame = taken.Value().front();
    EXPECT_EQ(frame.Sequence, 7u);
    EXPECT_EQ(frame.GameFrame, 91u);
    EXPECT_TRUE(frame.Terminal);
    EXPECT_TRUE(frame.HasOut("Done"));
    const Pout *pout = frame.FindPout("Value");
    ASSERT_NE(pout, nullptr);
    ASSERT_NE(pout->Get<std::int32_t>(), nullptr);
    EXPECT_EQ(*pout->Get<std::int32_t>(), 42);

    auto continued = std::move(call).Continue();
    ASSERT_TRUE(continued);
    Task task = std::move(continued).Value();
    auto info = task.Read();
    ASSERT_TRUE(info);
    EXPECT_EQ(info.Value().Kind, RunKind::Task);
    EXPECT_EQ(info.Value().State, RunState::Pending);
}

TEST(BehaviorAuthoring, PulseAndRaiiCloseUseTheRunHandle) {
    g_State = {};
    {
        auto opened = Session::Open();
        ASSERT_TRUE(opened);
        Session session = std::move(opened).Value();
        auto spawned = session.Use(CKGUID(3, 4)).Spawn();
        ASSERT_TRUE(spawned);
        Instance instance = std::move(spawned).Value();
        auto admission = instance.Pulse("Create");
        ASSERT_TRUE(admission);
        EXPECT_EQ(admission.Value(), Admission::Executed);
        EXPECT_EQ(g_State.Input, "Create");
    }
    EXPECT_EQ(g_State.RunCloses, 1);
    EXPECT_EQ(g_State.SessionCloses, 1);
}

TEST(BehaviorAuthoring, ClosingSessionInvalidatesExistingBlocks) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    Block block = session.Use(CKGUID(5, 6));
    session.Close();

    auto called = block.Call();
    EXPECT_FALSE(called);
    EXPECT_EQ(called.Code(), BML_ERROR_INVALID_HANDLE);
    EXPECT_EQ(g_State.SessionCloses, 1);
}

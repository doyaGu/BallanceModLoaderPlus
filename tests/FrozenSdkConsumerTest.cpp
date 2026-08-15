// A consumer of the frozen v0.3.12 SDK snapshot (tests/abi/frozen-sdk/v0.3.12).
// The target's include path puts the snapshot ahead of the current include/, so
// every BML/*.h below is the archived v0.3.12 header. The test is the "safe test
// host" from the interface-evolution plan: it builds a Mod against the archived
// SDK, drives it through the BMLEntry convention with a fake IBML, and asserts
// the archived inline machinery behaves exactly as it shipped.
//
// Two assertions double as snapshot-integrity markers: the archived ParseFloat
// still carries the FLT_MIN default lower bound that P0-1 later fixed, and
// BMLVersion still defaults to 0.3.12 from the reconstructed Version.h. If
// either fails, someone edited the snapshot; the fix is a new snapshot, never an
// edit to this one.
#include "BML/BML.h"
#include "BML/IBML.h"
#include "BML/ICommand.h"
#include "BML/IMod.h"

#include <cfloat>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

// The archived IMod declares these but the definitions live in BMLPlus.dll; the
// test host provides its own, like every other test that subclasses IMod.
IMod::~IMod() = default;
ILogger *IMod::GetLogger() { return nullptr; }
IConfig *IMod::GetConfig() { return nullptr; }

namespace {

// Allocation counters behind the archived ModDependency's BML_Malloc/BML_Free
// ownership convention: every Malloc must pair with one Free.
int g_MallocCalls = 0;
int g_FreeCalls = 0;

} // namespace

// Declared in the archived BML.h, defined by the loader. The host backs them
// with its own heap so the counters above see the calls.
void *BML_Malloc(size_t size) {
    ++g_MallocCalls;
    return malloc(size);
}

void BML_Free(void *ptr) {
    ++g_FreeCalls;
    free(ptr);
}

namespace {

// Every pure virtual of the archived IBML, as a passive host: most answers are
// defaults, and the few the driven Mod actually calls are recorded.
class FakeBML : public IBML {
public:
    CKContext *GetCKContext() override { return nullptr; }
    CKRenderContext *GetRenderContext() override { return nullptr; }

    void ExitGame() override { ++ExitGameCalls; }

    CKAttributeManager *GetAttributeManager() override { return nullptr; }
    CKBehaviorManager *GetBehaviorManager() override { return nullptr; }
    CKCollisionManager *GetCollisionManager() override { return nullptr; }
    InputHook *GetInputManager() override { return nullptr; }
    CKMessageManager *GetMessageManager() override { return nullptr; }
    CKPathManager *GetPathManager() override { return nullptr; }
    CKParameterManager *GetParameterManager() override { return nullptr; }
    CKRenderManager *GetRenderManager() override { return nullptr; }
    CKSoundManager *GetSoundManager() override { return nullptr; }
    CKTimeManager *GetTimeManager() override { return nullptr; }

    // The v0.3.12 header pair makes an int literal ambiguous (CKDWORD versus
    // float), so callers spelled the frame overload with a ul suffix; this host
    // does the same.
    void AddTimer(CKDWORD delay, std::function<void()> callback) override {
        FrameTimers.emplace_back(delay, std::move(callback));
    }
    void AddTimerLoop(CKDWORD delay, std::function<bool()> callback) override {
        FrameLoopTimers.emplace_back(delay, std::move(callback));
    }
    void AddTimer(float delay, std::function<void()> callback) override {
        SecondTimers.emplace_back(delay, std::move(callback));
    }
    void AddTimerLoop(float delay, std::function<bool()> callback) override {
        SecondLoopTimers.emplace_back(delay, std::move(callback));
    }

    bool IsCheatEnabled() override { return CheatEnabled; }
    void EnableCheat(bool enable) override { CheatEnabled = enable; }

    void SendIngameMessage(const char *msg) override {
        Messages.emplace_back(msg ? msg : "");
    }

    void RegisterCommand(ICommand *cmd) override { Commands.push_back(cmd); }

    void SetIC(CKBeObject *obj, bool hierarchy) override {}
    void RestoreIC(CKBeObject *obj, bool hierarchy) override {}
    void Show(CKBeObject *obj, CK_OBJECT_SHOWOPTION show, bool hierarchy) override {}

    bool IsIngame() override { return false; }
    bool IsPaused() override { return false; }
    bool IsPlaying() override { return false; }

    CKDataArray *GetArrayByName(const char *name) override { return nullptr; }
    CKGroup *GetGroupByName(const char *name) override { return nullptr; }
    CKMaterial *GetMaterialByName(const char *name) override { return nullptr; }
    CKMesh *GetMeshByName(const char *name) override { return nullptr; }
    CK2dEntity *Get2dEntityByName(const char *name) override { return nullptr; }
    CK3dEntity *Get3dEntityByName(const char *name) override { return nullptr; }
    CK3dObject *Get3dObjectByName(const char *name) override { return nullptr; }
    CKCamera *GetCameraByName(const char *name) override { return nullptr; }
    CKTargetCamera *GetTargetCameraByName(const char *name) override { return nullptr; }
    CKLight *GetLightByName(const char *name) override { return nullptr; }
    CKTargetLight *GetTargetLightByName(const char *name) override { return nullptr; }
    CKSound *GetSoundByName(const char *name) override { return nullptr; }
    CKTexture *GetTextureByName(const char *name) override { return nullptr; }
    CKBehavior *GetScriptByName(const char *name) override { return nullptr; }

    void RegisterBallType(const char *, const char *, const char *, const char *,
                          float, float, float, const char *, float, float, float, float) override {}
    void RegisterFloorType(const char *, float, float, float, const char *, bool) override {}
    void RegisterModulBall(const char *, bool, float, float, float, const char *, bool,
                           bool, bool, float, float, float) override {}
    void RegisterModulConvex(const char *, bool, float, float, float, const char *, bool,
                             bool, bool, float, float) override {}
    void RegisterTrafo(const char *) override {}
    void RegisterModul(const char *) override {}

    int GetModCount() override { return 0; }
    IMod *GetMod(int) override { return nullptr; }

    float GetSRScore() override { return 0.0f; }
    int GetHSScore() override { return 0; }

    void SkipRenderForNextTick() override { ++SkipRenderCalls; }

    int GetCommandCount() const override { return 0; }
    ICommand *GetCommand(int) const override { return nullptr; }
    ICommand *FindCommand(const char *) const override { return nullptr; }

    IMod *FindMod(const char *) const override { return nullptr; }

    void ExecuteCommand(const char *) override {}

    int RegisterDependency(IMod *, const char *dependencyId, int major, int minor, int patch) override {
        Dependencies.push_back({dependencyId ? dependencyId : "", BMLVersion(major, minor, patch), false});
        return BML_OK;
    }
    int RegisterOptionalDependency(IMod *, const char *dependencyId, int major, int minor, int patch) override {
        Dependencies.push_back({dependencyId ? dependencyId : "", BMLVersion(major, minor, patch), true});
        return BML_OK;
    }
    int CheckDependencies(IMod *) const override { return 1; }
    int GetDependencyCount(IMod *) const override { return 0; }
    int GetDependencyInfo(IMod *, int, char *, int, int *, int *, int *, int *) const override { return BML_ERROR_FAIL; }
    int ClearDependencies(IMod *) override { return BML_OK; }

    int ExitGameCalls = 0;
    int SkipRenderCalls = 0;
    bool CheatEnabled = false;
    std::vector<std::string> Messages;
    std::vector<ICommand *> Commands;
    std::vector<std::pair<CKDWORD, std::function<void()>>> FrameTimers;
    std::vector<std::pair<CKDWORD, std::function<bool()>>> FrameLoopTimers;
    std::vector<std::pair<float, std::function<void()>>> SecondTimers;
    std::vector<std::pair<float, std::function<bool()>>> SecondLoopTimers;
    struct RecordedDependency {
        std::string Id;
        BMLVersion MinVersion;
        bool Optional;
    };
    std::vector<RecordedDependency> Dependencies;
};

// A Mod exactly the way a v0.3.12-era native Mod DLL would write one, minus the
// dllimport plumbing: metadata, a handful of overridden callbacks, and calls
// back into the host from them.
class SnapshotMod : public IMod {
public:
    explicit SnapshotMod(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "frozen.consumer"; }
    const char *GetVersion() override { return "1.2.3"; }
    const char *GetName() override { return "Frozen SDK Consumer"; }
    const char *GetAuthor() override { return "BML Test Host"; }
    const char *GetDescription() override { return "Drives the archived v0.3.12 SDK surface."; }
    DECLARE_BML_VERSION;

    ~SnapshotMod() override { Destroyed = true; }

    void OnLoad() override {
        ++OnLoadCalls;
        m_BML->SendIngameMessage("frozen consumer loaded");
        m_BML->AddTimer(1ul, [this]() { ++TimerFirings; });
        AddDependency("some.host.mod", BMLVersion(1, 0, 0));
        AddOptionalDependency("some.other.mod", BMLVersion(0, 9, 0));
    }

    void OnUnload() override { ++OnUnloadCalls; }

    void OnProcess() override { ++OnProcessCalls; }

    void OnCheatEnabled(bool enable) override { LastCheatEnabled = enable; }

    void OnPreCommandExecute(ICommand *command, const std::vector<std::string> &args) override {
        ++OnPreCommandCalls;
    }

    static bool Destroyed;

    int OnLoadCalls = 0;
    int OnUnloadCalls = 0;
    int OnProcessCalls = 0;
    int OnPreCommandCalls = 0;
    int TimerFirings = 0;
    bool LastCheatEnabled = false;
};

bool SnapshotMod::Destroyed = false;

// The convention a native Mod DLL exports; the host drives it by hand because
// the real loader is not loadable in a unit test.
IMod *BMLEntry(IBML *bml) {
    return new SnapshotMod(bml);
}

TEST(FrozenSdkConsumer, BMLEntryDrivesModThroughArchivedAbi) {
    FakeBML host;
    SnapshotMod::Destroyed = false;

    IMod *mod = BMLEntry(&host);
    ASSERT_NE(mod, nullptr);

    // Metadata through the base pointer: the frozen vtable slots a v0.3.12-built
    // loader would call.
    EXPECT_STREQ(mod->GetID(), "frozen.consumer");
    EXPECT_STREQ(mod->GetVersion(), "1.2.3");
    const BMLVersion reported = mod->GetBMLVersion();
    EXPECT_EQ(reported.major, 0);
    EXPECT_EQ(reported.minor, 3);
    EXPECT_EQ(reported.patch, 12);

    // Lifecycle through the base pointer, with the Mod calling back into the
    // host the way real Mods do.
    mod->OnLoad();
    auto *snapshot = static_cast<SnapshotMod *>(mod);
    EXPECT_EQ(snapshot->OnLoadCalls, 1);
    ASSERT_EQ(host.Messages.size(), 1u);
    EXPECT_EQ(host.Messages[0], "frozen consumer loaded");
    ASSERT_EQ(host.FrameTimers.size(), 1u);
    EXPECT_EQ(host.FrameTimers[0].first, 1u);
    host.FrameTimers[0].second();
    EXPECT_EQ(snapshot->TimerFirings, 1);

    ASSERT_EQ(host.Dependencies.size(), 2u);
    EXPECT_EQ(host.Dependencies[0].Id, "some.host.mod");
    EXPECT_EQ(host.Dependencies[0].MinVersion.major, 1);
    EXPECT_FALSE(host.Dependencies[0].Optional);
    EXPECT_EQ(host.Dependencies[1].Id, "some.other.mod");
    EXPECT_TRUE(host.Dependencies[1].Optional);

    mod->OnProcess();
    mod->OnCheatEnabled(true);
    EXPECT_EQ(snapshot->OnProcessCalls, 1);
    EXPECT_TRUE(snapshot->LastCheatEnabled);

    const std::vector<std::string> args{"x"};
    mod->OnPreCommandExecute(nullptr, args);
    EXPECT_EQ(snapshot->OnPreCommandCalls, 1);

    // A callback the Mod does not override still answers through the archived
    // default implementation.
    mod->OnPauseLevel();
    mod->OnDead();

    mod->OnUnload();
    EXPECT_EQ(snapshot->OnUnloadCalls, 1);

    // Destruction through the base pointer exercises the virtual destructor
    // slot of the archived vtable.
    delete mod;
    EXPECT_TRUE(SnapshotMod::Destroyed);
}

TEST(FrozenSdkConsumer, ArchivedParsersBehaveAsShipped) {
    EXPECT_EQ(ICommand::ParseInteger("42"), 42);
    EXPECT_EQ(ICommand::ParseInteger("-7"), -7);
    EXPECT_EQ(ICommand::ParseInteger("99", 0, 10), 10);
    EXPECT_EQ(ICommand::ParseFloat("-1.5", -10.0f, 10.0f), -1.5f);
    EXPECT_EQ(ICommand::ParseFloat("2.5"), 2.5f);

    // Snapshot-integrity marker: v0.3.12 shipped with FLT_MIN as the default
    // lower bound, so a negative number clamps up to the smallest positive
    // normal value instead of staying negative. P0-1 later changed the default
    // to -FLT_MAX in the live headers; the archived SDK must keep shipping the
    // original behavior, or this snapshot has been tampered with.
    EXPECT_EQ(ICommand::ParseFloat("-1.5"), FLT_MIN);

    EXPECT_TRUE(ICommand::ParseBoolean("true"));
    EXPECT_TRUE(ICommand::ParseBoolean("on"));
    EXPECT_TRUE(ICommand::ParseBoolean("1"));
    EXPECT_FALSE(ICommand::ParseBoolean("false"));
    EXPECT_FALSE(ICommand::ParseBoolean("0"));
    EXPECT_FALSE(ICommand::ParseBoolean("off"));
}

TEST(FrozenSdkConsumer, ArchivedModDependencyOwnershipBalances) {
    const int mallocCallsBefore = g_MallocCalls;
    const int freeCallsBefore = g_FreeCalls;

    {
        ModDependency required("dep.id", BMLVersion(1, 2, 3));
        EXPECT_STREQ(required.id, "dep.id");
        EXPECT_EQ(required.minVersion.major, 1);
        EXPECT_EQ(required.minVersion.minor, 2);
        EXPECT_EQ(required.minVersion.patch, 3);
        EXPECT_EQ(required.optional, 0);

        ModDependency copy(required);
        EXPECT_EQ(copy, required);

        ModDependency assigned("other.id", BMLVersion(0, 0, 0), 1);
        EXPECT_EQ(assigned.optional, 1);
        assigned = required;
        EXPECT_EQ(assigned, required);
        EXPECT_NE(assigned, ModDependency("different.id", BMLVersion(9, 9, 9)));
    }

    // Every BML_Malloc the archived copy/assign/dtor path made was paired with
    // exactly one BML_Free.
    EXPECT_GT(g_MallocCalls, mallocCallsBefore);
    EXPECT_EQ(g_MallocCalls - mallocCallsBefore, g_FreeCalls - freeCallsBefore);
}

TEST(FrozenSdkConsumer, ArchivedVersionDefaultsToShippedRelease) {
    const BMLVersion defaultVersion;
    EXPECT_EQ(defaultVersion.major, 0);
    EXPECT_EQ(defaultVersion.minor, 3);
    EXPECT_EQ(defaultVersion.patch, 12);
    EXPECT_EQ(defaultVersion.ToString(), "0.3.12");

    const BMLVersion older(0, 3, 11);
    const BMLVersion same(0, 3, 12);
    EXPECT_TRUE(older < defaultVersion);
    EXPECT_FALSE(defaultVersion < older);
    EXPECT_TRUE(defaultVersion >= older);
    EXPECT_TRUE(defaultVersion == same);
}

} // namespace

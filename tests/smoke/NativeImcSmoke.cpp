#include <BML/Events.h>
#include <BML/Gameplay.h>
#include <BML/IBML.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>
#include <BML/Runtime.h>
#include <BML/Scene.h>
#include <BML/Speedrun.h>
#include <BML/UI.h>

#include "smoke_native_imc.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace {

namespace Smoke = BML::Imc::Generated::Smoke::Native;

bool IsNull(BML_ObjectRef reference) {
    return reference.Domain == 0 && reference.Slot == 0 && reference.Generation == 0;
}

bool IsOkOrUnavailable(int status) {
    return status == BML_OK || status == BML_ERROR_UNAVAILABLE;
}

bool NearlyEqual(float left, float right) {
    return std::fabs(left - right) <= 0.001f;
}

class BMLNativeImcSmoke final : public IMod {
public:
    explicit BMLNativeImcSmoke(IBML *bml) : IMod(bml) {
        AddDependency("BML");
        AddOptionalDependency("BMLNativeOptionalDependencySmoke");
    }

    const char *GetID() override { return "BMLNativeImcSmoke"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "BML Native IMC Smoke"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override { return "Validates BML native IMC interfaces in Player"; }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        m_RuntimePassed = CheckRuntime();
        m_UiPassed = CheckUi();
        m_SpeedrunPassed = CheckSpeedrun();
        m_EventsPassed = CheckEvents();
        m_ImcPassed = CheckImc();
    }

    void OnProcess() override {
        if (m_ExitRequested) {
            if (m_ExitEventChecked)
                return;
            const bool exitEvent = PollForEvent(BML_EVENT_EXIT_GAME);
            m_Passed = m_Passed && exitEvent;
            m_ExitEventChecked = true;
            GetLogger()->Info("BML native IMC smoke exit event: received=%s passed=%s",
                              Text(exitEvent), Text(m_Passed));
            return;
        }

        if (++m_ProcessCount != 30)
            return;

        const bool scene = CheckScene();
        const bool gameplay = CheckGameplay();
        const bool imc = m_ImcPassed && CheckImcNotice();
        GetLogger()->Info(
            "BML native IMC smoke: runtime=%s scene=%s gameplay=%s ui=%s speedrun=%s events=%s imc=%s",
            Text(m_RuntimePassed), Text(scene), Text(gameplay), Text(m_UiPassed),
            Text(m_SpeedrunPassed), Text(m_EventsPassed), Text(imc));
        m_Passed = m_RuntimePassed && scene && gameplay && m_UiPassed &&
                   m_SpeedrunPassed && m_EventsPassed && imc;

        GetLogger()->Info("BML native IMC smoke requesting exit");
        m_ExitRequested = true;
        m_BML->ExitGame();
    }

    void OnUnload() override {
        (void)m_Events.Close();
        (void)m_ImcNotices.Close();
        (void)m_ImcClient.Close();
        (void)m_ImcProvider.Close();
        GetLogger()->Info("BML native IMC smoke unloaded");
    }

private:
    static const char *Text(bool value) { return value ? "true" : "false"; }

    bool CheckRuntime() {
        BML::Runtime::State state{};
        BML::Runtime::Clock clock{};
        BML::Runtime::Score score{};
        if (BML::Runtime::ReadState(state) != BML_OK ||
            BML::Runtime::ReadClock(clock) != BML_OK ||
            BML::Runtime::ReadScore(score) != BML_OK) {
            return false;
        }

        // The interface flags are 0 or 1 rather than bool, so each comparison against an
        // IBML getter has to say which side is being narrowed.
        if ((state.InGame != 0) != m_BML->IsIngame() || (state.Paused != 0) != m_BML->IsPaused() ||
            (state.Playing != 0) != m_BML->IsPlaying() ||
            (state.CheatEnabled != 0) != m_BML->IsCheatEnabled() ||
            (state.InLevel && (!state.InGame || state.Paused || !state.Playing)) ||
            !std::isfinite(clock.TimeMs) || !std::isfinite(clock.AbsoluteMs) ||
            !std::isfinite(clock.DeltaMs) || clock.Frame < 0 ||
            !NearlyEqual(score.SR, m_BML->GetSRScore()) || score.HS != m_BML->GetHSScore()) {
            return false;
        }

        constexpr int sampleCount = 5000;
        const auto start = std::chrono::steady_clock::now();
        for (int sample = 0; sample < sampleCount; ++sample) {
            if (BML::Runtime::ReadState(state) != BML_OK)
                return false;
        }
        const auto elapsed = std::chrono::steady_clock::now() - start;
        const double seconds = std::chrono::duration<double>(elapsed).count();
        const double callsPerSecond = seconds > 0.0 ? sampleCount / seconds : 0.0;
        GetLogger()->Info("BML native IMC smoke runtime throughput: %.0f calls/s", callsPerSecond);
        return true;
    }

    bool CheckScene() {
        if (BML::Scene::RequireApi() != BML_OK)
            return false;

        BML::Scene::ObjectRef missing{};
        if (BML::Scene::FindObject("__bml_native_imc_smoke_missing__", missing) != BML_OK ||
            !IsNull(missing)) {
            return false;
        }

        BML::Scene::ObjectInfo invalidInfo{};
        if (BML::Scene::ReadObject({}, invalidInfo) != BML_ERROR_OBJECT_INVALID)
            return false;

        CKContext *context = m_BML->GetCKContext();
        if (!context)
            return false;

        for (const char *name : {"M_Start_But_01", "Menu_Start", "AllLevel"}) {
            CKObject *expected = context->GetObjectByName(const_cast<char *>(name));
            if (!expected)
                continue;

            BML::Scene::ObjectRef reference{};
            BML::Scene::ObjectInfo info{};
            BML::Scene::ObjectRef typedReference{};
            return BML::Scene::FindObject(name, reference) == BML_OK && !IsNull(reference) &&
                   BML::Scene::ReadObject(reference, info) == BML_OK && info.Id == expected->GetID() &&
                   BML::Scene::FindObject(name, expected->GetClassID(), typedReference) == BML_OK &&
                   typedReference.Domain == reference.Domain && typedReference.Slot == reference.Slot &&
                   typedReference.Generation == reference.Generation;
        }

        return true;
    }

    bool CheckGameplay() {
        if (BML::Gameplay::RequireApi() != BML_OK)
            return false;

        BML::Gameplay::LevelState level{};
        BML::Gameplay::EnergyState energy{};
        std::vector<BML::Gameplay::CatalogEntry> catalog;
        std::vector<BML::Gameplay::Checkpoint> checkpoints;
        std::vector<BML::Gameplay::Resetpoint> resetpoints;
        const int levelStatus = BML::Gameplay::ReadLevel(level);
        const int energyStatus = BML::Gameplay::ReadEnergy(energy);
        const int catalogStatus = BML::Gameplay::ReadCatalog(catalog);
        const int checkpointsStatus = BML::Gameplay::ReadCheckpoints(checkpoints);
        const int resetpointsStatus = BML::Gameplay::ReadResetpoints(resetpoints);
        GetLogger()->Info(
            "BML native IMC smoke gameplay: level=%d energy=%d catalog=%d checkpoints=%d resetpoints=%d",
            levelStatus, energyStatus, catalogStatus, checkpointsStatus, resetpointsStatus);
        if (levelStatus == BML_ERROR_UNAVAILABLE)
            LogArraySchema("CurrentLevel");
        if (catalogStatus == BML_ERROR_UNAVAILABLE)
            LogArraySchema("AllLevel");
        const bool catalogValues = catalogStatus == BML_ERROR_UNAVAILABLE ||
                                   (catalogStatus == BML_OK && catalog.size() == 13 &&
                                    catalog.front().File == "Level_01.nmo" &&
                                    catalog.front().StartBall == "Ball_Wood" &&
                                    catalog.front().Bonus == 100 && catalog.front().Music == 1);
        return IsOkOrUnavailable(levelStatus) && IsOkOrUnavailable(energyStatus) &&
               catalogValues && IsOkOrUnavailable(checkpointsStatus) &&
               IsOkOrUnavailable(resetpointsStatus);
    }

    void LogArraySchema(const char *name) {
        CKDataArray *array = m_BML->GetArrayByName(name);
        if (!array) {
            GetLogger()->Info("BML native IMC smoke array %s: missing", name);
            return;
        }

        std::string columns;
        for (int column = 0; column < array->GetColumnCount(); ++column) {
            if (!columns.empty())
                columns += '|';
            const char *columnName = array->GetColumnName(column);
            columns += columnName ? columnName : "<null>";
        }
        GetLogger()->Info("BML native IMC smoke array %s: rows=%d columns=%s",
                          name, array->GetRowCount(), columns.c_str());
    }

    bool CheckUi() {
        BML::UI::HUDState state{};
        return BML::UI::ReadHUDState(state) == BML_OK;
    }

    bool CheckSpeedrun() {
        BML::Speedrun::TimerState state{};
        return BML::Speedrun::ReadTimerState(state) == BML_OK &&
               std::isfinite(state.ElapsedTime) && state.ElapsedTime >= 0.0f;
    }

    bool CheckEvents() {
        if (m_Events.Open(8) != BML_OK || !m_Events.IsOpen())
            return false;

        int dropped = -1;
        BML::Events::Event event{};
        return m_Events.DroppedCount(dropped) == BML_OK && dropped == 0 &&
               m_Events.Poll(event) == BML_ERROR_NOT_FOUND;
    }

    // The loader publishes no .imc interface of its own, so the only way a native
    // Mod exercises the loader's IMC exports is by publishing one itself. This one
    // plays both sides: the provider answers the RPC and publishes the topic, the
    // client calls and subscribes.
    bool CheckImc() {
        Smoke::Provider::Handlers handlers;
        handlers.Userdata = this;
        handlers.Echo = &EchoHandler;
        if (m_ImcProvider.Start(handlers) != BML_OK || m_ImcClient.Open() != BML_OK)
            return false;

        bool available = false;
        if (m_ImcClient.IsEchoAvailable(available) != BML_OK || !available)
            return false;

        Smoke::EchoRequestValue request;
        request.Value = 41;
        request.Label = "smoke";
        Smoke::EchoReplyValue reply;
        // OnLoad runs on the game thread, so the handler answers inside this call
        // rather than waiting for the next pump.
        if (m_ImcClient.CallEcho(request, reply) != BML_OK || reply.Value != 42 ||
            reply.Label != "smoke") {
            return false;
        }

        if (m_ImcClient.SubscribeNotices(m_ImcNotices, &NoticeHandler, this, 8) != BML_OK)
            return false;

        std::size_t subscribers = 0;
        std::size_t delivered = 0;
        Smoke::NoticeValue notice;
        notice.Count = kNoticeCount;
        // A game-thread subscription is queued here and drained by the pump, so the
        // notice itself is checked back in OnProcess.
        return m_ImcProvider.Transport().GetNoticesSubscriberCount(subscribers) == BML_OK &&
               subscribers == 1 &&
               m_ImcProvider.Transport().PublishNotices(notice, &delivered) == BML_OK &&
               delivered == 1;
    }

    bool CheckImcNotice() {
        std::uint64_t dropped = 1;
        return m_NoticesReceived == 1 && !m_NoticeMismatched &&
               m_ImcNotices.DroppedCount(dropped) == BML_OK && dropped == 0;
    }

    static int EchoHandler(const Smoke::EchoRequestValue &request,
                           Smoke::EchoReplyValue &reply, void *) {
        reply.Value = request.Value + 1;
        reply.Label = request.Label;
        return BML_OK;
    }

    static void NoticeHandler(int status, Smoke::NoticeValue *value, const BML_ImcMessage *,
                              void *userdata) {
        auto *self = static_cast<BMLNativeImcSmoke *>(userdata);
        if (!self)
            return;
        if (status == BML_OK && value && value->Count == kNoticeCount)
            ++self->m_NoticesReceived;
        else
            self->m_NoticeMismatched = true;
    }

    bool PollForEvent(int expectedKind) {
        for (int attempt = 0; attempt < 16; ++attempt) {
            BML::Events::Event event{};
            const int status = m_Events.Poll(event);
            if (status == BML_ERROR_NOT_FOUND)
                return false;
            if (status != BML_OK)
                return false;
            if (event.Kind == expectedKind)
                return true;
        }
        return false;
    }

    static constexpr int kNoticeCount = 7;

    BML::Events::Stream m_Events;
    // The subscription belongs to the client, so it is declared after it and torn
    // down first.
    Smoke::Provider m_ImcProvider;
    Smoke::Client m_ImcClient;
    Smoke::NoticesSubscription m_ImcNotices;
    int m_NoticesReceived = 0;
    bool m_NoticeMismatched = false;
    int m_ProcessCount = 0;
    bool m_RuntimePassed = false;
    bool m_UiPassed = false;
    bool m_SpeedrunPassed = false;
    bool m_EventsPassed = false;
    bool m_ImcPassed = false;
    bool m_Passed = false;
    bool m_ExitRequested = false;
    bool m_ExitEventChecked = false;
};

} // namespace

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new BMLNativeImcSmoke(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}

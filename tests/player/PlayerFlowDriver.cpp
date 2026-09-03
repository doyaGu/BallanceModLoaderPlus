#include <BML/IBML.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>
#include <BML/InputHook.h>

#include "PlayerFrameCapture.h"
#include "PlayerNavigator.h"
#include "PlayerProbeApi.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

const char *ProbeStateName(std::uint32_t state) {
    switch (state) {
    case BML_PLAYER_PROBE_PASSED:
        return "pass";
    case BML_PLAYER_PROBE_FAILED:
        return "fail";
    case BML_PLAYER_PROBE_SKIPPED:
        return "skip";
    default:
        return "pending";
    }
}

// Drives the shipped Ballance flow the acceptance probes need: the menu graph
// into Level 01, the tutorial exit handshake, the frame captures, and the exit.
// It owns no subject under test. Every verdict comes from a probe Mod through
// PlayerProbeApi.h, so which subjects a run covers is decided by which probes
// the runner installs, not by this file.
class PlayerFlowDriver final : public IMod {
public:
    explicit PlayerFlowDriver(IBML *bml) : IMod(bml) {
        AddDependency("BML");
    }

    const char *GetID() override { return "PlayerFlowDriver"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "Player Flow Driver"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override {
        return "Drives the shipped Ballance flow and collects probe verdicts";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override { m_Navigator.Attach(m_BML, GetLogger()); }

    void OnPostStartMenu() override { m_Navigator.OnPostStartMenu(); }

    void OnStartLevel() override { m_Navigator.OnStartLevel(); }

    void OnBallNavActive() override {
        if (m_Navigator.ControlReady())
            return;
        m_Navigator.OnControlReady();
        // The runner waits for this line before it stops pressing the tutorial
        // exit key.
        GetLogger()->Info("Gameplay input: ready=true");
    }

    void OnProcess() override {
        if (m_Done)
            return;

        ++m_TotalFrames;
        m_Navigator.Observe(m_BML ? m_BML->GetInputManager() : nullptr);
        if (m_Navigator.TutorialFrameRequested())
            m_TutorialFrameCaptureRequested = true;

        const auto now = std::chrono::steady_clock::now();
        if (m_Navigator.LevelStarted() &&
            now - m_Navigator.LevelStartedAt() >
                std::chrono::seconds(kFlowTimeoutSeconds)) {
            Finish(false, "flow-timeout");
        }
        if (m_Navigator.MenuReady() && !m_Navigator.LevelStarted() &&
            now - m_Navigator.MenuStartedAt() >
                std::chrono::seconds(kMenuTimeoutSeconds)) {
            Finish(false, m_Navigator.Error());
        }

        switch (m_Phase) {
        case Phase::Menu:
            Navigate(m_Navigator.OpenLevelMenu(), Phase::LevelMenu);
            break;
        case Phase::LevelMenu:
            Navigate(m_Navigator.ChooseLevel(), Phase::Loading);
            break;
        case Phase::Loading:
            WaitForControl();
            break;
        case Phase::Probes:
            CollectProbes();
            break;
        case Phase::Stopping:
            Stop();
            break;
        }
    }

    void OnRender(CK_RENDER_FLAGS) override {
        if (m_TutorialFrameCaptureRequested &&
            !m_TutorialFrameCaptureAttempted) {
            m_TutorialFrameCaptureAttempted = true;
            m_TutorialFrameCaptured = CaptureFrame(
                std::getenv("BML_PLAYER_TUTORIAL_FRAME_PATH"),
                "Tutorial frame");
        }
        if (!m_FrameCaptureRequested || m_FrameCaptureAttempted)
            return;
        m_FrameCaptureAttempted = true;
        m_FrameCaptured = CaptureFrame(
            std::getenv("BML_PLAYER_FRAME_PATH"), "Player frame");
    }

    void OnExitGame() override {
        GetLogger()->Info("Player flow exit: status=%s",
                          m_Passed ? "pass" : "fail");
    }

private:
    enum class Phase {
        Menu,
        LevelMenu,
        Loading,
        Probes,
        Stopping,
    };

    struct Probe {
        std::string Id;
        BMLPlayerProbeReadFn Read = nullptr;
        BMLPlayerProbeStartFn StartFn = nullptr;
        std::uint32_t State = BML_PLAYER_PROBE_PENDING;
        std::string Detail = "not-run";
        bool Started = false;
    };

    static constexpr int kMenuTimeoutSeconds = 15;
    static constexpr int kFlowTimeoutSeconds = 180;
    static constexpr int kMinimumVisibleLevelSeconds = 5;
    static constexpr auto kStopDelay = std::chrono::milliseconds(100);
    // Probes that read gameplay state start on a settled world, the way the
    // level looks a moment after the tutorial releases input.
    static constexpr auto kWorldSettleTime = std::chrono::milliseconds(250);

    bool CaptureFrame(const char *path, const char *label) {
        CKRenderContext *render = m_BML ? m_BML->GetRenderContext() : nullptr;
        std::uint32_t directXVersion = 0;
        long nativeError = E_INVALIDARG;
        const bool captured = path && *path && render &&
            BML::PlayerTest::SaveRenderFrame(render, path, directXVersion,
                                             nativeError);
        GetLogger()->Info("%s: captured=%s directx=0x%04x native_error=%ld",
                          label, captured ? "true" : "false", directXVersion,
                          nativeError);
        return captured;
    }

    void Navigate(BML::PlayerTest::PlayerNavigator::Step step, Phase next) {
        using Step = BML::PlayerTest::PlayerNavigator::Step;
        if (step == Step::Failed) {
            Finish(false, m_Navigator.Error());
            return;
        }
        if (step == Step::Done)
            SetPhase(next);
    }

    void WaitForControl() {
        if (!m_Navigator.TutorialExited() || !m_Navigator.ControlReady() ||
            !m_BML->IsPlaying())
            return;
        SetPhase(Phase::Probes);
    }

    // A probe is any loaded Mod whose DLL exports the read entry point. The
    // driver never names a probe, so a run covers exactly what is installed.
    void DiscoverProbes() {
        m_ProbesDiscovered = true;
        const int count = m_BML ? m_BML->GetModCount() : 0;
        for (int index = 0; index < count; ++index) {
            IMod *mod = m_BML->GetMod(index);
            if (!mod || mod == this || !mod->GetID())
                continue;
            const std::string id = mod->GetID();
            const std::string file = id + ".bmodp";
            HMODULE module = ::GetModuleHandleA(file.c_str());
            if (!module)
                continue;
            auto read = reinterpret_cast<BMLPlayerProbeReadFn>(
                ::GetProcAddress(module, BML_PLAYER_PROBE_READ_SYMBOL));
            if (!read)
                continue;
            Probe probe;
            probe.Id = id;
            probe.Read = read;
            probe.StartFn = reinterpret_cast<BMLPlayerProbeStartFn>(
                ::GetProcAddress(module, BML_PLAYER_PROBE_START_SYMBOL));
            m_Probes.push_back(probe);
        }
        GetLogger()->Info("Player probes: discovered=%d mods=%d",
                          static_cast<int>(m_Probes.size()), count);
    }

    void StartProbes() {
        m_ProbesStarted = true;
        const BMLPlayerProbeStartInfo info;
        for (Probe &probe : m_Probes) {
            if (!probe.StartFn) {
                // The probe runs from its own Mod callbacks and does not read
                // gameplay state, so it is already running by now.
                probe.Started = true;
                GetLogger()->Info("Player probe: mod=%s start=self",
                                  probe.Id.c_str());
                continue;
            }
            probe.Started = probe.StartFn(&info) != 0;
            GetLogger()->Info("Player probe: mod=%s start=%s", probe.Id.c_str(),
                              probe.Started ? "true" : "false");
            if (!probe.Started)
                Finish(false, "probe-start-rejected");
        }
    }

    void CollectProbes() {
        if (!m_ProbesDiscovered)
            DiscoverProbes();
        if (!m_ProbesStarted) {
            if (!PhaseDone(kWorldSettleTime))
                return;
            StartProbes();
            if (m_Phase == Phase::Stopping)
                return;
        }

        bool pending = false;
        for (Probe &probe : m_Probes) {
            if (probe.State != BML_PLAYER_PROBE_PENDING)
                continue;
            BMLPlayerProbeResult result;
            if (!probe.Read(&result)) {
                probe.State = BML_PLAYER_PROBE_FAILED;
                probe.Detail = "read-rejected";
                continue;
            }
            if (result.State == BML_PLAYER_PROBE_PENDING) {
                pending = true;
                continue;
            }
            probe.State = result.State;
            probe.Detail = result.Detail;
        }
        if (pending)
            return;

        // Every probe has reported, so the level is still on screen for the
        // frame the runner compares against the window screenshot.
        m_FrameCaptureRequested = true;
        Check();
    }

    void Check() {
        bool probesPassed = true;
        for (const Probe &probe : m_Probes) {
            if (probe.State != BML_PLAYER_PROBE_PASSED &&
                probe.State != BML_PLAYER_PROBE_SKIPPED)
                probesPassed = false;
        }
        const bool passed = probesPassed &&
                            m_Navigator.MenuOpened() &&
                            m_Navigator.LevelChosen() &&
                            m_Navigator.ControlReady() &&
                            m_Navigator.TutorialExitDeclared() &&
                            m_Navigator.TutorialExitReadyObserved() &&
                            m_Navigator.TutorialExitInputObserved() &&
                            m_Navigator.TutorialExited() &&
                            m_Navigator.TutorialExitedByInput();
        Finish(passed, passed ? "completed"
                              : (probesPassed ? "flow-mismatch"
                                              : "probe-failed"));
    }

    void Finish(bool passed, const char *reason) {
        if (m_Phase == Phase::Stopping)
            return;
        m_Passed = passed;
        m_Reason = reason;
        SetPhase(Phase::Stopping);
    }

    void Stop() {
        if (!PhaseDone(kStopDelay))
            return;
        if (m_FrameCaptureRequested && !m_FrameCaptureAttempted)
            return;
        if (m_Navigator.LevelStarted() &&
            std::chrono::steady_clock::now() - m_Navigator.LevelStartedAt() <
                std::chrono::seconds(kMinimumVisibleLevelSeconds))
            return;

        int passed = 0;
        int skipped = 0;
        int failed = 0;
        int pendingProbes = 0;
        for (const Probe &probe : m_Probes) {
            switch (probe.State) {
            case BML_PLAYER_PROBE_PASSED:
                ++passed;
                break;
            case BML_PLAYER_PROBE_SKIPPED:
                ++skipped;
                break;
            case BML_PLAYER_PROBE_FAILED:
                ++failed;
                break;
            default:
                ++pendingProbes;
                break;
            }
            GetLogger()->Info(
                "Player probe: mod=%s state=%s started=%s detail=%s",
                probe.Id.c_str(), ProbeStateName(probe.State),
                probe.Started ? "true" : "false", probe.Detail.c_str());
        }
        GetLogger()->Info(
            "Player flow: status=%s reason=%s menu_opened=%s level_chosen=%s "
            "control_ready=%s tutorial_declared=%s tutorial_listener=%s "
            "tutorial_exited=%s tutorial_by_input=%s probes=%d passed=%d "
            "skipped=%d failed=%d pending=%d frames=%d",
            m_Passed ? "pass" : "fail", m_Reason,
            m_Navigator.MenuOpened() ? "true" : "false",
            m_Navigator.LevelChosen() ? "true" : "false",
            m_Navigator.ControlReady() ? "true" : "false",
            m_Navigator.TutorialExitDeclared() ? "true" : "false",
            m_Navigator.TutorialExitReadyObserved() ? "true" : "false",
            m_Navigator.TutorialExited() ? "true" : "false",
            m_Navigator.TutorialExitedByInput() ? "true" : "false",
            static_cast<int>(m_Probes.size()), passed, skipped, failed,
            pendingProbes, m_TotalFrames);
        m_Done = true;
        m_BML->ExitGame();
    }

    void SetPhase(Phase phase) {
        m_Phase = phase;
        m_PhaseStartedAt = std::chrono::steady_clock::now();
    }

    [[nodiscard]] bool PhaseDone(
        std::chrono::steady_clock::duration duration) const {
        return std::chrono::steady_clock::now() - m_PhaseStartedAt >= duration;
    }

    BML::PlayerTest::PlayerNavigator m_Navigator;
    Phase m_Phase = Phase::Menu;
    std::vector<Probe> m_Probes;
    const char *m_Reason = "not-completed";
    int m_TotalFrames = 0;
    bool m_ProbesDiscovered = false;
    bool m_ProbesStarted = false;
    bool m_FrameCaptureRequested = false;
    bool m_FrameCaptureAttempted = false;
    bool m_FrameCaptured = false;
    bool m_TutorialFrameCaptureRequested = false;
    bool m_TutorialFrameCaptureAttempted = false;
    bool m_TutorialFrameCaptured = false;
    bool m_Passed = false;
    bool m_Done = false;
    std::chrono::steady_clock::time_point m_PhaseStartedAt{};
};

} // namespace

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new PlayerFlowDriver(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}

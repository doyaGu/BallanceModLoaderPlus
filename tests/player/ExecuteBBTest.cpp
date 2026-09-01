#include <BML/ExecuteBB.h>
#include <BML/IBML.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>
#include <BML/ScriptHelper.h>

#include "BehaviorRuntimeProbe.h"

#include "Behavior/HookBlock.h"
#include "Behavior/Runtime.h"

#ifndef DIRECTDRAW_VERSION
#define DIRECTDRAW_VERSION 0x0700
#endif
#include <ddraw.h>
#include <d3d9.h>

#include <chrono>
#include <cstddef>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace {

struct ScriptHookSourceProbe {
    int Calls = 0;
};

int RunScriptHookSource(const CKBehaviorContext *, void *argument) {
    auto *probe = static_cast<ScriptHookSourceProbe *>(argument);
    if (!probe)
        return CKBR_BEHAVIORERROR;
    ++probe->Calls;
    return CKBR_OK;
}

std::uint8_t ColorComponent(std::uint32_t pixel, std::uint32_t mask) {
    if (!mask)
        return 0;
    unsigned shift = 0;
    while (((mask >> shift) & 1u) == 0u)
        ++shift;
    const std::uint32_t value = (pixel & mask) >> shift;
    const std::uint32_t maximum = mask >> shift;
    return static_cast<std::uint8_t>((value * 255u + maximum / 2u) / maximum);
}

bool WriteFrame(const char *path, const void *sourceData,
                std::ptrdiff_t sourcePitch, std::uint32_t width,
                std::uint32_t height, std::uint32_t bits,
                std::uint32_t redMask, std::uint32_t greenMask,
                std::uint32_t blueMask, long &nativeError) {
    if (!path || !*path || !sourceData || !width || !height ||
        (bits != 16 && bits != 24 && bits != 32)) {
        nativeError = E_INVALIDARG;
        return false;
    }
    const std::uint32_t targetPitch = (width * 3u + 3u) & ~3u;
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(targetPitch) * height, 0);
    const auto *source = static_cast<const std::uint8_t *>(sourceData);
    if (sourcePitch < 0)
        source -= sourcePitch * static_cast<std::ptrdiff_t>(height - 1u);
    const std::uint32_t sourceBytes = bits / 8u;
    for (std::uint32_t y = 0; y < height; ++y) {
        const auto *sourceRow = source +
            sourcePitch * static_cast<std::ptrdiff_t>(y);
        auto *targetRow = pixels.data() +
            static_cast<std::size_t>(targetPitch) * y;
        for (std::uint32_t x = 0; x < width; ++x) {
            std::uint32_t pixel = 0;
            std::memcpy(&pixel, sourceRow + x * sourceBytes, sourceBytes);
            targetRow[x * 3u] = ColorComponent(pixel, blueMask);
            targetRow[x * 3u + 1u] = ColorComponent(pixel, greenMask);
            targetRow[x * 3u + 2u] = ColorComponent(pixel, redMask);
        }
    }

    BITMAPFILEHEADER file{};
    BITMAPINFOHEADER info{};
    info.biSize = sizeof(info);
    info.biWidth = static_cast<LONG>(width);
    info.biHeight = -static_cast<LONG>(height);
    info.biPlanes = 1;
    info.biBitCount = 24;
    info.biCompression = BI_RGB;
    info.biSizeImage = static_cast<DWORD>(pixels.size());
    file.bfType = 0x4d42;
    file.bfOffBits = sizeof(file) + sizeof(info);
    file.bfSize = file.bfOffBits + info.biSizeImage;

    HANDLE output = CreateFileA(path, GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                nullptr);
    if (output == INVALID_HANDLE_VALUE) {
        nativeError = static_cast<long>(GetLastError());
        return false;
    }
    DWORD written = 0;
    const bool headerWritten =
        WriteFile(output, &file, sizeof(file), &written, nullptr) &&
        written == sizeof(file) &&
        WriteFile(output, &info, sizeof(info), &written, nullptr) &&
        written == sizeof(info);
    const DWORD pixelBytes = static_cast<DWORD>(pixels.size());
    const bool pixelsWritten = headerWritten &&
        WriteFile(output, pixels.data(), pixelBytes,
                  &written, nullptr) &&
        written == pixelBytes;
    DWORD writeError = pixelsWritten ? ERROR_SUCCESS : GetLastError();
    if (!pixelsWritten && writeError == ERROR_SUCCESS)
        writeError = ERROR_WRITE_FAULT;
    CloseHandle(output);
    if (!pixelsWritten) {
        nativeError = static_cast<long>(writeError);
        DeleteFileA(path);
        return false;
    }
    nativeError = 0;
    return true;
}

bool SaveRenderFrame(CKRenderContext *render, const char *path,
                     std::uint32_t &version, long &nativeError) {
    VxDirectXData *directX = render ? render->GetDirectXInfo() : nullptr;
    version = directX ? directX->DxVersion : 0;
    if (!directX) {
        nativeError = E_NOINTERFACE;
        return false;
    }

    if (directX->DxVersion == 0x0900) {
        auto *device = static_cast<IDirect3DDevice9 *>(directX->D3DDevice);
        if (!device) {
            nativeError = E_NOINTERFACE;
            return false;
        }
        IDirect3DSurface9 *backBuffer = nullptr;
        HRESULT result = device->GetBackBuffer(
            0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
        if (FAILED(result) || !backBuffer) {
            nativeError = result;
            return false;
        }
        D3DSURFACE_DESC description{};
        result = backBuffer->GetDesc(&description);
        IDirect3DSurface9 *memory = nullptr;
        if (SUCCEEDED(result)) {
            result = device->CreateOffscreenPlainSurface(
                description.Width, description.Height, description.Format,
                D3DPOOL_SYSTEMMEM, &memory, nullptr);
        }
        if (SUCCEEDED(result))
            result = device->GetRenderTargetData(backBuffer, memory);
        D3DLOCKED_RECT locked{};
        if (SUCCEEDED(result))
            result = memory->LockRect(&locked, nullptr, D3DLOCK_READONLY);
        if (FAILED(result)) {
            if (memory)
                memory->Release();
            backBuffer->Release();
            nativeError = result;
            return false;
        }

        std::uint32_t bits = 0;
        std::uint32_t redMask = 0;
        std::uint32_t greenMask = 0;
        std::uint32_t blueMask = 0;
        switch (description.Format) {
        case D3DFMT_A8R8G8B8:
        case D3DFMT_X8R8G8B8:
            bits = 32;
            redMask = 0x00ff0000u;
            greenMask = 0x0000ff00u;
            blueMask = 0x000000ffu;
            break;
        case D3DFMT_R5G6B5:
            bits = 16;
            redMask = 0xf800u;
            greenMask = 0x07e0u;
            blueMask = 0x001fu;
            break;
        case D3DFMT_A1R5G5B5:
        case D3DFMT_X1R5G5B5:
            bits = 16;
            redMask = 0x7c00u;
            greenMask = 0x03e0u;
            blueMask = 0x001fu;
            break;
        default:
            break;
        }
        const bool saved = WriteFrame(
            path, locked.pBits, locked.Pitch, description.Width,
            description.Height, bits, redMask, greenMask, blueMask,
            nativeError);
        memory->UnlockRect();
        memory->Release();
        backBuffer->Release();
        return saved;
    }

    if (directX->DxVersion == 0x0700) {
        if (!directX->DDBackBuffer) {
            nativeError = E_NOINTERFACE;
            return false;
        }
        auto *surface = static_cast<IDirectDrawSurface7 *>(
            directX->DDBackBuffer);
        DDSURFACEDESC2 description{};
        description.dwSize = sizeof(description);
        const HRESULT result = surface->Lock(
            nullptr, &description, DDLOCK_WAIT | DDLOCK_READONLY, nullptr);
        if (FAILED(result)) {
            nativeError = result;
            return false;
        }
        const bool saved = WriteFrame(
            path, description.lpSurface, description.lPitch,
            description.dwWidth, description.dwHeight,
            description.ddpfPixelFormat.dwRGBBitCount,
            description.ddpfPixelFormat.dwRBitMask,
            description.ddpfPixelFormat.dwGBitMask,
            description.ddpfPixelFormat.dwBBitMask, nativeError);
        surface->Unlock(nullptr);
        return saved;
    }

    nativeError = E_NOINTERFACE;
    return false;
}

class ExecuteBBTest final : public IMod {
public:
    explicit ExecuteBBTest(IBML *bml) : IMod(bml) {
        AddDependency("BML");
    }

    const char *GetID() override { return "ExecuteBBTest"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "ExecuteBB Test"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override {
        return "Tests the public ExecuteBB interface in Ballance Player";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        m_ScriptHookSetupFailed = !CreateScriptHookGraph();
    }

    void OnPostStartMenu() override {
        m_MenuReady = true;
        m_MenuStartedAt = std::chrono::steady_clock::now();
    }

    void OnStartLevel() override {
        m_LevelStarted = true;
        m_LevelStartedAt = std::chrono::steady_clock::now();
    }

    void OnBallNavActive() override {
        m_ControlReady = true;
        m_ControlReadyAt = std::chrono::steady_clock::now();
    }

    void OnProcess() override {
        if (m_Done)
            return;

        if (m_ScriptHookSetupFailed) {
            Finish(false, "script-hook-graph-create-failed");
            return;
        }
        if (!m_ScriptHookRetirementPassed)
            AdvanceScriptHook();
        if (m_Done)
            return;

        ++m_TotalFrames;
        if (m_ControlReady && !m_FrameCaptureAttempted &&
            std::chrono::steady_clock::now() - m_ControlReadyAt >=
                kGameplaySettleTime) {
            m_FrameCaptureRequested = true;
        }
        if (m_LevelStarted && std::chrono::steady_clock::now() - m_LevelStartedAt >
            std::chrono::seconds(kTestTimeoutSeconds)) {
            Finish(false, "test-timeout");
        }
        if (m_MenuReady && !m_LevelStarted &&
            std::chrono::steady_clock::now() - m_MenuStartedAt >
                std::chrono::seconds(kMenuTimeoutSeconds)) {
            Finish(false, m_MenuError);
        }

        switch (m_Phase) {
        case Phase::Menu:
            OpenLevelMenu();
            break;
        case Phase::LevelMenu:
            ChooseLevel();
            break;
        case Phase::Loading:
            WaitForControl();
            break;
        case Phase::World:
            CreateBody();
            break;
        case Phase::RuntimeProbe:
            AdvanceRuntimeProbe();
            break;
        case Phase::Body:
            Push();
            break;
        case Phase::Push:
            Pull();
            break;
        case Phase::Pull:
            Release();
            break;
        case Phase::Released:
            Check();
            break;
        case Phase::Stopping:
            Stop();
            break;
        }
    }

    void OnRender(CK_RENDER_FLAGS) override {
        if (!m_FrameCaptureRequested || m_FrameCaptureAttempted)
            return;
        m_FrameCaptureAttempted = true;
        const char *path = std::getenv("BML_PLAYER_FRAME_PATH");
        CKRenderContext *render = m_BML ? m_BML->GetRenderContext() : nullptr;
        std::uint32_t directXVersion = 0;
        long nativeError = E_INVALIDARG;
        m_FrameCaptured = path && *path && render &&
            SaveRenderFrame(render, path, directXVersion, nativeError);
        GetLogger()->Info(
            "Player frame: captured=%s directx=0x%04x native_error=%ld",
            m_FrameCaptured ? "true" : "false", directXVersion,
            nativeError);
    }

    void OnPhysicalize(CK3dEntity *target, CKBOOL, float, float, float, const char *,
                       CKBOOL, CKBOOL, CKBOOL, float, float, const char *, VxVector,
                       int, CKMesh **, int, VxVector *, float *, int, CKMesh **) override {
        if (target == m_Body)
            m_PhysicalizeSeen = true;
    }

    void OnUnphysicalize(CK3dEntity *target) override {
        if (target == m_Body)
            m_UnphysicalizeSeen = true;
    }

    void OnExitGame() override {
        GetLogger()->Info("ExecuteBB test exit: status=%s",
                          m_Passed ? "pass" : "fail");
    }

    void OnUnload() override {
        DestroyBody();
    }

private:
    enum class Phase {
        Menu,
        LevelMenu,
        Loading,
        World,
        RuntimeProbe,
        Body,
        Push,
        Pull,
        Released,
        Stopping,
    };

    static constexpr int kMenuDelayFrames = 5;
    static constexpr int kMenuTimeoutSeconds = 15;
    static constexpr int kTestTimeoutSeconds = 60;
    static constexpr int kMinimumVisibleLevelSeconds = 5;
    static constexpr float kForceMagnitude = 1000.0f;
    static constexpr float kMinimumTravel = 0.01f;
    static constexpr float kReleasedX = 321.0f;
    static constexpr float kReleaseTolerance = 0.05f;
    static constexpr auto kGameplaySettleTime = std::chrono::milliseconds(500);
    static constexpr auto kWorldSettleTime = std::chrono::milliseconds(250);
    static constexpr auto kPhysicalizationSettleTime = std::chrono::milliseconds(500);
    static constexpr auto kForceObservationTimeout = std::chrono::seconds(5);
    static constexpr auto kReleaseObservationTime = std::chrono::milliseconds(250);
    static constexpr auto kStopDelay = std::chrono::milliseconds(100);

    void OpenLevelMenu() {
        if (!m_MenuReady)
            return;
        if (++m_PhaseFrames < kMenuDelayFrames)
            return;

        CKBehavior *menuMain = m_BML->GetScriptByName("Menu_Main");
        if (!menuMain) {
            m_MenuError = "menu-main-not-ready";
            return;
        }

        CKBehavior *start = ScriptHelper::FindFirstBB(
            menuMain, "Start", false, 1, 1);
        if (!start) {
            Finish(false, "menu-start-path-not-found");
            return;
        }

        // Menu.nmo: Main Menu.Button 1 pressed -> Menu_Main/Start.In 0.
        // Activate the click's proven downstream seam and leave the original
        // Activate Script graph to open Menu_Start.
        start->ActivateInput(0);
        start->Activate();
        m_LevelMenuOpened = true;
        GetLogger()->Info(
            "ExecuteBB test menu: opened=true path=Menu_Main/Start.In0");
        SetPhase(Phase::LevelMenu);
    }

    void ChooseLevel() {
        CKBehavior *menuScript = m_BML->GetScriptByName("Menu_Start");
        CK2dEntity *levelButton = m_BML->Get2dEntityByName("M_Start_But_01");
        if (!menuScript || !levelButton || !levelButton->IsVisible()) {
            m_MenuError = "level-menu-not-ready";
            return;
        }

        CKBehavior *levelMenu = ScriptHelper::FindFirstBB(
            menuScript, "Start Menu", false, 1, 2);
        if (!levelMenu) {
            Finish(false, "level-menu-path-not-found");
            return;
        }

        CKBehavior *buttonBehavior = nullptr;
        for (int i = 0; i < levelMenu->GetSubBehaviorCount(); ++i) {
            CKBehavior *candidate = levelMenu->GetSubBehavior(i);
            if (!candidate || !candidate->GetName() ||
                std::strcmp(candidate->GetName(), "TT PushButton2") != 0 ||
                candidate->GetOutputCount() <= 2 || !candidate->GetTargetParameter()) {
                continue;
            }
            CKParameter *targetSource = candidate->GetTargetParameter()->GetRealSource();
            if (targetSource && targetSource->GetValueObject() == levelButton) {
                buttonBehavior = candidate;
                break;
            }
        }
        if (!buttonBehavior) {
            Finish(false, "level-button-path-not-found");
            return;
        }

        CKBehaviorLink *mouseDown = ScriptHelper::FindNextLink(
            levelMenu, buttonBehavior, "Parameter Selector", 2);
        CKBehaviorIO *selectorInput = mouseDown ? mouseDown->GetOutBehaviorIO() : nullptr;
        CKBehavior *selector = selectorInput ? selectorInput->GetOwner() : nullptr;
        int selectorInputIndex = -1;
        if (selector) {
            for (int i = 0; i < selector->GetInputCount(); ++i) {
                if (selector->GetInput(i) == selectorInput) {
                    selectorInputIndex = i;
                    break;
                }
            }
        }
        if (!selector || selectorInputIndex != 0) {
            Finish(false, "level-button-link-mismatch");
            return;
        }

        // Menu.nmo: M_Start_But_01.Mouse Down -> Parameter Selector.In 0.
        // Deliver exactly that link's effect; the selector and every following
        // message/test/load block remain the shipped graph's responsibility.
        selector->ActivateInput(selectorInputIndex);
        selector->Activate();
        m_LevelChosen = true;
        GetLogger()->Info(
            "ExecuteBB test menu: level=1 path=Start_Menu/Parameter_Selector.In0");
        SetPhase(Phase::Loading);
    }

    void WaitForControl() {
        if (!m_LevelStarted || !m_ControlReady || !m_BML->IsPlaying())
            return;
        if (std::chrono::steady_clock::now() - m_ControlReadyAt <
            kGameplaySettleTime)
            return;
        SetPhase(Phase::World);
    }

    void CreateBody() {
        CKContext *context = m_BML->GetCKContext();
        CKLevel *level = context ? context->GetCurrentLevel() : nullptr;
        CKScene *scene = context ? context->GetCurrentScene() : nullptr;
        if (!context || !level || !scene)
            return;

        if (!PhaseDone(kWorldSettleTime))
            return;

        auto *body = CK3dObject::Cast(context->CreateObject(
            CKCID_3DOBJECT, const_cast<char *>("__BML_ExecuteBB_Test"),
            static_cast<CK_OBJECTCREATION_OPTIONS>(CK_OBJECTCREATION_DYNAMIC |
                                                    CK_OBJECTCREATION_ACTIVATE)));
        if (!body) {
            Finish(false, "body-create-failed");
            return;
        }

        m_Body = body;
        if (level->AddObject(m_Body) != CK_OK) {
            Finish(false, "body-add-failed");
            return;
        }
        if (scene != level->GetLevelScene())
            scene->AddObject(m_Body);
        scene->Activate(m_Body, TRUE);

        const VxVector origin(0.0f, 0.0f, 5000.0f);
        m_Body->SetPosition(&origin);
        m_InitialX = ReadX();

        m_RuntimeProbe = std::make_unique<BehaviorRuntimeProbe>(context, m_Body);
        SetPhase(Phase::RuntimeProbe);
    }

    bool CreateScriptHookGraph() {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        if (!context)
            return false;

        m_ScriptHookRuntime = std::make_unique<BML::Behavior::Runtime>(context);
        m_ScriptHookGraph = static_cast<CKBehavior *>(context->CreateObject(
            CKCID_BEHAVIOR,
            const_cast<char *>("__BML_ScriptHook_Fixture"),
            CK_OBJECTCREATION_DYNAMIC));
        if (!m_ScriptHookGraph)
            return false;
        m_ScriptHookGraph->UseGraph();
        m_ScriptHookGraph->SetType(CKBEHAVIORTYPE_SCRIPT);
        CKBehaviorIO *input = m_ScriptHookGraph->CreateInput("In");
        CKBehaviorIO *output = m_ScriptHookGraph->CreateOutput("Out");
        if (!input || !output)
            return false;

        BML::Behavior::AttachResult source = m_ScriptHookRuntime->AddToGraph(
            m_ScriptHookGraph,
            BML::Behavior::HookBlock::Make(
                RunScriptHookSource, &m_ScriptHookSource, 1, 1));
        m_ScriptHookSourceBlock = source.Block;
        if (!source || !m_ScriptHookSourceBlock)
            return false;
        m_ScriptHookSourceBlock->SetName(
            const_cast<char *>("__BML_ScriptHook_Source"));

        auto createLink = [&](CKBehaviorIO *from, CKBehaviorIO *to) {
            auto *link = static_cast<CKBehaviorLink *>(context->CreateObject(
                CKCID_BEHAVIORLINK, nullptr, CK_OBJECTCREATION_DYNAMIC));
            if (!link || link->SetInBehaviorIO(from) != CK_OK ||
                link->SetOutBehaviorIO(to) != CK_OK ||
                m_ScriptHookGraph->AddSubBehaviorLink(link) != CK_OK) {
                if (link)
                    context->DestroyObject(link);
                return false;
            }
            return true;
        };
        if (!createLink(input, m_ScriptHookSourceBlock->GetInput(0)) ||
            !createLink(m_ScriptHookSourceBlock->GetOutput(0), output)) {
            return false;
        }
        bool outgoing = false;
        for (int index = 0;
             index < m_ScriptHookGraph->GetSubBehaviorLinkCount(); ++index) {
            CKBehaviorLink *link = m_ScriptHookGraph->GetSubBehaviorLink(index);
            if (link && link->GetInBehaviorIO() ==
                            m_ScriptHookSourceBlock->GetOutput(0)) {
                outgoing = true;
            }
        }
        GetLogger()->Info(
            "ScriptHook fixture: lookup=%s name=%s children=%d links=%d "
            "source_outputs=%d outgoing=%s",
            m_BML->GetScriptByName("__BML_ScriptHook_Fixture") ==
                    m_ScriptHookGraph
                ? "true"
                : "false",
            m_ScriptHookGraph->GetName(),
            m_ScriptHookGraph->GetSubBehaviorCount(),
            m_ScriptHookGraph->GetSubBehaviorLinkCount(),
            m_ScriptHookSourceBlock->GetOutputCount(),
            outgoing ? "true" : "false");
        return true;
    }

    void AdvanceScriptHook() {
        if (!m_ScriptHookGraph || !m_ScriptHookRuntime) {
            Finish(false, "script-hook-graph-missing");
            return;
        }
        if (++m_ScriptHookFrames > 1800) {
            Finish(false, "script-hook-timeout");
            return;
        }
        const int children = m_ScriptHookGraph->GetSubBehaviorCount();
        if (!m_ScriptHookInstalled) {
            if (children < 2)
                return;
            m_ScriptHookInstalled = true;
            CKBehavior *inserted = nullptr;
            for (int index = 0; index < children; ++index) {
                CKBehavior *candidate = m_ScriptHookGraph->GetSubBehavior(index);
                if (candidate && candidate->GetName() &&
                    std::strcmp(candidate->GetName(),
                                "__BML_ScriptHook_Inserted") == 0) {
                    inserted = candidate;
                    break;
                }
            }
            if (!inserted || !inserted->GetInput(0)) {
                Finish(false, "script-hook-inserted-block-missing");
                return;
            }
            const float delta =
                m_BML->GetCKContext()->m_BehaviorContext.DeltaTime;
            inserted->ActivateInput(0, TRUE);
            inserted->Activate(TRUE, FALSE);
            (void) inserted->Execute(delta);
            inserted->ActivateInput(0, TRUE);
            inserted->Activate(TRUE, FALSE);
            (void) inserted->Execute(delta);
            return;
        }
        if (children != 1)
            return;

        m_ScriptHookRetirementPassed = true;
        DestroyScriptHookGraph();
    }

    void AdvanceRuntimeProbe() {
        if (!m_RuntimeProbe) {
            Finish(false, "runtime-probe-missing");
            return;
        }
        m_RuntimeProbe->Advance(m_TotalFrames);
        if (!m_RuntimeProbe->Done())
            return;

        const BehaviorRuntimeProbeResult result = m_RuntimeProbe->Result();
        m_RuntimeProbePassed = result.Passed;
        m_LifecycleProbePassed = result.LifecyclePassed;
        m_AdditiveEditProbePassed = result.AdditiveEditPassed;
        m_RuntimeProbeDetail = result.Detail;
        m_RuntimeProbe.reset();
        if (!m_RuntimeProbePassed) {
            Finish(false, "runtime-probe-failed");
            return;
        }

        ExecuteBB::PhysicalizeBall(
            m_Body, FALSE, 0.0f, 0.0f, 1.0f, "", FALSE, FALSE, FALSE,
            0.0f, 0.0f, "", VxVector(), VxVector(), 2.0f);
        m_Physicalized = true;
        SetPhase(Phase::Body);
    }

    void Push() {
        if (!PhaseDone(kPhysicalizationSettleTime))
            return;

        if (!BodyPositionIsFinite()) {
            Finish(false, "invalid-body-position");
            return;
        }

        m_PushStartX = ReadX();
        ExecuteBB::SetPhysicsForce(m_Body, VxVector(), nullptr,
                                   VxVector(1.0f, 0.0f, 0.0f), nullptr,
                                   kForceMagnitude);
        m_ForceSet = true;
        SetPhase(Phase::Push);
    }

    void Pull() {
        if (!BodyPositionIsFinite()) {
            Finish(false, "invalid-positive-force-position");
            return;
        }

        m_PushedX = ReadX();
        if (m_PushedX - m_PushStartX <= kMinimumTravel) {
            if (PhaseDone(kForceObservationTimeout))
                Finish(false, "positive-force-no-motion");
            return;
        }
        ExecuteBB::SetPhysicsForce(m_Body, VxVector(), nullptr,
                                   VxVector(-1.0f, 0.0f, 0.0f), nullptr,
                                   kForceMagnitude);
        SetPhase(Phase::Pull);
    }

    void Release() {
        if (!BodyPositionIsFinite()) {
            Finish(false, "invalid-reverse-force-position");
            return;
        }

        m_PulledX = ReadX();
        if (m_PushedX - m_PulledX <= kMinimumTravel) {
            if (PhaseDone(kForceObservationTimeout))
                Finish(false, "reverse-force-no-motion");
            return;
        }
        ExecuteBB::UnsetPhysicsForce(m_Body);
        m_ForceSet = false;
        ExecuteBB::Unphysicalize(m_Body);
        m_Physicalized = false;

        const VxVector released(kReleasedX, 0.0f, 5000.0f);
        m_Body->SetPosition(&released);
        SetPhase(Phase::Released);
    }

    void Check() {
        if (!PhaseDone(kReleaseObservationTime))
            return;

        m_ReleasedX = ReadX();
        const float pushTravel = m_PushedX - m_PushStartX;
        const float pullTravel = m_PushedX - m_PulledX;
        const bool passed = BodyPositionIsFinite() &&
                            pushTravel > kMinimumTravel &&
                            pullTravel > kMinimumTravel &&
                            std::fabs(m_ReleasedX - kReleasedX) <= kReleaseTolerance &&
                            m_PhysicalizeSeen && m_UnphysicalizeSeen &&
                            m_RuntimeProbePassed && m_LifecycleProbePassed &&
                            m_AdditiveEditProbePassed &&
                            m_ScriptHookRetirementPassed;
        Finish(passed, passed ? "completed" : "result-mismatch");
    }

    void Finish(bool passed, const char *reason) {
        if (m_Phase == Phase::Stopping)
            return;

        m_Passed = passed;
        m_Reason = reason;
        if (m_Body) {
            if (m_ForceSet) {
                ExecuteBB::UnsetPhysicsForce(m_Body);
                m_ForceSet = false;
            }
            if (m_Physicalized) {
                ExecuteBB::Unphysicalize(m_Body);
                m_Physicalized = false;
            }
        }
        SetPhase(Phase::Stopping);
    }

    void Stop() {
        if (!PhaseDone(kStopDelay))
            return;
        if (m_LevelStarted && std::chrono::steady_clock::now() - m_LevelStartedAt <
                                  std::chrono::seconds(kMinimumVisibleLevelSeconds))
            return;

        DestroyBody();
        GetLogger()->Info(
            "ExecuteBB test: status=%s reason=%s x0=%.6f push_start=%.6f "
            "pushed=%.6f pulled=%.6f released=%.6f "
            "physicalize_event=%s unphysicalize_event=%s menu_opened=%s "
            "level_chosen=%s control_ready=%s runtime_probe=%s "
            "lifecycle_probe=%s additive_edit=%s script_hook_retirement=%s "
            "runtime_detail=%s frames=%d",
            m_Passed ? "pass" : "fail", m_Reason, m_InitialX, m_PushStartX,
            m_PushedX, m_PulledX, m_ReleasedX,
            m_PhysicalizeSeen ? "true" : "false",
            m_UnphysicalizeSeen ? "true" : "false",
            m_LevelMenuOpened ? "true" : "false",
            m_LevelChosen ? "true" : "false",
            m_ControlReady ? "true" : "false",
            m_RuntimeProbePassed ? "true" : "false",
            m_LifecycleProbePassed ? "true" : "false",
            m_AdditiveEditProbePassed ? "true" : "false",
            m_ScriptHookRetirementPassed ? "true" : "false",
            m_RuntimeProbeDetail.c_str(), m_TotalFrames);
        m_Done = true;
        m_BML->ExitGame();
    }

    void DestroyBody() {
        m_RuntimeProbe.reset();
        DestroyScriptHookGraph();
        if (!m_Body)
            return;
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        if (context)
            context->DestroyObject(m_Body);
        m_Body = nullptr;
    }

    void DestroyScriptHookGraph() {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        if (m_ScriptHookRuntime)
            m_ScriptHookRuntime->ResetWorld();
        m_ScriptHookRuntime.reset();
        m_ScriptHookSourceBlock = nullptr;
        if (context && m_ScriptHookGraph) {
            if (CKScene *scene = context->GetCurrentScene())
                scene->DeActivate(m_ScriptHookGraph);
            context->DestroyObject(m_ScriptHookGraph);
        }
        m_ScriptHookGraph = nullptr;
    }

    void SetPhase(Phase phase) {
        m_Phase = phase;
        m_PhaseFrames = 0;
        m_PhaseStartedAt = std::chrono::steady_clock::now();
    }

    bool PhaseDone(std::chrono::steady_clock::duration duration) const {
        return std::chrono::steady_clock::now() - m_PhaseStartedAt >= duration;
    }

    float ReadX() const {
        VxVector position;
        m_Body->GetPosition(&position);
        return position.x;
    }

    bool BodyPositionIsFinite() const {
        if (!m_Body)
            return false;
        VxVector position;
        m_Body->GetPosition(&position);
        return std::isfinite(position.x) && std::isfinite(position.y) &&
               std::isfinite(position.z);
    }

    Phase m_Phase = Phase::Menu;
    CK3dObject *m_Body = nullptr;
    std::unique_ptr<BehaviorRuntimeProbe> m_RuntimeProbe;
    std::unique_ptr<BML::Behavior::Runtime> m_ScriptHookRuntime;
    CKBehavior *m_ScriptHookGraph = nullptr;
    CKBehavior *m_ScriptHookSourceBlock = nullptr;
    ScriptHookSourceProbe m_ScriptHookSource;
    const char *m_Reason = "not-completed";
    const char *m_MenuError = "menu-timeout";
    int m_TotalFrames = 0;
    int m_PhaseFrames = 0;
    float m_InitialX = 0.0f;
    float m_PushStartX = 0.0f;
    float m_PushedX = 0.0f;
    float m_PulledX = 0.0f;
    float m_ReleasedX = 0.0f;
    bool m_PhysicalizeSeen = false;
    bool m_UnphysicalizeSeen = false;
    bool m_Physicalized = false;
    bool m_ForceSet = false;
    bool m_MenuReady = false;
    bool m_LevelMenuOpened = false;
    bool m_LevelChosen = false;
    bool m_LevelStarted = false;
    bool m_ControlReady = false;
    bool m_RuntimeProbePassed = false;
    bool m_LifecycleProbePassed = false;
    bool m_AdditiveEditProbePassed = false;
    bool m_ScriptHookSetupFailed = false;
    bool m_ScriptHookInstalled = false;
    bool m_ScriptHookRetirementPassed = false;
    bool m_FrameCaptureRequested = false;
    bool m_FrameCaptureAttempted = false;
    bool m_FrameCaptured = false;
    int m_ScriptHookFrames = 0;
    bool m_Passed = false;
    bool m_Done = false;
    std::string m_RuntimeProbeDetail = "not-run";
    std::chrono::steady_clock::time_point m_MenuStartedAt{};
    std::chrono::steady_clock::time_point m_LevelStartedAt{};
    std::chrono::steady_clock::time_point m_ControlReadyAt{};
    std::chrono::steady_clock::time_point m_PhaseStartedAt{};
};

} // namespace

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new ExecuteBBTest(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}

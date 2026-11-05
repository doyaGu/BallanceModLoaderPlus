#include <BML/BMLAll.h>
#include <SDL3/SDL.h>

#include <cstdlib>
#include <sstream>
#include <string>

namespace {
class PostProcessProbe final : public IMod {
public:
    explicit PostProcessProbe(IBML *bml) : IMod(bml) {}
    const char *GetID() override { return "SdlPostProcessProbe"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "SDL PostProcess regression"; }
    const char *GetAuthor() override { return "BML tests"; }
    const char *GetDescription() override { return "Checks the shipped loader's frame ordering and input edges"; }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        if (const char *path = std::getenv("BML_POSTPROCESS_RESULT"))
            m_ResultPath = path;
    }
    void OnPostStartMenu() override { m_MenuReady = true; }
    void OnRender(CK_RENDER_FLAGS) override { ++m_Renders; }
    void OnUnload() override {
        if (m_Block)
            m_BML->GetInputManager()->ReleaseBlock(m_Block);
        Write(m_ResultPath + ".unloaded", "unloaded\n");
    }

    void OnProcess() override {
        if (m_Finished)
            return;
        ++m_Frames;
        CKContext *context = m_BML->GetCKContext();
        auto *window = static_cast<SDL_Window *>(context->GetMainWindow());
        const SDL_WindowID windowId = SDL_GetWindowID(window);
        if (!windowId || SDL_GetWindowFromID(windowId) != window) {
            Finish(false, "sdl-window-not-owned-by-module-runtime");
            return;
        }
        auto *physics = context->GetManagerByGuid(CKGUID(0x6bed328b, 0x141f5148));
        auto *loader = context->GetManagerByGuid(CKGUID(0x32a40332, 0x3bf12a51));
        auto *native = static_cast<CKInputManager *>(context->GetManagerByGuid(INPUT_MANAGER_GUID));
        auto *input = m_BML->GetInputManager();
        if (!physics || !loader || !native || !input) {
            Finish(false, "missing-manager");
            return;
        }
        m_Priority = loader->GetFunctionPriority(CKMANAGER_FUNC_PostProcess);
        for (int i = 0; i < context->GetManagerCount(); ++i) {
            auto *manager = context->GetManager(i);
            if (manager != loader && (manager->GetValidFunctionsMask() & CKMANAGER_FUNC_PostProcess) &&
                manager->GetFunctionPriority(CKMANAGER_FUNC_PostProcess) <= m_Priority) {
                Finish(false, "loader-precedes-engine-manager");
                return;
            }
        }
        // CK2 resets this public profiling field before every Process. Physics
        // has no PreProcess, so a positive value proves this tick's PostProcess
        // has returned before the real BML OnProcess dispatch reaches us.
        if (physics->m_ProcessingTime > 0)
            ++m_PhysicsFrames;
        const CKDWORD tick = context->GetTimeManager()->GetMainTickCount();
        if (m_Frames > 1 && tick == m_LastTick) {
            Finish(false, "duplicate-mod-callback-in-tick");
            return;
        }
        m_LastTick = tick;
        if (!m_MenuReady) {
            if (m_Frames > 600)
                Finish(false, "menu-timeout");
            return;
        }

        bool passed = true;
        switch (m_Phase) {
        case 0:
            passed = Inject(context, true);
            break;
        case 1:
            passed = input->oIsKeyPressed(CKKEY_A) && native->IsKeyDown(CKKEY_A) &&
                     input->oIsMouseClicked(CK_MOUSEBUTTON_LEFT) && Inject(context, false);
            break;
        case 2:
            passed = input->oIsKeyReleased(CKKEY_A) && input->IsKeyReleased(CKKEY_A) &&
                     native->IsKeyToggled(CKKEY_A) && native->IsMouseToggled(CK_MOUSEBUTTON_LEFT);
            break;
        case 3:
            passed = !input->oIsKeyReleased(CKKEY_A) && !native->IsKeyDown(CKKEY_A) &&
                     !native->IsMouseToggled(CK_MOUSEBUTTON_LEFT);
            m_Block = input->AcquireBlock(InputHook::INPUT_BLOCK_KEYBOARD | InputHook::INPUT_BLOCK_MOUSE);
            passed = passed && m_Block && Inject(context, true);
            break;
        case 4:
            passed = input->oIsKeyPressed(CKKEY_A) && !native->IsKeyDown(CKKEY_A) &&
                     input->oIsMouseClicked(CK_MOUSEBUTTON_LEFT) &&
                     !native->IsMouseButtonDown(CK_MOUSEBUTTON_LEFT) && Inject(context, false);
            break;
        case 5:
            passed = input->oIsKeyReleased(CKKEY_A) && !input->IsKeyReleased(CKKEY_A) &&
                     !native->IsKeyToggled(CKKEY_A) && input->oIsMouseToggled(CK_MOUSEBUTTON_LEFT) &&
                     !native->IsMouseToggled(CK_MOUSEBUTTON_LEFT);
            input->ReleaseBlock(m_Block);
            m_Block = 0;
            break;
        case 6:
            passed = !input->oIsKeyReleased(CKKEY_A) && !native->IsKeyDown(CKKEY_A) &&
                     !native->IsMouseToggled(CK_MOUSEBUTTON_LEFT);
            break;
        default:
            if (m_Frames >= 120)
                Finish(m_PhysicsFrames > 0, m_PhysicsFrames ? "passed" : "physics-not-observed-before-mod");
            return;
        }
        if (!passed) {
            Finish(false, "input-phase-" + std::to_string(m_Phase));
            return;
        }
        ++m_Phase;
    }

private:
    static bool Write(const std::string &path, const std::string &text) {
        SDL_IOStream *file = SDL_IOFromFile(path.c_str(), "w");
        if (!file)
            return false;
        const bool written = SDL_WriteIO(file, text.data(), text.size()) == text.size();
        return SDL_CloseIO(file) && written;
    }
    static bool Inject(CKContext *context, bool down) {
        auto *window = static_cast<SDL_Window *>(context->GetMainWindow());
        SDL_Event key{};
        key.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        key.key.windowID = SDL_GetWindowID(window);
        key.key.scancode = SDL_SCANCODE_A;
        key.key.key = SDLK_A;
        key.key.down = down;
        key.key.timestamp = SDL_GetTicksNS();
        SDL_Event mouse{};
        mouse.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        mouse.button.windowID = key.key.windowID;
        mouse.button.button = SDL_BUTTON_LEFT;
        mouse.button.down = down;
        mouse.button.x = 1;
        mouse.button.y = 1;
        return SDL_PushEvent(&key) && SDL_PushEvent(&mouse);
    }
    void Finish(bool passed, const std::string &detail) {
        m_Finished = true;
        std::ostringstream result;
        result << "{\"passed\":" << (passed ? "true" : "false")
               << ",\"detail\":\"" << detail << "\",\"frames\":" << m_Frames
               << ",\"physics_frames\":" << m_PhysicsFrames << ",\"renders\":" << m_Renders
               << ",\"input_phases\":" << m_Phase << ",\"bml_priority\":" << m_Priority << "}\n";
        if (!Write(m_ResultPath, result.str()))
            GetLogger()->Error("Cannot write PostProcess test result: %s", SDL_GetError());
        GetLogger()->Info("PostProcess test: %s", result.str().c_str());
        m_BML->ExitGame();
    }

    std::string m_ResultPath;
    std::uint64_t m_Block = 0;
    CKDWORD m_LastTick = 0;
    int m_Frames = 0, m_PhysicsFrames = 0, m_Renders = 0, m_Phase = 0, m_Priority = 0;
    bool m_MenuReady = false, m_Finished = false;
};
} // namespace

BML_MOD_ENTRY(IMod *) BMLEntry(IBML *bml) { return new PostProcessProbe(bml); }
BML_MOD_ENTRY(void) BMLExit(IMod *mod) { delete mod; }

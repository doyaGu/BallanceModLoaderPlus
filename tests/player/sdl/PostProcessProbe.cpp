#include <BML/BMLAll.h>
#include <BML/Engine.hpp>
#include <BML/ImGui.hpp>
#include <SDL3/SDL.h>

#include <cstdlib>
#include <sstream>
#include <string>

namespace {
class PostProcessProbe final : public BML::Mod::Instance {
public:
    static BML::Mod::Definition Define(){return {"SdlPostProcessProbe","SDL PostProcess regression","1.0.0","BML tests","Checks the shipped loader frame ordering and input edges",BML_VERSION,{}};}
    void OnLoad(const BML::Mod::Context &context) override {
        m_Context=context;
        if (const char *path = std::getenv("BML_POSTPROCESS_RESULT"))
            m_ResultPath = path;
    }
    void OnPostStartMenu(const BML::Mod::Context &) override { m_MenuReady = true; }
    void OnRender(const BML::Mod::Context &,const BML::Mod::RenderEvent &) override { ++m_Renders; }
    void OnUnload(const BML::Mod::Context &) override {
        (void)m_Block.Release().Code();
        Write(m_ResultPath + ".unloaded", "unloaded\n");
    }

    void OnPreCommandExecute(const BML::Mod::Context &,
                             const BML::Mod::CommandEvent &event) override {
        if (event.Name != "bml_probe_command")
            return;
        CheckCommandEvent(event, BML_COMMAND_EVENT_PRE);
        m_CommandEvents.push_back(1);
        if (m_RemoveBeforeExecute)
            m_ProbeCommand.Unregister().Take();
        Require(!m_CommandLease.expired(), "command state retained through pre callback");
    }

    void OnPostCommandExecute(const BML::Mod::Context &,
                              const BML::Mod::CommandEvent &event) override {
        if (event.Name != "bml_probe_command")
            return;
        CheckCommandEvent(event, BML_COMMAND_EVENT_POST);
        Require(!m_CommandLease.expired(), "command state retained through post callback");
        m_CommandEvents.push_back(3);
    }

    void OnModifyConfig(const BML::Mod::Context &, const BML::Mod::ConfigEvent &event) override {
        if (event.Category != "Contract")
            return;
        Require(event.ModId == "SdlPostProcessProbe" && event.Property != 0,
                "configuration notification owner and property");
        if (event.Key == "Count") {
            Require(event.Type == BML_CONFIG_TYPE_INTEGER, "configuration notification type");
            ++m_CountNotifications;
        } else if (event.Key == "Text") {
            Require(event.Type == BML_CONFIG_TYPE_STRING, "text notification type");
            ++m_TextNotifications;
        }
    }

    void OnProcess(const BML::Mod::Context &) override {
        if (m_Finished)
            return;
        ++m_Frames;
        CKContext *context = BML::Engine::BorrowContext(m_Context).Value();
        auto *window = static_cast<SDL_Window *>(context->GetMainWindow());
        const SDL_WindowID windowId = SDL_GetWindowID(window);
        if (!windowId || SDL_GetWindowFromID(windowId) != window) {
            Finish(false, "sdl-window-not-owned-by-module-runtime");
            return;
        }
        auto *physics = context->GetManagerByGuid(CKGUID(0x6bed328b, 0x141f5148));
        auto *loader = context->GetManagerByGuid(CKGUID(0x32a40332, 0x3bf12a51));
        auto *native = static_cast<CKInputManager *>(context->GetManagerByGuid(INPUT_MANAGER_GUID));
        auto input = m_Context.Input();
        const auto raw=input.ReadKeyboard(BML_INPUT_RAW).Value();
        const auto filtered=input.ReadKeyboard().Value();
        const auto mouse=input.ReadMouse(BML_INPUT_RAW).Value();
        if (!physics || !loader || !native) {
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

        const auto repeated=input.ReadKeyboard(BML_INPUT_RAW).Value();
        if(raw.Keys.data()!=repeated.Keys.data() || raw.Events.data()!=repeated.Events.data()) {Finish(false,"unstable-input-snapshot");return;}
        bool passed = true;
        switch (m_Phase) {
        case 0:
            try { CheckNativeServices(context); }
            catch (const std::exception &error) { Finish(false, std::string("native-services: ") + error.what()); return; }
            passed = Inject(context, true);
            break;
        case 1:
            passed = raw.IsKeyPressed(CKKEY_A) && native->IsKeyDown(CKKEY_A) &&
                     (mouse.ClickedMask & 1u) && Inject(context, false);
            break;
        case 2:
            passed = raw.IsKeyReleased(CKKEY_A) && filtered.IsKeyReleased(CKKEY_A) &&
                     native->IsKeyToggled(CKKEY_A) && native->IsMouseToggled(CK_MOUSEBUTTON_LEFT);
            break;
        case 3:
            passed = !raw.IsKeyReleased(CKKEY_A) && !native->IsKeyDown(CKKEY_A) &&
                     !native->IsMouseToggled(CK_MOUSEBUTTON_LEFT);
            m_Block = input.AcquireBlock(BML_INPUT_BLOCK_KEYBOARD | BML_INPUT_BLOCK_MOUSE).Take();
            passed = passed && m_Block.IsValid() && Inject(context, true);
            break;
        case 4:
            passed = raw.IsKeyPressed(CKKEY_A) && !native->IsKeyDown(CKKEY_A) &&
                     (mouse.ClickedMask & 1u) &&
                     !native->IsMouseButtonDown(CK_MOUSEBUTTON_LEFT) && Inject(context, false);
            break;
        case 5:
            passed = raw.IsKeyReleased(CKKEY_A) && !filtered.IsKeyReleased(CKKEY_A) &&
                     !native->IsKeyToggled(CKKEY_A) && (mouse.ToggledMask & 1u) &&
                     !native->IsMouseToggled(CK_MOUSEBUTTON_LEFT);
            passed = passed && m_Block.Release().Code()==BML_OK;
            break;
        case 6:
            passed = !raw.IsKeyReleased(CKKEY_A) && !native->IsKeyDown(CKKEY_A) &&
                     !native->IsMouseToggled(CK_MOUSEBUTTON_LEFT);
            break;
        default:
            if (m_Frames >= 120) {
                const bool timers = m_NoArgCalls == 1 && m_VoidCalls == 2 && !m_NoArgs.IsValid() &&
                                    !m_VoidLoop.IsValid() && m_SelfCalls == 1 &&
                                    m_RepeatCalls == 3 && !m_Self.IsValid() &&
                                    !m_Repeat.IsValid() && m_SelfLease.expired() &&
                                    m_CountNotifications == 1 && m_TextNotifications == 1;
                Finish(m_PhysicsFrames > 0 && timers, !timers ? "timer-lifecycle" : m_PhysicsFrames ? "passed" : "physics-not-observed-before-mod");
            }
            return;
        }
        if (!passed) {
            Finish(false, "input-phase-" + std::to_string(m_Phase));
            return;
        }
        ++m_Phase;
    }

private:
    static void Require(bool value, const char *message) {
        if (!value)
            throw std::runtime_error(message);
    }

    void CheckCommandEvent(const BML::Mod::CommandEvent &event, uint32_t phase) {
        Require(event.Phase == phase && event.InvokedAs == "bpcheck" && !event.Cheat,
                "command event identity");
        Require(event.Arguments.size() == 2 && std::string_view(event.Arguments[0]) == "one" &&
                    std::string_view(event.Arguments[1]) == "two words",
                "command event arguments after shell expansion");
    }

    void CheckCommandLifetime(bool removeBeforeExecute) {
        m_RemoveBeforeExecute = removeBeforeExecute;
        m_CommandEvents.clear();
        auto retained = std::make_shared<int>(7);
        m_CommandLease = retained;
        m_ProbeCommand =
            m_Context.Commands()
                .Register({"bml_probe_command", "bpcheck", "Retained command invocation"},
                          [this, retained](const BML::Command::Invocation &invocation) {
                              Require(*retained == 7 && invocation.InvokedAs() == "bpcheck" &&
                                          invocation.Argument(0) == "one" &&
                                          invocation.Argument(1) == "two words",
                                      "command execute after registration removal");
                              m_CommandEvents.push_back(2);
                              if (!m_RemoveBeforeExecute)
                                  m_ProbeCommand.Unregister().Take();
                              return 7;
                          })
                .Take();
        retained.reset();
        int status = 0;
        Require(BML::Command::ExecuteLine("bpcheck one 'two words'", status) == BML_OK &&
                    status == 7,
                "command invocation status");
        Require(m_CommandEvents == std::vector<int>({1, 2, 3}), "pre execute post ordering");
        Require(m_CommandLease.expired(), "command state released after post callback");
        BML::Command::Info info;
        Require(BML::Command::Find("bpcheck", info) == BML_ERROR_NOT_FOUND,
                "removed command and alias no longer resolve");
    }

    void CheckNativeServices(CKContext *engine) {
        if (const char *expected = std::getenv("BML_EXPECT_MODS")) {
            std::istringstream ids(expected);
            for (std::string id; std::getline(ids, id, ',');) {
                if (id.empty()) continue;
                auto found = m_Context.FindMod(id);
                Require(found && found.Value().Read().Value().State == BML_MOD_STATE_LOADED,
                        ("Mod did not load: " + id).c_str());
            }
        }
        if (const char *scripts = std::getenv("BML_EXPECT_SCRIPTS"); scripts && *scripts == '1') {
            auto helper = m_Context.FindMod("SdlDependencyProbe");
            BML::Command::Info info;
            Require(helper && helper.Value().Read().Value().State == BML_MOD_STATE_LOADED &&
                        BML::Command::Find("bml_probe_dependencies", info) == BML_OK,
                    "script dependency values and invalid indices");
        }

        CheckCommandLifetime(true);
        CheckCommandLifetime(false);

        auto config = m_Context.Config();
        auto value = config.DeclareProperty({"Contract", "Count", int32_t(42)}).Take();
        Require(value.Get<int32_t>() == 42, "numeric configuration read");
        Require(value.Set(int32_t(43)).Code() == BML_OK, "configuration write");
        auto same = config.DeclareProperty({"Contract", "Count", int32_t(7)}).Take();
        Require(same.Read().Value().Handle == value.Read().Value().Handle && same.Get<int32_t>() == 43, "idempotent declaration");
        Require(config.DeclareProperty({"Contract", "Count", std::string("wrong")}).Code() == BML_ERROR_TYPE_MISMATCH, "configuration type conflict");
        auto text = config.DeclareProperty({"Contract", "Text", std::string(128, 'x')}).Take();
        const void *raw = nullptr;
        Require(BML_GetInterface(BML_CONFIG_INTERFACE_ID, 1, &raw) == BML_OK, "config C binding");
        const auto *api = static_cast<const BML_ConfigInterface *>(raw);
        struct Visitor { BML::Config::Property Property; bool Preserved = false; } visitor{text};
        const int read = api->ReadProperty(m_Context.Handle(), text.Read().Value().Handle,
            BML::Detail::Cdecl<[](void *data, const BML_ConfigPropertyInfo *info) {
                auto &state = *static_cast<Visitor *>(data);
                const char *borrowed = info->Value->Data.String;
                if (state.Property.Set(std::string(128, 'y')).Code() != BML_OK) return BML_ERROR_FAIL;
                state.Preserved = std::string_view(borrowed) == std::string(128, 'x');
                return BML_OK;
            }>::Call, &visitor);
        Require(read == BML_OK && visitor.Preserved && text.Get<std::string>() == std::string(128, 'y'), "reentrant configuration borrow");

        auto retained = std::make_shared<int>(7); m_SelfLease = retained;
        m_Self = m_Context.Timers().SetIntervalTicks(1,
            [this, retained](const auto &, const auto &) {
                ++m_SelfCalls;
                Require(m_Self.Cancel().Code() == BML_OK, "self cancellation");
                return true;
            }).Take();
        m_NoArgs = m_Context.Timers().SetTimeoutTicks(1, [this] { ++m_NoArgCalls; }).Take();
        m_VoidLoop = m_Context.Timers().SetIntervalTicks(1, [this] {
            if (++m_VoidCalls == 2) Require(m_VoidLoop.Cancel().Code() == BML_OK, "void interval cancellation");
        }).Take();
        std::function<void()> empty;
        Require(m_Context.Timers().SetTimeoutTicks(1, empty).Code() == BML_ERROR_INVALID_PARAMETER, "empty timer callback");
        BML::Timer::Definition repeat;
        repeat.Type = BML_TIMER_REPEAT; repeat.TimeBase = BML_TIMER_TIMEBASE_TICK;
        repeat.DelayTicks = 1; repeat.RepeatCount = 3;
        Require(m_Repeat.Register(m_Context, repeat, [this](const auto &, const auto &event) {
            Require(event.CompletedIterations == uint64_t(m_RepeatCalls), "timer event count");
            ++m_RepeatCalls; return true;
        }).Code() == BML_OK, "finite timer registration");

        Require(BML_GetInterface(BML_INPUT_INTERFACE_ID, 1, &raw) == BML_OK, "input C binding");
        const auto *input = static_cast<const BML_InputInterface *>(raw);
        struct Mouse { BML_InputMouseState State{}; uint64_t Tail = 0xdeadbeef; } mouse;
        mouse.State.StructSize = sizeof(mouse);
        Require(input->ReadMouse(m_Context.Handle(), BML_INPUT_RAW, &mouse.State) == BML_OK && mouse.Tail == 0xdeadbeef && mouse.State.StructSize == BML_INPUT_MOUSE_STATE_1_0_SIZE, "output prefix and unknown tail");
        mouse.State.StructSize = BML_INPUT_MOUSE_STATE_1_0_SIZE - 1;
        Require(input->ReadMouse(m_Context.Handle(), 0, &mouse.State) == BML_ERROR_INVALID_PARAMETER && mouse.Tail == 0xdeadbeef, "short output rejection");

        Require(BML_GetInterface(BML_GUI_INTERFACE_ID, 1, &raw) == BML_OK, "GUI C binding");
        const auto *gui = static_cast<const BML_GuiInterface *>(raw);
        for (uint32_t kind = BML_GUI_PANEL; kind <= BML_GUI_ELEMENT; ++kind) {
            BML_ObjectRef object{};
            Require(gui->CreateElement(m_Context.Handle(), kind, "NativeContractElement", &object) == BML_OK, "GUI construction");
            BML_GuiElementInfo info = BML_GUI_ELEMENT_INFO_INIT;
            Require(gui->ReadElement(m_Context.Handle(), object, &info) == BML_OK && info.Kind == kind, "GUI read");
            if (kind == BML_GUI_TEXT) {
                Require(gui->SetSize(m_Context.Handle(), object, {.15f, .03f}) == BML_OK, "GUI text resize");
                Require(gui->SetSize(m_Context.Handle(), object, {1e30f, .1f}) == BML_ERROR_INVALID_PARAMETER, "GUI overflow rejection");
            }
            if (kind == BML_GUI_INPUT) {
                Require(gui->SetText(m_Context.Handle(), object, "ABC") == BML_OK, "GUI input text");
                Require(gui->SetFocused(m_Context.Handle(), object, 1) == BML_OK, "GUI input focus");
                BML_GuiEvent event = BML_GUI_EVENT_INIT;
                Require(gui->OnCharTyped(m_Context.Handle(), object, CKKEY_BACK, &event) == BML_OK, "GUI backspace");
                char text[16]; size_t required = 0;
                Require(gui->GetText(m_Context.Handle(), object, text, sizeof text, &required) == BML_OK && std::string_view(text) == "AB", "GUI text editing");
            }
            if (kind == BML_GUI_PANEL) {
                auto entity = BML::Engine::BorrowObject<CK2dEntity>(m_Context, object).Value();
                engine->DestroyObject(entity->GetID());
                Require(gui->ReadElement(m_Context.Handle(), object, &info) == BML_ERROR_OBJECT_INVALID, "GUI engine deletion");
            }
            const int destroyed = gui->DestroyElement(m_Context.Handle(), object);
            Require(destroyed == BML_OK || (kind == BML_GUI_PANEL && destroyed == BML_ERROR_OBJECT_INVALID), "GUI destruction");
        }
        struct Window : Bui::Window { using Bui::Window::Window; void OnDraw() override {} };
        Window window(m_Context, "Native API contract");
        window.Show();
        if (window.Begin()) {
            Require(Bui::At(m_Context, {.1f, .1f}, [] { return 73; }) == 73, "Bui value callback");
            bool recovered = false;
            try { Bui::At(m_Context, {.1f, .1f}, [] { throw std::runtime_error("local"); }); }
            catch (const std::runtime_error &) { recovered = true; }
            Require(recovered, "Bui local exception propagation");
        }
        window.End();
    }
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
            (void)m_Context.LogError(std::string("Cannot write PostProcess test result: ")+SDL_GetError());
        (void)m_Context.LogInfo(std::string("PostProcess test: ")+result.str());
        (void)BML::Gameplay::ExitGame(m_Context);
    }

    std::string m_ResultPath;
    BML::Mod::Context m_Context;
    BML::Input::Block m_Block;
    BML::Timer::Registration m_Self, m_Repeat, m_NoArgs, m_VoidLoop;
    std::weak_ptr<int> m_SelfLease;
    BML::Command::Registration m_ProbeCommand;
    std::weak_ptr<int> m_CommandLease;
    std::vector<int> m_CommandEvents;
    bool m_RemoveBeforeExecute = false;
    int m_CountNotifications = 0;
    int m_TextNotifications = 0;
    int m_SelfCalls = 0, m_RepeatCalls = 0, m_NoArgCalls = 0, m_VoidCalls = 0;
    CKDWORD m_LastTick = 0;
    int m_Frames = 0, m_PhysicsFrames = 0, m_Renders = 0, m_Phase = 0, m_Priority = 0;
    bool m_MenuReady = false, m_Finished = false;
};
} // namespace

BML_DEFINE_MOD(PostProcessProbe, PostProcessProbe::Define)

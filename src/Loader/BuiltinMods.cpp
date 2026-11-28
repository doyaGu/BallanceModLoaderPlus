#include "Loader/ModLoader.h"
#include "Loader/ModContext.h"

#include "Api/BuiltinCapabilities.h"
#include "Diagnostics/BMLCommand.h"
#include "Gameplay/CheatCommand.h"
#include "HUD/HUDCommand.h"
#include "Mods/BMLMod.h"
#include "Mods/NewBallTypeMod.h"
#include "UI/FontCommand.h"
#include "UI/PaletteCommand.h"
#include "AngelScript/ScriptCommand.h"
#if BML_ENABLE_ANGELSCRIPT
#include "AngelScript/ScriptDevToolsService.h"
#endif
#if BML_ENABLE_UI_AUTOMATION
#include "player/UiAutomation.h"
#endif

#include <stdexcept>

namespace BML {

class BuiltinBmlHost final : public BMLMod::Host {
public:
    BuiltinBmlHost(ModContext &context, ModRecord &record)
        : m_Context(context), m_Record(record), m_Menu(context) {}

    CKContext &Engine() override { return *m_Context.GetCKContext(); }
    ::Config &Settings() override {
        auto *config = m_Record.GetConfig();
        if (!config)
            throw std::runtime_error("Cannot load the configuration for the built-in Mod");
        return *config;
    }
    Logger &Log() override { return *m_Record.GetLogger(); }
    ModMenu &Menu() override { return m_Menu; }
    UI::FontRuntime *UiFonts() override { return m_Context.GetUiFontRuntime(); }
    GameFontCatalog &GameFonts() override { return m_Context.GetGameFonts(); }
    std::string LoaderDirectory() override { return m_Context.GetDirectoryUtf8(BML_DIR_LOADER); }

    void StartFeatures(CustomMaps &maps, GameEventHooks &gameEvents, GameplayTweaks &tweaks,
                       Console &console, HUDRuntime &hud, FontCommandContext fonts) override {
        auto &logger = Log();
        maps.OnLoad(m_Context, logger, m_Context.GetDirectory(BML_DIR_GAME),
                    m_Context.GetDirectory(BML_DIR_LOADER), m_Context.GetDirectory(BML_DIR_TEMP));
        gameEvents.OnLoad(m_Context, logger);
        tweaks.OnLoad(m_Context, logger);
        console.OnLoad(m_Context, m_Context.GetCommandContext(), logger);
        hud.OnLoad(m_Context);
        m_Context.RegisterCommand(std::make_unique<BMLCommand>(&m_Context));
        m_Context.RegisterCommand(std::make_unique<FontCommand>(fonts));
        m_Context.RegisterCommand(std::make_unique<CheatCommand>());
        m_Context.RegisterCommand(std::make_unique<HUDCommand>(&hud));
        m_Context.RegisterCommand(std::make_unique<PaletteCommand>());
        m_Context.RegisterCommand(std::make_unique<ScriptCommand>());
        RegisterBuiltinCapabilities(m_Context, logger);
    }

    void StopFeatures() override { UnregisterBuiltinCapabilities(m_Context); }

    void LoadOptionsEntry(ModsMenuEntry &entry, Behavior::Session &session, CKBehavior *script) override {
        entry.Load(session, script, m_Context, Log());
    }

    void ScheduleFontAcquisition(std::function<void()> callback) override {
        m_Context.AddTimer(static_cast<CKDWORD>(1), std::move(callback));
    }

    void SetWidescreen(bool enabled) override { m_Context.SetWidescreenFix(enabled); }

    void RenderDevTools() override {
#if BML_ENABLE_ANGELSCRIPT
        if (auto *tools = m_Context.GetModLoader().GetScriptDevTools())
            tools->RenderPanel();
#endif
    }

#if BML_ENABLE_UI_AUTOMATION
    void StartUiAutomation(BMLMod &mod) override { UiAutomation::Start(m_Context, mod); }
#endif

private:
    ModContext &m_Context;
    ModRecord &m_Record;
    ModMenu m_Menu;
};

} // namespace BML

bool ModLoader::RegisterBuiltinMods() {
    m_BuiltinRecord = std::make_unique<ModRecord>(&m_Context);
    m_BMLMod = new BMLMod(std::make_unique<BML::BuiltinBmlHost>(m_Context, *m_BuiltinRecord));
    m_BuiltinRecord->Bind(*m_BMLMod);
    if (!RegisterMod(m_BuiltinRecord.get()))
        return false;

    m_BallTypeRecord = std::make_unique<ModRecord>(&m_Context);
    m_BallTypeMod = new NewBallTypeMod();
    m_BallTypeRecord->Bind(*m_BallTypeMod);
    if (!RegisterMod(m_BallTypeRecord.get()))
        return false;
    m_BallTypeMod->BindContext(BML::Mod::Detail::ContextAccess::Make(
        IssueModContext(m_BallTypeRecord.get())));
    return true;
}

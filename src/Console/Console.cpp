#include "Console/Console.h"

#include <algorithm>

#include "BML/IBML.h"
#include "BML/IConfig.h"
#include "BML/ILogger.h"

#include "UI/AnsiText.h"
#include "HUD/HUDRuntime.h"
#include "Console/CommandContext.h"
#include "Console/CommandInput.h"
#include "Console/Commands.h"
#include "Console/FontCommand.h"
#include "Console/Shell/FilterCommands.h"
#include "Console/Shell/ShellBuiltins.h"
#include "Loader/ModContext.h"
#include "PathUtils.h"
#include "StringUtils.h"
#include "UI/Ime/Presentation.h"

inline constexpr wchar_t HistoryFileName[] = L"CommandBar.history";

const Console::Setting *Console::GetSettings(size_t &count) {
    static const Setting settings[] = {
        {"MessageDuration", &Console::m_MessageDuration,
         [](Console &console, IProperty *property) {
             console.m_MessageBoard.SetMaxTimer(std::max(2000.0f, property->GetFloat() * 1000.0f));
         }},
        {"TabColumns", &Console::m_TabColumns,
         [](Console &console, IProperty *property) {
             console.m_MessageBoard.SetTabColumns(std::max(1, property->GetInteger()));
         }},
        {"LineSpacing", &Console::m_LineSpacing,
         [](Console &console, IProperty *property) {
             console.m_MessageBoard.SetLineSpacing(property->GetFloat());
         }},
        {"MessageBackgroundAlpha", &Console::m_MessageBackgroundAlpha,
         [](Console &console, IProperty *property) {
             console.m_MessageBoard.SetMessageBackgroundAlpha(std::clamp(property->GetFloat(), 0.0f, 1.0f));
         }},
        {"FadeMaxAlpha", &Console::m_FadeMaxAlpha,
         [](Console &console, IProperty *property) {
             console.m_MessageBoard.SetFadeMaxAlpha(std::clamp(property->GetFloat(), 0.0f, 1.0f));
         }},
        {"KeepOpen", &Console::m_KeepOpen,
         [](Console &console, IProperty *property) {
             console.m_CommandBar.SetKeepOpen(property->GetBoolean());
         }},
    };
    static_assert(sizeof(settings) / sizeof(settings[0]) == 6,
                  "Every built-in console config property must have one settings-table entry");

    count = sizeof(settings) / sizeof(settings[0]);
    return settings;
}

void Console::InitConfig(IConfig &config) {
    size_t count = 0;
    const Setting *settings = GetSettings(count);
    for (size_t i = 0; i < count; ++i) {
        this->*settings[i].property = config.GetProperty("CommandBar", settings[i].key);
    }

    config.SetCategoryComment("CommandBar", "Command Bar Settings");

    m_MessageDuration->SetComment("Maximum visible time of each notification message, in seconds (default: 6)");
    m_MessageDuration->SetDefaultFloat(6);

    m_TabColumns->SetComment("Tab width in columns for message wrapping (1..64, default: 4)");
    m_TabColumns->SetDefaultInteger(4);

    m_LineSpacing->SetComment("Line spacing between wrapped lines in messages (-1 to follow ImGui style).");
    m_LineSpacing->SetDefaultFloat(-1.0f);

    m_MessageBackgroundAlpha->SetComment("Alpha scale for message backgrounds (0..1, default: 0.80)");
    m_MessageBackgroundAlpha->SetDefaultFloat(0.80f);

    m_FadeMaxAlpha->SetComment("Maximum text/background alpha in notifications (0..1, default: 1.0)");
    m_FadeMaxAlpha->SetDefaultFloat(1.0f);

    m_KeepOpen->SetComment("Keep the command bar open after a command runs (default: false)");
    m_KeepOpen->SetDefaultBoolean(false);
}

void Console::ApplyConfig() {
    size_t count = 0;
    const Setting *settings = GetSettings(count);
    for (size_t i = 0; i < count; ++i) {
        ApplySetting(settings[i], this->*settings[i].property);
    }
}

bool Console::OnModifyConfig(const char *category, const char *key, IProperty *property) {
    if (!property || !utils::CStringEqual(category, "CommandBar")) {
        return false;
    }

    size_t count = 0;
    const Setting *settings = GetSettings(count);
    for (size_t i = 0; i < count; ++i) {
        const Setting &setting = settings[i];
        if (this->*setting.property != property || !utils::CStringEqual(key, setting.key)) {
            continue;
        }

        ApplySetting(setting, property);
        return true;
    }

    return false;
}

void Console::ApplySetting(const Setting &setting, IProperty *property) {
    if (setting.apply && property) {
        setting.apply(*this, property);
    }
}

void Console::OnLoad(IBML &bml, BML::CommandContext &commands, ILogger &logger, HUDRuntime &hud,
                     const FontCommandContext &fontContext) {
    m_Commands = &commands;
    m_Logger = &logger;

    m_OutputCallbackInstalled = m_Commands->SetOutputCallback(&OnCommandOutput, this);
    if (!m_OutputCallbackInstalled) {
        m_Logger->Warn("Could not register the built-in console output callback");
    }

    RegisterCommands(bml, hud, fontContext);
    AnsiText::Renderer::DefaultPalette().SaveSampleIfMissing();

    const wchar_t *loaderDirectory = BML_GetModContext()->GetDirectory(BML_DIR_LOADER);
    if (loaderDirectory && loaderDirectory[0] != L'\0')
        m_History.SetPath(utils::CombinePathW(loaderDirectory, HistoryFileName));
    m_History.Load();
    m_CommandBar.SetHistory(&m_History);
}

void Console::OnUnload() {
    CloseCommandBar();
    if (m_OutputCallbackInstalled && m_Commands) {
        m_Commands->ClearOutputCallback();
    }

    m_History.Save();
    m_CommandBar.SetHistory(nullptr);
    m_OutputCallbackInstalled = false;
    m_Logger = nullptr;
    m_Commands = nullptr;
}

void Console::CloseCommandBar() {
    m_CommandBar.ToggleCommandBar(false);
}

void Console::OnProcess() {
    if (!m_CommandBar.IsVisible() && ImGui::IsKeyPressed(ImGuiKey_Slash, false)) {
        if (m_Logger) {
            m_Logger->Info("Toggle Command Bar");
        }
        m_CommandBar.ToggleCommandBar();
    }

    const bool visible = m_CommandBar.IsVisible();
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    const float rowHeight = CommandBar::MeasureRowHeight();
    const float commandHeight = rowHeight * static_cast<float>(m_CommandBar.RowCount());
    const ConsoleLayout::Stack layout = ConsoleLayout::Calculate(
        {viewport->WorkPos.x, viewport->WorkPos.y, viewport->WorkSize.x, viewport->WorkSize.y},
        commandHeight, rowHeight);
    m_CommandBar.SetFrameLayout(layout);
    m_MessageBoard.SetFrameLayout(layout);
    m_CommandBar.SetCompositionActive(Overlay::Ime::Presentation::IsActive());
    m_MessageBoard.SetCommandBarVisible(visible);
    m_MessageBoard.Render();
    m_CommandBar.Render();

    if (m_CommandBar.HasActiveTextInput()) {
        Overlay::Ime::Presentation::ReservePlacement(
            ImVec2(layout.transientSurface.x, layout.transientSurface.y), layout.transientSurface.width);
    }
}

void Console::AddMessage(const char *message) {
    m_MessageBoard.AddMessage(message);

    if (m_Logger) {
        const std::string logMessage = utils::StripAnsiCodes(message);
        m_Logger->Info("%s", logMessage.c_str());
    }
}

void Console::ClearMessages() {
    m_MessageBoard.ClearMessages();
}

void Console::PrintHistory(std::size_t lastCount) {
    const std::vector<std::string> &entries = m_History.Entries();
    const std::size_t total = entries.size();
    const std::size_t first = lastCount == 0 || lastCount >= total ? 0 : total - lastCount;
    const std::size_t width = std::to_string(total).size();
    ModContext *context = BML_GetModContext();
    for (std::size_t i = first; i < total; ++i) {
        std::string number = std::to_string(i + 1);
        if (number.size() < width)
            number.insert(0, width - number.size(), ' ');
        const std::string line = number + "  " + CommandInput::SingleLinePreview(entries[i]);
        context->SendIngameMessage(line.c_str());
    }
}

void Console::ClearHistory() {
    m_History.Clear();
}

bool Console::EraseHistory(std::size_t number) {
    return m_History.Erase(number);
}

void Console::OnCommandOutput(const char *message, void *userdata) {
    auto *console = static_cast<Console *>(userdata);
    if (console) {
        console->AddMessage(message);
    }
}

void Console::RegisterCommands(IBML &bml, HUDRuntime &hud, const FontCommandContext &fontContext) {
    ModContext *context = BML_GetModContext();
    BML::Shell::Environment &environment = context->GetShellEnvironment();
    bml.RegisterCommand(new BML::Shell::CommandSet(environment));
    bml.RegisterCommand(new BML::Shell::CommandAlias(environment));
    bml.RegisterCommand(new BML::Shell::CommandUnalias(environment));
    bml.RegisterCommand(new BML::Shell::CommandTrue());
    bml.RegisterCommand(new BML::Shell::CommandFalse());
    bml.RegisterCommand(new BML::Shell::CommandXargs(
        [context](const std::vector<std::string> &args, const std::string *input) {
            return context->InvokeCommandArgs(args, input);
        }));
    bml.RegisterCommand(new BML::Shell::CommandGrep());
    bml.RegisterCommand(new BML::Shell::CommandHead());
    bml.RegisterCommand(new BML::Shell::CommandTail());
    bml.RegisterCommand(new BML::Shell::CommandWc());
    bml.RegisterCommand(new BML::Shell::CommandSort());
    bml.RegisterCommand(new BML::Shell::CommandUniq());

    bml.RegisterCommand(new CommandBML());
    bml.RegisterCommand(new CommandFont(fontContext));
    bml.RegisterCommand(new CommandHelp());
    bml.RegisterCommand(new CommandCheat());
    bml.RegisterCommand(new CommandEcho());
    bml.RegisterCommand(new CommandClear(this));
    bml.RegisterCommand(new CommandHistory(this));
    bml.RegisterCommand(new CommandExit());
    bml.RegisterCommand(new CommandHUD(&hud));
    bml.RegisterCommand(new CommandPalette());
    bml.RegisterCommand(new CommandScript());
}

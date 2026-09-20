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
inline constexpr char CommandBarCategory[] = "CommandBar";

const Console::Setting *Console::GetSettings(size_t &count) {
    static const Setting settings[] = {
        {CommandBarCategory, "MessageDuration", &Console::m_MessageDuration, 6.0f,
         "Maximum visible time of each notification message, in seconds (default: 6)",
         [](Console &console, IProperty *property) {
             console.m_MessageBoard.SetMaxTimer(std::max(2000.0f, property->GetFloat() * 1000.0f));
         }},
        {CommandBarCategory, "TabColumns", &Console::m_TabColumns, 4,
         "Tab width in columns for message wrapping (1..64, default: 4)",
         [](Console &console, IProperty *property) {
             console.m_MessageBoard.SetTabColumns(std::max(1, property->GetInteger()));
         }},
        {CommandBarCategory, "LineSpacing", &Console::m_LineSpacing, -1.0f,
         "Line spacing between wrapped lines in messages (-1 to follow ImGui style).",
         [](Console &console, IProperty *property) {
             console.m_MessageBoard.SetLineSpacing(property->GetFloat());
         }},
        {CommandBarCategory, "MessageBackgroundAlpha", &Console::m_MessageBackgroundAlpha, 0.80f,
         "Alpha scale for message backgrounds (0..1, default: 0.80)",
         [](Console &console, IProperty *property) {
             console.m_MessageBoard.SetMessageBackgroundAlpha(std::clamp(property->GetFloat(), 0.0f, 1.0f));
         }},
        {CommandBarCategory, "FadeMaxAlpha", &Console::m_FadeMaxAlpha, 1.0f,
         "Maximum text/background alpha in notifications (0..1, default: 1.0)",
         [](Console &console, IProperty *property) {
             console.m_MessageBoard.SetFadeMaxAlpha(std::clamp(property->GetFloat(), 0.0f, 1.0f));
         }},
        {CommandBarCategory, "KeepOpen", &Console::m_KeepOpen, false,
         "Keep the command bar open after a command runs (default: false)",
         [](Console &console, IProperty *property) {
             console.m_CommandBar.SetKeepOpen(property->GetBoolean());
         }},
        {CommandBarCategory, "EnableSyntaxHighlighting", &Console::m_EnableSyntaxHighlighting, true,
         "Colour commands, strings, variables, operators, comments, and syntax errors.",
         [](Console &console, IProperty *) { console.ApplyCommandBarFeatures(); }},
        {CommandBarCategory, "EnableTabCompletion", &Console::m_EnableTabCompletion, true,
         "Enable Tab completion and the completion candidate rail.",
         [](Console &console, IProperty *) { console.ApplyCommandBarFeatures(); }},
        {CommandBarCategory, "EnableHistorySuggestions", &Console::m_EnableHistorySuggestions, true,
         "Show and accept inline suggestions from command history.",
         [](Console &console, IProperty *) { console.ApplyCommandBarFeatures(); }},
        {CommandBarCategory, "EnableReverseHistorySearch", &Console::m_EnableReverseHistorySearch, true,
         "Enable Ctrl+R reverse history search and its transient rail.",
         [](Console &console, IProperty *) { console.ApplyCommandBarFeatures(); }},
        {CommandBarCategory, "EnableHistoryNavigation", &Console::m_EnableHistoryNavigation, true,
         "Enable filtered command-history navigation with Up and Down.",
         [](Console &console, IProperty *) { console.ApplyCommandBarFeatures(); }},
        {CommandBarCategory, "ShowNotifications", &Console::m_ShowNotifications, true,
         "Show timed notification messages while the command bar is closed.",
         [](Console &console, IProperty *) { console.ApplyMessageBoardDisplayPolicy(); }},
        {CommandBarCategory, "ShowScrollback", &Console::m_ShowScrollback, true,
         "Show stored message scrollback while the command bar is open.",
         [](Console &console, IProperty *) { console.ApplyMessageBoardDisplayPolicy(); }},
    };
    static_assert(sizeof(settings) / sizeof(settings[0]) == 13,
                  "Every built-in console config property must have one settings-table entry");

    count = sizeof(settings) / sizeof(settings[0]);
    return settings;
}

void Console::InitConfig(IConfig &config) {
    size_t count = 0;
    const Setting *settings = GetSettings(count);
    for (size_t i = 0; i < count; ++i) {
        const Setting &setting = settings[i];
        IProperty *property = config.GetProperty(setting.category, setting.key);
        this->*setting.property = property;
        DefineSetting(setting, property);
    }

    config.SetCategoryComment(CommandBarCategory, "Command Bar Settings");
    m_SyntaxThemeSettings.Define(config);
}

void Console::DefineSetting(const Setting &setting, IProperty *property) {
    if (!property)
        return;
    property->SetComment(setting.comment);
    if (const bool *value = std::get_if<bool>(&setting.defaultValue))
        property->SetDefaultBoolean(*value);
    else if (const int *value = std::get_if<int>(&setting.defaultValue))
        property->SetDefaultInteger(*value);
    else if (const float *value = std::get_if<float>(&setting.defaultValue))
        property->SetDefaultFloat(*value);
    else if (const char *const *value = std::get_if<const char *>(&setting.defaultValue))
        property->SetDefaultString(*value);
}

void Console::ApplyConfig() {
    size_t count = 0;
    const Setting *settings = GetSettings(count);
    for (size_t i = 0; i < count; ++i) {
        ApplySetting(settings[i], this->*settings[i].property);
    }
    m_CommandBar.SetSyntaxPalette(m_SyntaxThemeSettings.ReadPalette());
}

bool Console::OnModifyConfig(const char *category, const char *key, IProperty *property) {
    if (!property || !category || !key) {
        return false;
    }

    size_t count = 0;
    const Setting *settings = GetSettings(count);
    for (size_t i = 0; i < count; ++i) {
        const Setting &setting = settings[i];
        if (this->*setting.property != property || !utils::CStringEqual(category, setting.category) ||
            !utils::CStringEqual(key, setting.key)) {
            continue;
        }

        ApplySetting(setting, property);
        return true;
    }

    if (m_SyntaxThemeSettings.Owns(category, key, property)) {
        m_CommandBar.SetSyntaxPalette(m_SyntaxThemeSettings.ReadPalette());
        return true;
    }

    return false;
}

void Console::ApplySetting(const Setting &setting, IProperty *property) {
    if (setting.apply && property) {
        setting.apply(*this, property);
    }
}

void Console::ApplyCommandBarFeatures() {
    if (!m_EnableSyntaxHighlighting || !m_EnableTabCompletion || !m_EnableHistorySuggestions ||
        !m_EnableReverseHistorySearch || !m_EnableHistoryNavigation) {
        return;
    }
    m_CommandBar.SetFeatures(CommandBar::Features{
        m_EnableSyntaxHighlighting->GetBoolean(),
        m_EnableTabCompletion->GetBoolean(),
        m_EnableHistorySuggestions->GetBoolean(),
        m_EnableReverseHistorySearch->GetBoolean(),
        m_EnableHistoryNavigation->GetBoolean(),
    });
}

void Console::ApplyMessageBoardDisplayPolicy() {
    if (!m_ShowNotifications || !m_ShowScrollback)
        return;
    m_MessageBoard.SetDisplayPolicy(MessageBoard::DisplayPolicy{
        m_ShowNotifications->GetBoolean(),
        m_ShowScrollback->GetBoolean(),
    });
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
    m_MessageBoard.AdvanceNotificationTimers();
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

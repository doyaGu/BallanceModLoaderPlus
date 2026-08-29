#include "BuiltinConsole.h"

#include <algorithm>

#include "BML/IBML.h"
#include "BML/IConfig.h"
#include "BML/ILogger.h"

#include "AnsiText.h"
#include "BuiltinHUD.h"
#include "CommandContext.h"
#include "Commands.h"
#include "StringUtils.h"

const BuiltinConsole::Setting *BuiltinConsole::GetSettings(size_t &count) {
    static const Setting settings[] = {
        {"MessageDuration", &BuiltinConsole::m_MessageDuration,
         [](BuiltinConsole &console, IProperty *property) {
             console.m_MessageBoard.SetMaxTimer(std::max(2000.0f, property->GetFloat() * 1000.0f));
         }},
        {"TabColumns", &BuiltinConsole::m_TabColumns,
         [](BuiltinConsole &console, IProperty *property) {
             console.m_MessageBoard.SetTabColumns(std::max(1, property->GetInteger()));
         }},
        {"LineSpacing", &BuiltinConsole::m_LineSpacing,
         [](BuiltinConsole &console, IProperty *property) {
             console.m_MessageBoard.SetLineSpacing(property->GetFloat());
         }},
        {"MessageBackgroundAlpha", &BuiltinConsole::m_MessageBackgroundAlpha,
         [](BuiltinConsole &console, IProperty *property) {
             console.m_MessageBoard.SetMessageBackgroundAlpha(std::clamp(property->GetFloat(), 0.0f, 1.0f));
         }},
        {"WindowBackgroundAlpha", &BuiltinConsole::m_WindowBackgroundAlpha,
         [](BuiltinConsole &console, IProperty *property) {
             console.m_MessageBoard.SetWindowBackgroundAlpha(std::clamp(property->GetFloat(), 0.0f, 1.0f));
         }},
        {"FadeMaxAlpha", &BuiltinConsole::m_FadeMaxAlpha,
         [](BuiltinConsole &console, IProperty *property) {
             console.m_MessageBoard.SetFadeMaxAlpha(std::clamp(property->GetFloat(), 0.0f, 1.0f));
         }},
    };
    static_assert(sizeof(settings) / sizeof(settings[0]) == 6,
                  "Every built-in console config property must have one settings-table entry");

    count = sizeof(settings) / sizeof(settings[0]);
    return settings;
}

void BuiltinConsole::InitConfig(IConfig &config) {
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

    m_WindowBackgroundAlpha->SetComment("Alpha scale for message window background (0..1, default: 1.0)");
    m_WindowBackgroundAlpha->SetDefaultFloat(1.0f);

    m_FadeMaxAlpha->SetComment("Maximum text/background alpha in notifications (0..1, default: 1.0)");
    m_FadeMaxAlpha->SetDefaultFloat(1.0f);
}

void BuiltinConsole::ApplyConfig() {
    size_t count = 0;
    const Setting *settings = GetSettings(count);
    for (size_t i = 0; i < count; ++i) {
        ApplySetting(settings[i], this->*settings[i].property);
    }
}

bool BuiltinConsole::OnModifyConfig(const char *category, const char *key, IProperty *property) {
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

void BuiltinConsole::ApplySetting(const Setting &setting, IProperty *property) {
    if (setting.apply && property) {
        setting.apply(*this, property);
    }
}

void BuiltinConsole::OnLoad(IBML &bml, BML::CommandContext &commands, ILogger &logger, BuiltinHUD &hud) {
    m_Commands = &commands;
    m_Logger = &logger;

    m_OutputCallbackInstalled = m_Commands->SetOutputCallback(&OnCommandOutput, this);
    if (!m_OutputCallbackInstalled) {
        m_Logger->Warn("Could not register the built-in console output callback");
    }

    RegisterCommands(bml, hud);
    AnsiText::Renderer::DefaultPalette().SaveSampleIfMissing();
    m_CommandBar.LoadHistory();
}

void BuiltinConsole::OnUnload() {
    if (m_OutputCallbackInstalled && m_Commands) {
        m_Commands->ClearOutputCallback();
    }

    m_CommandBar.SaveHistory();
    m_OutputCallbackInstalled = false;
    m_Logger = nullptr;
    m_Commands = nullptr;
}

void BuiltinConsole::OnProcess() {
    const bool visible = m_CommandBar.IsVisible();
    if (!visible && ImGui::IsKeyPressed(ImGuiKey_Slash, false)) {
        if (m_Logger) {
            m_Logger->Info("Toggle Command Bar");
        }
        m_CommandBar.ToggleCommandBar();
    }

    m_MessageBoard.SetCommandBarVisible(visible);
    m_MessageBoard.Render();
    m_CommandBar.Render();
}

void BuiltinConsole::AddMessage(const char *message) {
    m_MessageBoard.Show();
    m_MessageBoard.AddMessage(message);

    if (m_Logger) {
        const std::string logMessage = utils::StripAnsiCodes(message);
        m_Logger->Info("%s", logMessage.c_str());
    }
}

void BuiltinConsole::ClearMessages() {
    m_MessageBoard.ClearMessages();
}

void BuiltinConsole::PrintHistory() {
    m_CommandBar.PrintHistory();
}

void BuiltinConsole::ClearHistory() {
    m_CommandBar.ClearHistory();
}

void BuiltinConsole::ExecuteHistory(int index) {
    m_CommandBar.ExecuteHistory(index);
}

void BuiltinConsole::OnCommandOutput(const char *message, void *userdata) {
    auto *console = static_cast<BuiltinConsole *>(userdata);
    if (console) {
        console->AddMessage(message);
    }
}

void BuiltinConsole::RegisterCommands(IBML &bml, BuiltinHUD &hud) {
    bml.RegisterCommand(new CommandBML());
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

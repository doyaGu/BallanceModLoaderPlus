#include "BuiltinConsole.h"

#include <algorithm>

#include "BML/IBML.h"
#include "BML/ILogger.h"

#include "AnsiText.h"
#include "Commands.h"
#include "ModContext.h"
#include "StringUtils.h"

void BuiltinConsole::OnLoad(IBML &bml, ILogger &logger, BMLMod *hudOwner) {
    m_Context = dynamic_cast<ModContext *>(&bml);
    m_Logger = &logger;

    if (m_Context) {
        m_Context->GetCommandContext().SetOutputCallback(&OnCommandOutput, this);
    }

    RegisterCommands(bml, hudOwner);
    AnsiText::Renderer::DefaultPalette().SaveSampleIfMissing();
    m_CommandBar.LoadHistory();
}

void BuiltinConsole::OnUnload() {
    if (m_Context) {
        m_Context->GetCommandContext().ClearOutputCallback();
    }

    m_CommandBar.SaveHistory();
    m_Logger = nullptr;
    m_Context = nullptr;
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
        m_Logger->Info(logMessage.c_str());
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

void BuiltinConsole::SetMessageDuration(float seconds) {
    m_MessageBoard.SetMaxTimer(std::max(2000.0f, seconds * 1000.0f));
}

void BuiltinConsole::SetTabColumns(int columns) {
    m_MessageBoard.SetTabColumns(std::max(1, columns));
}

void BuiltinConsole::SetLineSpacing(float spacing) {
    m_MessageBoard.SetLineSpacing(spacing);
}

void BuiltinConsole::SetMessageBackgroundAlpha(float alpha) {
    m_MessageBoard.SetMessageBackgroundAlpha(std::clamp(alpha, 0.0f, 1.0f));
}

void BuiltinConsole::SetWindowBackgroundAlpha(float alpha) {
    m_MessageBoard.SetWindowBackgroundAlpha(std::clamp(alpha, 0.0f, 1.0f));
}

void BuiltinConsole::SetFadeMaxAlpha(float alpha) {
    m_MessageBoard.SetFadeMaxAlpha(std::clamp(alpha, 0.0f, 1.0f));
}

void BuiltinConsole::OnCommandOutput(const char *message, void *userdata) {
    auto *console = static_cast<BuiltinConsole *>(userdata);
    if (console) {
        console->AddMessage(message);
    }
}

void BuiltinConsole::RegisterCommands(IBML &bml, BMLMod *hudOwner) {
    bml.RegisterCommand(new CommandBML());
    bml.RegisterCommand(new CommandHelp());
    bml.RegisterCommand(new CommandCheat());
    bml.RegisterCommand(new CommandEcho());
    bml.RegisterCommand(new CommandClear(this));
    bml.RegisterCommand(new CommandHistory(this));
    bml.RegisterCommand(new CommandExit());
    bml.RegisterCommand(new CommandHUD(hudOwner));
    bml.RegisterCommand(new CommandPalette());
    bml.RegisterCommand(new CommandScript());
}

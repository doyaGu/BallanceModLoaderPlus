#ifndef BML_CONSOLE_H
#define BML_CONSOLE_H

#include <cstddef>
#include <variant>

#include "Console/CommandBar.h"
#include "Console/MessageBoard.h"
#include "Console/Shell/ShellHistory.h"

class ModContext;
class Config;
class Logger;
class Property;

namespace BML {
    class CommandContext;
}

class Console {
public:
    void InitConfig(Config &config);
    void ApplyConfig();
    bool OnModifyConfig(const char *category, const char *key, Property *property);

    void OnLoad(ModContext &bml, BML::CommandContext &commands, Logger &logger);
    void OnUnload();
    void OnProcess();
    void CloseCommandBar();

    void AddMessage(const char *message);
    void ClearMessages();

    // Prints numbered history entries, oldest first; lastCount 0 prints all.
    void PrintHistory(std::size_t lastCount = 0);
    void ClearHistory();
    bool EraseHistory(std::size_t number);
    BML::Shell::History &GetHistory() { return m_History; }

private:
    using SettingDefault = std::variant<bool, int, float, const char *>;

    struct Setting {
        const char *category;
        const char *key;
        Property *Console::*property;
        SettingDefault defaultValue;
        const char *comment;
        void (*apply)(Console &console, Property *property);
    };

    static const Setting *GetSettings(size_t &count);
    static void DefineSetting(const Setting &setting, Property *property);
    void ApplySetting(const Setting &setting, Property *property);
    void ApplyCommandBarFeatures();
    void ApplyMessageBoardDisplayPolicy();

    static void OnCommandOutput(const char *message, void *userdata);
    void RegisterCommands(ModContext &bml);

    BML::CommandContext *m_Commands = nullptr;
    Logger *m_Logger = nullptr;
    bool m_OutputCallbackInstalled = false;
    BML::Shell::History m_History;
    CommandBar m_CommandBar;
    CommandBarTheme::Settings m_SyntaxThemeSettings;
    MessageBoard m_MessageBoard;

    Property *m_MessageDuration = nullptr;
    Property *m_TabColumns = nullptr;
    Property *m_LineSpacing = nullptr;
    Property *m_MessageBackgroundAlpha = nullptr;
    Property *m_FadeMaxAlpha = nullptr;
    Property *m_KeepOpen = nullptr;
    Property *m_EnableSyntaxHighlighting = nullptr;
    Property *m_EnableTabCompletion = nullptr;
    Property *m_EnableHistorySuggestions = nullptr;
    Property *m_EnableReverseHistorySearch = nullptr;
    Property *m_EnableHistoryNavigation = nullptr;
    Property *m_ShowNotifications = nullptr;
    Property *m_ShowScrollback = nullptr;
};

#endif // BML_CONSOLE_H

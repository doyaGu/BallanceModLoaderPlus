#ifndef BML_CONSOLE_H
#define BML_CONSOLE_H

#include <cstddef>
#include <variant>

#include "Console/CommandBar.h"
#include "Console/MessageBoard.h"
#include "Console/Shell/ShellHistory.h"

class IBML;
class IConfig;
class ILogger;
class IProperty;

namespace BML {
    class CommandContext;
}

class Console {
public:
    void InitConfig(IConfig &config);
    void ApplyConfig();
    bool OnModifyConfig(const char *category, const char *key, IProperty *property);

    void OnLoad(IBML &bml, BML::CommandContext &commands, ILogger &logger);
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
        IProperty *Console::*property;
        SettingDefault defaultValue;
        const char *comment;
        void (*apply)(Console &console, IProperty *property);
    };

    static const Setting *GetSettings(size_t &count);
    static void DefineSetting(const Setting &setting, IProperty *property);
    void ApplySetting(const Setting &setting, IProperty *property);
    void ApplyCommandBarFeatures();
    void ApplyMessageBoardDisplayPolicy();

    static void OnCommandOutput(const char *message, void *userdata);
    void RegisterCommands(IBML &bml);

    BML::CommandContext *m_Commands = nullptr;
    ILogger *m_Logger = nullptr;
    bool m_OutputCallbackInstalled = false;
    BML::Shell::History m_History;
    CommandBar m_CommandBar;
    CommandBarTheme::Settings m_SyntaxThemeSettings;
    MessageBoard m_MessageBoard;

    IProperty *m_MessageDuration = nullptr;
    IProperty *m_TabColumns = nullptr;
    IProperty *m_LineSpacing = nullptr;
    IProperty *m_MessageBackgroundAlpha = nullptr;
    IProperty *m_FadeMaxAlpha = nullptr;
    IProperty *m_KeepOpen = nullptr;
    IProperty *m_EnableSyntaxHighlighting = nullptr;
    IProperty *m_EnableTabCompletion = nullptr;
    IProperty *m_EnableHistorySuggestions = nullptr;
    IProperty *m_EnableReverseHistorySearch = nullptr;
    IProperty *m_EnableHistoryNavigation = nullptr;
    IProperty *m_ShowNotifications = nullptr;
    IProperty *m_ShowScrollback = nullptr;
};

#endif // BML_CONSOLE_H

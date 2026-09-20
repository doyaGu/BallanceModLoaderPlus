#ifndef BML_CONSOLE_H
#define BML_CONSOLE_H

#include <cstddef>

#include "Console/CommandBar.h"
#include "Console/MessageBoard.h"
#include "Console/Shell/ShellHistory.h"

class HUDRuntime;
class IBML;
class IConfig;
class ILogger;
class IProperty;
struct FontCommandContext;

namespace BML {
    class CommandContext;
}

class Console {
public:
    void InitConfig(IConfig &config);
    void ApplyConfig();
    bool OnModifyConfig(const char *category, const char *key, IProperty *property);

    void OnLoad(IBML &bml, BML::CommandContext &commands, ILogger &logger, HUDRuntime &hud,
                const FontCommandContext &fontContext);
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
    struct Setting {
        const char *key;
        IProperty *Console::*property;
        void (*apply)(Console &console, IProperty *property);
    };

    static const Setting *GetSettings(size_t &count);
    void ApplySetting(const Setting &setting, IProperty *property);

    static void OnCommandOutput(const char *message, void *userdata);
    void RegisterCommands(IBML &bml, HUDRuntime &hud, const FontCommandContext &fontContext);

    BML::CommandContext *m_Commands = nullptr;
    ILogger *m_Logger = nullptr;
    bool m_OutputCallbackInstalled = false;
    BML::Shell::History m_History;
    CommandBar m_CommandBar;
    MessageBoard m_MessageBoard;

    IProperty *m_MessageDuration = nullptr;
    IProperty *m_TabColumns = nullptr;
    IProperty *m_LineSpacing = nullptr;
    IProperty *m_MessageBackgroundAlpha = nullptr;
    IProperty *m_FadeMaxAlpha = nullptr;
    IProperty *m_KeepOpen = nullptr;
};

#endif // BML_CONSOLE_H

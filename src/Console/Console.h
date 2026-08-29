#ifndef BML_CONSOLE_H
#define BML_CONSOLE_H

#include <cstddef>

#include "Console/CommandBar.h"
#include "Console/MessageBoard.h"

class HUDRuntime;
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

    void OnLoad(IBML &bml, BML::CommandContext &commands, ILogger &logger, HUDRuntime &hud);
    void OnUnload();
    void OnProcess();

    void AddMessage(const char *message);
    void ClearMessages();

    void PrintHistory();
    void ClearHistory();
    void ExecuteHistory(int index);

private:
    struct Setting {
        const char *key;
        IProperty *Console::*property;
        void (*apply)(Console &console, IProperty *property);
    };

    static const Setting *GetSettings(size_t &count);
    void ApplySetting(const Setting &setting, IProperty *property);

    static void OnCommandOutput(const char *message, void *userdata);
    void RegisterCommands(IBML &bml, HUDRuntime &hud);

    BML::CommandContext *m_Commands = nullptr;
    ILogger *m_Logger = nullptr;
    bool m_OutputCallbackInstalled = false;
    CommandBar m_CommandBar;
    MessageBoard m_MessageBoard;

    IProperty *m_MessageDuration = nullptr;
    IProperty *m_TabColumns = nullptr;
    IProperty *m_LineSpacing = nullptr;
    IProperty *m_MessageBackgroundAlpha = nullptr;
    IProperty *m_WindowBackgroundAlpha = nullptr;
    IProperty *m_FadeMaxAlpha = nullptr;
};

#endif // BML_CONSOLE_H

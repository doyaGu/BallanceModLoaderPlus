#ifndef BML_BUILTINCONSOLE_H
#define BML_BUILTINCONSOLE_H

#include <cstddef>

#include "CommandBar.h"
#include "MessageBoard.h"

class BMLMod;
class IBML;
class IConfig;
class ILogger;
class IProperty;
class ModContext;

class BuiltinConsole {
public:
    void InitConfig(IConfig &config);
    void ApplyConfig();
    bool OnModifyConfig(const char *category, const char *key, IProperty *property);

    void OnLoad(IBML &bml, ILogger &logger, BMLMod *hudOwner);
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
        IProperty *BuiltinConsole::*property;
        void (*apply)(BuiltinConsole &console, IProperty *property);
    };

    static const Setting *GetSettings(size_t &count);
    void ApplySetting(const Setting &setting, IProperty *property);

    static void OnCommandOutput(const char *message, void *userdata);
    void RegisterCommands(IBML &bml, BMLMod *hudOwner);

    ModContext *m_Context = nullptr;
    ILogger *m_Logger = nullptr;
    CommandBar m_CommandBar;
    MessageBoard m_MessageBoard;

    IProperty *m_MessageDuration = nullptr;
    IProperty *m_TabColumns = nullptr;
    IProperty *m_LineSpacing = nullptr;
    IProperty *m_MessageBackgroundAlpha = nullptr;
    IProperty *m_WindowBackgroundAlpha = nullptr;
    IProperty *m_FadeMaxAlpha = nullptr;
};

#endif // BML_BUILTINCONSOLE_H

#ifndef BML_BUILTINCONSOLE_H
#define BML_BUILTINCONSOLE_H

#include "CommandBar.h"
#include "MessageBoard.h"

class BMLMod;
class IBML;
class ILogger;
class ModContext;

class BuiltinConsole {
public:
    void OnLoad(IBML &bml, ILogger &logger, BMLMod *hudOwner);
    void OnUnload();
    void OnProcess();

    void AddMessage(const char *message);
    void ClearMessages();

    void PrintHistory();
    void ClearHistory();
    void ExecuteHistory(int index);

    void SetMessageDuration(float seconds);
    void SetTabColumns(int columns);
    void SetLineSpacing(float spacing);
    void SetMessageBackgroundAlpha(float alpha);
    void SetWindowBackgroundAlpha(float alpha);
    void SetFadeMaxAlpha(float alpha);

private:
    static void OnCommandOutput(const char *message, void *userdata);
    void RegisterCommands(IBML &bml, BMLMod *hudOwner);

    ModContext *m_Context = nullptr;
    ILogger *m_Logger = nullptr;
    CommandBar m_CommandBar;
    MessageBoard m_MessageBoard;
};

#endif // BML_BUILTINCONSOLE_H

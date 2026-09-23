#include <BML/IBML.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>

class NoExitModTest final : public IMod {
public:
    explicit NoExitModTest(IBML *bml) : IMod(bml) { AddDependency("BML"); }

    const char *GetID() override { return "NoExitModTest"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "No Exit Mod Test"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override { return "Checks native teardown without BMLExit"; }
    DECLARE_BML_VERSION;

    void OnLoad() override { GetLogger()->Info("No-exit Mod: OnLoad"); }
    void OnUnload() override { GetLogger()->Info("No-exit Mod: OnUnload"); }
};

BML_MOD_ENTRY(IMod *) BMLEntry(IBML *bml) {
    // Deliberately leave the instance to process teardown: a caller in the
    // loader DLL must never delete a Mod allocated in this DLL.
    return new NoExitModTest(bml);
}

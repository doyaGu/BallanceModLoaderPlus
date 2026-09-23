#include <BML/BML.h>
#include <BML/Command.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>

class RejectedEntryInspectorTest final : public IMod {
public:
    explicit RejectedEntryInspectorTest(IBML *bml) : IMod(bml) { AddDependency("BML"); }

    const char *GetID() override { return "RejectedEntryInspectorTest"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "Rejected Entry Inspector Test"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override { return "Checks rejected entry command cleanup"; }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        const int result = BML_UnregisterCommand("rejected-entry-test");
        GetLogger()->Info("Rejected entry command: %s (result=%d)",
                          result == BML_ERROR_NOT_FOUND ? "absent" : "present", result);

        const void *interfacePtr = nullptr;
        if (BML_GetInterface(BML_COMMAND_INTERFACE_ID, BML_COMMAND_INTERFACE_MAJOR,
                             &interfacePtr) != BML_OK || !interfacePtr)
            return;
        m_Command.Api = static_cast<const BML_CommandInterface *>(interfacePtr);
        m_Command.Logger = GetLogger();

        BML_CommandDefinition definition{};
        definition.StructSize = sizeof(definition);
        definition.Name = "callback-self-unregister-test";
        definition.Alias = "callback-conflict-test";
        definition.UserData = &m_Command;
        definition.Execute = &ExecuteCommand;
        definition.Release = &ReleaseCommand;
        const int registered = m_Command.Api->Register(nullptr, &definition, &m_Command.Handle);
        if (registered != BML_OK) {
            GetLogger()->Error("Callback command registration failed: %d", registered);
            return;
        }

        BML_CommandDefinition conflicting = definition;
        conflicting.Name = "callback-other-test";
        BML_CommandHandle ignored = BML_COMMAND_INVALID_HANDLE;
        const int conflictResult = m_Command.Api->Register(nullptr, &conflicting, &ignored);
        GetLogger()->Info("Callback alias conflict: %s (result=%d)",
                          conflictResult == BML_ERROR_ALREADY_EXISTS ? "rejected" : "accepted",
                          conflictResult);

        m_BML->ExecuteCommand("callback-self-unregister-test");
        GetLogger()->Info("Callback command release after execution: %s",
                          m_Command.Released ? "yes" : "no");
    }

private:
    struct CommandState {
        const BML_CommandInterface *Api = nullptr;
        BML_CommandHandle Handle = BML_COMMAND_INVALID_HANDLE;
        ILogger *Logger = nullptr;
        bool Released = false;
    };

    static int BML_CDECL ExecuteCommand(void *userData, const BML_CommandInvocation *) {
        auto *state = static_cast<CommandState *>(userData);
        const int result = state->Api->Unregister(nullptr, state->Handle);
        state->Logger->Info("Callback self-unregister: %s", result == BML_OK ? "succeeded" : "failed");
        return result == BML_OK ? 0 : 1;
    }

    static void BML_CDECL ReleaseCommand(void *userData) {
        static_cast<CommandState *>(userData)->Released = true;
    }

    CommandState m_Command;
};

BML_MOD_ENTRY(IMod *) BMLEntry(IBML *bml) {
    return new RejectedEntryInspectorTest(bml);
}

BML_MOD_ENTRY(void) BMLExit(IMod *mod) {
    delete mod;
}

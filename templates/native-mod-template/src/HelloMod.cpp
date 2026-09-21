#include <BML/BML.h>
#include <BML/IMod.h>
#include <BML/IBML.h>
#include <BML/ICommand.h>
#include <BML/ILogger.h>

#include <memory>
#include <string>
#include <vector>

class CommandHello : public ICommand {
public:
    std::string GetName() override { return "hello"; }
    std::string GetAlias() override { return ""; }
    std::string GetDescription() override { return "Print a greeting: hello [name]"; }
    bool IsCheat() override { return false; }

    void Execute(IBML *bml, const std::vector<std::string> &args) override {
        const char *target = args.size() >= 2 ? args[1].c_str() : "world";
        std::string line = std::string("\x1b[36mHello, ") + target + "!\x1b[0m";
        bml->SendIngameMessage(line.c_str());
    }

    const std::vector<std::string> GetTabCompletion(IBML *, const std::vector<std::string> &args) override {
        if (args.size() == 2) return {"world", "Ballance", "BML"};
        return {};
    }
};

class HelloMod final : public IMod {
public:
    explicit HelloMod(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "HelloMod"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "Hello Mod"; }
    const char *GetAuthor() override { return "Template"; }
    const char *GetDescription() override { return "Minimal example mod for BML+"; }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        GetLogger()->Info("HelloMod loaded");
        m_Command = std::make_unique<CommandHello>();
        m_BML->RegisterCommand(m_Command.get());
        m_BML->SendIngameMessage("\x1b[32mHelloMod loaded. Type 'hello' in command bar.\x1b[0m");
    }

    void OnUnload() override {
        if (m_Command) {
            const int status = BML_UnregisterCommand("hello");
            if (status == BML_OK || status == BML_ERROR_NOT_FOUND ||
                status == BML_ERROR_ACCESS_DENIED) {
                m_Command.reset();
            } else {
                // The loader may still hold this pointer. Leaking it is safer than
                // leaving a dangling command during abnormal shutdown.
                (void)m_Command.release();
                GetLogger()->Error("Could not unregister hello command: %d", status);
            }
            GetLogger()->Info("Native profile basic: command_cleanup_status=%d", status);
        }
        GetLogger()->Info("HelloMod unloaded");
    }

private:
    std::unique_ptr<CommandHello> m_Command;
};

BML_MOD_ENTRY(IMod *) BMLEntry(IBML *bml) {
    return new HelloMod(bml);
}

BML_MOD_ENTRY(void) BMLExit(IMod *mod) {
    delete mod;
}

#include <BML/IMod.h>
#include <BML/IBML.h>
#include <BML/ICommand.h>
#include <BML/ILogger.h>

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
        m_BML->RegisterCommand(new CommandHello());
        m_BML->SendIngameMessage("\x1b[32mHelloMod loaded. Type 'hello' in command bar.\x1b[0m");
    }

    void OnUnload() override {
        GetLogger()->Info("HelloMod unloaded");
    }
};

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new HelloMod(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}

#include <BML/Mod.hpp>

// The common authoring surface needs only Mod.hpp and the core BML::BML target.
class MyMod final : public BML::Mod::Instance {
public:
    void OnLoad(const BML::Mod::Context &ctx) override {
        m_Enabled =
            ctx.Config().DeclareProperty({"General", "Enabled", true, "Enable the example"}).Take();
        m_Message = ctx.Config()
                        .DeclareProperty({"General", "Message", std::string("Hello from BML+")})
                        .Take();
        m_Key = ctx.Input().GetKeyFromName("F3").Take();
        m_Command =
            ctx.Commands()
                .Register({.Name = "native-example", .Description = "Print the configured message"},
                          [this](const BML::Command::Invocation &call) {
                              return call.Write(m_Message.Get<std::string>())
                                         ? BML_COMMAND_STATUS_SUCCESS
                                         : BML_COMMAND_STATUS_FAILURE;
                          })
                .Take();
        m_Startup =
            ctx.Timers()
                .SetTimeout(
                    1000,
                    [ctx] { (void)ctx.LogInfo("Press F3 or run native-example in the console"); })
                .Take();
    }

    void OnProcess(const BML::Mod::Context &ctx) override {
        if (!m_Enabled.Get<bool>())
            return;
        auto keyboard = ctx.Input().ReadKeyboard();
        if (keyboard && keyboard->IsKeyPressed(m_Key))
            (void)ctx.LogInfo(m_Message.Get<std::string>());
    }

private:
    BML::Config::Property m_Enabled, m_Message;
    BML::Command::Registration m_Command;
    BML::Timer::Registration m_Startup;
    uint32_t m_Key = 0;
};
BML_DEFINE_MOD(MyMod, {
    .Id = "example.friendly",
    .Name = "Native example",
    .Version = "1.0.0",
    .Author = "BML",
    .Description = "Config, command, timer and input",
})

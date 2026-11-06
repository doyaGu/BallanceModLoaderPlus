#include <BML/Mod.hpp>
#include <BML/UI.h>

class HelloMod final : public BML::Mod::Instance {
  public:
    void OnLoad(const BML::Mod::Context &ctx) override {
        m_Command = ctx.Commands()
                        .Register(
                            {.Name = "hello", .Description = "Print a greeting: hello [name]"},
                            [](const BML::Command::Invocation &call) {
                                const std::string target(
                                    call.Arguments().empty() ? "world" : call.Argument(0));
                                const auto message = "\x1b[36mHello, " + target + "!\x1b[0m";
                                return call.Write(message) == BML_OK ? BML_COMMAND_STATUS_SUCCESS
                                                                     : BML_COMMAND_STATUS_FAILURE;
                            },
                            [](const BML::Command::CompletionRequest &request,
                               const BML::Command::Completion &out) {
                                if (request.Arguments().size() != 1)
                                    return BML_OK;
                                for (const char *name : {"world", "Ballance", "BML"}) {
                                    const int code = out.Add(name);
                                    if (code != BML_OK)
                                        return code;
                                }
                                return BML_OK;
                            })
                        .Take();
        ctx.LogInfo("HelloMod loaded");
        (void)BML::UI::AddMessage("\x1b[32mHelloMod loaded. Type 'hello' in command bar.\x1b[0m");
    }

  private:
    BML::Command::Registration m_Command;
};

BML_DEFINE_MOD(HelloMod, {
    .Id = "HelloMod",
    .Name = "Hello Mod",
    .Version = "1.0.0",
    .Author = "Template",
    .Description = "Minimal example mod for BML+",
    .Dependencies = {{"BML"}},
})

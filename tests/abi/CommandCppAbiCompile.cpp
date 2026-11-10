#include "BML/Command.hpp"

int BML_TestCommandCppFacade(const BML::Mod::Context &ctx) {
    BML::Command::Definition definition;
    definition.Name = "cpp-command";

    auto result = ctx.Commands().Register(
        definition,
        [](const BML::Command::Invocation &invocation) {
            return invocation.Write("executed") ? BML_COMMAND_STATUS_SUCCESS
                                                : BML_COMMAND_STATUS_FAILURE;
        },
        [](const BML::Command::CompletionRequest &, const BML::Command::Completion &completion) {
            return completion.Add("candidate");
        });
    if (!result)
        return result.Code();

    auto registration = result.Take();
    BML::Command::Info info;
    int commandStatus = BML_COMMAND_STATUS_FAILURE;
    const auto commands = ctx.Commands().ReadCommands();
    const auto found = ctx.Commands().Find("cpp-command");
    const auto executed = ctx.Commands().ExecuteLine("cpp-command");
    const int closeStatus = registration.Unregister().Code();
    return commands && !commands.Value().empty() && found && executed &&
                   executed.Value() == BML_COMMAND_STATUS_SUCCESS
               ? closeStatus : BML_ERROR_FAIL;
}

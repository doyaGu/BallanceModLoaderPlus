#include "BML/Command.hpp"

int BML_TestCommandCppFacade() {
    BML::Command::Definition definition;
    definition.Name = "cpp-command";

    BML::Command::Registration registration;
    const int status = registration.Register(
        definition,
        [](const BML::Command::Invocation &invocation) {
            return invocation.Write("executed") == BML_OK
                ? BML_COMMAND_STATUS_SUCCESS
                : BML_COMMAND_STATUS_FAILURE;
        },
        [](const BML::Command::CompletionRequest &,
           const BML::Command::Completion &completion) {
            return completion.Add("candidate");
        });
    if (status != BML_OK)
        return status;

    BML::Command::Info info;
    int commandStatus = BML_COMMAND_STATUS_FAILURE;
    int visited = 0;
    const int visitStatus = BML::Command::Visit(
        [&visited](const BML::Command::Info &) {
            ++visited;
            return BML_OK;
        });
    const int findStatus = BML::Command::Find("cpp-command", info);
    const int executeStatus = BML::Command::ExecuteLine(
        "cpp-command", commandStatus);
    const int closeStatus = registration.Unregister();
    return visitStatus == BML_OK && findStatus == BML_OK &&
                   executeStatus == BML_OK &&
                   visited != 0 &&
                   commandStatus == BML_COMMAND_STATUS_SUCCESS
        ? closeStatus
        : BML_ERROR_FAIL;
}

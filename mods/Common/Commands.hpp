#ifndef BML_MODS_COMMON_COMMANDS_HPP
#define BML_MODS_COMMON_COMMANDS_HPP
#include "Common/ModSupport.hpp"
namespace ModSupport {
template <class T>
void RegisterCommand(const Context &context, std::vector<BML::Command::Registration> &registrations,
                     std::shared_ptr<T> command) {
    BML::Command::Registration registration;
    BML::Command::CompleteCallback complete;
    if constexpr (requires(const Context &c, const std::vector<std::string> &args) {
                      command->GetTabCompletion(c, args);
                  })
        complete = [command, context](const auto &request, const auto &output) {
            std::vector<std::string> args{std::string(request.InvokedAs())};
            for (auto *v : request.Arguments())
                args.emplace_back(v ? v : "");
            for (const auto &candidate : command->GetTabCompletion(context, args)) {
                const int code = output.Add(candidate);
                if (code != BML_OK)
                    return code;
            }
            return BML_OK;
        };
    const int code = registration.Register(
        T::Define(),
        [command, context](const auto &invocation) {
            std::vector<std::string> args{std::string(invocation.InvokedAs())};
            for (auto *v : invocation.Arguments())
                args.emplace_back(v ? v : "");
            command->Execute(context, args);
            return BML_COMMAND_STATUS_SUCCESS;
        },
        std::move(complete));
    Check(code);
    registrations.push_back(std::move(registration));
}
} // namespace ModSupport
#endif

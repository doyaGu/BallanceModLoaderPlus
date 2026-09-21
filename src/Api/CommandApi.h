#ifndef BML_COMMAND_API_H
#define BML_COMMAND_API_H

#include <memory>
#include <string>
#include <unordered_map>

#include "BML/Command.h"

class ModContext;

namespace BML::Api {

// Owns commands registered through bml.command and adapts their C callbacks to
// the command registry shared with legacy and script commands.
class CommandApi {
public:
    explicit CommandApi(ModContext &context) noexcept;
    ~CommandApi();

    CommandApi(const CommandApi &) = delete;
    CommandApi &operator=(const CommandApi &) = delete;

    int Register(const std::string &owner, const BML_CommandDefinition &definition,
                 BML_CommandHandle &handle);
    int Unregister(const std::string &owner, BML_CommandHandle handle);
    int SetEnabled(const std::string &owner, BML_CommandHandle handle,
                   bool enabled);
    bool CleanupOwner(const std::string &owner) noexcept;
    void FlushPending() noexcept;

private:
    struct Entry;

    BML_CommandHandle NextHandle() noexcept;
    int UnregisterLocked(const std::string &owner,
                         BML_CommandHandle handle, bool defer);

    ModContext &m_Context;
    std::unordered_map<BML_CommandHandle, std::shared_ptr<Entry>> m_Entries;
    BML_CommandHandle m_NextHandle = 1;
    bool m_CleaningOwner = false;
};

[[nodiscard]] const BML_CommandInterface &CommandInterface() noexcept;

} // namespace BML::Api

#endif // BML_COMMAND_API_H

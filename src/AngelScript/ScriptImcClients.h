#ifndef BML_SCRIPTIMCCLIENTS_H
#define BML_SCRIPTIMCCLIENTS_H

#include <mutex>
#include <string>

#include "BML/Generated/bml_events_imc.hpp"

class ModContext;

namespace BML {

class ScriptImcClients final {
public:
    ScriptImcClients(ModContext *context, std::string ownerId);
    ~ScriptImcClients();

    ScriptImcClients(const ScriptImcClients &) = delete;
    ScriptImcClients &operator=(const ScriptImcClients &) = delete;

    int Events(Imc::Generated::Bml::Events::Client *&out) noexcept;

    void Close() noexcept;

private:
    ModContext *m_Context = nullptr;
    std::string m_OwnerId;
    std::mutex m_Mutex;
    Imc::Generated::Bml::Events::Client m_Events;
};

} // namespace BML

#endif // BML_SCRIPTIMCCLIENTS_H

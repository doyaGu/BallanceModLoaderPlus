#ifndef BML_SCRIPTIMCCLIENTS_H
#define BML_SCRIPTIMCCLIENTS_H

#include <mutex>
#include <string>

#include "BML/Generated/bml_events_imc.hpp"
#include "BML/Generated/bml_gameplay_imc.hpp"
#include "BML/Generated/bml_runtime_imc.hpp"
#include "BML/Generated/bml_scene_imc.hpp"
#include "BML/Generated/bml_ui_imc.hpp"

class ModContext;

namespace BML {

class ScriptImcClients final {
public:
    ScriptImcClients(ModContext *context, std::string ownerId);
    ~ScriptImcClients();

    ScriptImcClients(const ScriptImcClients &) = delete;
    ScriptImcClients &operator=(const ScriptImcClients &) = delete;

    int Runtime(Imc::Generated::Bml::Runtime::Client *&out) noexcept;
    int Scene(Imc::Generated::Bml::Scene::Client *&out) noexcept;
    int Gameplay(Imc::Generated::Bml::Gameplay::Client *&out) noexcept;
    int Ui(Imc::Generated::Bml::Ui::Client *&out) noexcept;
    int Events(Imc::Generated::Bml::Events::Client *&out) noexcept;

    void Close() noexcept;

private:
    template <typename Client>
    int Ensure(Client &client, Client *&out) noexcept;

    ModContext *m_Context = nullptr;
    std::string m_OwnerId;
    std::mutex m_Mutex;
    Imc::Generated::Bml::Runtime::Client m_Runtime;
    Imc::Generated::Bml::Scene::Client m_Scene;
    Imc::Generated::Bml::Gameplay::Client m_Gameplay;
    Imc::Generated::Bml::Ui::Client m_Ui;
    Imc::Generated::Bml::Events::Client m_Events;
};

} // namespace BML

#endif // BML_SCRIPTIMCCLIENTS_H

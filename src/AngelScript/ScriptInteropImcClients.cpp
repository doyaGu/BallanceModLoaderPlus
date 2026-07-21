#include "ScriptInteropImcClients.h"

#include "ImcRuntime.h"
#include "ModContext.h"

namespace BML {

ScriptInteropImcClients::ScriptInteropImcClients(ModContext *context, std::string ownerId)
    : m_Context(context), m_OwnerId(std::move(ownerId)) {}

ScriptInteropImcClients::~ScriptInteropImcClients() {
    Close();
}

template <typename Client>
int ScriptInteropImcClients::Ensure(Client &client, Client *&out) noexcept {
    out = nullptr;
    std::lock_guard lock(m_Mutex);
    if (!m_Context || m_OwnerId.empty())
        return BML_ERROR_INTEROP_UNSUPPORTED;
    if (!client.Handle()) {
        BML_ImcClient raw = nullptr;
        const int openStatus = m_Context->GetImcRuntime().OpenClient(m_OwnerId, &raw);
        if (openStatus != BML_OK)
            return openStatus;
        const int adoptStatus = client.Adopt(raw);
        if (adoptStatus != BML_OK)
            return adoptStatus;
    }
    out = &client;
    return BML_OK;
}

int ScriptInteropImcClients::Runtime(Imc::Generated::Bml::Runtime::Client *&out) noexcept {
    return Ensure(m_Runtime, out);
}

int ScriptInteropImcClients::Scene(Imc::Generated::Bml::Scene::Client *&out) noexcept {
    return Ensure(m_Scene, out);
}

int ScriptInteropImcClients::Gameplay(Imc::Generated::Bml::Gameplay::Client *&out) noexcept {
    return Ensure(m_Gameplay, out);
}

int ScriptInteropImcClients::Ui(Imc::Generated::Bml::Ui::Client *&out) noexcept {
    return Ensure(m_Ui, out);
}

int ScriptInteropImcClients::Events(Imc::Generated::Bml::Events::Client *&out) noexcept {
    return Ensure(m_Events, out);
}

void ScriptInteropImcClients::Close() noexcept {
    std::lock_guard lock(m_Mutex);
    (void)m_Events.Close();
    (void)m_Ui.Close();
    (void)m_Gameplay.Close();
    (void)m_Scene.Close();
    (void)m_Runtime.Close();
}

} // namespace BML
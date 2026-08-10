#include "ScriptImcClients.h"

#include "ImcRuntime.h"
#include "ModContext.h"

namespace BML {

ScriptImcClients::ScriptImcClients(ModContext *context, std::string ownerId)
    : m_Context(context), m_OwnerId(std::move(ownerId)) {}

ScriptImcClients::~ScriptImcClients() {
    Close();
}

int ScriptImcClients::Events(
    Imc::Generated::Bml::Events::Client *&out) noexcept {
    out = nullptr;
    std::lock_guard lock(m_Mutex);
    if (!m_Context || m_OwnerId.empty())
        return BML_ERROR_IMC_UNSUPPORTED;
    if (!m_Events.Handle()) {
        BML_ImcClient raw = nullptr;
        const int openStatus = m_Context->GetImcRuntime().OpenClient(m_OwnerId, &raw);
        if (openStatus != BML_OK)
            return openStatus;
        const int adoptStatus = m_Events.Adopt(raw);
        if (adoptStatus != BML_OK)
            return adoptStatus;
    }
    out = &m_Events;
    return BML_OK;
}

void ScriptImcClients::Close() noexcept {
    std::lock_guard lock(m_Mutex);
    (void)m_Events.Close();
}

} // namespace BML

#include "InteropSessionService.h"

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace BML {
namespace {

struct Session {
    uint64_t Id = 0;
    std::string OwnerId;
};

std::atomic<uint64_t> g_NextServiceId{1};

void RemoveSessionLocked(InteropSessionService::State &state, const std::string &ownerId);
void AddSessionLocked(InteropSessionService::State &state, const std::string &ownerId);

} // namespace

struct InteropSessionService::State {
    std::mutex Mutex;
    uint64_t ServiceId = g_NextServiceId.fetch_add(1, std::memory_order_relaxed);
    uint64_t NextSessionId = 1;
    std::unordered_map<std::string, std::unique_ptr<Session>> Sessions;
    std::unordered_map<uint64_t, Session *> SessionsById;
    /* BML_InteropCallContext borrows OwnerId; interning keeps that spelling
     * stable while individual sessions are invalidated and replaced. */
    std::unordered_set<std::string> InternedOwnerIds;
};

namespace {

void RemoveSessionLocked(InteropSessionService::State &state, const std::string &ownerId) {
    const auto found = state.Sessions.find(ownerId);
    if (found == state.Sessions.end() || !found->second)
        return;
    state.SessionsById.erase(found->second->Id);
    state.Sessions.erase(found);
}

void AddSessionLocked(InteropSessionService::State &state, const std::string &ownerId) {
    auto session = std::make_unique<Session>();
    session->Id = state.NextSessionId++;
    session->OwnerId = ownerId;
    Session *raw = session.get();
    state.Sessions[ownerId] = std::move(session);
    state.SessionsById[raw->Id] = raw;
    state.InternedOwnerIds.emplace(ownerId);
}

int MissingSessionStatus(const BML_InteropCallContext *context) {
    if (!context || context->SessionId == 0)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    /* Rotated sessions and contexts from another ModContext are both opaque
     * stale handles at the public ABI boundary. */
    return BML_ERROR_INTEROP_HANDLE_STALE;
}

} // namespace

InteropSessionService::InteropSessionService()
    : m_State(std::make_unique<State>()) {}

InteropSessionService::~InteropSessionService() = default;

void InteropSessionService::RegisterMod(const char *ownerId) {
    if (!ownerId || !*ownerId)
        return;
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    if (m_State->Sessions.find(ownerId) != m_State->Sessions.end())
        return;
    AddSessionLocked(*m_State, ownerId);
}

void InteropSessionService::InvalidateMod(const char *ownerId) {
    if (!ownerId || !*ownerId)
        return;
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    RemoveSessionLocked(*m_State, ownerId);
}

void InteropSessionService::RotateMod(const char *ownerId) {
    if (!ownerId || !*ownerId)
        return;
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    RemoveSessionLocked(*m_State, ownerId);
    AddSessionLocked(*m_State, ownerId);
}

BML_InteropCallContext InteropSessionService::CreateContextForOwner(const std::string &ownerId) const {
    BML_InteropCallContext context{};
    if (ownerId.empty())
        return context;

    std::lock_guard<std::mutex> lock(m_State->Mutex);
    const auto found = m_State->Sessions.find(ownerId);
    if (found == m_State->Sessions.end() || !found->second)
        return context;

    const auto interned = m_State->InternedOwnerIds.find(ownerId);
    context.OwnerId = interned == m_State->InternedOwnerIds.end() ? nullptr : interned->c_str();
    context.ServiceId = m_State->ServiceId;
    context.SessionId = found->second->Id;
    return context;
}

int InteropSessionService::ValidateContext(const BML_InteropCallContext *context, bool requireSession) const {
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    if (!context || context->ServiceId != m_State->ServiceId) {
        if (!requireSession && (!context || context->SessionId == 0))
            return BML_OK;
        return MissingSessionStatus(context);
    }
    if (context->SessionId == 0)
        return requireSession ? BML_ERROR_INTEROP_UNSUPPORTED : BML_OK;

    const auto found = m_State->SessionsById.find(context->SessionId);
    if (found == m_State->SessionsById.end() || !found->second ||
        !context->OwnerId || found->second->OwnerId != context->OwnerId) {
        return BML_ERROR_INTEROP_HANDLE_STALE;
    }
    return BML_OK;
}

uint64_t InteropSessionService::GetSessionId(const BML_InteropCallContext *context) const {
    return ValidateContext(context, false) == BML_OK && context ? context->SessionId : 0;
}

} // namespace BML

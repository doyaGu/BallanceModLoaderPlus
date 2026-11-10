#include "ScriptMenuPageService.h"

#include <angelscript.h>

#include <cstring>
#include <new>
#include <optional>
#include <unordered_map>
#include <utility>

#include "AngelScriptImGuiBindings.h"
#include "Loader/ModContext.h"
#include "ModMenu/ModMenuPages.h"
#include "ScriptDiagnostic.h"
#include "ScriptFunctionSupport.h"
#include "ScriptMod.h"

namespace BML {

namespace {

struct ScriptMenuPageEntry {
    ~ScriptMenuPageEntry() {
        if (Draw)
            Draw->Release();
        if (Enter)
            Enter->Release();
        if (Leave)
            Leave->Release();
    }

    std::shared_ptr<ScriptControlTicket> Ticket;
    ScriptMod *Owner = nullptr;
    asIScriptFunction *Draw = nullptr;
    asIScriptFunction *Enter = nullptr;
    asIScriptFunction *Leave = nullptr;
    std::uint64_t Generation = 0;
    bool Active = true;
};

bool ValidPageId(const std::string &id) {
    return !id.empty() && id.size() < BML_MOD_MENU_PAGE_ID_CAPACITY &&
           id.find('\0') == std::string::npos;
}

void Report(ScriptMod *owner, const std::string &message) {
    if (owner)
        owner->RecordScriptDiagnostic(
            MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, message));
}

int WriteFrameArg(asIScriptContext *context, void *data) {
    return context->SetArgObject(0, data);
}

int WriteReasonArg(asIScriptContext *context, void *data) {
    return context->SetArgDWord(0, *static_cast<asDWORD *>(data));
}

bool CallPage(const ScriptMenuPageEntry &entry, asIScriptFunction *callback,
              ScriptFunctionArgWriter writeArgs, void *args, const char *phase) {
    if (!callback)
        return true;

    ScriptFunctionCall call;
    call.Function = callback;
    call.Owner = entry.Owner;
    call.Phase = ScriptDiagnosticPhase::Callback;
    call.FailurePrefix = phase;
    call.InvalidStateMessage = "Mod Menu page callback has invalid runtime state.";
    call.SuspendedMessage = "Mod Menu page callback suspended";
    call.WriteArgs = writeArgs;
    call.UserData = args;

    BMLImGuiASCallbackRecoveryScope recovery;
    recovery.Begin();
    ScriptDiagnostic diagnostic;
    const bool success = ExecuteScriptFunction(call, diagnostic);
    recovery.End(entry.Owner ? entry.Owner->GetID() : nullptr, phase);
    if (!success && entry.Owner)
        entry.Owner->RecordScriptDiagnostic(diagnostic);
    return success;
}

int BML_CDECL DrawPage(void *userData, BML_ModMenuPageFrame *frame) {
    auto *retained = static_cast<std::shared_ptr<ScriptMenuPageEntry> *>(userData);
    if (!retained || !*retained || !(*retained)->Active || !frame ||
        frame->StructSize < BML_MOD_MENU_PAGE_FRAME_1_0_SIZE)
        return BML_ERROR_NOT_FOUND;

    const std::shared_ptr<ScriptMenuPageEntry> entry = *retained;
    ScriptMenuPageFrame scriptFrame;
    if (!CallPage(*entry, entry->Draw, WriteFrameArg, &scriptFrame, "Mod Menu page Draw failed"))
        return BML_ERROR_FAIL;

    const std::string &target = scriptFrame.GetTargetPageId();
    if (target.size() >= sizeof(frame->TargetPageId))
        return BML_ERROR_MALFORMED_MESSAGE;
    std::memcpy(frame->TargetPageId, target.c_str(), target.size() + 1);
    frame->Action = scriptFrame.GetAction();
    return BML_OK;
}

int BML_CDECL EnterPage(void *userData, BML_ModMenuPageEnterReason reason) {
    auto *retained = static_cast<std::shared_ptr<ScriptMenuPageEntry> *>(userData);
    if (!retained || !*retained || !(*retained)->Active)
        return BML_ERROR_NOT_FOUND;
    const std::shared_ptr<ScriptMenuPageEntry> entry = *retained;
    asDWORD scriptReason = static_cast<asDWORD>(reason);
    return CallPage(*entry, entry->Enter, WriteReasonArg, &scriptReason,
                    "Mod Menu page Enter failed")
               ? BML_OK
               : BML_ERROR_FAIL;
}

int BML_CDECL LeavePage(void *userData, BML_ModMenuPageLeaveReason reason) {
    auto *retained = static_cast<std::shared_ptr<ScriptMenuPageEntry> *>(userData);
    if (!retained || !*retained || !(*retained)->Active)
        return BML_ERROR_NOT_FOUND;
    const std::shared_ptr<ScriptMenuPageEntry> entry = *retained;
    asDWORD scriptReason = static_cast<asDWORD>(reason);
    return CallPage(*entry, entry->Leave, WriteReasonArg, &scriptReason,
                    "Mod Menu page Leave failed")
               ? BML_OK
               : BML_ERROR_FAIL;
}

void BML_CDECL ReleasePage(void *userData) {
    delete static_cast<std::shared_ptr<ScriptMenuPageEntry> *>(userData);
}

} // namespace

class ScriptMenuPageServiceState {
public:
    ModContext *Context = nullptr;
    ScriptMod *Owner = nullptr;
    bool Active = false;
    std::uint64_t NextGeneration = 1;
    std::unordered_map<std::string, std::shared_ptr<ScriptMenuPageEntry>> Pages;
};

bool ScriptMenuPageFrame::Push(const std::string &id) {
    if (!ValidPageId(id))
        return false;
    m_TargetPageId = id;
    m_Action = BML_MOD_MENU_PAGE_PUSH;
    return true;
}

bool ScriptMenuPageFrame::Replace(const std::string &id) {
    if (!ValidPageId(id))
        return false;
    m_TargetPageId = id;
    m_Action = BML_MOD_MENU_PAGE_REPLACE;
    return true;
}

void ScriptMenuPageFrame::Back() {
    m_TargetPageId.clear();
    m_Action = BML_MOD_MENU_PAGE_BACK;
}

void ScriptMenuPageFrame::Close() {
    m_TargetPageId.clear();
    m_Action = BML_MOD_MENU_PAGE_CLOSE;
}

ScriptMenuPageRef::ScriptMenuPageRef(std::weak_ptr<ScriptMenuPageServiceState> state,
                                     std::string id, std::uint64_t generation)
    : m_State(std::move(state)), m_Id(std::move(id)), m_Generation(generation) {}

void ScriptMenuPageRef::AddRef() {
    ++m_RefCount;
}

void ScriptMenuPageRef::Release() {
    if (--m_RefCount == 0)
        delete this;
}

bool ScriptMenuPageRef::IsValid() const {
    std::optional<ScriptRuntimeAccess> access;
    if (Ticket) {
        access.emplace(*Ticket);
        if (access->Code() != BML_OK)
            return false;
    }
    const std::shared_ptr<ScriptMenuPageServiceState> state = m_State.lock();
    if (!state || !state->Active)
        return false;
    if (Ticket && Ticket->Terminal)
        return false;
    const auto page = state->Pages.find(m_Id);
    if (page == state->Pages.end() || page->second->Generation != m_Generation || !state->Context ||
        !state->Owner)
        return false;
    return state->Context->GetModMenuPages().Contains(state->Owner->GetID(), m_Id);
}

std::string ScriptMenuPageRef::GetId() const {
    return IsValid() ? m_Id : std::string();
}

int ScriptMenuPageRef::Unregister() {
    if (!Ticket)
        return BML_ERROR_INVALID_HANDLE;
    ScriptRuntimeAccess access(*Ticket, true);
    if (access.Code() != BML_OK || Ticket->Terminal)
        return access.Code();
    const auto state = m_State.lock();
    if (!state || !state->Active || !state->Owner)
        return BML_ERROR_INVALID_HANDLE;
    const auto page = state->Pages.find(m_Id);
    if (page == state->Pages.end() || page->second->Generation != m_Generation)
        return BML_ERROR_INVALID_HANDLE;
    return state->Owner->MenuPagesForAuthoring().Unregister(m_Id);
}

ScriptMenuPageService::ScriptMenuPageService() = default;

ScriptMenuPageService::~ScriptMenuPageService() {
    Release();
}

bool ScriptMenuPageService::Bind(ModContext *context, ScriptMod *owner) {
    try {
        if (m_State && !m_State->Active && !Release())
            return false;
        if (!m_State)
            m_State = std::make_shared<ScriptMenuPageServiceState>();
        m_State->Context = context;
        m_State->Owner = owner;
        m_State->Active = true;
        return true;
    } catch (const std::bad_alloc &) {
        return false;
    }
}

int ScriptMenuPageService::Register(const ScriptMenuPageDefinition &definition,
                                    asIScriptFunction *draw, asIScriptFunction *enter,
                                    asIScriptFunction *leave, ScriptMenuPageRef *&out) {
    out = nullptr;
    if (!m_State || !m_State->Active || !m_State->Context || !m_State->Owner)
        return BML_ERROR_INVALID_PARAMETER;
    if (!m_State->Context->IsMainThread() || !m_State->Context->AreModsLoaded()) {
        Report(m_State->Owner,
               "Mod Menu pages must be registered on the game thread after Mods load.");
        return BML_ERROR_INVALID_PARAMETER;
    }
    if (!ValidPageId(definition.Id) || definition.Label.empty() ||
        definition.Label.find('\0') != std::string::npos || !draw ||
        m_State->Pages.contains(definition.Id)) {
        Report(m_State->Owner, "Invalid or duplicate Mod Menu page registration.");
        return BML_ERROR_INVALID_PARAMETER;
    }

    const ScriptFunctionParam frameParam[] = {
        {"BML::MenuPageFrame", asTM_INOUTREF},
    };
    const ScriptFunctionParam enterParam[] = {
        {"BML::MenuPageEnterReason", 0},
    };
    const ScriptFunctionParam leaveParam[] = {
        {"BML::MenuPageLeaveReason", 0},
    };
    if (!ScriptFunctionHasSignature(draw, asTYPEID_VOID, frameParam, 1) ||
        (enter && !ScriptFunctionHasSignature(enter, asTYPEID_VOID, enterParam, 1)) ||
        (leave && !ScriptFunctionHasSignature(leave, asTYPEID_VOID, leaveParam, 1))) {
        Report(m_State->Owner, "Mod Menu page callback signature does not match its funcdef.");
        return BML_ERROR_INVALID_PARAMETER;
    }

    try {
        auto entry = std::make_shared<ScriptMenuPageEntry>();
        entry->Ticket = std::make_shared<ScriptControlTicket>(
            m_State->Owner->BorrowContextView()->RuntimeScope().lock());
        entry->Owner = m_State->Owner;
        entry->Draw = draw;
        entry->Enter = enter;
        entry->Leave = leave;
        entry->Generation = m_State->NextGeneration++;
        if (m_State->NextGeneration == 0)
            m_State->NextGeneration = 1;
        draw->AddRef();
        if (enter)
            enter->AddRef();
        if (leave)
            leave->AddRef();

        auto retained = std::make_unique<std::shared_ptr<ScriptMenuPageEntry>>(entry);
        auto ref = std::make_unique<ScriptMenuPageRef>(m_State, definition.Id, entry->Generation);
        ref->Ticket = entry->Ticket;

        BML_ModMenuPage page = {};
        page.StructSize = sizeof(page);
        page.Id = definition.Id.c_str();
        page.Label = definition.Label.c_str();
        page.Description = definition.Description.c_str();
        page.UserData = retained.get();
        page.Draw = DrawPage;
        page.Enter = enter ? EnterPage : nullptr;
        page.Leave = leave ? LeavePage : nullptr;
        page.Release = ReleasePage;
        page.Flags =
            definition.ShowInDetails ? BML_MOD_MENU_PAGE_VISIBLE : BML_MOD_MENU_PAGE_HIDDEN;

        const std::string ownerId = m_State->Owner->GetID();
        m_State->Pages.emplace(definition.Id, entry);

        struct PendingPage {
            ScriptMenuPageServiceState &State;
            const std::string &Id;
            bool Committed = false;

            ~PendingPage() {
                if (!Committed)
                    State.Pages.erase(Id);
            }
        } pending{*m_State, definition.Id};

        const int status = m_State->Context->GetModMenuPages().Register(ownerId, page);
        if (status != BML_OK) {
            Report(m_State->Owner,
                   "Mod Menu page registration failed with status " + std::to_string(status) + ".");
            return status;
        }
        pending.Committed = true;
        retained.release(); // The page registry now owns its Release callback.

        out = ref.release();
        return BML_OK;
    } catch (const std::bad_alloc &) {
        Report(m_State->Owner, "Out of memory registering Mod Menu page.");
        return BML_ERROR_OUT_OF_MEMORY;
    }
}

int ScriptMenuPageService::Unregister(const std::string &id) {
    if (!m_State || !m_State->Active || !m_State->Context || !m_State->Owner ||
        !m_State->Context->IsMainThread())
        return BML_ERROR_INVALID_HANDLE;
    const auto page = m_State->Pages.find(id);
    if (page == m_State->Pages.end())
        return BML_ERROR_INVALID_HANDLE;

    const int status = m_State->Context->GetModMenuPages().Unregister(m_State->Owner->GetID(), id);
    if (status != BML_OK && status != BML_ERROR_NOT_FOUND)
        return status;
    page->second->Active = false;
    if (page->second->Ticket)
        page->second->Ticket->Terminal = true;
    m_State->Pages.erase(page);
    return BML_OK;
}

bool ScriptMenuPageService::Release() {
    if (!m_State)
        return true;
    m_State->Active = false;
    for (auto it = m_State->Pages.begin(); it != m_State->Pages.end();) {
        auto &page = it->second;
        page->Active = false;
        const int status =
            m_State->Context && m_State->Owner
                ? m_State->Context->GetModMenuPages().Unregister(m_State->Owner->GetID(), it->first)
                : BML_ERROR_FROZEN;
        if (status != BML_OK && status != BML_ERROR_NOT_FOUND) {
            ++it;
            continue;
        }
        if (page->Ticket)
            page->Ticket->Terminal = true;
        it = m_State->Pages.erase(it);
    }
    if (!m_State->Pages.empty())
        return false;
    m_State.reset();
    return true;
}

} // namespace BML

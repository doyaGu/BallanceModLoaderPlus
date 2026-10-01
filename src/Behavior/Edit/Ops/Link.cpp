#include "Behavior/Edit/Transaction.h"

#include "Behavior/Engine/Graph.h"

#include <algorithm>

namespace BML::Behavior::Internal {

Status CKEdit::Transaction::AddLink(CKBehaviorIO *source, CKBehaviorIO *sink,
                                    int delay, Stamp *added) {
    auto *link = static_cast<CKBehaviorLink *>(m_Context->CreateObject(CKCID_BEHAVIORLINK));
    if (!link)
        return Failure(Error::CreateFailed,
                       "Virtools failed to create a Behavior Link.",
                       CKERR_OUTOFMEMORY);
    CKERROR error = link->SetInBehaviorIO(source);
    if (error == CK_OK)
        error = link->SetOutBehaviorIO(sink);
    if (error == CK_OK) {
        link->SetInitialActivationDelay(delay);
        link->SetActivationDelay(delay);
        error = m_Graph->AddSubBehaviorLink(link);
    }
    if (error != CK_OK) {
        m_Context->DestroyObject(link);
        return Failure(Error::GraphChanged,
                       "Virtools rejected a Behavior Link.", error);
    }
    const Stamp value = Capture(link);
    m_Journal.Links.push_back({value});
    if (added)
        *added = value;
    return {};
}

// Reconnect moves the exact native Link. CK2 therefore keeps both the
// Link identity and any admitted activation delay; only its endpoint
// relations change. The before-image is recorded before the first setter
// so Apply failure can use the same inverse as an ordinary Patch close.
Status CKEdit::Transaction::Reconnect() {
    Status status;
    for (const CheckedReconnect &reconnect : m_Checked.Reconnections) {
        const auto nativeRecord = std::find_if(
            m_Base.Links.begin(), m_Base.Links.end(), [&](const GraphLink &item) {
                return item.Object == reconnect.Target.Anchor;
            });
        m_Graph = GraphFor();
        CKBehaviorLink *link = nativeRecord == m_Base.Links.end()
            ? nullptr
            : Resolve<CKBehaviorLink>(
                  m_Context,
                  {static_cast<CK_ID>(nativeRecord->Id),
                   m_Context->GetObject(static_cast<CK_ID>(nativeRecord->Id))},
                  CKCID_BEHAVIORLINK);
        if (!m_Graph || !link || !Engine::Contains(m_Graph, link) ||
            link->GetInitialActivationDelay() !=
                reconnect.Target.Delay ||
            link->GetActivationDelay() != nativeRecord->RemainingDelay) {
            return Failure(
                Error::GraphChanged,
                "The Link selected by Reconnect changed identity or delay before Apply.",
                CKERR_INVALIDOBJECT);
        }
        GraphLinkShape current;
        if (!Describe(link, current) ||
            current.Source != reconnect.Target.Source ||
            current.Target != reconnect.Target.Sink) {
            return Failure(
                Error::GraphChanged,
                "The Link selected by Reconnect changed endpoints before Apply.",
                CKERR_INVALIDOBJECT);
        }

        CKBehaviorIO *source = nullptr;
        CKBehaviorIO *sink = nullptr;
        status = ControlPort(reconnect.Source, source);
        if (status)
            status = ControlPort(reconnect.Sink, sink);
        if (!status)
            return status;

        Patch::Journal::Reconnection change;
        change.Anchor = reconnect.Target.Anchor;
        change.Link = Capture(link);
        change.OriginalSource = Capture(link->GetInBehaviorIO());
        change.OriginalSink = Capture(link->GetOutBehaviorIO());
        change.InstalledSource = Capture(source);
        change.InstalledSink = Capture(sink);
        change.InitialDelay = link->GetInitialActivationDelay();
        change.Delay = link->GetActivationDelay();
        m_Journal.Reconnections.push_back(change);
        auto &stored = m_Journal.Reconnections.back();

        CKERROR error = CK_OK;
        if (link->GetInBehaviorIO() != source)
            error = link->SetInBehaviorIO(source);
        if (error == CK_OK && link->GetOutBehaviorIO() != sink)
            error = link->SetOutBehaviorIO(sink);
        if (error != CK_OK) {
            (void) link->SetInBehaviorIO(
                ResolveIo(m_Context, stored.OriginalSource));
            (void) link->SetOutBehaviorIO(
                ResolveIo(m_Context, stored.OriginalSink));
            return Failure(
                Error::GraphChanged,
                "Virtools rejected a reconnected Link endpoint.", error);
        }
        stored.Applied = true;
        if (link->GetInitialActivationDelay() != stored.InitialDelay ||
            link->GetActivationDelay() != stored.Delay) {
            return Failure(
                Error::GraphChanged,
                "Reconnect changed the native Link activation delay.",
                CKERR_INVALIDOBJECT);
        }
    }
    return {};
}

// Control Flow is installed last, after every Block has observed and
// validated its final parameter relations.
Status CKEdit::Transaction::Flow() {
    Status status;
    const auto infrastructure = [&](Node node) {
        const Ops::EditNode *value = m_Edit->Find(node);
        return value && value->Role == NodeRole::Infrastructure;
    };
    for (const CheckedFlow &flow : m_Checked.Flows) {
        CKBehaviorIO *source = nullptr;
        CKBehaviorIO *sink = nullptr;
        status = ControlPort(flow.Source, source);
        if (status)
            status = ControlPort(flow.Sink, sink);
        Stamp link;
        if (status)
            status = AddLink(source, sink, flow.Delay, &link);
        if (!status)
            return status;
        if (infrastructure(flow.Source.Owner) ||
            infrastructure(flow.Sink.Owner)) {
            m_Journal.InfrastructureLinks.push_back(link);
        }
    }
    return {};
}

} // namespace BML::Behavior::Internal

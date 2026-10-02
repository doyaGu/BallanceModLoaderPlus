#include "Behavior/Edit/Transaction.h"

#include "Behavior/Engine/Graph.h"
#include "Behavior/Runtime.h"

#include <algorithm>

namespace BML::Behavior::Internal {
namespace {

CKERROR RestoreStoredValue(CKContext *context, CKParameter *stored,
                           CKParameter *before) {
    auto *output = CKParameterOut::Cast(stored);
    if (!output)
        return stored->CopyValue(before, FALSE);

    // Even CKParameter::CopyValue can call a type copier that invokes virtual
    // SetValue. Isolate this Pout for the whole copy: each original destination
    // has its own journal entry and conflict check, and new destinations must
    // retain their values. Restore the current relations in their original order.
    const Stamp source = Capture(output);
    std::vector<Stamp> destinations;
    for (int index = 0; index < output->GetDestinationCount(); ++index)
        destinations.push_back(Capture(output->GetDestination(index)));
    output->RemoveAllDestinations();
    CKERROR error = output->CopyValue(before, FALSE);
    output = Resolve<CKParameterOut>(context, source, CKCID_PARAMETEROUT);
    if (!output)
        return CKERR_INVALIDOBJECT;
    for (Stamp destination : destinations) {
        auto *parameter = Resolve<CKParameter>(context, destination, CKCID_PARAMETER);
        const CKERROR restored = parameter
            ? output->AddDestination(parameter, FALSE) : CKERR_INVALIDOBJECT;
        if (error == CK_OK)
            error = restored;
    }
    return error;
}

} // namespace

Status CKEdit::Transaction::Revert() {
    m_Journal.Conflicts.clear();
    m_GraphId = m_Journal.Graph.Id
        ? static_cast<std::uint32_t>(m_Journal.Graph.Id) : 0;
    m_Graph = Resolve<CKBehavior>(m_Context, m_Journal.Graph, CKCID_BEHAVIOR);
    RetireParked();
    Status status = CheckPublished();
    if (status)
        status = RetireLayer();
    if (!status)
        return status;
    RevertPushes();
    RevertValues();
    RevertBinds();
    RevertReconnections();
    RestoreStructure();
    // Scheduler order is restored only once every Node and Link is back,
    // and again after the objects this Patch created are gone.
    if (!StructureRestored() || !RestoreOrder())
        return m_First;
    ReleaseDetached();
    RetireOperations();
    RetireLinks();
    RetirePorts();
    ReleaseClaims();
    ReleaseLiterals();
    RetireBlocks();
    RetireGraphNodes();
    m_Graph = Resolve<CKBehavior>(m_Context, m_Journal.Graph, CKCID_BEHAVIOR);
    if (!RestoreOrder())
        return m_First;
    Republish();
    NotifyBlocksClosed();
    NotifyGraphClosed();
    ForgetGraph();
    return m_First;
}

bool CKEdit::Transaction::RestoreOrder() {
    if (!m_Graph)
        return true;
    Status ordered = ArrangeOrder(
        m_Context, m_Graph, m_Journal.BeforeOrder, {}, {}, true);
    const bool arranged = static_cast<bool>(ordered);
    Remember(std::move(ordered));
    return arranged;
}

void CKEdit::Transaction::NoteConflict(RevertConflict conflict) {
    m_Journal.Conflicts.push_back(std::move(conflict));
}

void CKEdit::Transaction::Remember(Status status) {
    if (!status && m_First)
        m_First = std::move(status);
}

Status CKEdit::Transaction::Conflict(RevertSubject subject,
                                     std::string message) {
    Status conflict = Failure(Error::RevertConflict, std::move(message),
                              CKERR_INVALIDOBJECT);
    conflict.Details.Stage = Phase::Teardown;
    NoteConflict({subject, {}, {}, {}, {}, {}, conflict});
    return conflict;
}

void CKEdit::Transaction::RetireParked() {
    if (!m_Graph) {
        // RemoveSubBehavior and RemoveSubBehaviorLink remove the parked objects
        // from the former graph's owning arrays even though a Node keeps its
        // cached parent identity. If that graph is deleted while the Patch is
        // open, retire those objects explicitly; otherwise CK2 has no remaining
        // graph membership through which to delete them.
        for (Patch::Journal::RemovedLink &removed : m_Journal.RemovedLinks) {
            if (removed.Restored)
                continue;
            if (CKBehaviorLink *link = Resolve<CKBehaviorLink>(
                    m_Context, removed.Value, CKCID_BEHAVIORLINK))
                Engine::DestroyOrphanLink(m_Context, link);
            removed.Removed = false;
            removed.SourceDetached = false;
            removed.SinkDetached = false;
            removed.Restored = true;
        }
        for (Patch::Journal::Removal &removal : m_Journal.Removals) {
            if (removal.Restored)
                continue;
            if (CKBehavior *node = Resolve<CKBehavior>(
                    m_Context, removal.Node, CKCID_BEHAVIOR))
                m_Context->DestroyObject(node);
            removal.Removed = false;
            removal.Restored = true;
        }
        // Replace also parks its original Node. It obeys the same ownership
        // rule when the graph disappears before the Patch closes.
        for (Patch::Journal::Replacement &replacement : m_Journal.Replacements) {
            if (replacement.Restored)
                continue;
            if (replacement.OriginalRemoved) {
                if (CKBehavior *original = Resolve<CKBehavior>(
                        m_Context, replacement.Original, CKCID_BEHAVIOR))
                    m_Context->DestroyObject(original);
                replacement.OriginalRemoved = false;
            }
            replacement.Restored = true;
        }
        for (Patch::Journal::Reconnection &reconnection :
             m_Journal.Reconnections) {
            reconnection.Applied = false;
            reconnection.Reverted = true;
        }
    }
}

Status CKEdit::Transaction::CheckPublished() {
    if (m_Graph) {
        CKBehavior *parent = m_Graph->GetParent();
        if (!IsSameObject(m_Context, m_Graph->GetOwner(), m_Journal.GraphOwner,
                          CKCID_BEOBJECT) ||
            !IsSameObject(m_Context, parent, m_Journal.GraphParent,
                          CKCID_BEHAVIOR) ||
            (parent && !Engine::Contains(parent, m_Graph))) {
            return Conflict(
                RevertSubject::Node,
                "The edited Behavior graph changed owner or parent before the Patch closed.");
        }
    }
    if (m_Graph && m_Editor.m_StructuralEdits.contains(m_GraphId) &&
        !m_Journal.StructuralEditClaim) {
        return Conflict(
            RevertSubject::Node,
            "This Patch cannot close while the graph contains an active structural edit.");
    }
    // Closing would destroy the objects a later Patch still edits.
    if (m_Graph && !m_Journal.Defines) {
        for (const std::weak_ptr<Patch::Journal> &entry : m_Editor.m_Published) {
            const std::shared_ptr<Patch::Journal> other = entry.lock();
            if (!other || other.get() == &m_Journal ||
                other->State == PatchState::Closed ||
                other->State == PatchState::Failed)
                continue;
            const bool builds = AnyOwned(m_Journal, [&](Stamp owned) {
                return std::find(other->Named.begin(), other->Named.end(),
                                 owned) != other->Named.end();
            });
            if (builds)
                return Conflict(
                    RevertSubject::Node,
                    "This Patch cannot close while the active Patch '" +
                        other->Key.Owner + "/" + other->Key.Name +
                        "' edits an object it introduced.");
        }
    }
    if (m_Graph && m_Journal.Published) {
        RevertSubject subject = RevertSubject::Node;
        Status order = ValidateOrder(
            m_Context, m_Graph, m_Journal.AfterOrder, subject);
        if (!order) {
            NoteConflict({subject, {}, {}, {}, {}, {}, order});
            return order;
        }
    }
    if (m_Graph) {
        for (const Patch::Journal::Replacement &replacement :
             m_Journal.Replacements) {
            if (replacement.Restored)
                continue;
            CKBehavior *original = Resolve<CKBehavior>(
                m_Context, replacement.Original, CKCID_BEHAVIOR);
            CKBehavior *installed = Resolve<CKBehavior>(
                m_Context, replacement.Installed, CKCID_BEHAVIOR);
            if (!original)
                return Conflict(
                    RevertSubject::Node,
                    "The original replacement Node no longer exists.");
            if (!installed)
                return Conflict(
                    RevertSubject::Node,
                    "The installed replacement Node no longer exists.");
            if (installed->GetParent() != m_Graph ||
                !Engine::Contains(m_Graph, installed))
                return Conflict(
                    RevertSubject::Node,
                    "The installed replacement Node left its graph.");
            if (!IsSameObject(m_Context, installed->GetOwner(),
                              m_Journal.GraphOwner, CKCID_BEOBJECT))
                return Conflict(
                    RevertSubject::Node,
                    "The installed replacement Node changed owner.");
            if (!IsSameObject(m_Context, original->GetOwner(),
                              m_Journal.GraphOwner, CKCID_BEOBJECT))
                return Conflict(
                    RevertSubject::Node,
                    "The original replacement Node changed owner.");
            if (Engine::Contains(m_Graph, original) ==
                replacement.OriginalRemoved)
                return Conflict(
                    RevertSubject::Node,
                    "The original replacement Node changed graph membership.");
            for (const auto &change : replacement.Links) {
                if (!change.Applied)
                    continue;
                CKBehaviorLink *link = Resolve<CKBehaviorLink>(
                    m_Context, change.Link, CKCID_BEHAVIORLINK);
                const auto removed = std::find_if(
                    m_Journal.RemovedLinks.begin(), m_Journal.RemovedLinks.end(),
                    [&](const Patch::Journal::RemovedLink &item) {
                        return !item.Restored && item.Value == change.Link;
                    });
                const bool parked = removed != m_Journal.RemovedLinks.end();
                if (!link ||
                    (parked &&
                     (removed->Source != change.InstalledSource ||
                      removed->Sink != change.InstalledSink)) ||
                    (!parked &&
                     (Capture(link->GetInBehaviorIO()) !=
                          change.InstalledSource ||
                      Capture(link->GetOutBehaviorIO()) !=
                          change.InstalledSink))) {
                    return Conflict(
                        RevertSubject::Link,
                        "A replacement Link changed after the Patch was published.");
                }
            }
            for (const auto &change : replacement.Inputs) {
                if (!change.Applied)
                    continue;
                CKParameterIn *input = Resolve<CKParameterIn>(
                    m_Context, change.Input, CKCID_PARAMETERIN);
                CKParameterIn *shared = Resolve<CKParameterIn>(
                    m_Context, change.InstalledShared, CKCID_PARAMETERIN);
                CKParameter *direct = Resolve<CKParameter>(
                    m_Context, change.InstalledDirect, CKCID_PARAMETER);
                const bool unchanged = change.InstalledShared.Id
                    ? input && input->GetSharedSource() == shared
                    : input && input->GetSharedSource() == nullptr &&
                        input->GetDirectSource() == direct;
                if (!unchanged)
                    return Conflict(
                        RevertSubject::PinSource,
                        "A replacement Pin source changed after the Patch was published.");
            }
            for (const auto &change : replacement.Destinations) {
                if (!change.Applied)
                    continue;
                CKParameterOut *oldOutput = Resolve<CKParameterOut>(
                    m_Context, change.OriginalSource, CKCID_PARAMETEROUT);
                CKParameterOut *newOutput = Resolve<CKParameterOut>(
                    m_Context, change.InstalledSource, CKCID_PARAMETEROUT);
                CKParameter *originalDestination = Resolve<CKParameter>(
                    m_Context, change.OriginalParameter, CKCID_PARAMETER);
                CKParameter *installedDestination = Resolve<CKParameter>(
                    m_Context, change.InstalledParameter, CKCID_PARAMETER);
                if (!oldOutput || !newOutput || !originalDestination ||
                    !installedDestination ||
                    ContainsDestination(oldOutput, originalDestination) ||
                    !ContainsDestination(newOutput, installedDestination)) {
                    return Conflict(
                        RevertSubject::PinSource,
                        "A replacement Pout destination changed after the Patch was published.");
                }
            }
        }
        for (const Patch::Journal::Removal &removal : m_Journal.Removals) {
            if (removal.Restored)
                continue;
            CKBehavior *node = Resolve<CKBehavior>(
                m_Context, removal.Node, CKCID_BEHAVIOR);
            if (!node ||
                !IsSameObject(m_Context, node->GetOwner(), m_Journal.GraphOwner,
                              CKCID_BEOBJECT) ||
                Engine::Contains(m_Graph, node) == removal.Removed) {
                return Conflict(
                    RevertSubject::Node,
                    "A removed Behavior Node changed after the Patch was published.");
            }
        }
        for (const Patch::Journal::RemovedLink &removed :
             m_Journal.RemovedLinks) {
            if (removed.Restored)
                continue;
            CKBehaviorLink *link = Resolve<CKBehaviorLink>(
                m_Context, removed.Value, CKCID_BEHAVIORLINK);
            CKBehaviorIO *source = ResolveIo(
                m_Context, removed.SourceDetached
                    ? m_Journal.DetachedSource : removed.Source);
            CKBehaviorIO *sink = ResolveIo(
                m_Context, removed.SinkDetached
                    ? m_Journal.DetachedSink : removed.Sink);
            if (!link || Engine::Contains(m_Graph, link) == removed.Removed ||
                !source || !sink || link->GetInBehaviorIO() != source ||
                link->GetOutBehaviorIO() != sink ||
                link->GetInitialActivationDelay() != removed.InitialDelay ||
                link->GetActivationDelay() != removed.Delay) {
                return Conflict(
                    RevertSubject::Link,
                    "A removed Behavior Link changed after the Patch was published.");
            }
        }
        for (const Patch::Journal::Reconnection &reconnection :
             m_Journal.Reconnections) {
            if (!reconnection.Applied || reconnection.Reverted)
                continue;
            CKBehaviorLink *link = Resolve<CKBehaviorLink>(
                m_Context, reconnection.Link, CKCID_BEHAVIORLINK);
            // A reconnected Link stays in the graph and keeps running, so CK2
            // counts its current ActivationDelay down and resets it on every
            // activation. Only the initial delay is part of its identity.
            if (!link || !Engine::Contains(m_Graph, link) ||
                Capture(link->GetInBehaviorIO()) !=
                    reconnection.InstalledSource ||
                Capture(link->GetOutBehaviorIO()) !=
                    reconnection.InstalledSink ||
                link->GetInitialActivationDelay() !=
                    reconnection.InitialDelay) {
                return Conflict(
                    RevertSubject::Link,
                    "A reconnected Behavior Link changed after the Patch was published.");
            }
        }
    }
    return {};
}

Status CKEdit::Transaction::RetireLayer() {
    const auto registeredRoot = m_Editor.m_Links->Roots.find(m_GraphId);
    m_OwnsLogicalGraph =
        registeredRoot != m_Editor.m_Links->Roots.end() &&
        registeredRoot->second == m_Journal.Graph;

    if (m_OwnsLogicalGraph &&
        (!m_Journal.Layer.Links.empty() || !m_Journal.Layer.Outs.empty())) {
        const auto topology = m_Editor.m_Topology.find(m_GraphId);
        auto graphSites = m_Editor.m_Links->Sites.find(m_GraphId);
        const bool removed = topology != m_Editor.m_Topology.end() &&
                             topology->second.Remove(m_Journal.Key);
        if (m_Graph && removed) {
            Status materialized = m_Editor.Materialize(m_GraphId, m_Graph);
            if (!materialized) {
                (void) topology->second.Set(m_Journal.Layer);
                for (const LinkOverlays &group : m_Journal.Layer.Links) {
                    const LogicalLink *link = topology->second.Find(group.Link);
                    RevertConflict conflict;
                    conflict.Subject = RevertSubject::Link;
                    if (link)
                        conflict.Link = link->Base.Anchor;
                    conflict.Diagnostic = materialized;
                    NoteConflict(std::move(conflict));
                }
                // The layer is back in the Topology and every native resource
                // is still installed. Only a RevertConflict keeps the journal
                // for a later retry; any other code would let CloseNow report
                // Closed over a Patch that is still physically present.
                if (materialized.Code == Error::RevertConflict)
                    return materialized;
                Status conflict = Failure(
                    Error::RevertConflict,
                    "The spliced Links could not be restored: " +
                        materialized.Message,
                    materialized.CkError);
                conflict.Details = materialized.Details;
                return conflict;
            }
        }
        // Materialize needs both the old and desired chains to remain
        // resolvable. The retiring Patch's endpoints stop being part of the
        // graph only after the exact Link has been published successfully.
        if (graphSites != m_Editor.m_Links->Sites.end()) {
            for (const auto &[link, ordinal] : m_Journal.Splices) {
                (void) link;
                graphSites->second.erase(Links::Key{m_Journal.Key, ordinal});
            }
            for (const auto &[link, ordinal] : m_Journal.Redirects) {
                (void) link;
                graphSites->second.erase(Links::Key{m_Journal.Key, ordinal});
            }
        }
    }
    return {};
}

void CKEdit::Transaction::RevertPushes() {
    for (auto item = m_Journal.Pushes.rbegin(); item != m_Journal.Pushes.rend(); ++item) {
        auto *source = Resolve<CKParameterOut>(
            m_Context, item->Source, CKCID_PARAMETEROUT);
        auto *target = Resolve<CKParameter>(
            m_Context, item->Target, CKCID_PARAMETER);
        if (source && target && ContainsDestination(source, target))
            source->RemoveDestination(target);
    }
}

void CKEdit::Transaction::RevertValues() {
    for (auto item = m_Journal.Values.rbegin(); item != m_Journal.Values.rend(); ++item) {
        if (item->Reverted)
            continue;
        auto *stored = Resolve<CKParameter>(
            m_Context, item->Parameter, CKCID_PARAMETER);
        auto *before = Resolve<CKParameterLocal>(
            m_Context, item->Before, CKCID_PARAMETERLOCAL);
        auto *expected = Resolve<CKParameterLocal>(
            m_Context, item->Expected, CKCID_PARAMETERLOCAL);
        const auto releaseCopies = [&] {
            if (before)
                m_Context->DestroyObject(before);
            if (expected && expected != before)
                m_Context->DestroyObject(expected);
            item->Before = {};
            item->Expected = {};
        };
        if (!stored) {
            // The parameter went with its Block, so there is nothing left to
            // hand back. The journal still owns and releases both copies.
            item->Reverted = true;
            releaseCopies();
            continue;
        }
        if (!before) {
            Status conflict = Failure(
                Error::RevertConflict,
                "The previous value disappeared from the Patch journal.",
                CKERR_INVALIDOBJECT);
            conflict.Details.Stage = Phase::Teardown;
            NoteConflict({RevertSubject::Value, item->Slot, {}, {}, {}, {},
                          conflict});
            Remember(std::move(conflict));
            continue;
        }
        if (item->Expected.Id && !expected) {
            Status conflict = Failure(
                Error::RevertConflict,
                "The installed value disappeared from the Patch journal.",
                CKERR_INVALIDOBJECT);
            conflict.Details.Stage = Phase::Teardown;
            NoteConflict({RevertSubject::Value, item->Slot, {}, {}, {}, {},
                          conflict});
            Remember(std::move(conflict));
            continue;
        }
        bool unchanged = true;
        Status compared;
        if (expected)
            compared = Parameter::Equal(
                m_Context, stored, expected, unchanged);
        if (!compared) {
            Status conflict = Failure(
                Error::RevertConflict,
                "The written value could not be compared through its Virtools type.",
                compared.CkError);
            conflict.Details.Stage = Phase::Teardown;
            NoteConflict({RevertSubject::Value, item->Slot, {}, {}, {}, {},
                          conflict});
            Remember(std::move(conflict));
            continue;
        }
        if (!unchanged) {
            Status conflict = Failure(
                Error::RevertConflict,
                "A written value changed after the Patch was published.");
            conflict.Details.Stage = Phase::Teardown;
            NoteConflict({RevertSubject::Value, item->Slot, {}, {}, {}, {},
                          conflict});
            Remember(std::move(conflict));
            continue;
        }
        const CKERROR error = RestoreStoredValue(m_Context, stored, before);
        if (error != CK_OK) {
            Status conflict = Failure(
                Error::RevertConflict,
                "Virtools rejected the previous value.", error);
            conflict.Details.Stage = Phase::Teardown;
            NoteConflict({RevertSubject::Value, item->Slot, {}, {}, {}, {},
                          conflict});
            Remember(std::move(conflict));
        } else {
            item->Reverted = true;
            releaseCopies();
        }
    }
}

void CKEdit::Transaction::RevertBinds() {
    for (auto item = m_Journal.Binds.rbegin(); item != m_Journal.Binds.rend(); ++item) {
        if (item->Reverted)
            continue;
        auto *input = Resolve<CKParameterIn>(
            m_Context, item->Input, CKCID_PARAMETERIN);
        if (item->OwnedTarget) {
            if (input) {
                if (input->GetSharedSource())
                    (void) input->ShareSourceWith(nullptr);
                else
                    (void) input->SetDirectSource(nullptr);
            }
            item->Reverted = true;
            continue;
        }
        if (!input) {
            // Nothing is left to hand back, so the revert is settled even
            // though the owner still hears about the missing Pin.
            item->Reverted = true;
            if (m_Graph) {
                Status conflict = Failure(
                    Error::RevertConflict,
                    "A Pin disappeared after the Patch was published.");
                conflict.Details.Stage = Phase::Teardown;
                NoteConflict({RevertSubject::PinSource, item->Pin, {},
                              item->Before, item->Expected, {}, conflict});
                Remember(std::move(conflict));
            }
            continue;
        }
        CKParameter *installedDirect = Resolve<CKParameter>(
            m_Context, item->InstalledDirect, CKCID_PARAMETER);
        CKParameterIn *installedShared = Resolve<CKParameterIn>(
            m_Context, item->InstalledShared, CKCID_PARAMETERIN);
        const bool unchanged = item->InstalledShared.Id
            ? input->GetSharedSource() == installedShared
            : input->GetSharedSource() == nullptr &&
                input->GetDirectSource() == installedDirect;
        if (!unchanged) {
            Status conflict = Failure(
                Error::RevertConflict,
                "A Pin source changed after the Patch was published.");
            conflict.Details.Stage = Phase::Teardown;
            NoteConflict({RevertSubject::PinSource, item->Pin, {},
                          item->Before, item->Expected, DescribeSource(input),
                          conflict});
            Remember(std::move(conflict));
            continue;
        }
        CKParameterIn *previousShared = Resolve<CKParameterIn>(
            m_Context, item->PreviousShared, CKCID_PARAMETERIN);
        CKParameter *previousDirect = Resolve<CKParameter>(
            m_Context, item->PreviousDirect, CKCID_PARAMETER);
        const bool previousMissing =
            (item->PreviousShared.Id && !previousShared) ||
            (item->PreviousDirect.Id && !previousDirect);
        if (previousMissing) {
            Status conflict = Failure(
                Error::RevertConflict,
                "The previous Pin source no longer exists.");
            conflict.Details.Stage = Phase::Teardown;
            NoteConflict({RevertSubject::PinSource, item->Pin, {},
                          item->Before, item->Expected, DescribeSource(input),
                          conflict});
            Remember(std::move(conflict));
            continue;
        }
        const CKERROR error = item->PreviousShared.Id
            ? input->ShareSourceWith(previousShared)
            : input->SetDirectSource(previousDirect);
        if (error != CK_OK) {
            Status conflict = Failure(
                Error::RevertConflict,
                "Virtools rejected the previous Pin source.", error);
            conflict.Details.Stage = Phase::Teardown;
            NoteConflict({RevertSubject::PinSource, item->Pin, {},
                          item->Before, item->Expected, DescribeSource(input),
                          conflict});
            Remember(std::move(conflict));
        } else {
            item->Reverted = true;
        }
    }
}

// Put moved Links back before any Edit-owned endpoint can be removed.
// Failed restoration is rolled back to the installed endpoints so a
// conflicted Patch remains retryable at a later safe point.
void CKEdit::Transaction::RevertReconnections() {
    for (auto item = m_Journal.Reconnections.rbegin();
         item != m_Journal.Reconnections.rend(); ++item) {
        if (!item->Applied || item->Reverted)
            continue;
        m_Graph = Resolve<CKBehavior>(m_Context, m_Journal.Graph, CKCID_BEHAVIOR);
        if (!m_Graph) {
            item->Applied = false;
            item->Reverted = true;
            continue;
        }
        CKBehaviorLink *link = Resolve<CKBehaviorLink>(
            m_Context, item->Link, CKCID_BEHAVIORLINK);
        CKBehaviorIO *source = ResolveIo(m_Context, item->OriginalSource);
        CKBehaviorIO *sink = ResolveIo(m_Context, item->OriginalSink);
        CKBehaviorIO *installedSource = ResolveIo(
            m_Context, item->InstalledSource);
        CKBehaviorIO *installedSink = ResolveIo(
            m_Context, item->InstalledSink);
        CKERROR error = link && source && sink && installedSource &&
                installedSink && Engine::Contains(m_Graph, link)
            ? link->SetInBehaviorIO(source) : CKERR_INVALIDOBJECT;
        if (error == CK_OK)
            error = link->SetOutBehaviorIO(sink);
        const bool restored = error == CK_OK &&
            link->GetInBehaviorIO() == source &&
            link->GetOutBehaviorIO() == sink &&
            link->GetInitialActivationDelay() == item->InitialDelay;
        if (!restored) {
            if (link && installedSource && installedSink) {
                (void) link->SetInBehaviorIO(installedSource);
                (void) link->SetOutBehaviorIO(installedSink);
            }
            Status conflict = Conflict(
                RevertSubject::Link,
                "Virtools could not restore a reconnected Behavior Link.");
            RevertConflict detail;
            detail.Subject = RevertSubject::Link;
            detail.Link = item->Anchor;
            detail.Diagnostic = conflict;
            NoteConflict(std::move(detail));
            Remember(std::move(conflict));
            continue;
        }
        item->Applied = false;
        item->Reverted = true;
    }
}

// Restore replacement relations while the installed Block still has its
// complete public interface. Ports introduced by this Patch are removed
// only after the original Node once again owns every native relation.
void CKEdit::Transaction::RestoreStructure() {
    m_PinsRetained = std::any_of(
            m_Journal.Binds.begin(), m_Journal.Binds.end(),
            [](const Patch::Journal::Binding &item) { return !item.Reverted; }) ||
        std::any_of(
            m_Journal.Values.begin(), m_Journal.Values.end(),
            [](const Patch::Journal::Written &item) { return !item.Reverted; });
    if (!m_PinsRetained) {
        for (auto item = m_Journal.Removals.rbegin();
             item != m_Journal.Removals.rend(); ++item) {
            if (item->Restored)
                continue;
            m_Graph = Resolve<CKBehavior>(
                m_Context, m_Journal.Graph, CKCID_BEHAVIOR);
            if (!m_Graph) {
                item->Restored = true;
                continue;
            }
            CKBehavior *node = Resolve<CKBehavior>(
                m_Context, item->Node, CKCID_BEHAVIOR);
            if (!node) {
                Remember(Conflict(
                    RevertSubject::Node,
                    "The removed Behavior Node no longer exists."));
                continue;
            }
            if (item->Removed) {
                const CKERROR added = Engine::AddChild(m_Graph, node);
                if (added != CK_OK || !Engine::Contains(m_Graph, node)) {
                    Remember(Conflict(
                        RevertSubject::Node,
                        "Virtools could not restore a removed Behavior Node."));
                    continue;
                }
                item->Removed = false;
            }
            item->Restored = true;
        }

        const bool removalNodesRestored = std::all_of(
            m_Journal.Removals.begin(), m_Journal.Removals.end(),
            [](const Patch::Journal::Removal &item) {
                return item.Restored;
            });
        if (removalNodesRestored) {
            for (auto item = m_Journal.RemovedLinks.rbegin();
                 item != m_Journal.RemovedLinks.rend(); ++item) {
                if (item->Restored)
                    continue;
                m_Graph = Resolve<CKBehavior>(
                    m_Context, m_Journal.Graph, CKCID_BEHAVIOR);
                if (!m_Graph) {
                    item->Restored = true;
                    continue;
                }
                CKBehaviorLink *link = Resolve<CKBehaviorLink>(
                    m_Context, item->Value, CKCID_BEHAVIORLINK);
                CKBehaviorIO *source = ResolveIo(m_Context, item->Source);
                CKBehaviorIO *sink = ResolveIo(m_Context, item->Sink);
                if (!link || !source || !sink) {
                    Remember(Conflict(
                        RevertSubject::Link,
                        "A removed Behavior Link cannot be restored."));
                    continue;
                }
                if (item->SourceDetached) {
                    const CKERROR restored = link->SetInBehaviorIO(source);
                    if (restored != CK_OK) {
                        Remember(Conflict(
                            RevertSubject::Link,
                            "Virtools could not restore a Behavior Link source."));
                        continue;
                    }
                    item->SourceDetached = false;
                }
                if (item->SinkDetached) {
                    const CKERROR restored = link->SetOutBehaviorIO(sink);
                    if (restored != CK_OK) {
                        Remember(Conflict(
                            RevertSubject::Link,
                            "Virtools could not restore a Behavior Link sink."));
                        continue;
                    }
                    item->SinkDetached = false;
                }
                if (link->GetInBehaviorIO() != source ||
                    link->GetOutBehaviorIO() != sink) {
                    Remember(Conflict(
                        RevertSubject::Link,
                        "A removed Behavior Link cannot be restored."));
                    continue;
                }
                if (item->Removed) {
                    link->SetInitialActivationDelay(item->InitialDelay);
                    link->SetActivationDelay(item->Delay);
                    const CKERROR added = m_Graph->AddSubBehaviorLink(link);
                    if (added != CK_OK || !Engine::Contains(m_Graph, link)) {
                        Remember(Conflict(
                            RevertSubject::Link,
                            "Virtools could not restore a removed Behavior Link."));
                        continue;
                    }
                    item->Removed = false;
                }
                item->Restored = true;
            }
        }

        for (auto item = m_Journal.Replacements.rbegin();
             item != m_Journal.Replacements.rend(); ++item) {
            if (item->Restored)
                continue;
            m_Graph = Resolve<CKBehavior>(
                m_Context, m_Journal.Graph, CKCID_BEHAVIOR);
            if (!m_Graph) {
                item->Restored = true;
                continue;
            }
            CKBehavior *original = Resolve<CKBehavior>(
                m_Context, item->Original, CKCID_BEHAVIOR);
            if (!original) {
                Remember(Conflict(
                    RevertSubject::Node,
                    "The original replacement Node no longer exists."));
                continue;
            }
            if (item->OriginalRemoved) {
                const CKERROR added = Engine::AddChild(m_Graph, original);
                if (added != CK_OK) {
                    Remember(Conflict(
                        RevertSubject::Node,
                        "Virtools could not restore the original Behavior Node."));
                    continue;
                }
                item->OriginalRemoved = false;
            }

            bool restored = true;
            for (auto change = item->Links.rbegin();
                 change != item->Links.rend(); ++change) {
                if (!change->Applied)
                    continue;
                CKBehaviorLink *link = Resolve<CKBehaviorLink>(
                    m_Context, change->Link, CKCID_BEHAVIORLINK);
                CKBehaviorIO *source = ResolveIo(
                    m_Context, change->OriginalSource);
                CKBehaviorIO *sink = ResolveIo(
                    m_Context, change->OriginalSink);
                CKERROR error = link && source && sink
                    ? link->SetInBehaviorIO(source) : CKERR_INVALIDOBJECT;
                if (error == CK_OK)
                    error = link->SetOutBehaviorIO(sink);
                if (error != CK_OK) {
                    Remember(Conflict(
                        RevertSubject::Link,
                        "Virtools could not restore a replacement Link."));
                    restored = false;
                    break;
                }
                change->Applied = false;
            }
            if (!restored)
                continue;

            for (auto change = item->Inputs.rbegin();
                 change != item->Inputs.rend(); ++change) {
                if (!change->Applied)
                    continue;
                CKParameterIn *input = Resolve<CKParameterIn>(
                    m_Context, change->Input, CKCID_PARAMETERIN);
                CKERROR error = CKERR_INVALIDOBJECT;
                if (input && change->OriginalShared.Id) {
                    error = input->ShareSourceWith(Resolve<CKParameterIn>(
                        m_Context, change->OriginalShared,
                        CKCID_PARAMETERIN));
                } else if (input) {
                    error = input->SetDirectSource(Resolve<CKParameter>(
                        m_Context, change->OriginalDirect,
                        CKCID_PARAMETER));
                }
                if (error != CK_OK) {
                    Remember(Conflict(
                        RevertSubject::PinSource,
                        "Virtools could not restore a replacement Pin source."));
                    restored = false;
                    break;
                }
                change->Applied = false;
            }
            if (!restored)
                continue;

            // AddDestination appends, so the original destinations return in
            // their recorded order. DataChanged pushes follow that order.
            for (auto change = item->Destinations.begin();
                 change != item->Destinations.end(); ++change) {
                if (!change->Applied)
                    continue;
                CKParameterOut *oldOutput = Resolve<CKParameterOut>(
                    m_Context, change->OriginalSource,
                    CKCID_PARAMETEROUT);
                CKParameterOut *newOutput = Resolve<CKParameterOut>(
                    m_Context, change->InstalledSource,
                    CKCID_PARAMETEROUT);
                CKParameter *originalDestination = Resolve<CKParameter>(
                    m_Context, change->OriginalParameter,
                    CKCID_PARAMETER);
                CKParameter *installedDestination = Resolve<CKParameter>(
                    m_Context, change->InstalledParameter,
                    CKCID_PARAMETER);
                const CKERROR error = oldOutput && newOutput &&
                        originalDestination && installedDestination
                    ? oldOutput->AddDestination(originalDestination, TRUE)
                    : CKERR_INVALIDOBJECT;
                if (error != CK_OK) {
                    Remember(Conflict(
                        RevertSubject::PinSource,
                        "Virtools could not restore a replacement Pout destination."));
                    restored = false;
                    break;
                }
                newOutput->RemoveDestination(installedDestination);
                change->Applied = false;
            }
            if (restored)
                item->Restored = true;
        }
    }
}

bool CKEdit::Transaction::StructureRestored() {
    const bool replacementsRestored = std::all_of(
        m_Journal.Replacements.begin(), m_Journal.Replacements.end(),
        [](const Patch::Journal::Replacement &item) {
            return item.Restored;
        });
    const bool removalsRestored = std::all_of(
            m_Journal.Removals.begin(), m_Journal.Removals.end(),
            [](const Patch::Journal::Removal &item) {
                return item.Restored;
            }) &&
        std::all_of(
            m_Journal.RemovedLinks.begin(), m_Journal.RemovedLinks.end(),
            [](const Patch::Journal::RemovedLink &item) {
                return item.Restored;
            });
    const bool reconnectionsRestored = std::all_of(
        m_Journal.Reconnections.begin(), m_Journal.Reconnections.end(),
        [](const Patch::Journal::Reconnection &item) {
            return item.Reverted || !item.Applied;
        });
    return replacementsRestored && removalsRestored &&
        reconnectionsRestored;
}

void CKEdit::Transaction::ReleaseDetached() {
    if (m_Journal.DetachedSource.Id) {
        if (CKBehaviorIO *source = ResolveIo(
                m_Context, m_Journal.DetachedSource))
            m_Context->DestroyObject(source);
        m_Journal.DetachedSource = {};
    }
    if (m_Journal.DetachedSink.Id) {
        if (CKBehaviorIO *sink = ResolveIo(m_Context, m_Journal.DetachedSink))
            m_Context->DestroyObject(sink);
        m_Journal.DetachedSink = {};
    }
    if (m_Journal.StructuralEditClaim) {
        m_Editor.m_StructuralEdits.erase(m_GraphId);
        m_Journal.StructuralEditClaim = false;
    }
}

void CKEdit::Transaction::RetireOperations() {
    for (auto item = m_Journal.Operations.rbegin();
         item != m_Journal.Operations.rend(); ++item) {
        auto *operation = Resolve<CKParameterOperation>(
            m_Context, item->Value, CKCID_PARAMETEROPERATION);
        if (!operation)
            continue;
        const Stamp result = Capture(operation->GetOutParameter());
        const bool retained = std::any_of(
            m_Journal.Binds.begin(), m_Journal.Binds.end(),
            [&](const Patch::Journal::Binding &binding) {
                return !binding.Reverted && binding.InstalledDirect == result;
            });
        if (retained)
            continue;
        auto *owner = Resolve<CKBehavior>(
            m_Context, item->Owner, CKCID_BEHAVIOR);
        if (owner)
            (void) owner->RemoveParameterOperation(operation);
        m_Context->DestroyObject(operation);
        item->Value = {};
    }
}

void CKEdit::Transaction::RetireLinks() {
    for (auto item = m_Journal.Links.rbegin(); item != m_Journal.Links.rend(); ++item) {
        CKBehaviorLink *link = Resolve<CKBehaviorLink>(
            m_Context, item->Value, CKCID_BEHAVIORLINK);
        if (!link)
            continue;
        if (m_Graph) {
            m_Graph->RemoveSubBehaviorLink(link);
            m_Context->DestroyObject(link);
        } else {
            Engine::DestroyOrphanLink(m_Context, link);
        }
    }
}

namespace {

// The class each Port kind is cast to below. A CK_ID reused by an object of
// another class must not reach that cast.
CK_CLASSID PortClass(SlotKind kind) noexcept {
    switch (kind) {
    case SlotKind::Input:
    case SlotKind::Output:
        return CKCID_BEHAVIORIO;
    case SlotKind::InputParameter:
        return CKCID_PARAMETERIN;
    case SlotKind::OutputParameter:
        return CKCID_PARAMETEROUT;
    case SlotKind::Local:
        return CKCID_PARAMETERLOCAL;
    default:
        return CKCID_OBJECT;
    }
}

} // namespace

void CKEdit::Transaction::RetirePorts() {
    for (auto item = m_Journal.Ports.rbegin(); item != m_Journal.Ports.rend(); ++item) {
        CKBehavior *behavior = Resolve<CKBehavior>(
            m_Context, item->Behavior, CKCID_BEHAVIOR);
        CKObject *port = Resolve<CKObject>(
            m_Context, item->Port, PortClass(item->Kind));
        if (!behavior || !port)
            continue;
        CKObject *removed = nullptr;
        switch (item->Kind) {
        // A Link something outside the Patch made to the IO would stay in
        // the graph with a cleared end.
        case SlotKind::Input: {
            auto *io = static_cast<CKBehaviorIO *>(port);
            const int index = behavior->GetInputPosition(io);
            if (index >= 0)
                Engine::DestroyConnectedLinks(m_Context, io);
            removed = index >= 0 ? behavior->RemoveInput(index) : nullptr;
            break;
        }
        case SlotKind::Output: {
            auto *io = static_cast<CKBehaviorIO *>(port);
            const int index = behavior->GetOutputPosition(io);
            if (index >= 0)
                Engine::DestroyConnectedLinks(m_Context, io);
            removed = index >= 0 ? behavior->RemoveOutput(index) : nullptr;
            break;
        }
        case SlotKind::InputParameter: {
            auto *parameter = static_cast<CKParameterIn *>(port);
            const int index = behavior->GetInputParameterPosition(parameter);
            removed = index >= 0 ? behavior->RemoveInputParameter(index) : nullptr;
            break;
        }
        case SlotKind::OutputParameter: {
            auto *parameter = static_cast<CKParameterOut *>(port);
            const int index = behavior->GetOutputParameterPosition(parameter);
            removed = index >= 0 ? behavior->RemoveOutputParameter(index) : nullptr;
            break;
        }
        case SlotKind::Local: {
            auto *parameter = static_cast<CKParameterLocal *>(port);
            const int index = behavior->GetLocalParameterPosition(parameter);
            removed = index >= 0 ? behavior->RemoveLocalParameter(index) : nullptr;
            break;
        }
        default:
            break;
        }
        if (removed == port) {
            m_Context->DestroyObject(removed);
        }
    }

    // A Pin removed with the ports above has nothing left to hand back.
    for (Patch::Journal::Binding &item : m_Journal.Binds) {
        if (!item.Reverted &&
            !Resolve<CKParameterIn>(m_Context, item.Input, CKCID_PARAMETERIN))
            item.Reverted = true;
    }
}

// A Pin this Patch could not hand back still points at a source the Patch
// owns. Until that clears, the Patch keeps its relation claim so no other
// Patch can take the Pin, and keeps its key in the graph's active set so
// the same key cannot be applied on top of the unfinished teardown.
void CKEdit::Transaction::ReleaseClaims() {
    if (m_OwnsLogicalGraph && !m_PinsRetained) {
        if (!m_Journal.Data.Pins.empty()) {
            const auto relations = m_Editor.m_Relations.find(m_GraphId);
            if (relations != m_Editor.m_Relations.end())
                (void) relations->second.Remove(m_Journal.Key);
        }
        const auto active = m_Editor.m_Active.find(m_GraphId);
        if (active != m_Editor.m_Active.end())
            active->second.erase(m_Journal.Key);
    }
}

void CKEdit::Transaction::ReleaseLiterals() {
    for (const Patch::Journal::Binding &item : m_Journal.Binds) {
        CKParameterLocal *literal = Resolve<CKParameterLocal>(
            m_Context, item.Literal, CKCID_PARAMETERLOCAL);
        CKParameterIn *input = Resolve<CKParameterIn>(
            m_Context, item.Input, CKCID_PARAMETERIN);
        // An unreverted Bind may still read this value, so the literal outlives
        // the failed teardown and is destroyed on a later close.
        if (literal && item.Reverted &&
            (!input || input->GetDirectSource() != literal))
            m_Context->DestroyObject(literal);
    }
}

// Restore the graph before retiring Blocks introduced by this Patch. A
// Block can still inspect its parent during native teardown, but no graph
// relation may continue to depend on a Block once it is destroyed.
void CKEdit::Transaction::RetireBlocks() {
    for (auto item = m_Journal.Nodes.rbegin(); item != m_Journal.Nodes.rend(); ++item) {
        CKBehavior *node = Resolve<CKBehavior>(
            m_Context, *item, CKCID_BEHAVIOR);
        if (!node)
            continue;
        Status closed = m_Runtime.Close(node);
        if (closed)
            *item = {};
        Remember(std::move(closed));
    }
}

// Plain graph Nodes have no Runtime lifecycle. At this point every Link,
// parameter relation, operation, and interface element introduced by the
// Patch is already gone. Remove parent membership before native
// destruction so no scheduler-visible relation retains the Node.
void CKEdit::Transaction::RetireGraphNodes() {
    for (auto item = m_Journal.GraphNodes.rbegin();
         item != m_Journal.GraphNodes.rend(); ++item) {
        CKBehavior *node = Resolve<CKBehavior>(
            m_Context, *item, CKCID_BEHAVIOR);
        if (!node) {
            *item = {};
            continue;
        }
        CKBehavior *parent = node->GetParent();
        // A link something outside the Patch made to the Node would keep
        // pointing at IOs CK2 frees with it, since CKBehaviorIO::PreDelete
        // leaves links alone when its owner is being deleted.
        if (parent)
            Engine::DestroyConnectedLinks(m_Context, parent, node);
        if (parent && parent->RemoveSubBehavior(node) != node) {
            Remember(Failure(
                Error::RevertConflict,
                "An Edit-owned graph Node could not leave its parent.",
                CKERR_INVALIDOBJECT));
            continue;
        }
        if (m_Context->DestroyObject(node) != CK_OK) {
            Remember(Failure(
                Error::RevertConflict,
                "Virtools could not destroy an Edit-owned graph Node.",
                CKERR_INVALIDOBJECT));
            continue;
        }
        *item = {};
    }
}

void CKEdit::Transaction::Republish() {
    if (m_OwnsLogicalGraph) {
        const auto infrastructure = m_Editor.m_Links->Patches.find(m_GraphId);
        if (infrastructure != m_Editor.m_Links->Patches.end())
            infrastructure->second.erase(m_Journal.Key);
        Remember(m_Editor.PublishLogicalGraph(m_GraphId));
    }
}

// Closing an authored Block runs native teardown callbacks, and publishing
// the Logical view can invoke author observers. Either may delete or
// replace the graph, so no graph pointer from before those calls survives
// this boundary.
void CKEdit::Transaction::NotifyBlocksClosed() {
    m_Graph = Resolve<CKBehavior>(m_Context, m_Journal.Graph, CKCID_BEHAVIOR);

    // Only now is the previous graph observable again: data relations and
    // interface changes are gone, added Blocks have retired, and the Logical
    // view has been republished. Notify only Blocks that actually observed the
    // candidate state when Apply failed before publication.
    const std::vector<Stamp> &observedBlocks = m_Journal.Published
        ? m_Journal.EditedNodes : m_Journal.ObservedEditedNodes;
    if (m_Graph && !m_PinsRetained && !m_Journal.RestoredEdited &&
        !observedBlocks.empty()) {
        for (Stamp edited : observedBlocks) {
            m_Graph = Resolve<CKBehavior>(
                m_Context, m_Journal.Graph, CKCID_BEHAVIOR);
            if (!m_Graph) {
                Remember(Failure(
                    Error::GraphChanged,
                    "The graph disappeared before its Blocks observed Patch teardown.",
                    CKERR_INVALIDOBJECT));
                break;
            }
            CKBehavior *block = Resolve<CKBehavior>(
                m_Context, edited, CKCID_BEHAVIOR);
            if (!block)
                continue;
            const int result = m_Runtime.NotifyEdited(block);
            if (result != CK_OK) {
                Remember(Failure(
                    Error::CallbackFailed,
                    "A Block EDITED callback failed while the Patch closed.",
                    result));
                continue;
            }
            m_Graph = Resolve<CKBehavior>(
                m_Context, m_Journal.Graph, CKCID_BEHAVIOR);
            block = Resolve<CKBehavior>(m_Context, edited, CKCID_BEHAVIOR);
            if (!m_Graph || !block || block->GetParent() != m_Graph ||
                !Engine::Contains(m_Graph, block) ||
                !IsSameObject(m_Context, block->GetOwner(), m_Journal.GraphOwner,
                              CKCID_BEOBJECT)) {
                Remember(Failure(
                    Error::GraphChanged,
                    "A Block or its graph changed owner, parent, or identity during its teardown EDITED callback.",
                    CKERR_INVALIDOBJECT));
                continue;
            }
            (void) m_Runtime.Describe(block);
        }
        m_Journal.RestoredEdited = true;
    }
}

void CKEdit::Transaction::NotifyGraphClosed() {
    m_Graph = Resolve<CKBehavior>(m_Context, m_Journal.Graph, CKCID_BEHAVIOR);
    if (m_Graph && !m_PinsRetained && !m_Journal.RestoredGraph &&
        (m_Journal.Published || m_Journal.GraphObserved)) {
        const int result = m_Runtime.NotifyEdited(m_Graph);
        if (result != CK_OK)
            Remember(Failure(Error::CallbackFailed,
                             "The graph EDITED callback failed while the Patch closed.",
                             result));
        else {
            CKBehavior *current = Resolve<CKBehavior>(
                m_Context, m_Journal.Graph, CKCID_BEHAVIOR);
            CKBehavior *parent = current ? current->GetParent() : nullptr;
            if (!current ||
                !IsSameObject(m_Context, current->GetOwner(), m_Journal.GraphOwner,
                              CKCID_BEOBJECT) ||
                !IsSameObject(m_Context, parent, m_Journal.GraphParent,
                              CKCID_BEHAVIOR) ||
                (parent && !Engine::Contains(parent, current))) {
                Remember(Failure(
                    Error::GraphChanged,
                    "The graph changed owner, parent, or identity during its teardown EDITED callback.",
                    CKERR_INVALIDOBJECT));
            }
        }
        m_Journal.RestoredGraph = true;
    }
}

void CKEdit::Transaction::ForgetGraph() {
    if (m_OwnsLogicalGraph && !m_PinsRetained) {
        const auto active = m_Editor.m_Active.find(m_GraphId);
        if (active == m_Editor.m_Active.end() || active->second.empty()) {
            if (active != m_Editor.m_Active.end())
                m_Editor.m_Active.erase(active);
            // No Patch owns a logical projection now. Retaining the last
            // published Link inventory would make a later native structural
            // edit look like an out-of-band graph change.
            m_Source.SetLogicalGraph(Native(m_Journal.Graph), {});
            m_Editor.m_Topology.erase(m_GraphId);
            m_Editor.m_Relations.erase(m_GraphId);
            if (m_Editor.m_Links) {
                m_Editor.m_Links->Chains.erase(m_GraphId);
                m_Editor.m_Links->Sites.erase(m_GraphId);
                m_Editor.m_Links->Patches.erase(m_GraphId);
                m_Editor.m_Links->Roots.erase(m_GraphId);
            }
        }
    }
}

} // namespace BML::Behavior::Internal

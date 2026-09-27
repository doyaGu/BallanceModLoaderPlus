#include "Behavior/Edit/Transaction.h"

#include "Behavior/Blocks/HookBlock.h"
#include "Behavior/Core/Hash.h"
#include "Behavior/Runtime.h"

#include <algorithm>
#include <iterator>
#include <optional>

namespace BML::Behavior::Internal {
namespace {

std::uint64_t HashTap(const GraphEndpoint &source, std::uint32_t ordinal) {
    std::uint64_t hash = Fnv::Offset;
    const auto add = [&](std::uint64_t value) { Fnv::Value(hash, value); };
    add(source.Node);
    add(static_cast<std::uint64_t>(source.Kind));
    add(static_cast<std::uint32_t>(source.Index));
    add(ordinal);
    return hash;
}

} // namespace

void CKEdit::AdoptGraph(CKBehavior *graph) {
    if (!graph || !m_Links)
        return;
    const std::uint64_t graphId = static_cast<std::uint32_t>(graph->GetID());
    const Stamp current = Capture(graph);
    const auto known = m_Links->Roots.find(graphId);
    if (known != m_Links->Roots.end() && known->second != current) {
        m_Graph.SetLogicalGraph(Native(known->second), {});
        m_Topology.erase(graphId);
        m_Relations.erase(graphId);
        m_Active.erase(graphId);
        m_LostOverlays.erase(graphId);
        m_StructuralEdits.erase(graphId);
        m_Links->Chains.erase(graphId);
        m_Links->Sites.erase(graphId);
        m_Links->Patches.erase(graphId);
    }
    m_Links->Roots[graphId] = current;
}

Status CKEdit::PublishLogicalGraph(std::uint64_t graphId) {
    if (!m_Links)
        return Failure(Error::InvalidState,
                       "The graph Link registry is unavailable.");
    const auto root = m_Links->Roots.find(graphId);
    if (root == m_Links->Roots.end())
        return Failure(Error::InvalidGraphLocality,
                       "The logical graph root is unavailable.");
    if (!Resolve<CKBehavior>(m_Context, root->second, CKCID_BEHAVIOR)) {
        m_Graph.SetLogicalGraph(Native(root->second), {});
        return {};
    }

    LogicalGraph logical;
    const auto patches = m_Links->Patches.find(graphId);
    if (patches != m_Links->Patches.end()) {
        for (const auto &[key, infrastructure] : patches->second) {
            (void) key;
            for (Stamp node : infrastructure.Nodes)
                logical.InfrastructureNodes.push_back(Native(node));
            for (Stamp linkStamp : infrastructure.Links) {
                CKBehaviorLink *link = Resolve<CKBehaviorLink>(
                    m_Context, linkStamp, CKCID_BEHAVIORLINK);
                GraphLinkShape live;
                if (!link || !Describe(link, live))
                    return Failure(Error::GraphChanged,
                                   "A Tap infrastructure Link disappeared.");
                logical.Links.push_back({Native(linkStamp), live, std::nullopt});
            }
        }
    }

    const auto topology = m_Topology.find(graphId);
    const auto chains = m_Links->Chains.find(graphId);
    if (topology != m_Topology.end() && chains != m_Links->Chains.end()) {
        for (const auto &[id, chain] : chains->second) {
            if (chain.Order.empty() && !chain.Redirected)
                continue;
            const LogicalLink *link = topology->second.Find(id);
            CKBehaviorLink *anchor = Resolve<CKBehaviorLink>(
                m_Context, chain.Anchor, CKCID_BEHAVIORLINK);
            GraphLinkShape live;
            if (!link || !anchor || !Describe(anchor, live))
                return Failure(Error::GraphChanged,
                               "A Splice anchor disappeared.");
            // A Splice is infrastructure, so the Logical view keeps reporting
            // the original destination. A Redirect is a deliberate change of
            // destination, so the Logical view reports the new one.
            logical.Links.push_back(
                {Native(chain.Anchor), live,
                 GraphLinkShape{link->Base.Source, EffectiveSink(*link),
                                link->Base.Delay}});
            for (Stamp continuation : chain.Continuations) {
                CKBehaviorLink *native = Resolve<CKBehaviorLink>(
                    m_Context, continuation, CKCID_BEHAVIORLINK);
                if (!native || !Describe(native, live))
                    return Failure(Error::GraphChanged,
                                   "A Splice continuation disappeared.");
                logical.Links.push_back(
                    {Native(continuation), live, std::nullopt});
            }
        }
    }
    m_Graph.SetLogicalGraph(Native(root->second), std::move(logical));
    return {};
}

void CKEdit::ForgetUnusedLinks(std::uint64_t graphId) {
    const auto topology = m_Topology.find(graphId);
    if (topology == m_Topology.end() || !m_Links)
        return;
    const auto chains = m_Links->Chains.find(graphId);
    for (LinkId id : topology->second.Unused()) {
        Links::Chain *chain = nullptr;
        if (chains != m_Links->Chains.end()) {
            const auto found = chains->second.find(id);
            if (found != chains->second.end())
                chain = &found->second;
        }
        // Splice nodes or a Redirect still on the native Link come off only
        // through Materialize, which needs the chain for that.
        if (chain && (!chain->Order.empty() || chain->Redirected))
            continue;
        if (topology->second.Forget(id) && chain)
            chains->second.erase(id);
    }
}

Status CKEdit::Materialize(std::uint64_t graphId, CKBehavior *graph) {
    if (!graph || !m_Links)
        return Failure(Error::InvalidGraphLocality,
                       "The graph for Link materialization is unavailable.");
    ForgetUnusedLinks(graphId);
    const auto topology = m_Topology.find(graphId);
    const auto chains = m_Links->Chains.find(graphId);
    if (topology == m_Topology.end() || chains == m_Links->Chains.end())
        return {};
    auto &sites = m_Links->Sites[graphId];

    struct Prepared {
        Links::Chain *Chain = nullptr;
        CKBehaviorLink *Anchor = nullptr;
        CKBehaviorIO *OldHead = nullptr;
        CKBehaviorIO *NewHead = nullptr;
        CKBehaviorIO *Terminal = nullptr;
        bool Redirected = false;
        std::vector<Links::Key> Order;
        std::vector<Stamp> Continuations;
    };
    std::vector<Prepared> prepared;
    const auto siteInput = [&](const Links::Key &key) -> CKBehaviorIO * {
        const auto found = sites.find(key);
        return found == sites.end()
            ? nullptr : ResolveIo(m_Context, found->second.Input);
    };
    const auto lostOverlay = [&](const Links::Key &key) {
        const auto found = m_LostOverlays.find(graphId);
        return found != m_LostOverlays.end() &&
            found->second.contains({key.Patch, key.Ordinal});
    };
    const auto lostEndpoint = [&](Stamp endpoint) {
        const auto lost = m_LostOverlays.find(graphId);
        if (lost == m_LostOverlays.end())
            return false;
        return std::any_of(
            lost->second.begin(), lost->second.end(),
            [&](const auto &key) {
                const auto site = sites.find({key.first, key.second});
                return site != sites.end() &&
                    (site->second.Input == endpoint ||
                     site->second.Output == endpoint);
            });
    };

    const auto discard = [&] {
        for (Prepared &change : prepared) {
            for (Stamp stamp : change.Continuations) {
                CKBehaviorLink *link = Resolve<CKBehaviorLink>(
                    m_Context, stamp, CKCID_BEHAVIORLINK);
                if (!link)
                    continue;
                graph->RemoveSubBehaviorLink(link);
                m_Context->DestroyObject(link);
            }
            change.Continuations.clear();
        }
    };

    for (auto &[id, chain] : chains->second) {
        const LogicalLink *logical = topology->second.Find(id);
        if (!logical)
            return Failure(Error::GraphChanged,
                           "A materialized Link lost its logical identity.");
        std::vector<Links::Key> order;
        bool redirected = false;
        Links::Key redirect;
        for (const OrderedOverlay &overlay : logical->Overlays) {
            if (overlay.Kind == OverlayKind::Splice) {
                order.push_back({overlay.Patch, overlay.Ordinal});
            } else if (overlay.Kind == OverlayKind::Redirect) {
                redirected = true;
                redirect = {overlay.Patch, overlay.Ordinal};
            }
        }

        auto *anchor = Resolve<CKBehaviorLink>(
            m_Context, chain.Anchor, CKCID_BEHAVIORLINK);
        CKBehaviorIO *source = ResolveIo(m_Context, chain.Source);
        CKBehaviorIO *sink = ResolveIo(m_Context, chain.Terminal);
        CKBehaviorIO *desired = redirected
            ? siteInput(redirect) : ResolveIo(m_Context, chain.Sink);
        if (!desired) {
            discard();
            return Failure(Error::GraphChanged,
                           "A Link destination disappeared before publication.");
        }
        if (order == chain.Order && desired == sink)
            continue;
        CKBehaviorIO *oldHead = chain.Order.empty()
            ? sink : siteInput(chain.Order.front());
        const bool lostHead = chain.Order.empty()
            ? lostEndpoint(chain.Terminal)
            : lostOverlay(chain.Order.front());
        if (!oldHead && !lostHead && !chain.Order.empty()) {
            const Links::Key &head = chain.Order.front();
            const auto site = sites.find(head);
            discard();
            std::string message = "The first Splice input for '" +
                head.Patch.Owner + "/" + head.Patch.Name + "' (action " +
                std::to_string(head.Ordinal) + ") ";
            if (site == sites.end()) {
                message += "is missing from the graph registry.";
            } else {
                message +=
                    "was destroyed while its overlay remained active (object " +
                    std::to_string(site->second.Input.Id) + ").";
            }
            return Failure(Error::RevertConflict, std::move(message));
        }
        if (!anchor || !source || (!sink && !lostHead) ||
            (!oldHead && !lostHead) || !desired) {
            discard();
            return Failure(Error::RevertConflict,
                           !anchor ? "A Splice anchor disappeared."
                           : !source ? "A Splice source disappeared."
                           : !sink ? "A Splice destination disappeared."
                           : !oldHead ? "A Splice input disappeared."
                           : "A redirected Link destination disappeared.");
        }
        if (anchor->GetInBehaviorIO() != source) {
            discard();
            return Failure(Error::RevertConflict,
                           "A Splice anchor changed its source.");
        }
        CKBehaviorIO *nativeHead = anchor->GetOutBehaviorIO();
        const auto headSite = chain.Order.empty()
            ? sites.end() : sites.find(chain.Order.front());
        CKObject *recordedHead = chain.Order.empty()
            ? chain.Terminal.Address
            : headSite == sites.end() ? nullptr
                                      : headSite->second.Input.Address;
        if ((!lostHead && nativeHead != oldHead) ||
            (lostHead && nativeHead && nativeHead != recordedHead)) {
            discard();
            return Failure(Error::RevertConflict,
                           "A Splice anchor changed its destination.");
        }
        if (anchor->GetInitialActivationDelay() != chain.Base.Delay) {
            discard();
            return Failure(Error::RevertConflict,
                           "A Splice anchor changed its initial delay.");
        }
        if (chain.Continuations.size() != chain.Order.size()) {
            discard();
            return Failure(Error::RevertConflict,
                           "A Splice chain lost a continuation Link.");
        }
        for (std::size_t index = 0; index < chain.Order.size(); ++index) {
            const auto site = sites.find(chain.Order[index]);
            CKBehaviorLink *link = Resolve<CKBehaviorLink>(
                m_Context, chain.Continuations[index], CKCID_BEHAVIORLINK);
            CKBehaviorIO *expectedSource = site == sites.end()
                ? nullptr : ResolveIo(m_Context, site->second.Output);
            CKBehaviorIO *expectedSink = index + 1 < chain.Order.size()
                ? siteInput(chain.Order[index + 1])
                : sink;
            const bool lostSegment = lostOverlay(chain.Order[index]) ||
                (index + 1 < chain.Order.size() &&
                 lostOverlay(chain.Order[index + 1])) ||
                (index + 1 == chain.Order.size() &&
                 lostEndpoint(chain.Terminal));
            if (lostSegment)
                continue;
            if (!link || !expectedSource || !expectedSink ||
                link->GetInBehaviorIO() != expectedSource ||
                link->GetOutBehaviorIO() != expectedSink ||
                link->GetInitialActivationDelay() != 0) {
                discard();
                return Failure(Error::RevertConflict,
                               "A physical Splice chain changed outside its Patch.");
            }
        }

        Prepared change;
        change.Chain = &chain;
        change.Anchor = anchor;
        change.OldHead = nativeHead;
        change.Order = order;
        change.Terminal = desired;
        change.Redirected = redirected;
        change.NewHead = order.empty()
            ? desired : siteInput(order.front());
        if (!change.NewHead) {
            discard();
            return Failure(Error::GraphChanged,
                           "A Splice node disappeared before publication.");
        }

        prepared.push_back(std::move(change));
        Prepared &pending = prepared.back();
        for (std::size_t index = 0; index < order.size(); ++index) {
            const auto site = sites.find(order[index]);
            CKBehaviorIO *from = site == sites.end()
                ? nullptr : ResolveIo(m_Context, site->second.Output);
            CKBehaviorIO *to = index + 1 < order.size()
                ? siteInput(order[index + 1])
                : desired;
            if (!from || !to) {
                discard();
                return Failure(Error::GraphChanged,
                               "A Splice endpoint disappeared before publication.");
            }
            auto *link = static_cast<CKBehaviorLink *>(m_Context->CreateObject(
                CKCID_BEHAVIORLINK, nullptr, CK_OBJECTCREATION_DYNAMIC));
            if (!link) {
                discard();
                return Failure(Error::CreateFailed,
                               "Virtools failed to create a Splice continuation.",
                               CKERR_OUTOFMEMORY);
            }
            CKERROR error = link->SetInBehaviorIO(from);
            if (error == CK_OK)
                error = link->SetOutBehaviorIO(to);
            if (error == CK_OK) {
                link->SetInitialActivationDelay(0);
                link->SetActivationDelay(0);
                error = graph->AddSubBehaviorLink(link);
            }
            if (error != CK_OK) {
                m_Context->DestroyObject(link);
                discard();
                return Failure(Error::GraphChanged,
                               "Virtools rejected a Splice continuation.", error);
            }
            pending.Continuations.push_back(Capture(link));
        }
    }

    std::size_t rewired = 0;
    for (; rewired < prepared.size(); ++rewired) {
        const CKERROR error =
            prepared[rewired].Anchor->SetOutBehaviorIO(prepared[rewired].NewHead);
        if (error == CK_OK)
            continue;
        while (rewired > 0) {
            --rewired;
            (void) prepared[rewired].Anchor->SetOutBehaviorIO(
                prepared[rewired].OldHead);
        }
        discard();
        return Failure(Error::GraphChanged,
                       "Virtools rejected the exact Link rewire.", error);
    }

    for (Prepared &change : prepared) {
        for (Stamp stamp : change.Chain->Continuations) {
            CKBehaviorLink *link = Resolve<CKBehaviorLink>(
                m_Context, stamp, CKCID_BEHAVIORLINK);
            if (!link)
                continue;
            graph->RemoveSubBehaviorLink(link);
            m_Context->DestroyObject(link);
        }
        change.Chain->Order = std::move(change.Order);
        change.Chain->Continuations = std::move(change.Continuations);
        change.Chain->Terminal = Capture(change.Terminal);
        change.Chain->Redirected = change.Redirected;
    }
    return {};
}

Status CKEdit::Transaction::CreateTaps() {
    Status status;
    for (const CheckedTap &tap : m_Checked.Taps) {
        BlockSpec observerSpec = HookBlock::Make(tap.Callback, 1, 0);
        AttachResult added = m_Runtime.CreateInGraph(m_Graph, observerSpec);
        if (!added || !added.Block)
            return added.Detail;
        status = m_Runtime.EditInGraph(added.Block, observerSpec);
        if (!status)
            return status;
        const Stamp node = Capture(added.Block);
        m_TapNodes.emplace(tap.Ordinal, node);
        m_Journal.Nodes.push_back(node);
        m_Journal.InfrastructureNodes.push_back(node);
        status = ValidateNodes();
        if (!status)
            return status;
        m_Graph = GraphFor();
    }
    return {};
}

Status CKEdit::Transaction::LinkTaps() {
    Status status;
    for (const CheckedTap &tap : m_Checked.Taps) {
        CKBehaviorIO *source = nullptr;
        status = ControlPort(tap.Source, source);
        const auto observer = m_TapNodes.find(tap.Ordinal);
        CKBehavior *block = observer == m_TapNodes.end()
            ? nullptr
            : Resolve<CKBehavior>(m_Context, observer->second,
                                  CKCID_BEHAVIOR);
        if (status && (!block || !block->GetInput(0)))
            status = Failure(Error::GraphChanged,
                             "A Tap observer disappeared during Apply.");
        Stamp link;
        if (status)
            status = AddLink(source, block->GetInput(0), 0, &link);
        if (!status)
            return status;
        m_Journal.InfrastructureLinks.push_back(link);
    }
    return {};
}

Status CKEdit::Transaction::PublishLayer() {
    Status status;
    PatchLayer layer = m_SpliceLayer;
    layer.Patch = m_Edit->Key();
    for (const CheckedTap &tap : m_Checked.Taps) {
        CKBehavior *owner = BehaviorFor(tap.Source.Owner);
        SlotInfo slot;
        status = LiveSlot(tap.Source, slot);
        if (!status)
            return status;
        GraphEndpoint source{
            static_cast<std::uint64_t>(
                static_cast<std::uint32_t>(owner->GetID())),
            slot.Kind, slot.NativeIndex};
        auto found = std::find_if(
            layer.Outs.begin(), layer.Outs.end(),
            [&](const OutTaps &candidate) {
                return candidate.Out == source;
            });
        if (found == layer.Outs.end()) {
            layer.Outs.push_back({source, {}});
            found = std::prev(layer.Outs.end());
        }
        found->Taps.push_back({tap.Ordinal, HashTap(source, tap.Ordinal)});
    }

    // Splice and Redirect both work on one bookkeeping chain per logical Link,
    // so both open it the same way.
    const auto openChain = [&](const LinkBase &target, LinkId id) -> Status {
        const auto nativeRecord = std::find_if(
            m_Base.Links.begin(), m_Base.Links.end(), [&](const GraphLink &candidate) {
                return candidate.Object == target.Anchor;
            });
        CKBehaviorLink *anchor = nativeRecord == m_Base.Links.end()
            ? nullptr
            : Resolve<CKBehaviorLink>(
                  m_Context,
                  {static_cast<CK_ID>(nativeRecord->Id),
                   m_Context->GetObject(static_cast<CK_ID>(nativeRecord->Id))},
                  CKCID_BEHAVIORLINK);
        if (!anchor)
            return Failure(Error::LinkNotFound,
                           "The exact native Link disappeared before Apply.");

        auto &chains = m_Editor.m_Links->Chains[m_GraphId];
        const auto chain = chains.find(id);
        if (chain == chains.end()) {
            CKBehaviorIO *source = anchor->GetInBehaviorIO();
            CKBehaviorIO *sink = anchor->GetOutBehaviorIO();
            if (!source || !sink)
                return Failure(Error::GraphChanged,
                               "The selected Link lost an endpoint.");
            const LogicalLink *logical = m_GraphTopology->Find(id);
            if (!logical)
                return Failure(Error::GraphChanged,
                               "The selected Link lost its logical identity.");
            Links::Chain value;
            value.Id = id;
            value.Base = logical->Base;
            value.Anchor = Capture(anchor);
            value.Source = Capture(source);
            value.Sink = Capture(sink);
            value.Terminal = value.Sink;
            chains.emplace(id, std::move(value));
            return {};
        }
        if (chain->second.Anchor.Address != anchor)
            return Failure(Error::GraphChanged,
                           "The selected Link anchor changed identity.");
        return {};
    };

    for (std::size_t spliceIndex = 0;
         spliceIndex < m_Checked.Splices.size(); ++spliceIndex) {
        const CheckedSplice &splice = m_Checked.Splices[spliceIndex];
        const LinkId id = m_SpliceLinks[spliceIndex];
        status = openChain(splice.Target, id);
        if (!status)
            return status;

        CKBehaviorIO *input = nullptr;
        CKBehaviorIO *output = nullptr;
        status = ControlPort(splice.Input, input);
        if (status)
            status = ControlPort(splice.Output, output);
        if (!status)
            return status;

        const Links::Key key{m_Edit->Key(), splice.Ordinal};
        auto &sites = m_Editor.m_Links->Sites[m_GraphId];
        if (sites.contains(key))
            return Failure(Error::InvalidState,
                           "The Splice action is already installed.");
        sites.emplace(key, Links::Site{Capture(input), Capture(output)});
        m_Journal.Splices.emplace_back(id, splice.Ordinal);
        // Keep the candidate in the journal as it is assembled so any later
        // validation or materialization failure can remove every admitted
        // Splice site, including sites recorded before the failure.
        m_Journal.Layer = layer;
    }

    for (std::size_t index = 0; index < m_Checked.Redirects.size(); ++index) {
        const CheckedRedirect &redirect = m_Checked.Redirects[index];
        const LinkId id = m_RedirectLinks[index];
        CKBehaviorIO *sink = nullptr;
        status = ControlPort(redirect.Sink, sink);
        if (!status)
            return status;
        CKBehavior *owner = BehaviorFor(redirect.Sink.Owner);
        SlotInfo slot;
        status = LiveSlot(redirect.Sink, slot);
        if (!status)
            return status;
        const GraphEndpoint target{
            static_cast<std::uint64_t>(
                static_cast<std::uint32_t>(owner->GetID())),
            slot.Kind, slot.NativeIndex};

        const auto group = std::find_if(
            layer.Links.begin(), layer.Links.end(),
            [id](const LinkOverlays &candidate) { return candidate.Link == id; });
        if (group == layer.Links.end())
            return Failure(Error::InvalidState,
                           "A Redirect lost its Link group.");
        const auto overlay = std::find_if(
            group->Overlays.begin(), group->Overlays.end(),
            [&](const Overlay &candidate) {
                return candidate.Kind == OverlayKind::Redirect &&
                    candidate.Ordinal == redirect.Ordinal;
            });
        if (overlay == group->Overlays.end())
            return Failure(Error::InvalidState,
                           "A Redirect lost its Link overlay.");
        overlay->Target = target;
        overlay->Fingerprint =
            HashRedirect(redirect.Target, redirect.Ordinal, target);

        status = openChain(redirect.Target, id);
        if (!status)
            return status;

        const Links::Key key{m_Edit->Key(), redirect.Ordinal};
        auto &sites = m_Editor.m_Links->Sites[m_GraphId];
        if (sites.contains(key))
            return Failure(Error::InvalidState,
                           "The Redirect action is already installed.");
        sites.emplace(key, Links::Site{Capture(sink), {}});
        m_Journal.Redirects.emplace_back(id, redirect.Ordinal);
        m_Journal.Layer = layer;
    }

    m_Journal.Layer = layer;
    if (!layer.Links.empty() || !layer.Outs.empty()) {
        status = m_GraphTopology->Set(layer);
        if (status) {
            m_Editor.m_Active[m_GraphId].insert(m_Edit->Key());
            status = m_Editor.Materialize(m_GraphId, m_Graph);
        }
        if (!status)
            return status;
        m_Editor.m_Links->Patches[m_GraphId][m_Journal.Key] = {
            m_Journal.InfrastructureNodes, m_Journal.InfrastructureLinks};
        status = m_Editor.PublishLogicalGraph(m_GraphId);
        if (!status)
            return status;
    }
    return {};
}

} // namespace BML::Behavior::Internal

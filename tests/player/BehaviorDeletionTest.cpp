#include "BML/Behavior.hpp"
#include "BML/Gui/Label.h"
#include "BML/Guids/Logics.h"
#include "BML/IMod.h"

#include "PlayerProbe.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace {

// CK2 normally queues these requests from a Behavior callback. Exercise the
// same queue here without navigating away from the driver's active level.
class DeferredDeletion final {
public:
    explicit DeferredDeletion(CKContext *context)
        : m_Context(context), m_Previous(context->m_DeferDestroyObjects) {
        m_Context->m_DeferDestroyObjects = TRUE;
    }
    ~DeferredDeletion() { m_Context->m_DeferDestroyObjects = m_Previous; }

    DeferredDeletion(const DeferredDeletion &) = delete;
    DeferredDeletion &operator=(const DeferredDeletion &) = delete;

private:
    CKContext *m_Context;
    CKDWORD m_Previous;
};

struct ObjectIdentity {
    CK_ID Id;
    CKObject *Address;
};

bool IsLive(CKContext *context, ObjectIdentity object) {
    return object.Address && context->GetObject(object.Id) == object.Address;
}

std::vector<ObjectIdentity> CaptureObjects(CKContext *context) {
    std::vector<ObjectIdentity> objects;
    for (CK_CLASSID type : {CKCID_BEHAVIOR, CKCID_BEHAVIORIO,
                           CKCID_BEHAVIORLINK, CKCID_PARAMETERIN,
                           CKCID_PARAMETEROUT, CKCID_PARAMETERLOCAL,
                           CKCID_PARAMETEROPERATION, CKCID_2DENTITY,
                           CKCID_3DOBJECT, CKCID_BEOBJECT}) {
        const int count = context->GetObjectsCountByClassID(type);
        CK_ID *ids = context->GetObjectsListByClassID(type);
        for (int index = 0; index < count; ++index)
            objects.push_back({ids[index], context->GetObject(ids[index])});
    }
    std::sort(objects.begin(), objects.end(),
        [](ObjectIdentity left, ObjectIdentity right) { return left.Id < right.Id; });
    return objects;
}

CKBehaviorIO *CopiedIO(CKBehavior *source, CKBehavior *copy, CKBehaviorIO *io) {
    if (!source || !copy || !io)
        return nullptr;
    for (int index = 0; index < source->GetInputCount(); ++index) {
        if (source->GetInput(index) == io)
            return copy->GetInput(index);
    }
    for (int index = 0; index < source->GetOutputCount(); ++index) {
        if (source->GetOutput(index) == io)
            return copy->GetOutput(index);
    }
    for (int index = 0; index < source->GetSubBehaviorCount(); ++index) {
        CKBehaviorIO *mapped = CopiedIO(source->GetSubBehavior(index), copy->GetSubBehavior(index), io);
        if (mapped)
            return mapped;
    }
    return nullptr;
}

bool IndependentCopy(CKBehavior *source, CKBehavior *copy) {
    if (!source || !copy || source == copy ||
        source->GetInputCount() != copy->GetInputCount() ||
        source->GetOutputCount() != copy->GetOutputCount() ||
        source->GetInputParameterCount() != copy->GetInputParameterCount() ||
        source->GetOutputParameterCount() != copy->GetOutputParameterCount() ||
        source->GetLocalParameterCount() != copy->GetLocalParameterCount() ||
        source->GetSubBehaviorCount() != copy->GetSubBehaviorCount() ||
        source->GetSubBehaviorLinkCount() != copy->GetSubBehaviorLinkCount())
        return false;

    for (int index = 0; index < source->GetInputCount(); ++index) {
        CKBehaviorIO *io = copy->GetInput(index);
        if (!io || io == source->GetInput(index) || io->GetOwner() != copy)
            return false;
    }
    for (int index = 0; index < source->GetOutputCount(); ++index) {
        CKBehaviorIO *io = copy->GetOutput(index);
        if (!io || io == source->GetOutput(index) || io->GetOwner() != copy)
            return false;
    }
    for (int index = 0; index < source->GetInputParameterCount(); ++index) {
        CKParameterIn *parameter = copy->GetInputParameter(index);
        if (!parameter || parameter == source->GetInputParameter(index) || parameter->GetOwner() != copy)
            return false;
    }
    for (int index = 0; index < source->GetOutputParameterCount(); ++index) {
        CKParameterOut *parameter = copy->GetOutputParameter(index);
        if (!parameter || parameter == source->GetOutputParameter(index) || parameter->GetOwner() != copy)
            return false;
    }
    for (int index = 0; index < source->GetLocalParameterCount(); ++index) {
        CKParameterLocal *parameter = copy->GetLocalParameter(index);
        if (!parameter || parameter == source->GetLocalParameter(index) || parameter->GetOwner() != copy)
            return false;
    }
    for (int index = 0; index < source->GetSubBehaviorCount(); ++index) {
        CKBehavior *child = copy->GetSubBehavior(index);
        if (!IndependentCopy(source->GetSubBehavior(index), child) || child->GetParent() != copy)
            return false;
    }
    for (int index = 0; index < source->GetSubBehaviorLinkCount(); ++index) {
        CKBehaviorLink *original = source->GetSubBehaviorLink(index);
        CKBehaviorLink *link = copy->GetSubBehaviorLink(index);
        CKBehaviorIO *input = original ? CopiedIO(source, copy, original->GetInBehaviorIO()) : nullptr;
        CKBehaviorIO *output = original ? CopiedIO(source, copy, original->GetOutBehaviorIO()) : nullptr;
        if (!original || !link || original == link ||
            !input || !output || link->GetInBehaviorIO() != input || link->GetOutBehaviorIO() != output)
            return false;
    }
    return true;
}

class BehaviorDeletionTest final : public IMod {
public:
    explicit BehaviorDeletionTest(IBML *bml) : IMod(bml) { AddDependency("BML"); }

    const char *GetID() override { return "BehaviorDeletionTest"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "Behavior Deletion Test"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override {
        return "Checks mixed deferred deletion through the production GUI and Behavior APIs";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        BML::PlayerTest::ProbeReport::Reset();
        auto opened = BML::Behavior::Session::Open();
        if (!opened) {
            Finish(false, "session");
            return;
        }
        m_Session = opened.Take();
    }

    void OnProcess() override {
        if (!BML::PlayerTest::ProbeReport::Started() ||
            BML::PlayerTest::ProbeReport::Reported())
            return;
        CKContext *context = m_BML->GetCKContext();
        if (m_Waiting) {
            Observe(context);
            return;
        }
        const std::vector<ObjectIdentity> before = CaptureObjects(context);
        bool created = false;
        switch (m_Case) {
        case Case::Labels: created = DeleteLabels(context); break;
        case Case::Instances: created = DeleteInstances(context); break;
        case Case::Graph: created = DeleteGraph(context, false); break;
        case Case::Root: created = DeleteRoot(context); break;
        case Case::Owner: created = DeleteGraph(context, true); break;
        case Case::EntityOwner: created = DeleteGraph(context, true, CKCID_3DOBJECT); break;
        case Case::DynamicGraph: created = DeleteDynamicGraph(context); break;
        case Case::OrphanSource: created = DeleteOrphanSource(context); break;
        case Case::ValueCopies:
        case Case::ParkedGraph:
        case Case::ParkedReplacement: created = DeleteJournalObjects(context); break;
        case Case::Copy: created = DeleteCopy(context); break;
        case Case::Complete: Finish(true, "complete"); return;
        }
        // Immediate deletions have already gone. Remember queued objects by
        // both id and address, so a later reuse of a CK_ID is not a false leak.
        auto previous = before.begin();
        for (ObjectIdentity object : CaptureObjects(context)) {
            if (std::any_of(m_NativeSurvivors.begin(), m_NativeSurvivors.end(),
                [object](ObjectIdentity retained) { return object.Id == retained.Id && object.Address == retained.Address; }))
                continue;
            while (previous != before.end() && previous->Id < object.Id)
                ++previous;
            if (previous == before.end() || previous->Id != object.Id ||
                previous->Address != object.Address)
                m_Pending.push_back(object);
        }
        if (!created) {
            Finish(false, Name());
            return;
        }
        m_Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        m_Waiting = true;
    }

    void OnUnload() override {
        m_Instances.clear();
        m_Script.reset();
        m_Patch.reset();
        m_Session.Reset();
        CKContext *context = m_BML->GetCKContext();
        for (auto object = m_Pending.rbegin(); object != m_Pending.rend(); ++object) {
            if (IsLive(context, *object))
                context->DestroyObject(object->Id);
        }
        m_Pending.clear();
        for (auto object = m_NativeSurvivors.rbegin(); object != m_NativeSurvivors.rend(); ++object) {
            if (IsLive(context, *object))
                context->DestroyObject(object->Id);
        }
        m_NativeSurvivors.clear();
    }

private:
    enum class Case {
        Labels, Instances, Graph, Root, Owner, EntityOwner, DynamicGraph,
        OrphanSource, ValueCopies, ParkedGraph, ParkedReplacement, Copy, Complete
    };

    const char *Name() const {
        switch (m_Case) {
        case Case::Labels: return "labels";
        case Case::Instances: return "instances";
        case Case::Graph: return "graph";
        case Case::Root: return "root";
        case Case::Owner: return "owner";
        case Case::EntityOwner: return "entity-owner";
        case Case::DynamicGraph: return "dynamic-graph";
        case Case::OrphanSource: return "orphan-source";
        case Case::ValueCopies: return "value-copies";
        case Case::ParkedGraph: return "parked-graph";
        case Case::ParkedReplacement: return "parked-replacement";
        case Case::Copy: return "copy";
        case Case::Complete: return "complete";
        }
        return "unknown";
    }

    CKBeObject *CreateObject(CKContext *context, bool dynamic = false,
                            CK_CLASSID type = CKCID_BEOBJECT) {
        auto *object = CKBeObject::Cast(context->CreateObject(
            type, "__BML_Deletion_Owner",
            dynamic ? CK_OBJECTCREATION_DYNAMIC : CK_OBJECTCREATION_NONAMECHECK));
        CKLevel *level = context->GetCurrentLevel();
        CKScene *scene = context->GetCurrentScene();
        if (!object || !level || !scene || level->AddObject(object) != CK_OK) {
            if (object)
                context->DestroyObject(object);
            return nullptr;
        }
        if (scene != level->GetLevelScene())
            scene->AddObject(object);
        if (!object->IsInScene(scene)) {
            context->DestroyObject(object);
            return nullptr;
        }
        return object;
    }

    bool DeleteLabels(CKContext *context) {
        std::vector<CK_ID> balls;
        std::vector<std::unique_ptr<BGui::Label>> labels;
        for (int index = 0; index < 4; ++index) {
            CKBeObject *ball = CreateObject(context, false, CKCID_3DOBJECT);
            if (!ball)
                return false;
            balls.push_back(ball->GetID());
            auto label = std::make_unique<BGui::Label>("__BML_Peer_Label");
            label->SetText("Peer");
            labels.push_back(std::move(label));
        }
        DeferredDeletion deferred(context);
        for (std::size_t index = 0; index < balls.size(); ++index) {
            context->DestroyObject(balls[index]);
            labels[index].reset();
        }
        return true;
    }

    bool DeleteInstances(CKContext *context) {
        std::vector<CK_ID> owners;
        for (int index = 0; index < 4; ++index) {
            CKBeObject *owner = CreateObject(context, index % 2 != 0);
            if (!owner)
                return false;
            owners.push_back(owner->GetID());
            auto reference = m_Session.Reference(owner);
            if (!reference)
                return false;
            auto created = m_Session.Use(VT_LOGICS_DELAYER).Spawn(reference.Value());
            if (!created)
                return false;
            m_Instances.push_back(created.Take());
        }
        DeferredDeletion deferred(context);
        for (std::size_t index = 0; index < owners.size(); ++index) {
            (void) m_Instances[index].Close();
            context->DestroyObject(owners[index]);
        }
        m_Instances.clear();
        return true;
    }

    BML::Behavior::Edit GraphBody() {
        BML::Behavior::Edit body;
        auto graph = body.Root();
        const auto nested = graph.AddGraph("Nested").Graph();
        const auto input = nested.AppendIn("In");
        const auto output = nested.AppendOut("Out");
        nested.Flow(input, output);
        (void) nested.AppendLocal(nested.Root(), "State", CKPGUID_INT);
        (void) nested.Add(m_Session.Use(VT_LOGICS_DELAYER));
        return body;
    }

    bool CreateGraph(CKContext *context, bool dynamicOwner, CKBeObject *&owner,
                     CK_CLASSID type = CKCID_BEOBJECT) {
        owner = CreateObject(context, dynamicOwner, type);
        if (!owner)
            return false;
        auto created = m_Session.CreateScript(owner, "__BML_Deletion_Script", GraphBody());
        if (!created) {
            GetLogger()->Error("Behavior deletion setup: %s", created.GetStatus().Message.c_str());
            return false;
        }
        m_Script.emplace(created.Take());
        return true;
    }

    bool DeleteDynamicGraph(CKContext *context) {
        CKBeObject *owner = CreateObject(context);
        auto *root = CKBehavior::Cast(context->CreateObject(
            CKCID_BEHAVIOR, "__BML_Deletion_DynamicGraph", CK_OBJECTCREATION_DYNAMIC));
        if (!owner || !root)
            return false;
        root->UseGraph();
        root->SetType(CKBEHAVIORTYPE_SCRIPT);
        if (owner->AddScript(root) != CK_OK)
            return false;
        auto graph = m_Session.Inspect(root);
        if (!graph)
            return false;
        auto applied = graph->Apply("deletion-dynamic-graph", GraphBody());
        if (!applied)
            return false;
        m_Patch.emplace(applied.Take());
        DeferredDeletion deferred(context);
        context->DestroyObject(root);
        context->DestroyObject(owner);
        return true;
    }

    bool DeleteGraph(CKContext *context, bool dynamicOwner,
                     CK_CLASSID type = CKCID_BEOBJECT) {
        CKBeObject *owner = nullptr;
        if (!CreateGraph(context, dynamicOwner, owner, type))
            return false;
        CKBeObject *witness = CreateObject(context);
        if (!witness)
            return false;
        DeferredDeletion deferred(context);
        if (!dynamicOwner)
            (void) m_Script->Close();
        // Dynamic CK3dObject itself leaves a dependency stack entry in retail
        // CK2, even without BML Behaviors. Keep that independent native bug
        // out of the entity-owner test; the plain owner case queues its
        // witness after the owner.
        if (type == CKCID_3DOBJECT)
            context->DestroyObject(witness);
        context->DestroyObject(owner);
        if (type != CKCID_3DOBJECT)
            context->DestroyObject(witness);
        return true;
    }

    bool DeleteOrphanSource(CKContext *context) {
        CKBeObject *owner = CreateObject(context);
        auto *root = CKBehavior::Cast(context->CreateObject(
            CKCID_BEHAVIOR, "__BML_Deletion_DynamicGraph", CK_OBJECTCREATION_DYNAMIC));
        auto *source = CKBehavior::Cast(context->CreateObject(CKCID_BEHAVIOR, "Source"));
        if (!owner || !root || !source)
            return false;
        root->UseGraph();
        root->SetType(CKBEHAVIORTYPE_SCRIPT);
        source->UseGraph();
        CKBehaviorIO *input = source->CreateInput("In");
        CKBehaviorIO *output = source->CreateOutput("Out");
        if (!input || !output || owner->AddScript(root) != CK_OK ||
            root->AddSubBehavior(source) != CK_OK)
            return false;
        // The ordinary native child survives its dynamic parent. Unlike
        // authored Nodes, it is not owned by the Patch's retirement journal.
        m_NativeSurvivors = {{source->GetID(), source}, {input->GetID(), input}, {output->GetID(), output}};
        auto graph = m_Session.Inspect(root);
        if (!graph)
            return false;
        BML::Behavior::Edit edit;
        const auto node = edit.Root().Require("Source");
        edit.Root().Flow(node.Out(), node.In(), 1);
        auto applied = graph->Apply("deletion-orphan-source", edit);
        if (!applied) {
            GetLogger()->Error("Behavior deletion setup: %s", applied.GetStatus().Message.c_str());
            return false;
        }
        m_Patch.emplace(applied.Take());
        DeferredDeletion deferred(context);
        context->DestroyObject(root);
        context->DestroyObject(owner);
        return true;
    }

    bool DeleteJournalObjects(CKContext *context) {
        CKBeObject *owner = CreateObject(context);
        auto *root = CKBehavior::Cast(context->CreateObject(CKCID_BEHAVIOR, "__BML_Deletion_Journal"));
        if (!owner || !root)
            return false;
        root->UseGraph();
        root->SetType(CKBEHAVIORTYPE_SCRIPT);
        if (owner->AddScript(root) != CK_OK)
            return false;
        BML::Behavior::Edit body;
        if (m_Case == Case::ParkedGraph) {
            auto *node = CKBehavior::Cast(context->CreateObject(CKCID_BEHAVIOR, "Parked"));
            if (!node)
                return false;
            node->UseGraph();
            CKBehaviorIO *entry = root->CreateInput("In");
            CKBehaviorIO *exit = root->CreateOutput("Out");
            CKBehaviorIO *input = node->CreateInput("In");
            CKBehaviorIO *output = node->CreateOutput("Out");
            if (!entry || !exit || !input || !output || root->AddSubBehavior(node) != CK_OK)
                return false;
            // Remove parks incident native Links and temporary endpoint IOs
            // outside the graph. Root deletion must retire those as well.
            for (const auto &endpoints : {std::pair(entry, input), std::pair(output, exit)}) {
                auto *link = CKBehaviorLink::Cast(context->CreateObject(CKCID_BEHAVIORLINK));
                if (!link || link->SetInBehaviorIO(endpoints.first) != CK_OK ||
                    link->SetOutBehaviorIO(endpoints.second) != CK_OK || root->AddSubBehaviorLink(link) != CK_OK)
                    return false;
            }
            body.Root().Remove(body.Root().Require("Parked"));
        } else if (m_Case == Case::ParkedReplacement) {
            auto *node = CKBehavior::Cast(context->CreateObject(CKCID_BEHAVIOR, "Parked"));
            if (!node || node->InitFromGuid(VT_LOGICS_DELAYER) != CK_OK || root->AddSubBehavior(node) != CK_OK)
                return false;
            (void) body.Root().Replace(body.Root().Require(VT_LOGICS_DELAYER), m_Session.Use(VT_LOGICS_DELAYER));
        } else {
            if (!root->CreateLocalParameter("State", CKPGUID_INT))
                return false;
            body.Root().Set(body.Root().Root().Local("State"), 23);
        }
        auto graph = m_Session.Inspect(root);
        if (!graph)
            return false;
        auto applied = graph->Apply("deletion-journal", body);
        if (!applied) {
            GetLogger()->Error("Behavior deletion setup: %s", applied.GetStatus().Message.c_str());
            return false;
        }
        m_Patch.emplace(applied.Take());
        DeferredDeletion deferred(context);
        context->DestroyObject(root);
        context->DestroyObject(owner);
        return true;
    }

    bool DeleteRoot(CKContext *context) {
        CKBeObject *owner = nullptr;
        if (!CreateGraph(context, false, owner))
            return false;
        auto inspected = m_Script->Inspect();
        CKBehavior *root = inspected
            ? CKBehavior::Cast(context->GetObject(static_cast<CK_ID>(inspected->Root().Id())))
            : nullptr;
        CKBeObject *witness = CreateObject(context);
        if (!root || !witness)
            return false;
        DeferredDeletion deferred(context);
        context->DestroyObject(root);
        context->DestroyObject(owner);
        context->DestroyObject(witness);
        return true;
    }

    bool DeleteCopy(CKContext *context) {
        CKBeObject *owner = nullptr;
        if (!CreateGraph(context, false, owner))
            return false;
        auto inspected = m_Script->Inspect();
        CKBehavior *root = inspected
            ? CKBehavior::Cast(context->GetObject(static_cast<CK_ID>(inspected->Root().Id())))
            : nullptr;
        CKDependencies dependencies;
        dependencies.m_Flags = CK_DEPENDENCIES_FULL;
        auto *copy = root ? CKBehavior::Cast(context->CopyObject(root, &dependencies)) : nullptr;
        const bool independent = IndependentCopy(root, copy);
        DeferredDeletion deferred(context);
        (void) m_Script->Close();
        if (copy)
            context->DestroyObject(copy);
        context->DestroyObject(owner);
        return independent;
    }

    void Observe(CKContext *context) {
        std::size_t alive = 0;
        for (ObjectIdentity object : m_Pending) {
            if (IsLive(context, object))
                ++alive;
        }
        if (alive && std::chrono::steady_clock::now() < m_Deadline)
            return;
        GetLogger()->Info("Behavior deletion: case=%s tracked=%zu alive=%zu",
                          Name(), m_Pending.size(), alive);
        if (alive) {
            for (ObjectIdentity object : m_Pending) {
                if (IsLive(context, object))
                    GetLogger()->Error("Behavior deletion residual: id=%u class=%d name=%s",
                        object.Id, object.Address->GetClassID(), object.Address->GetName());
            }
            Finish(false, Name());
            return;
        }
        if (!m_NativeSurvivors.empty()) {
            if (!std::all_of(m_NativeSurvivors.begin(), m_NativeSurvivors.end(),
                [context](ObjectIdentity object) { return IsLive(context, object); })) {
                Finish(false, "orphan-source-not-exercised");
                return;
            }
            // Make CK2 grow and free the surviving source array again. An
            // inline SDK removal must not have moved it to the caller's CRT.
            auto *link = CKBehaviorLink::Cast(context->CreateObject(CKCID_BEHAVIORLINK));
            auto *input = static_cast<CKBehaviorIO *>(m_NativeSurvivors[1].Address);
            auto *output = static_cast<CKBehaviorIO *>(m_NativeSurvivors[2].Address);
            if (!link || link->SetInBehaviorIO(output) != CK_OK || link->SetOutBehaviorIO(input) != CK_OK) {
                if (link)
                    context->DestroyObject(link);
                Finish(false, "orphan-source-reuse");
                return;
            }
            m_NativeSurvivors.push_back({link->GetID(), link});
            context->DestroyObject(link);
            context->DestroyObject(m_NativeSurvivors.front().Id);
            m_Pending.clear();
            m_Pending.swap(m_NativeSurvivors);
            m_Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            return;
        }
        m_Script.reset();
        m_Patch.reset();
        m_Pending.clear();
        m_Waiting = false;
        m_Case = static_cast<Case>(static_cast<int>(m_Case) + 1);
    }

    void Finish(bool passed, const char *detail) {
        if (passed)
            BML::PlayerTest::ProbeReport::Pass(detail);
        else
            BML::PlayerTest::ProbeReport::Fail(detail);
    }

    BML::Behavior::Session m_Session;
    std::vector<BML::Behavior::Instance> m_Instances;
    std::optional<BML::Behavior::Script> m_Script;
    std::optional<BML::Behavior::Patch> m_Patch;
    std::vector<ObjectIdentity> m_Pending;
    std::vector<ObjectIdentity> m_NativeSurvivors;
    std::chrono::steady_clock::time_point m_Deadline;
    Case m_Case = Case::Labels;
    bool m_Waiting = false;
};

} // namespace

BML_PLAYER_PROBE_EXPORTS()

BML_MOD_ENTRY(IMod *) BMLEntry(IBML *bml) { return new BehaviorDeletionTest(bml); }
BML_MOD_ENTRY(void) BMLExit(IMod *mod) { delete mod; }

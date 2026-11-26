[bml.mod id="SdlBehaviorInstallationsProbe" name="Behavior installation regression" version="1.0.0"]
class BehaviorInstallationsProbe {
    BML::Behavior::Plan@ plan;
    BML::Behavior::Edit@ planBody;
    BML::Behavior::Patch@ patch;
    BML::Behavior::Graph@ patchBase;
    BML::Behavior::Edit@ patchBody;
    BML::Behavior::EditNode@ patchNode;
    BML::Behavior::Watch@ watch;
    BML::Behavior::Patch@ trigger;
    BML::ObjectRef original;
    int step = 0;
    int frames = 0;

    CKBehavior@ FindGraph(const BML::ModContext &in ctx) {
        BML::ObjectRef reference;
        CKObject@ object;
        if (BML::Scene::FindObject("Event_handler", CKCID_BEHAVIOR, reference) != BML::ERROR_OK ||
            BML::Engine::BorrowObject(ctx, reference, CKCID_BEHAVIOR, object) != BML::ERROR_OK)
            return null;
        return cast<CKBehavior>(object);
    }

    CKBehavior@ FindMarker(CKBehavior@ graph) {
        for (int i = 0; i < graph.GetSubBehaviorCount(); ++i) {
            CKBehavior@ child = graph.GetSubBehavior(i);
            if (BML::CK::GetName(child) == "__BML_Installations_Plan")
                return child;
        }
        return null;
    }

    void Require(bool condition, const string &in message) {
        if (!condition)
            throw(message);
    }

    bool OwnsOriginalNode(const BML::ModContext &in ctx, CKBehavior@ node) {
        CKObject@ object;
        return node !is null &&
            BML::Engine::BorrowObject(ctx, original, CKCID_BEHAVIOR, object) == BML::ERROR_OK &&
            BML::CK::GetId(object) == BML::CK::GetId(node);
    }

    void TriggerWatch(CKBehavior@ source, const string &in name) {
        BML::Behavior::Graph@ graph = BML::Behavior::Inspect(source);
        @watch = graph.Watch(BML::Behavior::WatchCallback(this.OnGraphChanged));
        BML::Behavior::Edit@ edit = BML::Behavior::Edit();
        edit.Root().AddGraph(name);
        @trigger = graph.Apply(name, edit);
    }

    void OnGraphChanged(const BML::ModContext &in ctx, const BML::Behavior::Change &in change) {
        if (step == 2) {
            step = 3;
            BML::Behavior::Edit@ invalid = BML::Behavior::Edit();
            invalid.Root().Require(BML::Behavior::NodePattern("__BML_Installations_Missing"));
            Require(plan.Replace("Event_handler", invalid), "Plan replacement request was not queued");
        } else if (step == 9) {
            step = 10;
            Require(patch.Replace(patchBase, patchBody), "Patch rebinding request was not queued");
        }
    }

    void OnProcess(const BML::ModContext &in ctx) {
        if (step == 11)
            return;
        CKBehavior@ source = FindGraph(ctx);
        if (source is null)
            return;
        CKBehavior@ marker = FindMarker(source);
        if (step == 0) {
            @planBody = BML::Behavior::Edit();
            planBody.Root().AddGraph("__BML_Installations_Plan");
            @plan = planBody.Plan("installation regression", "Event_handler");
            step = 1;
        } else if (step == 1 && plan.State == BML::Behavior::PlanState::Active) {
            Require(marker !is null, "Initial Plan did not install its marker");
            step = 2;
            TriggerWatch(source, "__BML_Installations_Failed_Replace");
        } else if (step == 3 && !plan.Error.isEmpty()) {
            Require(plan.State == BML::Behavior::PlanState::Active && marker !is null,
                    "A failed Plan replacement lost the previous definition");
            Require(plan.Error.findFirst("matched no Node") >= 0, "Unexpected Plan replacement error");
            ctx.LogInfo("Behavior installations PASS failed Plan replacement recovery");
            watch.Close();
            trigger.Close();
            step = 4;
        } else if (step == 4) {
            Require(BML::Scene::FindObject("__BML_Installations_Plan", CKCID_BEHAVIOR, original) ==
                    BML::ERROR_OK, "Could not record the installed Plan node identity");
            Require(plan.Replace("Event_handler", planBody), "Identical Plan replacement request failed");
            step = 5;
        } else if (step == 5 && plan.State == BML::Behavior::PlanState::Active) {
            Require(OwnsOriginalNode(ctx, marker) && plan.Error.isEmpty(),
                    "Identical Plan replacement rebuilt the installed node");
            ctx.LogInfo("Behavior installations PASS unchanged Plan installation");
            Require(int(source.GetCKContext().DestroyObject(marker)) == 0, "CK rejected marker deletion");
            if (FindMarker(source) is null)
                Require(plan.State != BML::Behavior::PlanState::Active,
                        "Plan kept reporting an invalid installation");
            step = 6;
        } else if (step == 6 && plan.State == BML::Behavior::PlanState::Active) {
            Require(marker !is null && !OwnsOriginalNode(ctx, marker),
                    "Plan did not reinstall after its node was deleted");
            ctx.LogInfo("Behavior installations PASS deleted Plan installation");
            @patchBase = BML::Behavior::Inspect(source);
            @patchBody = BML::Behavior::Edit();
            @patchNode = patchBody.Root().AddGraph("__BML_Installations_Patch");
            @patch = patchBase.Apply("installation rebinding", patchBody);
            step = 8;
        } else if (step == 8 && patch.State == BML::Behavior::PatchState::Active) {
            Require(patch.Resolve(patchNode) !is null, "Initial Patch node is not resolved");
            step = 9;
            TriggerWatch(source, "__BML_Installations_Rebind");
        } else if (step == 10 && ++frames >= 2) {
            Require(patch.State == BML::Behavior::PatchState::Active && patch.Resolve(patchNode) !is null,
                    "Queued Patch replacement did not publish the new binding");
            ctx.LogInfo("Behavior installations PASS deferred Patch binding");
            BML::CommandDefinition definition;
            definition.Name = "bml_probe_behavior_installations";
            BML::CommandRef@ command;
            Require(ctx.Commands().Register(definition, BML::CommandExecute(this.Marker), command) ==
                    BML::ERROR_OK, "Behavior regression marker registration failed");
            step = 11;
        }
    }

    int Marker(const BML::ModContext &in ctx, const BML::CommandInvocation &in call) { return 0; }
}

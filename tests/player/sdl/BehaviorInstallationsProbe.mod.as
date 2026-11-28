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
    BML::Behavior::Submission@ submission;
    BML::Behavior::EditPort@ input;
    BML::Behavior::Installation@ installation;
    BML::Behavior::Submission@ controlled;
    BML::Behavior::Edit@ controlledBody;
    BML::Behavior::EditNode@ controlledNode;
    BML::Behavior::EditPort@ controlledInput;

    CKBehavior@ FindGraph(const BML::ModContext &in ctx) {
        BML::ObjectRef reference;
        CKObject@ object;
        if (BML::Scene::FindObject("Event_handler", CKCID_BEHAVIOR, reference) != BML::ERROR_OK ||
            BML::Engine::BorrowObject(ctx, reference, CKCID_BEHAVIOR, object) != BML::ERROR_OK)
            return null;
        return cast<CKBehavior>(object);
    }

    CKBehavior@ FindMarker(CKBehavior@ graph, const string &in name = "__BML_Installations_Plan") {
        for (int i = 0; i < graph.GetSubBehaviorCount(); ++i) {
            CKBehavior@ child = graph.GetSubBehavior(i);
            if (BML::CK::GetName(child) == name)
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
        } else if (step == 14) {
            step = 15;
            patchBody.Root().Require(BML::Behavior::NodePattern("__BML_Installations_Missing"));
            Require(patch.Replace(patchBase, patchBody), "Failed replacement did not queue");
        } else if (step == 9) {
            step = 10;
            Require(patch.Replace(patchBase, patchBody), "Patch rebinding request was not queued");
        } else if (step == 20) {
            step = 21;
            Require(controlled.SetEnabled(false) && controlled.SetEnabled(true),
                    "Rapid disable and enable were not accepted");
        } else if (step == 24) {
            step = 25;
            BML::Behavior::Edit@ invalid = BML::Behavior::Edit();
            invalid.Root().Require(BML::Behavior::NodePattern("__BML_Installations_Missing"));
            Require(controlled.Replace("Event_handler", invalid), "Submission replacement did not queue");
        }
    }

    void OnProcess(const BML::ModContext &in ctx) {
        if (step == 13)
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
            // The Watch trigger edits source. Keep the replacement's exact
            // snapshot on a separate graph so its fingerprint stays valid.
            @patchBase = BML::Behavior::Inspect(marker);
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
            watch.Close();
            trigger.Close();
            step = 14;
            TriggerWatch(source, "__BML_Installations_Reject");
        } else if (step == 15 && !patch.Error.isEmpty()) {
            Require(patch.State == BML::Behavior::PatchState::Active,
                    "Old Patch was not restored, state=" + int(patch.State) + " " + patch.Error);
            Require(patch.Error.findFirst("matched no Node") >= 0, patch.Error);
            Require(patch.Resolve(patchNode) !is null, "Failed replacement lost the old Node binding");
            ctx.LogInfo("Behavior installations PASS rejected replacement Node");
            watch.Close();
            trigger.Close();
            BML::Behavior::Edit@ edit = BML::Behavior::Edit();
            BML::Behavior::EditGraph@ graph = edit.Root();
            graph.AddGraph("__BML_Receipt_A");
            graph.AddGraph("__BML_Receipt_B");
            @input = graph.AppendLocal("__BML_Apply_Input", BML::Behavior::Value(1).Type);
            Require(graph.Bind(input, BML::Behavior::Value(1)), "initial Edit input");
            @submission = edit.Apply("unified script edit", "Event_handler",
                BML::Behavior::EditOwnership::Submission);
            Require(submission !is null && submission.SetInput(input, BML::Behavior::Value(2)),
                    "SetInput before installation failed");
            step = 11;
        } else if (step == 11) {
            Require(submission.ApplyError.isEmpty(), submission.ApplyError);
            if (submission.State != BML::Behavior::SubmissionState::Applied)
                return;
            @installation = submission.Find();
            int value = 0;
            Require(installation !is null && installation.Read(input).Read(value) && value == 2,
                    "Pending input was not applied to the new installation");
            Require(installation.Set(input, BML::Behavior::Value(3)) &&
                    installation.Read(input).Read(value) && value == 3,
                    "Installation-local value did not read back");
            Require(submission.SetInput(input, BML::Behavior::Value(4)) &&
                    installation.Read(input).Read(value) && value == 4,
                    "SetInput did not update the live installation");
            Require(int(source.GetCKContext().DestroyObject(FindMarker(source, "__BML_Receipt_A"))) == 0,
                    "Could not remove installation member");
            step = 16;
        } else if (step == 16 && FindMarker(source, "__BML_Receipt_A") !is null) {
            bool staleRead = false;
            try {
                BML::Behavior::ObservedValue@ oldValue = installation.Read(input);
                staleRead = oldValue !is null;
            } catch {}
            Require(!installation.IsValid && !staleRead, "Reinstalled graph left its old receipt valid");
            int reinstalled = 0;
            Require(submission.Find().Read(input).Read(reinstalled) && reinstalled == 4,
                    "Reinstalled Local lost the submitted input");
            int siblings = 0;
            for (int i = 0; i < source.GetSubBehaviorCount(); ++i)
                if (BML::CK::GetName(source.GetSubBehavior(i)) == "__BML_Receipt_B") ++siblings;
            Require(siblings == 1 && submission.Find().IsValid, "Reinstall duplicated a sibling");
            step = 12;
        } else if (step == 12) {
            if (submission.Close() == BML::Behavior::CloseState::Closing)
                return;
            Require(!installation.IsValid, "A closed submission retained a live receipt");
            Require(FindMarker(source, "__BML_Receipt_A") is null &&
                    FindMarker(source, "__BML_Receipt_B") is null, "Close stranded an installation member");
            ctx.LogInfo("Behavior installations PASS member loss and receipt invalidation");
            ctx.LogInfo("Behavior installations PASS unified script Apply and inputs");
            @controlledBody = BML::Behavior::Edit();
            @controlledNode = controlledBody.Root().AddGraph("__BML_Controlled");
            @controlledInput = controlledBody.Root().AppendLocal("__BML_Controlled_Input", BML::Behavior::Value(1).Type);
            Require(controlledBody.Root().Bind(controlledInput, BML::Behavior::Value(1)),
                    "Could not declare the controlled input");
            @controlled = controlledBody.Apply("controlled submission", "Event_handler",
                BML::Behavior::EditOwnership::Submission, false, false);
            Require(controlled !is null && !controlled.RequestedEnabled &&
                    controlled.State == BML::Behavior::SubmissionState::Disabled &&
                    FindMarker(source, "__BML_Controlled") is null,
                    "Initially disabled submission changed the graph");
            Require(controlled.SetEnabled(true), "Could not enable the submission");
            step = 19;
        } else if (step == 19 && controlled.State == BML::Behavior::SubmissionState::Applied) {
            Require(controlled.SetInput(controlledInput, BML::Behavior::Value(8)) &&
                    BML::Scene::FindObject("__BML_Controlled", CKCID_BEHAVIOR, original) == BML::ERROR_OK,
                    "Could not capture the original installation");
            Require(controlled.Replace("Event_handler", controlledBody), "Identical replacement was refused");
            step = 18;
        } else if (step == 18 && controlled.State == BML::Behavior::SubmissionState::Applied) {
            bool wrongTypeRejected = false;
            try {
                array<int> wrongType;
                controlled.ReadInstallations(wrongType);
            } catch {
                wrongTypeRejected = true;
            }
            Require(wrongTypeRejected, "ReadInstallations accepted an incompatible output array");
            array<BML::Behavior::Installation@> entries;
            int value = 0;
            Require(controlled.RequestedEnabled && controlled.ReadInstallations(entries) &&
                    entries.length() == 1 && entries[0].Resolve(controlledNode) !is null &&
                    entries[0].Read(controlledInput).Read(value) && value == 8 &&
                    OwnsOriginalNode(ctx, FindMarker(source, "__BML_Controlled")),
                    "Identical replacement lost its node or input binding");
            Require(controlled.SetInput(controlledInput, BML::Behavior::Value(9)) &&
                    entries[0].Read(controlledInput).Read(value) && value == 9,
                    "Rebound input did not reach the preserved installation");
            @installation = entries[0];
            step = 20;
            TriggerWatch(source, "__BML_Installations_Toggle");
        } else if (step == 21) {
            Require(installation.IsValid && controlled.RequestedEnabled &&
                    controlled.State == BML::Behavior::SubmissionState::Applied,
                    "Rapid toggles changed the installation: valid=" + installation.IsValid +
                    " enabled=" + controlled.RequestedEnabled + " state=" + int(controlled.State));
            watch.Close();
            trigger.Close();
            Require(controlled.SetEnabled(false), "Could not disable the submission");
            step = 22;
        } else if (step == 22 && controlled.State == BML::Behavior::SubmissionState::Disabled) {
            Require(!controlled.RequestedEnabled && !installation.IsValid &&
                    FindMarker(source, "__BML_Controlled") is null,
                    "Disable did not restore the graph");
            Require(controlled.SetEnabled(true), "Could not re-enable the submission");
            step = 23;
        } else if (step == 23 && controlled.State == BML::Behavior::SubmissionState::Applied) {
            int value = 0;
            Require(FindMarker(source, "__BML_Controlled") !is null && controlled.Find().IsValid &&
                    controlled.Find().Read(controlledInput).Read(value) && value == 9,
                    "Re-enable did not reinstall the same definition and input values");
            ctx.LogInfo("Behavior installations PASS submission enable disable and rapid toggle");
            step = 24;
            TriggerWatch(source, "__BML_Installations_Submission_Replace");
        } else if (step == 25 && !controlled.ApplyError.isEmpty()) {
            Require(controlled.RestoreError.isEmpty() &&
                    controlled.Find().Resolve(controlledNode) !is null &&
                    FindMarker(source, "__BML_Controlled") !is null,
                    "Failed replacement lost the previous Submission definition");
            ctx.LogInfo("Behavior installations PASS submission replacement recovery");
            watch.Close();
            trigger.Close();
            step = 26;
        } else if (step == 26) {
            if (controlled.Close() == BML::Behavior::CloseState::Closing)
                return;
            Require(FindMarker(source, "__BML_Controlled") is null,
                    "Close stranded the controlled installation");
            BML::CommandDefinition definition;
            definition.Name = "bml_probe_behavior_installations";
            BML::CommandRef@ command;
            Require(ctx.Commands().Register(definition, BML::CommandExecute(this.Marker), command) ==
                    BML::ERROR_OK, "Behavior regression marker registration failed");
            step = 13;
        }
    }

    int Marker(const BML::ModContext &in ctx, const BML::CommandInvocation &in call) { return 0; }
}

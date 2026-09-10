[bml.mod id="bml.lifecycle.player.test"
         name="BML Lifecycle Player Test"
         version="1.0.0"
         author="BML+"
         description="Test-only HookBlock lifecycle probe"
         bml="0.3.13"]
class LifecyclePlayerTest {
    BML::HookBlockRef@ hook;
    int callbackCount = 0;
    bool complete = false;
    bool processSeen = false;
    bool graphChecked = false;
    BML::Behavior::Patch@ behaviorPatch;
    BML::Behavior::Watch@ behaviorWatch;
    BML::Behavior::Watch@ behaviorUnloadWatch;
    int behaviorHookCount = 0;
    int behaviorWatchCount = 0;
    bool behaviorHookObjects = true;
    bool behaviorWatchKinds = true;
    bool behaviorCallbacksInstalled = false;
    bool behaviorCallbacksComplete = false;
    BML::Behavior::ObjectRef@ behaviorCapturedObject;
    bool behaviorObjectCaptured = false;
    bool behaviorObjectStale = false;

    bool Closed(BML::Behavior::CloseState state) {
      return state == BML::Behavior::CloseState::Closing ||
          state == BML::Behavior::CloseState::Closed;
    }

    void OnLoad(const BML::ModContext &in ctx) {
      ctx.LogInfo("ScriptHookRetirement loaded=true");
      TestBehaviorApi(ctx);
    }

    void TestBehaviorApi(const BML::ModContext &in ctx) {
      BML::Behavior::Block@ block = BML::Behavior::Use(
          CKGUID(0x14c32f0d, 0x56a97b21));
      BML::Behavior::Block@ found = BML::Behavior::Find(
          "BML Behavior Transport Fixture", "BML/Test");
      const bool useOk = block !is null && block.IsValid;
      const bool findOk = found !is null && found.IsValid;
      bool callOk = false;
      bool poutOk = false;
      bool layoutOk = false;
      bool liveSetOk = false;
      bool objectRefOk = false;
      if (useOk) {
        array<BML::Behavior::SlotValue> settings = {
          BML::Behavior::SlotValue("Extended Layout",
                                   BML::Behavior::Value(false)),
          BML::Behavior::SlotValue("Retry",
                                   BML::Behavior::Value(false))
        };
        array<BML::Behavior::SlotValue> pins = {
          BML::Behavior::SlotValue(BML::Behavior::Named("Number", 0),
                                   BML::Behavior::Value(37))
        };
        @block = block.Settings(settings).Pins(pins);
        BML::Behavior::Call@ call = block.Call(
            BML::Behavior::Unique("Echo Number"),
            BML::Behavior::Signals(4).Pouts());
        callOk = call !is null && call.IsValid;
        if (callOk) {
          BML::Behavior::Frames@ frames = call.TakeFrames();
          if (frames !is null && frames.Count == 1) {
            BML::Behavior::Frame@ frame = frames[0];
            int value = 0;
            poutOk = frame !is null &&
                frame.HasOut(BML::Behavior::Unique("Done")) &&
                frame.Read(BML::Behavior::Named("Number", 0), value) &&
                value == 37;
          }
          call.Close();
        }

        BML::Behavior::Call@ objectCall = block.Call(
            BML::Behavior::Unique("Make Object"),
            BML::Behavior::Signals(1).Pouts());
        if (objectCall !is null && objectCall.IsValid) {
          BML::Behavior::Frames@ objectFrames = objectCall.TakeFrames();
          if (objectFrames !is null && objectFrames.Count == 1) {
            @behaviorCapturedObject = objectFrames[0].ReadObject(
                BML::Behavior::Unique("Object"));
            objectRefOk = behaviorCapturedObject !is null &&
                behaviorCapturedObject.IsValid &&
                behaviorCapturedObject.Borrow() !is null;
            behaviorObjectCaptured = objectRefOk;
          }
          objectCall.Close();
        }

        BML::Behavior::Instance@ instance = block.Spawn(
            BML::Behavior::Signals(4).Pouts());
        if (instance !is null && instance.IsValid) {
          array<BML::Behavior::SlotValue> liveSettings = {
            BML::Behavior::SlotValue("Retry", BML::Behavior::Value(false))
          };
          const bool stageOk = instance.Settings(liveSettings);
          BML::Behavior::Layout@ layout = instance.Layout();
          BML::Behavior::Slot@ number = layout is null ? null : layout.Find(
              BML::Behavior::SlotKind::Pin,
              BML::Behavior::Named("Number", 0));
          layoutOk = stageOk && layout !is null && layout.IsValid &&
              layout.Kind == BML::Behavior::BehaviorKind::Function &&
              number !is null && number.IsValid && number.Name == "Number";
          if (layoutOk) {
            liveSetOk = instance.Set(number, BML::Behavior::Value(41)) &&
                instance.Pulse(BML::Behavior::Unique("Echo Number")) ==
                    BML::Behavior::PulseResult::Ran;
            BML::Behavior::Frames@ liveFrames = instance.TakeFrames();
            int liveNumber = 0;
            liveSetOk = liveSetOk && liveFrames !is null &&
                liveFrames.Count == 1 && liveFrames[0].Read(
                    BML::Behavior::Named("Number", 0), liveNumber) &&
                liveNumber == 41;
          }
          instance.Close();
        }
      }
      const bool passed = useOk && findOk && callOk && poutOk &&
          layoutOk && liveSetOk && objectRefOk;
      ctx.LogInfo("Behavior script API: status=" +
                  (passed ? "pass" : "fail") +
                  " use=" + (useOk ? "true" : "false") +
                  " find=" + (findOk ? "true" : "false") +
                  " call=" + (callOk ? "true" : "false") +
                  " pout=" + (poutOk ? "true" : "false") +
                  " layout=" + (layoutOk ? "true" : "false") +
                  " live_set=" + (liveSetOk ? "true" : "false") +
                  " object_ref=" + (objectRefOk ? "true" : "false"));
    }

    int OnHook(const BML::ModContext &in ctx,
               const BML::HookBlockEvent &in event) {
      ++callbackCount;
      const bool uninstalled = hook !is null && hook.Uninstall();
      ctx.LogInfo("ScriptHookRetirement callback=1 uninstall=" +
                  (uninstalled ? "true" : "false"));

      // Try to schedule the retiring block again. Binding admission is already
      // closed, so no second script invocation may be admitted.
      CKBehavior@ block = event.BorrowBlock();
      if (block !is null)
        block.Activate(true, false);
      return CKBR_OK;
    }

    void OnProcess(const BML::ModContext &in ctx) {
      if (!processSeen) {
        ctx.LogInfo("ScriptHookRetirement process=true");
        processSeen = true;
      }
      if (behaviorObjectCaptured && !behaviorObjectStale &&
          behaviorCapturedObject !is null &&
          behaviorCapturedObject.IsStale &&
          behaviorCapturedObject.Borrow() is null) {
        behaviorObjectStale = true;
        ctx.LogInfo("Behavior script ObjectRef: status=pass stale=true");
      }
      if (complete)
        return;
      if (behaviorCallbacksInstalled && !behaviorCallbacksComplete) {
        if (behaviorHookCount < 1 || behaviorWatchCount < 1)
          return;
        const bool patchClosed = behaviorPatch !is null &&
            Closed(behaviorPatch.Close());
        const bool watchClosed = behaviorWatch !is null &&
            Closed(behaviorWatch.Close());
        behaviorCallbacksComplete = patchClosed && watchClosed &&
            behaviorHookObjects && behaviorWatchKinds;
        ctx.LogInfo("Behavior script callbacks: status=" +
                    (behaviorCallbacksComplete ? "pass" : "fail") +
                    " hook=" + behaviorHookCount +
                    " watch=" + behaviorWatchCount +
                    " hook_objects=" +
                    (behaviorHookObjects ? "true" : "false") +
                    " watch_graph=" +
                    (behaviorWatchKinds ? "true" : "false") +
                    " patch_close=" + (patchClosed ? "true" : "false") +
                    " watch_close=" + (watchClosed ? "true" : "false"));
        if (!behaviorCallbacksComplete) {
          complete = true;
          return;
        }
      }
      if (graphChecked && !behaviorCallbacksComplete)
        return;
      if (hook is null) {
        CKBehavior@ owner = ctx.BorrowScriptByName(
            "__BML_ScriptHook_Fixture");
        if (owner is null || owner.GetSubBehaviorCount() < 1)
          return;
        CKBehavior@ source = ctx.BorrowScriptByName(
            "__BML_ScriptHook_Source");
        if (source is null)
          return;
        if (!graphChecked) {
          TestBehaviorGraphApi(ctx, owner, source);
          graphChecked = true;
          // Apply from a script callback is admitted now and committed at the
          // next Behavior safe point. Let the native fixture drive that graph
          // only after the patch has become live.
          return;
        }
        BML::HookBlockCallback@ callback =
            BML::HookBlockCallback(this.OnHook);
        @hook = ctx.InsertHookBlockAfter(
            owner, source, callback, "__BML_ScriptHook_Inserted", 0, -1);
        if (hook !is null) {
          ctx.LogInfo("ScriptHookRetirement installed=true");
        } else {
          BML::ModRef@ self = ctx.FindMod("bml.lifecycle.player.test");
          ctx.LogError("ScriptHookRetirement installed=false diagnostic=" +
                       (self !is null ? self.Diagnostic : "unavailable"));
          complete = true;
        }
        return;
      }
      if (!hook.IsValid && callbackCount == 1) {
        ctx.LogInfo("ScriptHookRetirement retired=true callbacks=1");
        complete = true;
      }
    }

    void TestBehaviorGraphApi(const BML::ModContext &in ctx,
                              CKBehavior@ owner, CKBehavior@ source) {
      BML::Behavior::Graph@ graph = BML::Behavior::Inspect(owner);
      BML::Behavior::Node@ node = graph is null ? null : graph.Find(
          BML::Behavior::Unique("__BML_ScriptHook_Source"), CKGUID(0, 0));
      const bool inspectOk = graph !is null && graph.IsValid &&
          node !is null && node.IsValid && node.Name == source.GetName();
      BML::Behavior::Link@ leaving = inspectOk ? graph.Leaving(node) : null;
      BML::Behavior::Node@ next = inspectOk ? graph.Next(node) : null;
      const bool topologyOk = leaving !is null && leaving.IsValid &&
          graph.OutgoingCount(node) == 1 && next !is null && next.IsValid;

      BML::Behavior::Edit@ edit = BML::Behavior::Edit();
      BML::Behavior::EditGraph@ root = edit is null ? null : edit.Root();
      BML::Behavior::Block@ block = BML::Behavior::Use(
          CKGUID(0x14c32f0d, 0x56a97b21));
      BML::Behavior::EditNode@ added =
          root is null || block is null ? null : root.Add(block);
      BML::Behavior::Patch@ patch =
          added is null ? null : graph.Apply("script-graph-probe", edit);
      BML::Behavior::ObjectRef@ addedObject =
          patch is null ? null : patch.Resolve(added);
      const bool resolveOk = addedObject !is null && addedObject.IsValid &&
          addedObject.Borrow() !is null;
      const bool patchOk = patch !is null && patch.IsValid && resolveOk &&
          Closed(patch.Close());

      BML::Behavior::Edit@ body = BML::Behavior::Edit();
      CKContext@ ck = ctx.BorrowCKContext();
      CKBeObject@ scriptOwner = ck is null ? null : ck.GetCurrentLevel();
      BML::Behavior::Script@ script = body.CreateScript(
          scriptOwner, "__BML_Script_Created");
      BML::Behavior::Edit@ update = BML::Behavior::Edit();
      BML::Behavior::Patch@ updatePatch = script is null ? null :
          script.Apply("script-update-probe", update);
      const bool createOk = script !is null && script.IsValid &&
          updatePatch !is null && updatePatch.IsValid &&
          Closed(updatePatch.Close()) && script.Activate() &&
          script.Deactivate() && Closed(script.Close());

      BML::Behavior::Edit@ empty = BML::Behavior::Edit();
      BML::Behavior::Plan@ plan = empty.Plan(
          "script-plan-probe", "__BML_ScriptHook_Fixture");
      const bool planOk = plan !is null && plan.IsValid &&
          plan.Disable() && plan.Enable() &&
          plan.Replace("__BML_ScriptHook_Fixture", empty) &&
          Closed(plan.Close());

      BML::Behavior::Edit@ callbackEdit = BML::Behavior::Edit();
      BML::Behavior::EditGraph@ callbackRoot = callbackEdit is null
          ? null : callbackEdit.Root();
      BML::Behavior::NodePattern sourcePattern(
          BML::Behavior::Unique("__BML_ScriptHook_Source"));
      sourcePattern.Outs(1).Outs(1);
      BML::Behavior::EditNodes@ matchingSources = callbackRoot is null
          ? null : callbackRoot.Each(sourcePattern);
      BML::Behavior::EditPorts@ matchingOutputs = matchingSources is null
          ? null : matchingSources.Out(BML::Behavior::Only());
      const bool patternOk = matchingSources !is null &&
          matchingSources.IsValid && matchingOutputs !is null &&
          matchingOutputs.IsValid;
      BML::Behavior::EditNode@ callbackSource = callbackRoot is null
          ? null : callbackRoot.Require(node);
      BML::Behavior::HookCallback@ hookCallback =
          BML::Behavior::HookCallback(this.OnBehaviorHook);
      const bool hookAdded = callbackSource !is null && callbackRoot.Tap(
          callbackSource.Out(BML::Behavior::Only()), hookCallback);
      BML::Behavior::EditNode@ watchMarker = callbackRoot is null || block is null
          ? null : callbackRoot.Add(block);
      BML::Behavior::WatchCallback@ watchCallback =
          BML::Behavior::WatchCallback(this.OnBehaviorWatch);
      @behaviorWatch = graph is null ? null : graph.Watch(watchCallback);
      @behaviorUnloadWatch = graph is null ? null : graph.Watch(watchCallback);
      @behaviorPatch = !hookAdded || watchMarker is null ? null :
          graph.Apply("script-callback-probe", callbackEdit);
      behaviorCallbacksInstalled = behaviorPatch !is null &&
          behaviorPatch.IsValid && behaviorWatch !is null &&
          behaviorWatch.IsValid && behaviorUnloadWatch !is null &&
          behaviorUnloadWatch.IsValid;
      if (behaviorCallbacksInstalled) {
        ctx.LogInfo(
            "Behavior script owner retirement: active_watch=true");
        owner.ActivateInput(0, true);
        owner.Activate(true, false);
        owner.Execute(1.0f);
      }

      const bool passed = inspectOk && topologyOk && patchOk && createOk &&
          planOk && patternOk;
      ctx.LogInfo("Behavior script graph API: status=" +
                  (passed ? "pass" : "fail") +
                  " inspect=" + (inspectOk ? "true" : "false") +
                  " topology=" + (topologyOk ? "true" : "false") +
                  " patch=" + (patchOk ? "true" : "false") +
                  " resolve=" + (resolveOk ? "true" : "false") +
                  " create=" + (createOk ? "true" : "false") +
                  " plan=" + (planOk ? "true" : "false") +
                  " pattern=" + (patternOk ? "true" : "false") +
                  " callbacks=" +
                  (behaviorCallbacksInstalled ? "true" : "false"));
    }

    BML::Behavior::HookResult OnBehaviorHook(
        const BML::ModContext &in ctx,
        const BML::Behavior::HookEvent &in event) {
      ++behaviorHookCount;
      const bool objects = event.BorrowBlock() !is null &&
          event.BorrowScript() !is null;
      behaviorHookObjects = behaviorHookObjects && objects;
      ctx.LogInfo("Behavior script Hook: call=" + behaviorHookCount +
                  " objects=" + (objects ? "true" : "false"));
      return BML::Behavior::HookResult::Ok;
    }

    void OnBehaviorWatch(const BML::ModContext &in ctx,
                         const BML::Behavior::Change &in change) {
      ++behaviorWatchCount;
      behaviorWatchKinds = behaviorWatchKinds &&
          change.Kind == BML::Behavior::ChangeKind::Graph;
      ctx.LogInfo("Behavior script Watch: call=" + behaviorWatchCount +
                  " kind=" + change.Kind +
                  " sequence=" + change.Sequence);
    }
}

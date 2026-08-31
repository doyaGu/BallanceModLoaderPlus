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

    void OnLoad(const BML::ModContext &in ctx) {
      ctx.LogInfo("ScriptHookRetirement loaded=true");
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
      if (complete)
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
}

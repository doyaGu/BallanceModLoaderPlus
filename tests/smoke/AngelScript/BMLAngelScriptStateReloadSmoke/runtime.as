void TraceStateHookPhase(BML::ReloadPhase expected, const string &in label) {
  BML::ModContext current;
  if (BML::BorrowCurrentContext(current)) {
    const bool phaseOk = current.IsReloading && current.ReloadPhase == expected;
    current.LogInfo("BML state hook phase: " + label + "=" + (phaseOk ? "valid" : "unexpected"));
  }
}

[bml.mod id="bml.state.reload.smoke" name="BML AngelScript State Reload Smoke" version="1.0.0" author="BML" bml="0.3.0" reload="auto" description="Smoke test for script hot reload state migration."]
class BMLStateReloadSmokeMod {
  int frames = 0;
  bool requestedCommand = false;
  bool requestedExit = false;
  BML::TimerRef@ reloadTimer;
  BML::CommandRef@ reloadCommand;

  void OnLoad(const BML::ModContext &in ctx) {
    BML::Logger@ logger = ctx.BorrowLogger();
    if (logger !is null) {
      string phase = "unexpected";
      if (!ctx.IsReloading && ctx.ReloadPhase == BML::RELOAD_NONE) {
        phase = "initial";
      } else if (ctx.IsReloading && ctx.ReloadPhase == BML::RELOAD_ROLLBACK) {
        phase = "rollback";
      }
      logger.Info("BML state reload phase: v1 load=" + phase);
      logger.Info("BML state reload smoke v1 ready");
    }
    InstallServices(ctx);
  }

  void OnUnload(const BML::ModContext &in ctx) {
    BML::Logger@ logger = ctx.BorrowLogger();
    if (logger !is null) {
      string phase = "unexpected";
      if (ctx.IsReloading && ctx.ReloadPhase == BML::RELOAD_UNLOAD) {
        phase = "reload";
      } else if (!ctx.IsReloading && ctx.ReloadPhase == BML::RELOAD_NONE) {
        phase = "shutdown";
      }
      logger.Info("BML state reload phase: v1 unload=" + phase);
    }
  }

  void OnProcess(const BML::ModContext &in ctx) {
    DrawWindow("v1");
    ++frames;
    if (!requestedCommand) {
      requestedCommand = true;
      ctx.ExecuteCommand("bml_state_reload_probe");
    }
    if ((frames % 30) != 0) {
      return;
    }

    BML::Logger@ logger = ctx.BorrowLogger();
    if (logger !is null) {
      if (GetCKContext(0).GetObjectByName("__BML_StateMigrationMutationProbe") !is null) {
        logger.Error("BML state reload mutation leaked into the live world");
      }
      logger.Info("BML state reload smoke v1 heartbeat " + frames);
    }
    if (frames >= 180 && !requestedExit) {
      requestedExit = true;
      ctx.ExecuteCommand("exit");
    }
  }

  void SaveState(BML::StateBag@ state) {
    TraceStateHookPhase(BML::RELOAD_SAVE_STATE, "v1 save");
    if (state is null) {
      return;
    }
    state.SetBool("saved", true);
    state.SetInt("counter", 1234);
    state.SetString("text", "from-v1");
  }

  void RestoreState(BML::StateBag@ state) {
    TraceStateHookPhase(BML::RELOAD_RESTORE_STATE, "v1 restore");
    if (state is null) {
      return;
    }
  }

  private void InstallServices(const BML::ModContext &in ctx) {
    BML::TimerLoopCallback@ timerCallback = BML::TimerLoopCallback(this.OnReloadTimer);
    @reloadTimer = ctx.SetIntervalTicks(1, timerCallback, "state-reload-v1");

    BML::CommandDefinition commandDefinition;
    commandDefinition.Name = "bml_state_reload_probe";
    commandDefinition.Description = "Exercise script service ownership during hot reload";
    commandDefinition.Enabled = true;
    BML::CommandCallback@ commandCallback = BML::CommandCallback(this.OnReloadCommand);
    @reloadCommand = ctx.RegisterCommand(commandDefinition, commandCallback);

    const bool timerValid = reloadTimer !is null && reloadTimer.IsValid;
    const bool commandValid = reloadCommand !is null && reloadCommand.IsValid;
    ctx.LogInfo("BML state reload services: v1 timer=" + (timerValid ? "valid" : "invalid") +
                " command=" + (commandValid ? "valid" : "invalid"));
  }

  private bool OnReloadTimer(const BML::ModContext &in ctx,
                             const BML::TimerEvent &in event) {
    ctx.LogInfo("BML state reload timer callback: v1");
    return true;
  }

  private void OnReloadCommand(const BML::ModContext &in ctx,
                               const BML::CommandEvent &in event) {
    ctx.LogInfo("BML state reload command callback: v1");
  }

  private void DrawWindow(const string &in label) {
    ImGui::SetNextWindowPos(ImVec2(64.0f, 64.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_Once);
    if (ImGui::Begin("State Reload Smoke")) {
      ImGui::TextUnformatted("runtime " + label);
      ImGui::TextUnformatted("frames " + frames);
    }
    ImGui::End();
  }
}

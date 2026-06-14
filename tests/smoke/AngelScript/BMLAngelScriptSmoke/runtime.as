[bml.mod id="bml.bindings.smoke" name="BML Bindings Smoke" version="1.0.0" author="BML+" bml="0.3.11" description="Smoke test for BML AngelScript script mod bindings."]
[bml.require id="BML" version="0.3.11"]
[bml.require id="bml.native.interop.smoke" version="1.0.0"]
[bml.optional id="bml.optional.missing.smoke" version="9.9.9"]
[external.tool name="metadata ignored by BML"]
class BMLBindingsSmokeMod {
  bool firstFrame = true;
  bool firstRender = true;
  bool firstPreStartMenu = true;
  bool firstPostStartMenu = true;
  bool firstCheatEnabled = true;
  bool firstLoadObject = true;
  bool firstLoadScript = true;
  bool firstPreCommand = true;
  bool firstPostCommand = true;
  bool firstScriptCommand = true;
  bool firstPhysicalize = true;
  bool firstUnphysicalize = true;
  bool firstSelfUnregisterCommand = true;
  bool timerOnceSeen = false;
  bool timerLoopSeen = false;
  bool timerCancelledSeen = false;
  bool dataShareImmediateSeen = false;
  bool dataSharePendingSeen = false;
  bool dataShareBoolSeen = false;
  bool uiChoice = false;
  int uiCounter = 3;
  float uiFloat = 1.5f;
  string uiSearch = "ball";
  bool imguiCheck = false;
  float imguiFloat = 0.5f;
  int imguiInt = 2;
  int imguiCombo = 0;
  int imguiList = 1;
  string imguiText = "hello";
  string imguiMultiline = "line1\nline2";
  ImVec4 imguiColor = ImVec4(0.2f, 0.4f, 0.8f, 1.0f);
  int selfUnregisterCommandCalls = 0;
  BML::CommandRef@ smokeCommand;
  BML::CommandRef@ selfCommand;
  BML::TimerRef@ onceTimer;
  BML::TimerRef@ loopTimer;
  BML::TimerRef@ cancelledTimer;
  BML::DataShareRequestRef@ dataShareImmediateRequest;
  BML::DataShareRequestRef@ dataSharePendingRequest;
  BML::DataShareRequestRef@ dataShareBoolRequest;
  BML::Command@ heldCommandObject;
  BML::Timer@ heldTimerObject;
  BML::DataShareRequest@ heldDataShareObject;

  string BoolText(bool value) {
    return value ? "true" : "false";
  }

  void LogFailedModState(const BML::ModContext &in ctx, const string &in id) {
    BML::ModRef@ failedMod = BML::FindMod(id);
    if (failedMod !is null && failedMod.IsValid && failedMod.IsFailed) {
      ctx.LogInfo("BML failed mod state: id=" + failedMod.Id + " kind=" + failedMod.Kind + " state=" + failedMod.State);
      ctx.LogInfo("BML failed mod diagnostic: " + failedMod.Diagnostic);
    }
  }

  [bml.export]
  string Echo(const string &in value) {
    return "echo:" + value;
  }

  [bml.export]
  bool Not(bool value) {
    return !value;
  }

  [bml.export]
  int AddOne(int value) {
    return value + 1;
  }

  [bml.export]
  int Sum(int left, int right) {
    return left + right;
  }

  [bml.export]
  float Scale(float value) {
    return value * 2.0f;
  }

  void OnLoad(const BML::ModContext &in ctx) {
    ctx.LogInfo("BML script mod smoke loaded: " + ctx.GetModId() + " / " + ctx.GetModName());
    ctx.LogInfo("BML version: " + BML::GetVersion());
    ctx.LogInfo("BML initialized: " + BoolText(BML::IsInitialized()));
    ctx.LogInfo("BML ctx runtime: paused=" + BoolText(ctx.IsPaused) +
                " playing=" + BoolText(ctx.IsPlaying) +
                " cheat=" + BoolText(ctx.IsCheatEnabled) +
                " sr=" + ctx.GetSRScore() +
                " hs=" + ctx.GetHSScore());
    Vx2DVector mousePos = BML::GetMousePosition();
    VxVector mouseRel = BML::GetMouseRelativePosition();
    uint globalFrameCount = BML::GetFrameCount();
    uint ctxFrameCount = ctx.GetFrameCount();
    BML::InputHook@ input = BML::GetInputManager();
    BML::InputHook@ ctxInput = ctx.GetInputManager();
    ctx.LogInfo("BML time/input api: timeOk=" + BoolText(BML::GetTimeMs() >= 0.0f &&
                BML::GetAbsoluteTimeMs() >= 0.0f &&
                BML::GetDeltaTimeMs() >= 0.0f) +
                " frameOk=" + BoolText(globalFrameCount == ctxFrameCount) +
                " keyboard=" + BoolText(BML::IsKeyboardAttached()) +
                " mouse=" + BoolText(BML::IsMouseAttached()) +
                " escUp=" + BoolText(BML::IsKeyUp(CKKEY_ESCAPE)) +
                " keyName=" + BoolText(BML::GetKeyName(CKKEY_ESCAPE) != "") +
                " mousePosOk=" + BoolText(mousePos.x == mousePos.x && mousePos.y == mousePos.y) +
                " mouseRelOk=" + BoolText(mouseRel.x == mouseRel.x && mouseRel.y == mouseRel.y && mouseRel.z == mouseRel.z));
    ctx.LogInfo("BML ctx time/input api: timeOk=" + BoolText(ctx.GetTimeMs() >= 0.0f &&
                ctx.GetAbsoluteTimeMs() >= 0.0f &&
                ctx.GetDeltaTimeMs() >= 0.0f) +
                " frameOk=" + BoolText(ctxFrameCount == globalFrameCount) +
                " keyboard=" + BoolText(ctx.IsKeyboardAttached()) +
                " mouse=" + BoolText(ctx.IsMouseAttached()) +
                " escUp=" + BoolText(ctx.IsKeyUp(CKKEY_ESCAPE)) +
                " keyName=" + BoolText(ctx.GetKeyName(CKKEY_ESCAPE) != "") +
                " input=" + BoolText(ctxInput !is null));
    ctx.LogInfo("BML borrowed manager api: ck=" + BoolText(BML::GetCKContext() !is null) +
                " renderContext=" + BoolText(BML::GetRenderContext() !is null) +
                " attr=" + BoolText(BML::GetAttributeManager() !is null) +
                " behavior=" + BoolText(BML::GetBehaviorManager() !is null) +
                " collision=" + BoolText(BML::GetCollisionManager() !is null) +
                " input=" + BoolText(input !is null) +
                " message=" + BoolText(BML::GetMessageManager() !is null) +
                " path=" + BoolText(BML::GetPathManager() !is null) +
                " parameter=" + BoolText(BML::GetParameterManager() !is null) +
                " render=" + BoolText(BML::GetRenderManager() !is null) +
                " sound=" + BoolText(BML::GetSoundManager() !is null) +
                " time=" + BoolText(BML::GetTimeManager() !is null));
    ctx.LogInfo("BML ctx borrowed manager api: ck=" + BoolText(ctx.GetCKContext() !is null) +
                " renderContext=" + BoolText(ctx.GetRenderContext() !is null) +
                " attr=" + BoolText(ctx.GetAttributeManager() !is null) +
                " behavior=" + BoolText(ctx.GetBehaviorManager() !is null) +
                " collision=" + BoolText(ctx.GetCollisionManager() !is null) +
                " input=" + BoolText(ctxInput !is null) +
                " message=" + BoolText(ctx.GetMessageManager() !is null) +
                " path=" + BoolText(ctx.GetPathManager() !is null) +
                " parameter=" + BoolText(ctx.GetParameterManager() !is null) +
                " render=" + BoolText(ctx.GetRenderManager() !is null) +
                " sound=" + BoolText(ctx.GetSoundManager() !is null) +
                " time=" + BoolText(ctx.GetTimeManager() !is null));
    if (input !is null) {
      ctx.LogInfo("BML InputHook methods: keyboard=" + BoolText(input.IsKeyboardAttached()) +
                  " mouse=" + BoolText(input.IsMouseAttached()) +
                  " escUp=" + BoolText(input.IsKeyUp(CKKEY_ESCAPE)) +
                  " keyName=" + BoolText(input.GetKeyName(CKKEY_ESCAPE) != "") +
                  " mousePos=" + BoolText(input.GetMousePosition().x == input.GetMousePosition().x));
    }
    CKObject@ nullObject = null;
    CK3dEntity@ nullEntity = null;
    VxVector nullEntityPos = BML::Get3dEntityPosition(nullEntity);
    VxVector ctxNullEntityPos = ctx.Get3dEntityPosition(nullEntity);
    ctx.LogInfo("BML object api null defaults: valid=" + BoolText(BML::IsObjectValid(nullObject)) +
                " id=" + BML::GetObjectId(nullObject) +
                " nameEmpty=" + BoolText(BML::GetObjectName(nullObject) == "") +
                " class=" + BML::GetObjectClassId(nullObject) +
                " childCount=" + BML::Get3dEntityChildCount(nullEntity) +
                " positionZero=" + BoolText(nullEntityPos.x == 0.0f && nullEntityPos.y == 0.0f && nullEntityPos.z == 0.0f));
    ctx.LogInfo("BML ctx object api null defaults: valid=" + BoolText(ctx.IsObjectValid(nullObject)) +
                " id=" + ctx.GetObjectId(nullObject) +
                " nameEmpty=" + BoolText(ctx.GetObjectName(nullObject) == "") +
                " class=" + ctx.GetObjectClassId(nullObject) +
                " childCount=" + ctx.Get3dEntityChildCount(nullEntity) +
                " positionZero=" + BoolText(ctxNullEntityPos.x == 0.0f && ctxNullEntityPos.y == 0.0f && ctxNullEntityPos.z == 0.0f) +
                " missingArray=" + BoolText(ctx.GetArrayByName("__missing_bml_smoke_array__") is null));
    int originalHud = ctx.GetHUD();
    int menuSafeHud = originalHud & ~BML::HUD_SR;
    ctx.ShowTitle((originalHud & BML::HUD_TITLE) != 0);
    BML::ShowFPS((originalHud & BML::HUD_FPS) != 0);
    ctx.ShowSRTimer(false);
    ctx.SetHUD(menuSafeHud);
    BML::SetHUD(originalHud);
    ctx.ShowSRTimer(false);
    ctx.PauseSRTimer();
    BML::StartSRTimer();
    ctx.PauseSRTimer();
    ctx.ResetSRTimer();
    ctx.ShowSRTimer(false);
    ctx.LogInfo("BML hud/timer api: flags=" + originalHud +
                " title=" + BoolText((originalHud & BML::HUD_TITLE) != 0) +
                " fps=" + BoolText((originalHud & BML::HUD_FPS) != 0) +
                " srHud=" + BoolText((originalHud & BML::HUD_SR) != 0) +
                " ctxGlobal=" + BoolText(ctx.GetHUD() == BML::GetHUD()) +
                " srTimeOk=" + BoolText(ctx.GetSRTime() >= 0.0f && BML::GetSRTime() >= 0.0f));
    ctx.LogInfo("BML ctx directories: game=" + BoolText(ctx.GetDirectoryUtf8(BML::DIR_GAME) != "") +
                " loader=" + BoolText(ctx.GetDirectoryUtf8(BML::DIR_LOADER) != "") +
                " config=" + BoolText(ctx.GetDirectoryUtf8(BML::DIR_CONFIG) != "") +
                " invalid=" + BoolText(ctx.GetDirectoryUtf8(999) == ""));
    ctx.LogInfo("BML mod root valid: " + BoolText(BML::DirectoryExistsUtf8(ctx.GetModRootUtf8())));
    ctx.LogInfo("BML mod path valid: " + BoolText(BML::FileExistsUtf8(ctx.ResolveModPathUtf8("runtime.as"))));
    ctx.LogInfo("BML mod path escape blocked: " + BoolText(ctx.ResolveModPathUtf8("../base.cmo") == ""));
    ctx.LogInfo("BML mod resource helpers: file=" + BoolText(ctx.ModFileExistsUtf8("runtime.as")) +
                " dir=" + BoolText(ctx.ModDirectoryExistsUtf8(".")) +
                " read=" + BoolText(ctx.ReadModTextFileUtf8("Smoke.mod.as", "") != "") +
                " escapeRead=" + BoolText(ctx.ReadModTextFileUtf8("../base.cmo", "blocked") == "blocked") +
                " missingRead=" + BoolText(ctx.ReadModTextFileUtf8("missing.txt", "missing") == "missing"));
    ctx.LogInfo("BML command query: count=" + ctx.GetCommandCount() +
                " first=" + ctx.GetCommandName(0) +
                " echo=" + BoolText(ctx.HasCommand("echo")) +
                " missing=" + BoolText(ctx.HasCommand("bml-missing-command")) +
                " echoCheat=" + BoolText(ctx.IsCommandCheat("echo")));
    ctx.LogInfo("BML command details valid: " + BoolText(ctx.GetCommandDescription(0) != "" &&
                ctx.GetCommandName(-1) == ""));
    int modCount = BML::GetModCount();
    bool foundSelfByIndex = false;
    bool ctxFoundSelfByIndex = false;
    for (int i = 0; i < modCount; ++i) {
      if (BML::GetModId(i) == ctx.GetModId()) {
        BML::ModRef@ indexedSelf = BML::GetMod(i);
        foundSelfByIndex = indexedSelf !is null && indexedSelf.IsValid && indexedSelf.Id == ctx.GetModId();
      }
      if (ctx.GetModId(i) == ctx.GetModId()) {
        BML::ModRef@ ctxIndexedSelf = ctx.GetMod(i);
        ctxFoundSelfByIndex = ctxIndexedSelf !is null && ctxIndexedSelf.IsValid && ctxIndexedSelf.Id == ctx.GetModId();
      }
    }
    BML::ModRef@ invalidIndexedMod = BML::GetMod(-1);
    BML::ModRef@ ctxInvalidIndexedMod = ctx.GetMod(-1);
    BML::ModRef@ ctxSelf = ctx.FindMod(ctx.GetModId());
    ctx.LogInfo("BML mod registry query: count=" + modCount +
                " first=" + BML::GetModId(0) +
                " selfByIndex=" + BoolText(foundSelfByIndex) +
                " invalid=" + BoolText(BML::GetModId(-1) == "" && (invalidIndexedMod is null)));
    ctx.LogInfo("BML ctx mod registry query: count=" + ctx.GetModCount() +
                " first=" + ctx.GetModId(0) +
                " selfByIndex=" + BoolText(ctxFoundSelfByIndex) +
                " selfFind=" + BoolText(ctxSelf !is null && ctxSelf.IsValid && ctxSelf.Id == ctx.GetModId()) +
                " invalid=" + BoolText(ctx.GetModId(-1) == "" && (ctxInvalidIndexedMod is null)));

    ctx.SetConfigString("Greeting", "hello");
    ctx.SetConfigBool("Enabled", true);
    ctx.SetConfigInt("Answer", 42);
    ctx.SetConfigFloat("Scale", 2.5f);
    ctx.LogInfo("BML config: greeting=" + ctx.GetConfigString("Greeting", "missing") +
                " enabled=" + BoolText(ctx.GetConfigBool("Enabled", false)) +
                " answer=" + ctx.GetConfigInt("Answer", -1) +
                " scale=" + ctx.GetConfigFloat("Scale", -1.0f));
    ctx.LogInfo("BML config mismatch fallback: " + ctx.GetConfigInt("Greeting", -33));

    BML::DataShareSetString("AngelScriptSmoke", "loaded");
    ctx.LogInfo("BML datashare: " + BML::DataShareGetString("AngelScriptSmoke", "missing"));
    BML::DataShareSetBool("AngelScriptSmokeBool", true);
    BML::DataShareSetInt("AngelScriptSmokeInt", 42);
    BML::DataShareSetFloat("AngelScriptSmokeFloat", 3.5f);
    ctx.LogInfo("BML typed datashare: bool=" + BoolText(BML::DataShareGetBool("AngelScriptSmokeBool", false)) +
                " int=" + BML::DataShareGetInt("AngelScriptSmokeInt", -1) +
                " float=" + BML::DataShareGetFloat("AngelScriptSmokeFloat", -1.0f));
    ctx.LogInfo("BML typed datashare mismatch fallback: " + BML::DataShareGetInt("AngelScriptSmoke", -77) +
                " size=" + BML::DataShareSizeOf("AngelScriptSmoke"));
    dataShareImmediateSeen = false;
    dataSharePendingSeen = false;
    dataShareBoolSeen = false;
    BML::DataShareSetString("AngelScriptSmokeRequestImmediate", "ready");
    @dataShareImmediateRequest = BML::RequestDataShare(SmokeDataShareRequest(this, "AngelScriptSmokeRequestImmediate", BML::DATASHARE_STRING));
    @heldDataShareObject = SmokeDataShareRequest(this, "AngelScriptSmokeRequestPending", BML::DATASHARE_INT);
    @dataSharePendingRequest = ctx.RequestDataShare(heldDataShareObject);
    @dataShareBoolRequest = BML::RequestDataShare(SmokeDataShareRequest(this, "AngelScriptSmokeRequestBool", BML::DATASHARE_BOOL));
    BML::DataShareRequestRef@ invalidDataShareRequest = ctx.RequestDataShare(SmokeInvalidDataShareRequest());
    BML::DataShareSetInt("AngelScriptSmokeRequestPending", 77);
    BML::DataShareSetBool("AngelScriptSmokeRequestBool", true);
    ctx.LogInfo("BML datashare request: immediate=" + BoolText(dataShareImmediateRequest !is null && dataShareImmediateSeen) +
                " pending=" + BoolText(dataSharePendingRequest !is null && dataSharePendingSeen) +
                " bool=" + BoolText(dataShareBoolRequest !is null && dataShareBoolSeen) +
                " invalid=" + BoolText(invalidDataShareRequest is null));

    @onceTimer = BML::AddTimer(SmokeOnceTimer(this));
    @heldTimerObject = SmokeLoopTimer(this);
    @loopTimer = ctx.AddTimer(heldTimerObject);
    BML::TimerRef@ invalidTimer = BML::AddTimer(SmokeInvalidTimer());
    @cancelledTimer = ctx.AddTimer(SmokeCancelledTimer(this));
    if (loopTimer !is null) {
      loopTimer.Pause();
      loopTimer.Resume();
    }
    if (cancelledTimer !is null) {
      cancelledTimer.Cancel();
    }
    ctx.LogInfo("BML script timer registration: once=" + BoolText(onceTimer !is null && onceTimer.IsValid) +
                " loop=" + BoolText(loopTimer !is null && loopTimer.IsValid) +
                " invalid=" + BoolText(invalidTimer is null) +
                " cancelledState=" + (cancelledTimer is null ? -1 : cancelledTimer.State));
    @heldCommandObject = SmokeCommand(this);
    @smokeCommand = ctx.RegisterCommand(heldCommandObject);
    BML::CommandRef@ invalidCommand = ctx.RegisterCommand(SmokeInvalidCommand());
    bool commandRefOk = smokeCommand !is null && smokeCommand.IsValid &&
                        smokeCommand.Name == "assmoke" &&
                        smokeCommand.Alias == "ass" &&
                        !smokeCommand.IsCheat &&
                        smokeCommand.IsEnabled &&
                        smokeCommand.SetEnabled(false) &&
                        !smokeCommand.IsEnabled &&
                        smokeCommand.SetEnabled(true) &&
                        smokeCommand.IsEnabled;
    ctx.LogInfo("BML script command registration: " + BoolText(commandRefOk));
    @selfCommand = ctx.RegisterCommand(SelfUnregisterCommand(this));
    ctx.LogInfo("BML script command collision/self registration: collisionBlocked=" +
                BoolText(ctx.RegisterCommand(EchoCollisionCommand(this)) is null) +
                " self=" +
                BoolText(selfCommand !is null && selfCommand.IsValid) +
                " invalid=" +
                BoolText(invalidCommand is null));
    ctx.LogInfo("BML script object ownership: transientTimer=" + BoolText(onceTimer !is null) +
                " heldTimer=" + BoolText(loopTimer !is null && heldTimerObject !is null) +
                " transientCommand=" + BoolText(selfCommand !is null) +
                " heldCommand=" + BoolText(smokeCommand !is null && heldCommandObject !is null) +
                " transientDataShare=" + BoolText(dataShareImmediateRequest !is null) +
                " heldDataShare=" + BoolText(dataSharePendingRequest !is null && heldDataShareObject !is null));
    ctx.LogInfo("BML content registration OnLoad: trafo=" + BoolText(ctx.RegisterTrafo("BMLAngelScriptSmokeTrafo")));

    bool originalCheat = BML::IsCheatEnabled();
    BML::EnableCheat(!originalCheat);
    BML::EnableCheat(originalCheat);

    BML::ModRef@ self = BML::FindMod(ctx.GetModId());
    if (self !is null && self.IsValid) {
      ctx.LogInfo("BML modref: " + self.Id + " kind=" + self.Kind + " state=" + self.State +
                  " script=" + BoolText(self.IsScript));
      ctx.LogInfo("BML modref metadata: author=" + self.Author +
                  " desc=" + BoolText(self.Description != "") +
                  " bmlVersion=" + self.BMLVersion +
                  " parts=" + self.BMLVersionMajor + "." + self.BMLVersionMinor + "." + self.BMLVersionPatch);
      ctx.LogInfo("BML modref dependencies: check=" + self.CheckDependencies() +
                  " count=" + self.GetDependencyCount() +
                  " first=" + self.GetDependencyId(0) + "@" + self.GetDependencyVersion(0) +
                  " firstOptional=" + BoolText(self.IsDependencyOptional(0)) +
                  " second=" + self.GetDependencyId(1) + "@" + self.GetDependencyVersionMajor(1) +
                  "." + self.GetDependencyVersionMinor(1) + "." + self.GetDependencyVersionPatch(1) +
                  " secondOptional=" + BoolText(self.IsDependencyOptional(1)) +
                  " invalid=" + BoolText(self.GetDependencyId(-1) == "" &&
                  self.GetDependencyVersion(-1) == "" &&
                  !self.IsDependencyOptional(-1)));
      bool foundSumExport = false;
      bool indexedSumCall = false;
      for (int i = 0; i < self.GetExportCount(); ++i) {
        if (self.GetExportName(i) == "Sum" &&
            self.GetExportSignature(i) != "") {
          foundSumExport = true;
          BML::ExportRef@ indexedSum = self.GetExport(i);
          if (indexedSum !is null && indexedSum.IsValid) {
            BML::CallFrame@ indexedFrame = BML::CallFrame();
            int indexedStatus = indexedFrame.SetInt(0, 1);
            if (indexedStatus == 0) {
              indexedStatus = indexedFrame.SetInt(1, 2);
            }
            if (indexedStatus == 0) {
              indexedStatus = indexedSum.Call(indexedFrame);
            }
            int indexedResult = 0;
            if (indexedStatus == 0) {
              indexedFrame.GetResultInt(indexedResult);
            }
            indexedSumCall = indexedStatus == 0 && indexedResult == 3;
          }
        }
      }
      BML::ExportRef@ invalidIndexedExport = self.GetExport(-1);
      ctx.LogInfo("BML script export registry: count=" + self.GetExportCount() +
                  " sum=" + BoolText(foundSumExport) +
                  " indexedCall=" + BoolText(indexedSumCall) +
                  " invalid=" + BoolText(self.GetExportName(-1) == "" &&
                  self.GetExportSignature(-1) == "" &&
                  (invalidIndexedExport is null)));
      BML::ExportRef@ echo = self.FindExport("Echo");
      if (echo !is null && echo.IsValid) {
        string exportResult;
        int exportStatus = echo.CallString("phase5", exportResult);
        ctx.LogInfo("BML export Echo status=" + exportStatus + " result=" + exportResult);
      }
      BML::ExportRef@ addOne = self.FindExport("AddOne");
      if (addOne !is null && addOne.IsValid) {
        int addOneResult = 0;
        int addOneStatus = addOne.CallInt(40, addOneResult);
        ctx.LogInfo("BML export AddOne status=" + addOneStatus + " result=" + addOneResult);
      }
      BML::ExportRef@ scale = self.FindExport("Scale");
      if (scale !is null && scale.IsValid) {
        float scaleResult = 0.0f;
        int scaleStatus = scale.CallFloat(2.5f, scaleResult);
        ctx.LogInfo("BML export Scale status=" + scaleStatus + " result=" + scaleResult);
      }
      BML::ExportRef@ notExport = self.FindExport("Not");
      if (notExport !is null && notExport.IsValid) {
        bool notResult = false;
        int notStatus = notExport.CallBool(true, notResult);
        ctx.LogInfo("BML export Not status=" + notStatus + " result=" + BoolText(notResult));
      }
    }

    BML::ModRef@ bml = BML::FindMod("BML");
    if (bml !is null && bml.IsValid) {
      ctx.LogInfo("BML native modref: " + bml.Id + " kind=" + bml.Kind + " state=" + bml.State);
      ctx.LogInfo("BML native modref metadata: author=" + bml.Author +
                  " desc=" + BoolText(bml.Description != "") +
                  " bmlVersion=" + bml.BMLVersion);
    }

    BML::ModRef@ nativeSmoke = BML::FindMod("bml.native.interop.smoke");
    if (nativeSmoke !is null && nativeSmoke.IsValid) {
      ctx.LogInfo("BML native smoke modref: " + nativeSmoke.Id + " kind=" + nativeSmoke.Kind + " state=" + nativeSmoke.State);
      ctx.LogInfo("BML native smoke metadata: author=" + nativeSmoke.Author +
                  " desc=" + BoolText(nativeSmoke.Description != "") +
                  " bmlVersion=" + nativeSmoke.BMLVersion);
      bool foundNativeSumExport = false;
      bool indexedNativeSumCall = false;
      for (int i = 0; i < nativeSmoke.GetExportCount(); ++i) {
        if (nativeSmoke.GetExportName(i) == "NativeSum" &&
            nativeSmoke.GetExportSignature(i) == "int NativeSum(int left, int right)") {
          foundNativeSumExport = true;
          BML::ExportRef@ indexedNativeSum = nativeSmoke.GetExport(i);
          if (indexedNativeSum !is null && indexedNativeSum.IsValid) {
            BML::CallFrame@ indexedNativeFrame = BML::CallFrame();
            int indexedNativeStatus = indexedNativeFrame.SetInt(0, 3);
            if (indexedNativeStatus == 0) {
              indexedNativeStatus = indexedNativeFrame.SetInt(1, 4);
            }
            if (indexedNativeStatus == 0) {
              indexedNativeStatus = indexedNativeSum.Call(indexedNativeFrame);
            }
            int indexedNativeResult = 0;
            if (indexedNativeStatus == 0) {
              indexedNativeFrame.GetResultInt(indexedNativeResult);
            }
            indexedNativeSumCall = indexedNativeStatus == 0 && indexedNativeResult == 7;
          }
        }
      }
      BML::ExportRef@ invalidIndexedNativeExport = nativeSmoke.GetExport(-1);
      ctx.LogInfo("BML native export registry: count=" + nativeSmoke.GetExportCount() +
                  " sum=" + BoolText(foundNativeSumExport) +
                  " indexedCall=" + BoolText(indexedNativeSumCall) +
                  " invalid=" + BoolText(nativeSmoke.GetExportName(-1) == "" &&
                  nativeSmoke.GetExportSignature(-1) == "" &&
                  (invalidIndexedNativeExport is null)));
      BML::ExportRef@ nativeEcho = nativeSmoke.FindExport("NativeEcho");
      if (nativeEcho !is null && nativeEcho.IsValid) {
        string nativeResult;
        int nativeStatus = nativeEcho.CallString("script", nativeResult);
        ctx.LogInfo("BML native export NativeEcho status=" + nativeStatus +
                    " signature=" + nativeEcho.Signature +
                    " result=" + nativeResult);
      }
      BML::ExportRef@ nativeAddOne = nativeSmoke.FindExport("NativeAddOne", "int NativeAddOne(int value)");
      if (nativeAddOne !is null && nativeAddOne.IsValid) {
        int nativeAddOneResult = 0;
        int nativeAddOneStatus = nativeAddOne.CallInt(41, nativeAddOneResult);
        ctx.LogInfo("BML native export NativeAddOne status=" + nativeAddOneStatus + " result=" + nativeAddOneResult);
      }
      BML::ExportRef@ nativeSum = nativeSmoke.FindExport("NativeSum", "int NativeSum(int left, int right)");
      if (nativeSum !is null && nativeSum.IsValid) {
        BML::CallFrame@ sumFrame = BML::CallFrame();
        int nativeSumStatus = sumFrame.SetInt(0, 19);
        if (nativeSumStatus == 0) {
          nativeSumStatus = sumFrame.SetInt(1, 23);
        }
        if (nativeSumStatus == 0) {
          nativeSumStatus = nativeSum.Call(sumFrame);
        }
        int nativeSumResult = 0;
        if (nativeSumStatus == 0) {
          sumFrame.GetResultInt(nativeSumResult);
        }
        ctx.LogInfo("BML native export NativeSum status=" + nativeSumStatus + " result=" + nativeSumResult);
      }
      BML::ExportRef@ nativeScale = nativeSmoke.FindExport("NativeScale", "float NativeScale(float value)");
      if (nativeScale !is null && nativeScale.IsValid) {
        float nativeScaleResult = 0.0f;
        int nativeScaleStatus = nativeScale.CallFloat(3.0f, nativeScaleResult);
        ctx.LogInfo("BML native export NativeScale status=" + nativeScaleStatus + " result=" + nativeScaleResult);
      }
      BML::ExportRef@ nativeNot = nativeSmoke.FindExport("NativeNot", "bool NativeNot(bool value)");
      if (nativeNot !is null && nativeNot.IsValid) {
        bool nativeNotResult = true;
        int nativeNotStatus = nativeNot.CallBool(true, nativeNotResult);
        ctx.LogInfo("BML native export NativeNot status=" + nativeNotStatus + " result=" + BoolText(nativeNotResult));
      }
      BML::ExportRef@ nativeFrameReport = nativeSmoke.FindExport("NativeFrameReport");
      if (nativeFrameReport !is null && nativeFrameReport.IsValid) {
        BML::CallFrame@ reportFrame = BML::CallFrame();
        int reportStatus = reportFrame.SetInt(0, 5);
        if (reportStatus == 0) {
          reportStatus = reportFrame.SetString(1, "meta");
        }
        int reportArgCountBefore = reportFrame.ArgCount;
        int reportType0Before = reportFrame.GetArgType(0);
        int reportType1Before = reportFrame.GetArgType(1);
        if (reportStatus == 0) {
          reportStatus = nativeFrameReport.Call(reportFrame);
        }
        int reportResult = 0;
        int reportResultType = reportFrame.ResultType;
        if (reportStatus == 0) {
          reportFrame.GetResultInt(reportResult);
        }
        int reportArgCountAfter = reportFrame.ArgCount;
        int reportType1After = reportFrame.GetArgType(1);
        int reportClearMissing = reportFrame.ClearArg(9);
        int reportClearResult = reportFrame.ClearResult();
        int reportResultTypeAfter = reportFrame.ResultType;
        ctx.LogInfo("BML native export NativeFrameReport status=" + reportStatus +
                    " signature=" + nativeFrameReport.Signature +
                    " beforeCount=" + reportArgCountBefore +
                    " beforeTypes=" + reportType0Before + "/" + reportType1Before +
                    " result=" + reportResult +
                    " resultType=" + reportResultType +
                    " afterCount=" + reportArgCountAfter +
                    " afterType1=" + reportType1After +
                    " clearMissing=" + reportClearMissing +
                    " clearResult=" + reportClearResult +
                    " resultTypeAfter=" + reportResultTypeAfter +
                    " enumOk=" + BoolText(reportType0Before == BML::CALL_VALUE_INT &&
                    reportType1Before == BML::CALL_VALUE_STRING &&
                    reportResultType == BML::CALL_VALUE_INT &&
                    reportResultTypeAfter == BML::CALL_VALUE_EMPTY));
      }
      BML::ExportRef@ tryNativeEcho;
      int tryNativeEchoStatus = nativeSmoke.TryFindExport("NativeEcho", tryNativeEcho);
      BML::ExportRef@ tryNativeAmbiguous;
      int tryNativeAmbiguousStatus = nativeSmoke.TryFindExport("NativeAmbiguous", tryNativeAmbiguous);
      BML::ExportRef@ tryNativeMismatch;
      int tryNativeMismatchStatus = nativeSmoke.TryFindExport("NativeAddOne", tryNativeMismatch, "float NativeAddOne(float value)");
      BML::ExportRef@ tryNativeBadSignature;
      int tryNativeBadSignatureStatus = nativeSmoke.TryFindExport("NativeAddOne", tryNativeBadSignature, "double NativeAddOne(double value)");
      BML::ExportRef@ tryNativeMissing;
      int tryNativeMissingStatus = nativeSmoke.TryFindExport("MissingExport", tryNativeMissing, "void MissingExport()");
      int wrongArgStatus = 0;
      if (nativeAddOne !is null && nativeAddOne.IsValid) {
        string wrongResult;
        wrongArgStatus = nativeAddOne.CallString("wrong", wrongResult);
      }
      BML::ExportRef@ nativeThrow = nativeSmoke.FindExport("NativeThrow");
      int exceptionStatus = nativeThrow !is null ? nativeThrow.CallVoid() : BML::ERROR_INTEROP_EXPORT_NOT_FOUND;
      BML::ExportRef@ nativeVoid = nativeSmoke.FindExport("NativeAutoCleanupSmoke");
      string voidStringResult;
      int voidStringStatus = nativeVoid !is null ? nativeVoid.CallString(voidStringResult) : BML::ERROR_INTEROP_EXPORT_NOT_FOUND;
      BML::ExportRef@ nativeFailWithResult = nativeSmoke.FindExport("NativeFailWithResult");
      BML::CallFrame@ failFrame = BML::CallFrame();
      failFrame.SetResultInt(5);
      int failStatus = nativeFailWithResult !is null ? nativeFailWithResult.Call(failFrame) : BML::ERROR_INTEROP_EXPORT_NOT_FOUND;
      int failResultType = failFrame.ResultType;
      bool errorConstantsOk = BML::ERROR_INTEROP_EXPORT_AMBIGUOUS == -703 &&
                              BML::ERROR_INTEROP_SIGNATURE_MISMATCH == -705 &&
                              BML::ERROR_INTEROP_TYPE_MISMATCH == -707 &&
                              BML::ERROR_INTEROP_TARGET_EXECUTION_FAILED == -708 &&
                              BML::ERROR_INTEROP_HANDLE_STALE == -709 &&
                              BML::GetErrorString(BML::ERROR_INTEROP_EXPORT_AMBIGUOUS) != "";
      ctx.LogInfo("BML native export hardening script tryEcho=" + tryNativeEchoStatus +
                  " echoValid=" + BoolText(tryNativeEcho !is null && tryNativeEcho.IsValid) +
                  " ambiguous=" + tryNativeAmbiguousStatus +
                  " ambiguousNull=" + BoolText(tryNativeAmbiguous is null) +
                  " mismatch=" + tryNativeMismatchStatus +
                  " badSig=" + tryNativeBadSignatureStatus +
                  " missing=" + tryNativeMissingStatus +
                  " wrongArg=" + wrongArgStatus +
                  " exception=" + exceptionStatus +
                  " voidString=" + voidStringStatus +
                  " fail=" + failStatus +
                  " failResultType=" + failResultType +
                  " constants=" + BoolText(errorConstantsOk));
      if (nativeAddOne !is null && nativeAddOne.IsValid && nativeEcho !is null && nativeEcho.IsValid) {
        BML::CallFrame@ frame = BML::CallFrame();
        int frameSetStatus = frame.SetInt(0, 51);
        int frameCallStatus = frameSetStatus == 0 ? nativeAddOne.Call(frame) : frameSetStatus;
        int frameIntResult = 0;
        if (frameCallStatus == 0) {
          frame.GetResultInt(frameIntResult);
        }
        ctx.LogInfo("BML reusable frame NativeAddOne status=" + frameCallStatus + " result=" + frameIntResult);

        frame.Clear();
        frameSetStatus = frame.SetString(0, "frame");
        frameCallStatus = frameSetStatus == 0 ? nativeEcho.Call(frame) : frameSetStatus;
        string frameStringResult;
        if (frameCallStatus == 0) {
          frame.GetResultString(frameStringResult);
        }
        ctx.LogInfo("BML reusable frame NativeEcho status=" + frameCallStatus + " result=" + frameStringResult);
      }
    }

    if (self !is null && self.IsValid) {
      BML::ExportRef@ sum = self.FindExport("Sum");
      if (sum !is null && sum.IsValid) {
        BML::CallFrame@ scriptSumFrame = BML::CallFrame();
        int scriptSumStatus = scriptSumFrame.SetInt(0, 12);
        if (scriptSumStatus == 0) {
          scriptSumStatus = scriptSumFrame.SetInt(1, 30);
        }
        if (scriptSumStatus == 0) {
          scriptSumStatus = sum.Call(scriptSumFrame);
        }
        int scriptSumResult = 0;
        if (scriptSumStatus == 0) {
          scriptSumFrame.GetResultInt(scriptSumResult);
        }
        ctx.LogInfo("BML export Sum status=" + scriptSumStatus + " result=" + scriptSumResult);
      }
    }
  }

  void OnProcess(const BML::ModContext &in ctx) {
    if (!firstFrame) {
      return;
    }
    firstFrame = false;

    ctx.LogInfo("BML first process callback");
    ctx.LogInfo("ctx ingame=" + BoolText(ctx.IsInGame) +
                " inLevel=" + BoolText(ctx.IsInLevel) +
                " paused=" + BoolText(ctx.IsPaused) +
                " playing=" + BoolText(ctx.IsPlaying) +
                " cheat=" + BoolText(ctx.IsCheatEnabled));
    ctx.LogInfo("ctx/global runtime match=" + BoolText(ctx.IsPaused == BML::IsPaused() &&
                ctx.IsPlaying == BML::IsPlaying() &&
                ctx.IsCheatEnabled == BML::IsCheatEnabled()));
    ctx.LogInfo("global ingame=" + BoolText(BML::IsIngame()) +
                " inLevel=" + BoolText(BML::IsInLevel()) +
                " paused=" + BoolText(BML::IsPaused()));

    LogFailedModState(ctx, "script:BMLAngelScriptCompileErrorSmoke");
    LogFailedModState(ctx, "bml.runtime.error.smoke");

    ctx.SendIngameMessage("BML AngelScript ctx smoke");
    BML::SendIngameMessage("BML AngelScript global smoke");
    ctx.ExecuteCommand("echo bml-command-smoke");
    BML::ExecuteCommand("echo bml-command-global-smoke");
    BML::ExecuteCommand("assmoke alpha beta");
    BML::ExecuteCommand("assself first");
    BML::ExecuteCommand("assself second");
    ctx.LogInfo("BML content registration after OnLoad blocked=" + BoolText(!BML::RegisterModul("BMLAngelScriptSmokeLateModul")));
    ctx.OpenModsMenu();
    ctx.CloseModsMenu();
    BML::OpenMapMenu();
    BML::CloseMapMenu();
    BML::OpenModsMenu();
    BML::CloseModsMenu();
    ctx.ClearIngameMessages();
    BML::ClearIngameMessages();
    ctx.LogInfo("BML menu/message api: exercised=true");
    ctx.LogInfo("BML UI outside render safe=" + BoolText(!BML::UI::MainButton("outside-render")));
    ctx.LogInfo("BML ImGui outside render safe=" + BoolText(!ImGui::Button("outside-render")));
  }

  void OnRender(const BML::ModContext &in ctx, const BML::RenderEvent &in event) {
    if (!firstRender) {
      return;
    }
    firstRender = false;

    ctx.LogInfo("BML first render callback flags=" + event.Flags);
    int uiPages = BML::UI::CalcPageCount(9, 4);
    bool uiNavOk = !BML::UI::CanPrevPage(0) && BML::UI::CanNextPage(0, 9, 4);
    float uiButtonSize = BML::UI::GetButtonSizeCoordX(BML::UI::BUTTON_MAIN);
    BML::UI::Title("BML UI Smoke", 0.08f, 1.0f);
    BML::UI::WrappedText("BML UI smoke text", 240.0f, 0.0f, 1.0f);
    BML::UI::SetCursorCoord(0.4f, 0.30f);
    BML::UI::MainButton("BML UI Main");
    BML::UI::OptionButton("BML UI Option");
    BML::UI::LevelButton("BML UI Level", uiChoice);
    BML::UI::SmallButton("BML UI Small", uiChoice);
    BML::UI::LeftButton("Prev");
    BML::UI::RightButton("Next");
    BML::UI::PlusButton("Plus");
    BML::UI::MinusButton("Minus");
    BML::UI::YesNoButton("BML UI Bool", uiChoice);
    BML::UI::InputIntButton("BML UI Int", uiCounter);
    BML::UI::InputFloatButton("BML UI Float", uiFloat);
    BML::UI::SearchBar(uiSearch);
    BML::UI::NavLeft();
    BML::UI::NavRight();
    BML::UI::NavBack();
    BML::UI::PlayMenuClickSound();
    ctx.LogInfo("BML UI facade smoke: pages=" + uiPages +
                " nav=" + BoolText(uiNavOk) +
                " sizeCoord=" + uiButtonSize +
                " search=" + uiSearch);

    bool imguiWindow = ImGui::Begin("BML ImGui Smoke", ImGuiWindowFlags_AlwaysAutoResize);
    if (imguiWindow) {
      ImGui::TextUnformatted("BML advanced ImGui smoke");
      ImGui::Button("Button");
      ImGui::Checkbox("Check", imguiCheck);
      ImGui::SliderFloat("Slider", imguiFloat, 0.0f, 1.0f);
      ImGui::DragFloat("Drag", imguiFloat);
      ImGui::InputFloat("Float", imguiFloat);
      ImGui::InputInt("Int", imguiInt);
      ImGui::InputText("Text", imguiText, 64);
      ImGui::InputTextMultiline("Multiline", imguiMultiline, 128);
      ImGui::ComboText("Combo", imguiCombo, "One\nTwo\nThree");
      ImGui::ListBoxText("List", imguiList, "Alpha\nBeta\nGamma");
      ImGui::ProgressBar(0.25f);
      ImGui::ColorPicker4("Color", imguiColor);
      if (ImGui::BeginTable("Table", 2)) {
        ImGui::TableSetupColumn("A");
        ImGui::TableSetupColumn("B");
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("left");
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("right");
        ImGui::EndTable();
      }
      if (ImGui::TreeNode("Tree")) {
        ImGui::TextUnformatted("tree child");
        ImGui::TreePop();
      }
    }
    ImGui::End();

    ImDrawList@ draw = ImGui::GetForegroundDrawList();
    if (draw !is null) {
      draw.AddLine(ImVec2(8, 8), ImVec2(64, 8), 0xffffffff);
      draw.AddQuad(ImVec2(8, 16), ImVec2(64, 16), ImVec2(64, 48), ImVec2(8, 48), 0xff00ffff);
      draw.AddBezierCubic(ImVec2(72, 16), ImVec2(96, 0), ImVec2(120, 64), ImVec2(144, 16), 0xffff00ff);
      draw.AddNgon(ImVec2(176, 32), 20.0f, 0xff44ff44, 6);
      draw.AddPolyline4(ImVec2(200, 16), ImVec2(224, 32), ImVec2(200, 48), ImVec2(224, 64), 0xffffffff);
      draw.PathClear();
      draw.PathLineTo(ImVec2(240, 16));
      draw.PathArcTo(ImVec2(256, 32), 16.0f, 0.0f, 3.14f);
      draw.PathStroke(0xffffffff);
    }
    ctx.LogInfo("BML ImGui advanced smoke: window=" + BoolText(imguiWindow) +
                " combo=" + imguiCombo +
                " list=" + imguiList +
                " text=" + imguiText);
  }

  void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
    if (event == BML::GAME_EVENT_PRE_START_MENU && firstPreStartMenu) {
      firstPreStartMenu = false;
      ctx.LogInfo("BML first pre-start-menu callback");
    } else if (event == BML::GAME_EVENT_POST_START_MENU && firstPostStartMenu) {
      firstPostStartMenu = false;
      ctx.LogInfo("BML first post-start-menu callback");
    }
  }

  void OnCheatEnabled(const BML::ModContext &in ctx, const BML::CheatEvent &in event) {
    if (!firstCheatEnabled) {
      return;
    }
    firstCheatEnabled = false;

    ctx.LogInfo("BML cheat callback enable=" + BoolText(event.Enabled));
  }

  void OnLoadObject(const BML::ModContext &in ctx,
                    const BML::LoadObjectEvent &in event) {
    if (!firstLoadObject) {
      return;
    }
    firstLoadObject = false;

    ctx.LogInfo("BML load object filename=" + event.Filename +
                " isMap=" + BoolText(event.IsMap) +
                " filterClass=" + event.FilterClass +
                " addToScene=" + BoolText(event.AddToScene) +
                " dynamic=" + BoolText(event.IsDynamic));
  }

  void OnLoadScript(const BML::ModContext &in ctx, const BML::LoadScriptEvent &in event) {
    if (!firstLoadScript) {
      return;
    }
    firstLoadScript = false;

    ctx.LogInfo("BML load script filename=" + event.Filename + " scriptId=" + event.ScriptId);
  }

  void OnCommandEvent(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
    if (event.IsPre && firstPreCommand) {
      firstPreCommand = false;
      ctx.LogInfo("BML pre command callback command=" + event.CommandName +
                  " args=" + event.ArgsText +
                  " argCount=" + event.ArgCount +
                  " cheat=" + BoolText(event.IsCheat));
    } else if (event.IsPost && firstPostCommand) {
      firstPostCommand = false;
      ctx.LogInfo("BML post command callback command=" + event.CommandName +
                  " args=" + event.ArgsText +
                  " argCount=" + event.ArgCount +
                  " cheat=" + BoolText(event.IsCheat));
    }
  }

  void RecordSmokeCommandExecute(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
    if (!firstScriptCommand) {
      return;
    }
    firstScriptCommand = false;
    ctx.LogInfo("BML script command callback command=" + event.CommandName +
                " execute=" + BoolText(event.IsExecute) +
                " args=" + event.ArgsText +
                " count=" + event.ArgCount);
  }

  void RecordSmokeCommandComplete(const BML::ModContext &in ctx, const BML::CommandEvent &in event, BML::CommandCompletion &inout completions) {
    if (!event.IsComplete || event.CommandName != "assmoke") {
      ctx.LogWarn("BML script command completion unexpected event");
      return;
    }
    completions.Add("alpha");
    completions.Add("beta");
    ctx.LogInfo("BML script command completion callback command=" + event.CommandName +
                " args=" + event.ArgsText +
                " count=" + completions.Count);
  }

  void RecordSelfUnregisterCommandExecute(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
    if (!firstSelfUnregisterCommand) {
      return;
    }
    firstSelfUnregisterCommand = false;
    selfUnregisterCommandCalls++;
    ctx.LogInfo("BML self unregister command callback command=" + event.CommandName +
                " args=" + event.ArgsText +
                " unregistered=" + BoolText(ctx.UnregisterCommand(event.CommandName)));
  }

  void RecordSmokeTimerOnce(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
    timerOnceSeen = true;
    ctx.LogInfo("BML script timer once callback id=" + event.Id +
                " name=" + event.Name +
                " valid=" + BoolText(event.IsValid) +
                " state=" + event.State +
                " type=" + event.Type +
                " progress=" + event.Progress);
  }

  bool RecordSmokeTimerLoop(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
    timerLoopSeen = true;
    ctx.LogInfo("BML script timer loop callback id=" + event.Id +
                " name=" + event.Name +
                " remaining=" + event.RemainingIterations +
                " completed=" + event.CompletedIterations);
    return false;
  }

  void RecordCancelledTimer(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
    timerCancelledSeen = true;
    ctx.LogWarn("BML cancelled timer unexpectedly fired id=" + event.Id + " name=" + event.Name);
  }

  void OnPhysicalize(const BML::ModContext &in ctx,
                     const BML::PhysicalizeEvent &in event) {
    if (!firstPhysicalize) {
      return;
    }
    firstPhysicalize = false;

    ctx.LogInfo("BML physicalize callback targetId=" + event.TargetId +
                " target=" + event.TargetName +
                " fixed=" + BoolText(event.Fixed) +
                " mass=" + event.Mass +
                " friction=" + event.Friction +
                " elasticity=" + event.Elasticity +
                " collGroup=" + event.CollisionGroup +
                " surface=" + event.CollisionSurface +
                " center=" + event.MassCenterX + "," + event.MassCenterY + "," + event.MassCenterZ +
                " convex=" + event.ConvexCount +
                " ball=" + event.BallCount +
                " concave=" + event.ConcaveCount);
  }

  void OnUnphysicalize(const BML::ModContext &in ctx, const BML::ObjectEvent &in event) {
    if (!firstUnphysicalize) {
      return;
    }
    firstUnphysicalize = false;

    ctx.LogInfo("BML unphysicalize callback targetId=" + event.TargetId + " target=" + event.TargetName);
  }

  void OnUnload(const BML::ModContext &in ctx) {
    ctx.LogInfo("BML script mod smoke unloading");
    BML::DataShareRemove("AngelScriptSmoke");
    BML::DataShareRemove("AngelScriptSmokeBool");
    BML::DataShareRemove("AngelScriptSmokeInt");
    BML::DataShareRemove("AngelScriptSmokeFloat");
    BML::DataShareRemove("AngelScriptSmokeRequestImmediate");
    BML::DataShareRemove("AngelScriptSmokeRequestPending");
    BML::DataShareRemove("AngelScriptSmokeRequestBool");
    if (smokeCommand !is null && smokeCommand.IsValid) {
      smokeCommand.Unregister();
    }
    if (selfCommand !is null && selfCommand.IsValid) {
      selfCommand.Unregister();
    }
    ctx.LogInfo("BML script timer summary: once=" + BoolText(timerOnceSeen) +
                " loop=" + BoolText(timerLoopSeen) +
                " cancelled=" + BoolText(timerCancelledSeen) +
                " refsInvalid=" + BoolText((onceTimer is null || !onceTimer.IsValid) &&
                                           (loopTimer is null || !loopTimer.IsValid) &&
                                           (cancelledTimer is null || !cancelledTimer.IsValid)) +
                " commandRefsInvalid=" + BoolText((smokeCommand is null || !smokeCommand.IsValid) &&
                                                  (selfCommand is null || !selfCommand.IsValid)) +
                " dataShareRefsInvalid=" + BoolText((dataShareImmediateRequest is null || !dataShareImmediateRequest.IsValid) &&
                                                    (dataSharePendingRequest is null || !dataSharePendingRequest.IsValid) &&
                                                    (dataShareBoolRequest is null || !dataShareBoolRequest.IsValid)) +
                " command=" + BoolText(!firstScriptCommand) +
                " selfCommandCalls=" + selfUnregisterCommandCalls);
  }
}

class SmokeOnceTimer : BML::Timer {
  BMLBindingsSmokeMod@ owner;

  SmokeOnceTimer(BMLBindingsSmokeMod@ mod) {
    @owner = mod;
  }

  string get_Name() const { return "bml-smoke-once"; }
  uint get_DelayTicks() const { return 1; }

  bool Tick(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
    owner.RecordSmokeTimerOnce(ctx, event);
    return false;
  }
}

class SmokeLoopTimer : BML::Timer {
  BMLBindingsSmokeMod@ owner;

  SmokeLoopTimer(BMLBindingsSmokeMod@ mod) {
    @owner = mod;
  }

  string get_Name() const { return "bml-smoke-loop"; }
  uint get_DelayTicks() const { return 1; }
  bool get_Loop() const { return true; }

  bool Tick(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
    return owner.RecordSmokeTimerLoop(ctx, event);
  }
}

class SmokeCancelledTimer : BML::Timer {
  BMLBindingsSmokeMod@ owner;

  SmokeCancelledTimer(BMLBindingsSmokeMod@ mod) {
    @owner = mod;
  }

  string get_Name() const { return "bml-smoke-cancelled"; }
  uint get_DelayTicks() const { return 1; }

  bool Tick(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
    owner.RecordCancelledTimer(ctx, event);
    return false;
  }
}

class SmokeInvalidTimer : BML::Timer {
  bool Tick(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
    return false;
  }
}

class SmokeDataShareRequest : BML::DataShareRequest {
  BMLBindingsSmokeMod@ owner;
  string key;
  int type;

  SmokeDataShareRequest(BMLBindingsSmokeMod@ mod, const string &in requestKey, int requestType) {
    @owner = mod;
    key = requestKey;
    type = requestType;
  }

  string get_Key() const { return key; }
  int get_Type() const { return type; }

  void Receive(const BML::ModContext &in ctx, const BML::DataShareEvent &in event) {
    if (type == BML::DATASHARE_STRING) {
      owner.dataShareImmediateSeen = event.Exists &&
                                     event.Key == "AngelScriptSmokeRequestImmediate" &&
                                     event.StringValue == "ready";
      ctx.LogInfo("BML datashare request immediate callback key=" + event.Key +
                  " exists=" + owner.BoolText(event.Exists) +
                  " value=" + event.StringValue);
    } else if (type == BML::DATASHARE_INT) {
      owner.dataSharePendingSeen = event.Exists &&
                                   event.Key == "AngelScriptSmokeRequestPending" &&
                                   event.IntValue == 77;
      ctx.LogInfo("BML datashare request pending callback key=" + event.Key +
                  " exists=" + owner.BoolText(event.Exists) +
                  " value=" + event.IntValue);
    } else if (type == BML::DATASHARE_BOOL) {
      owner.dataShareBoolSeen = event.Exists &&
                                event.Key == "AngelScriptSmokeRequestBool" &&
                                event.BoolValue;
      ctx.LogInfo("BML datashare request bool callback key=" + event.Key +
                  " exists=" + owner.BoolText(event.Exists) +
                  " value=" + owner.BoolText(event.BoolValue));
    }
  }
}

class SmokeInvalidDataShareRequest : BML::DataShareRequest {
  string get_Key() const { return ""; }
  int get_Type() const { return BML::DATASHARE_STRING; }

  void Receive(const BML::ModContext &in ctx, const BML::DataShareEvent &in event) {
    ctx.LogWarn("BML invalid datashare request unexpectedly fired");
  }
}

class SmokeCommand : BML::Command {
  BMLBindingsSmokeMod@ owner;

  SmokeCommand(BMLBindingsSmokeMod@ mod) {
    @owner = mod;
  }

  string get_Name() const { return "assmoke"; }
  string get_Alias() const { return "ass"; }
  string get_Description() const { return "AngelScript smoke command"; }
  string get_Usage() const { return "assmoke [value]"; }
  string get_Category() const { return "Smoke"; }
  bool get_Cheat() const { return false; }
  bool get_Hidden() const { return false; }
  bool get_Enabled() const { return true; }

  void Execute(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
    owner.RecordSmokeCommandExecute(ctx, event);
  }

  void Complete(const BML::ModContext &in ctx, const BML::CommandEvent &in event, BML::CommandCompletion &inout completions) {
    owner.RecordSmokeCommandComplete(ctx, event, completions);
  }
}

class SmokeInvalidCommand : BML::Command {
  string get_Name() const { return ""; }

  void Execute(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
    ctx.LogWarn("BML invalid command unexpectedly executed");
  }
}

class SelfUnregisterCommand : BML::Command {
  BMLBindingsSmokeMod@ owner;

  SelfUnregisterCommand(BMLBindingsSmokeMod@ mod) {
    @owner = mod;
  }

  string get_Name() const { return "assself"; }

  void Execute(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
    owner.RecordSelfUnregisterCommandExecute(ctx, event);
  }
}

class EchoCollisionCommand : BML::Command {
  BMLBindingsSmokeMod@ owner;

  EchoCollisionCommand(BMLBindingsSmokeMod@ mod) {
    @owner = mod;
  }

  string get_Name() const { return "echo"; }

  void Execute(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
    owner.RecordSmokeCommandExecute(ctx, event);
  }
}

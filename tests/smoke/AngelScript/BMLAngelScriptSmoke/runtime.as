int menuPageEnterCount = 0;
int menuPageLeaveCount = 0;

void DrawSmokeMenuPage(BML::MenuPageFrame &inout frame) {
  ImGui::TextUnformatted("Script Mod menu page");
  if (ImGui::Button("Open child")) frame.Push("child");
}

void DrawSmokeMenuChild(BML::MenuPageFrame &inout frame) {
  ImGui::TextUnformatted("Hidden child page");
  if (ImGui::Button("Return to overview")) frame.Back();
}

void EnterSmokeMenuPage(BML::MenuPageEnterReason reason) {
  ++menuPageEnterCount;
}

void LeaveSmokeMenuPage(BML::MenuPageLeaveReason reason) {
  ++menuPageLeaveCount;
}

[bml.mod id="bml.bindings.smoke" name="BML Bindings Smoke" version="1.0.0" author="BML+" bml="0.3.14" reload="auto" description="Smoke test for BML's built-in AngelScript capability APIs."]
[bml.require id="BML" version="0.3.14"]
class BMLBindingsSmokeMod {
  bool loggedGameplay = false;
  array<BML::Gameplay::CatalogEntry> catalogCache;
  BML::MenuPageRef@ menuPage;
  BML::MenuPageRef@ menuChild;

  void OnLoad(const BML::ModContext &in ctx) {
    bool stateOk = !ctx.IsPlaying || (ctx.IsInLevel && !ctx.IsPaused);
    bool clockOk = ctx.GetFrameCount() >= 0 && ctx.GetTimeMs() >= 0.0f;
    int highScore = 0;
    int highScoreStatus = BML::Gameplay::ReadHighScore(highScore);
    bool highScoreOk = ctx.IsInLevel
        ? highScoreStatus == BML::ERROR_OK && highScore == ctx.GetHSScore()
        : highScoreStatus == BML::ERROR_UNAVAILABLE;
    ctx.LogInfo("BML capability smoke: state=" + (stateOk ? "true" : "false") +
                " clock=" + (clockOk ? "true" : "false") +
                " highscore=" + (highScoreOk ? "true" : "false"));

    CKContext@ host = ctx.BorrowCKContext();
    CKObject@ raw = host is null
        ? null
        : host.CreateObject(CKCID_OBJECT, "__BML_RawHandleValidityProbe");
    bool rawLive = BML::CK::IsValid(raw);
    if (host !is null && raw !is null) {
      // Older supported CKAngelScript builds expose this as one declaration
      // with a non-compilable reference default. Supplying all arguments also
      // exercises the same API on newer builds that publish explicit overloads.
      CKDependencies dependencies;
      host.DestroyObject(raw, 0, dependencies);
    }
    bool rawDeleted = !BML::CK::IsValid(raw);
    ctx.LogInfo("BML raw handle validity smoke: live=" + (rawLive ? "true" : "false") +
                " deleted=" + (rawDeleted ? "true" : "false"));
    ctx.LogInfo("BML script mod summary: capabilities");

    BML::MenuPageDefinition entry;
    entry.Id = "overview";
    entry.Label = "Script diagnostics";
    entry.Description = "Script-owned Mods menu page";
    @menuPage = ctx.RegisterMenuPage(entry, DrawSmokeMenuPage,
                                     EnterSmokeMenuPage, LeaveSmokeMenuPage);
    BML::MenuPageDefinition child;
    child.Id = "child";
    child.Label = "Child";
    child.ShowInDetails = false;
    @menuChild = ctx.RegisterMenuPage(child, DrawSmokeMenuChild);
    ctx.LogInfo("BML script menu pages: entry=" +
                (menuPage !is null && menuPage.IsValid ? "true" : "false") +
                " child=" +
                (menuChild !is null && menuChild.IsValid ? "true" : "false"));
  }

  void OnProcess(const BML::ModContext &in ctx) {
    if (!loggedGameplay) {
      int count = 0;
      int status = BML::Gameplay::ReadCatalogCount(count);
      if (status == BML::ERROR_OK) {
        BML::Gameplay::CatalogEntry first;
        BML::Gameplay::CatalogEntry missing;
        int checkpointCount = 0;
        int resetpointCount = 0;
        BML::Gameplay::Checkpoint checkpoint;
        BML::Gameplay::Resetpoint resetpoint;
        bool valuesOk =
            count > 0 &&
            BML::Gameplay::ReadCatalogEntry(0, first) == BML::ERROR_OK &&
            first.File.length() > 0 &&
            BML::Gameplay::ReadCatalogEntry(-1, missing) == BML::ERROR_INVALID_PARAMETER &&
            BML::Gameplay::ReadCatalogEntry(count, missing) == BML::ERROR_NOT_FOUND &&
            BML::Gameplay::ReadCheckpointCount(checkpointCount) == BML::ERROR_OK &&
            (checkpointCount == 0 ||
             BML::Gameplay::ReadCheckpoint(0, checkpoint) == BML::ERROR_OK) &&
            BML::Gameplay::ReadCheckpoint(checkpointCount, checkpoint) == BML::ERROR_NOT_FOUND &&
            BML::Gameplay::ReadResetpointCount(resetpointCount) == BML::ERROR_OK &&
            (resetpointCount == 0 ||
             BML::Gameplay::ReadResetpoint(0, resetpoint) == BML::ERROR_OK) &&
            BML::Gameplay::ReadResetpoint(resetpointCount, resetpoint) == BML::ERROR_NOT_FOUND;
        if (valuesOk) {
          catalogCache.insertLast(first);
          valuesOk = catalogCache.length() == 1 && catalogCache[0].File == first.File;
        }
        ctx.LogInfo("BML gameplay snapshot: status=" + status +
                    " count=" + count +
                    " values=" + (valuesOk ? "true" : "false"));
        loggedGameplay = true;
      } else if (status != BML::ERROR_UNAVAILABLE) {
        ctx.LogInfo("BML gameplay snapshot: status=" + status +
                    " count=-1 values=false");
        loggedGameplay = true;
      }
    }

  }

  void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
    if (event == BML::GameEvent::GAME_EVENT_EXIT_GAME)
      ctx.LogInfo("BML script event callback: exit_game");
  }

  void OnUnload(const BML::ModContext &in ctx) {
    if (menuChild !is null) menuChild.Unregister();
    if (menuPage !is null) menuPage.Unregister();
    ctx.LogInfo("BML script menu page callbacks: enter=" + menuPageEnterCount +
                " leave=" + menuPageLeaveCount);
    ctx.LogInfo("Goodbye!");
  }
}

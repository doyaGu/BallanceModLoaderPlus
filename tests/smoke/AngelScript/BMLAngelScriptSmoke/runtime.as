[bml.mod id="bml.bindings.smoke" name="BML Bindings Smoke" version="1.0.0" author="BML+" bml="0.3.13" description="Smoke test for BML's built-in AngelScript capability APIs."]
[bml.require id="BML" version="0.3.13"]

class BMLBindingsSmokeMod {
  bool loggedGameplay = false;
  array<BML::Gameplay::CatalogEntry> catalogCache;

  void OnLoad(const BML::ModContext &in ctx) {
    BML::Runtime::State runtime = BML::Runtime::GetState();
    BML::Runtime::Clock clock = BML::Runtime::GetClock();
    BML::Runtime::Score score = BML::Runtime::GetScore();
    bool runtimeOk = runtime.Playing == (runtime.InGame && !runtime.Paused) &&
                     clock.Frame >= 0 && score.HS >= 0;
    ctx.LogInfo("BML capability smoke: runtime=" + (runtimeOk ? "true" : "false"));

    CKContext@ host = ctx.BorrowCKContext();
    CKObject@ raw = host is null
        ? null
        : host.CreateObject(CKCID_OBJECT, "__BML_RawHandleValidityProbe");
    bool rawLive = BML::CK::IsValid(raw);
    if (host !is null && raw !is null) {
      host.DestroyObject(raw);
    }
    bool rawDeleted = !BML::CK::IsValid(raw);
    ctx.LogInfo("BML raw handle validity smoke: live=" + (rawLive ? "true" : "false") +
                " deleted=" + (rawDeleted ? "true" : "false"));
    ctx.LogInfo("BML script mod summary: capabilities");
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
    ctx.LogInfo("Goodbye!");
  }
}

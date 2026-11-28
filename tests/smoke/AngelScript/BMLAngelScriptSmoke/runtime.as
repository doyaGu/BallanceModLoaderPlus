int menuPageEnterCount = 0;
int menuPageLeaveCount = 0;

void DrawSmokeMenuPage(BML::MenuPageFrame &inout frame) {
  ImGui::TextUnformatted("Script Mod menu page");
  if (BML::UI::MainButton("Open child")) frame.Push("child");
}

void DrawSmokeMenuChild(BML::MenuPageFrame &inout frame) {
  ImGui::TextUnformatted("Hidden child page");
  if (BML::UI::MainButton("Return to overview")) frame.Back();
}

void EnterSmokeMenuPage(BML::MenuPageEnterReason reason) {
  ++menuPageEnterCount;
}

void LeaveSmokeMenuPage(BML::MenuPageLeaveReason reason) {
  ++menuPageLeaveCount;
}

string CheckDataArrayText(CKContext@ host) {
  if (host is null) return "FAIL: no CK context";
  CKObject@ object = host.CreateObject(CKCID_DATAARRAY, "__BML_DataArrayTextProbe");
  CKDataArray@ table = cast<CKDataArray>(object);
  if (table is null) return "FAIL: cannot create the DataArray";
  // Append is supported by both CK runtimes when the table starts empty.
  table.InsertColumn(-1, CKARRAYTYPE_STRING, "Text");
  table.InsertRow(-1);

  array<string> candidates = {"café", "茅", "あ", "Ж"};
  const bool hasCell = table.GetRowCount() == 1 && table.GetColumnCount() == 1;
  string result = hasCell ? "FAIL: no representable non-ASCII value"
                          : "FAIL: could not create a one-cell string table";
  for (uint i = 0; hasCell && i < candidates.length(); ++i) {
    const string text = candidates[i];
    if (!BML::CK::SetString(table, 0, 0, text)) continue;
    string embeddedNul = "before after";
    embeddedNul[6] = 0;
    if (BML::CK::GetString(table, 0, 0, "missing") != text)
      result = "FAIL: non-ASCII string changed on read";
    else if (BML::CK::SetString(table, 0, 0, embeddedNul))
      result = "FAIL: embedded NUL accepted";
    else if (BML::CK::GetString(table, 0, 0, "missing") != text)
      result = "FAIL: rejected write changed the stored value";
    else if (BML::CK::GetColumnName(table, 0) != "Text" ||
             BML::CK::FindColumn(table, "Text") != 0)
      result = "FAIL: column name lookup failed";
    else if (BML::CK::GetString(table, 1, 0, "missing") != "missing")
      result = "FAIL: missing cell did not use the default";
    else if (!BML::CK::SetString(table, 0, 0, "") ||
             !BML::CK::GetString(table, 0, 0, "missing").isEmpty())
      result = "FAIL: cannot round-trip an empty string";
    else
      result = "PASS";
    break;
  }
  CKDependencies dependencies;
  host.DestroyObject(object, 0, dependencies);
  return result;
}

[bml.mod id="bml.bindings.smoke" name="BML Bindings Smoke" version="1.0.0" author="BML+" bml="0.4.0" reload="auto" description="Smoke test for BML's built-in AngelScript capability APIs."]
[bml.require id="BML" version="0.4.0"]
class BMLBindingsSmokeMod {
  bool loggedGameplay = false;
  array<BML::Gameplay::CatalogEntry> catalogCache;
  BML::MenuPageRef@ menuPage;
  BML::MenuPageRef@ menuChild;

  void OnLoad(const BML::ModContext &in ctx) {
    BML::Gameplay::State state;
    BML::Time::Clock clock;
    bool stateOk = BML::Gameplay::ReadState(state) == BML::ERROR_OK &&
                   (!state.Playing || !state.Paused);
    bool clockOk = BML::Time::ReadClock(clock) == BML::ERROR_OK;
    int highScore = 0;
    int highScoreStatus = BML::Gameplay::ReadHighScore(highScore);
    bool highScoreOk = (state.Playing || state.Paused)
        ? highScoreStatus == BML::ERROR_OK : highScoreStatus == BML::ERROR_UNAVAILABLE;
    ctx.LogInfo("BML capability smoke: state=" + (stateOk ? "true" : "false") +
                " clock=" + (clockOk ? "true" : "false") +
                " highscore=" + (highScoreOk ? "true" : "false"));

    CKContext@ host;
    BML::Engine::BorrowContext(ctx, host);
    ctx.LogInfo("BML DataArray text smoke: " + CheckDataArrayText(host));
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
    ctx.RegisterMenuPage(entry, DrawSmokeMenuPage, menuPage,
                                     EnterSmokeMenuPage, LeaveSmokeMenuPage);
    BML::MenuPageDefinition child;
    child.Id = "child";
    child.Label = "Child";
    child.ShowInDetails = false;
    ctx.RegisterMenuPage(child, DrawSmokeMenuChild, menuChild);
    ctx.LogInfo("BML script menu pages: entry=" +
                (menuPage !is null && menuPage.IsValid() ? "true" : "false") +
                " child=" +
                (menuChild !is null && menuChild.IsValid() ? "true" : "false"));
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

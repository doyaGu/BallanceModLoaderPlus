[bml.mod id="complete.mod" name="Complete Mod" version="1.0.0" author="Tutorial" bml="0.3.13" description="A complete tutorial mod with config, command, UI, and physics"]
class CompleteMod {
    // ---- 配置 ----
    private bool showWindow = true;
    private bool autoSpawn = false;
    private BML::ConfigProperty@ showWindowProp;
    private BML::ConfigProperty@ autoSpawnProp;

    // ---- 命令 ----
    private BML::CommandRef@ commandRef;

    // ---- 物理对象 ----
    private CK3dEntity@ ball = null;
    private bool ballPhysical = false;
    private bool forceActive = false;
    private int forceFramesLeft = 0;

    // ---- 状态 ----
    private string lastAction = "loaded";

    // ==== 生命周期 ====

    void OnLoad(const BML::ModContext &in ctx) {
        LoadConfig(ctx);
        RegisterCommand(ctx);
        ctx.LogInfo("CompleteMod loaded");
    }

    void OnUnload(const BML::ModContext &in ctx) {
        CleanupBall(ctx, "unload");
        if (commandRef !is null && commandRef.IsValid) {
            commandRef.Unregister();
        }
        ctx.LogInfo("CompleteMod unloaded");
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL ||
            event == BML::GAME_EVENT_PRE_LOAD_LEVEL ||
            event == BML::GAME_EVENT_EXIT_GAME) {
            CleanupBall(ctx, "game-event");
        }

        if (event == BML::GAME_EVENT_START_LEVEL && autoSpawn && ball is null) {
            SpawnBall(ctx, "auto");
        }
    }

    void OnProcess(const BML::ModContext &in ctx) {
        HandleInput(ctx);
        TickForce(ctx);
        if (showWindow) {
            DrawWindow(ctx);
        }
    }

    // ==== 配置 ====

    private void LoadConfig(const BML::ModContext &in ctx) {
        BML::Config@ config = ctx.BorrowConfig();
        if (config is null) return;

        config.SetCategoryComment("CompleteMod", "Complete Mod settings");

        @showWindowProp = config.GetProperty("CompleteMod", "ShowWindow");
        if (showWindowProp !is null) {
            showWindowProp.SetDefaultBoolean(true);
            showWindowProp.SetComment("Show the status window.");
            showWindow = showWindowProp.GetBoolean(true);
        }

        @autoSpawnProp = config.GetProperty("CompleteMod", "AutoSpawn");
        if (autoSpawnProp !is null) {
            autoSpawnProp.SetDefaultBoolean(false);
            autoSpawnProp.SetComment("Spawn ball automatically on level start.");
            autoSpawn = autoSpawnProp.GetBoolean(false);
        }
    }

    // ==== 命令 ====

    private void RegisterCommand(const BML::ModContext &in ctx) {
        BML::CommandDefinition def;
        def.Name = "cmod";
        def.Alias = "cm";
        def.Description = "Complete Mod control";
        def.Usage = "cmod [spawn|despawn|push|status|window]";
        def.Category = "Tutorial";
        def.Enabled = true;

        BML::CommandCallback@ execute = BML::CommandCallback(this.OnCommand);
        BML::CommandCompletionCallback@ complete = BML::CommandCompletionCallback(this.CompleteCommand);
        @commandRef = ctx.RegisterCommand(def, execute, complete);
    }

    private void OnCommand(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
        string action = "status";
        if (event.ArgCount > 0) {
            action = event.GetArg(0);
        }

        if (action == "spawn") {
            SpawnBall(ctx, "command");
        } else if (action == "despawn") {
            CleanupBall(ctx, "command");
        } else if (action == "push") {
            PushBall(ctx);
        } else if (action == "window") {
            showWindow = !showWindow;
            SaveBoolean(showWindowProp, showWindow);
            SetLast(ctx, "window=" + BoolText(showWindow));
        } else {
            SetLast(ctx, "status ball=" + BoolText(ball !is null) +
                         " physical=" + BoolText(ballPhysical) +
                         " force=" + BoolText(forceActive));
        }
    }

    private void CompleteCommand(const BML::ModContext &in ctx,
                                  const BML::CommandEvent &in event,
                                  BML::CommandCompletion &inout completions) {
        completions.Add("spawn");
        completions.Add("despawn");
        completions.Add("push");
        completions.Add("status");
        completions.Add("window");
    }

    // ==== ImGui 窗口 ====

    private void DrawWindow(const BML::ModContext &in ctx) {
        ImGui::SetNextWindowSize(ImVec2(350.0f, 180.0f), ImGuiCond_Once);

        if (ImGui::Begin("Complete Mod", showWindow)) {
            ImGui::TextUnformatted("Game: inLevel=" + BoolText(ctx.IsInLevel) +
                                   " playing=" + BoolText(ctx.IsPlaying));
            ImGui::TextUnformatted("Ball: exists=" + BoolText(ball !is null) +
                                   " physical=" + BoolText(ballPhysical));
            ImGui::TextUnformatted("Force: active=" + BoolText(forceActive) +
                                   " frames=" + forceFramesLeft);
            ImGui::TextUnformatted("Last: " + lastAction);

            if (ImGui::Button("Spawn", ImVec2(80.0f, 0.0f))) {
                SpawnBall(ctx, "ui");
            }
            ImGui::SameLine();
            if (ImGui::Button("Push", ImVec2(80.0f, 0.0f))) {
                PushBall(ctx);
            }
            ImGui::SameLine();
            if (ImGui::Button("Despawn", ImVec2(80.0f, 0.0f))) {
                CleanupBall(ctx, "ui");
            }
        }
        ImGui::End();
    }

    // ==== 输入 ====

    private void HandleInput(const BML::ModContext &in ctx) {
        BML::InputHook@ input = ctx.BorrowInputManager();
        if (input is null) return;

        if (input.IsKeyPressed(CKKEY_J)) {
            SpawnBall(ctx, "key");
        }
        if (input.IsKeyPressed(CKKEY_K)) {
            CleanupBall(ctx, "key");
        }
        if (input.IsKeyPressed(CKKEY_I)) {
            PushBall(ctx);
        }
    }

    // ==== 物理对象管理 ====

    private void SpawnBall(const BML::ModContext &in ctx, const string &in reason) {
        if (ball !is null) {
            CleanupBall(ctx, "respawn");
        }

        string relativePath = "3D Entities\\PH\\P_Ball_Wood.nmo";
        string resourcePath = BML::Path::Combine(ctx.GetDirectoryUtf8(BML::DIR_GAME), relativePath);
        if (!BML::Path::IsFile(resourcePath)) {
            SetLast(ctx, "resource missing");
            return;
        }

        BML::ObjectLoadOptions options;
        options.File = resourcePath;
        options.Rename = true;
        options.MasterName = "CompleteMod_Ball";
        options.AddToScene = true;
        options.ReuseMeshes = true;
        options.ReuseMaterials = true;
        options.Dynamic = true;

        BML::ObjectLoadResult@ result = BML::CK::LoadObject(options);
        if (result is null || !result.Success || result.Count <= 0) {
            SetLast(ctx, "load failed");
            return;
        }

        @ball = FindFirstEntity(result);
        if (ball is null) {
            SetLast(ctx, "no entity found");
            return;
        }

        CK3dEntity@ playerBall = BorrowActiveBall(ctx);
        if (playerBall !is null && BML::CK::IsValid(playerBall)) {
            VxVector pos = BML::CK::GetPosition(playerBall);
            pos.y += 5.0f;
            BML::CK::SetPosition(ball, pos);
        } else {
            BML::CK::SetPosition(ball, VxVector(0.0f, 10.0f, 0.0f));
        }

        BML::PhysicalizeDefinition physics;
        physics.Fixed = false;
        physics.Friction = 0.6f;
        physics.Elasticity = 0.2f;
        physics.Mass = 2.0f;
        physics.EnableCollision = true;
        physics.LinearDamp = 0.6f;
        physics.RotDamp = 0.1f;
        physics.CollisionSurface = "P_Ball_Wood_Mesh";

        ballPhysical = BML::Physics::PhysicalizeBall(ball, physics,
                                                      VxVector(0.0f, 0.0f, 0.0f), 2.0f);
        BML::CK::Show(ball, CKSHOW, true);
        SetLast(ctx, "spawned by " + reason + " physical=" + BoolText(ballPhysical));
    }

    private void PushBall(const BML::ModContext &in ctx) {
        if (ball is null || !BML::CK::IsValid(ball)) {
            SetLast(ctx, "no ball to push");
            return;
        }

        BML::Physics::WakeUp(ball);
        bool ok = BML::Physics::Impulse(
            ball,
            VxVector(0.0f, 0.0f, 0.0f), null,
            VxVector(1.0f, 0.0f, 0.0f), null,
            1.5f);

        SetLast(ctx, "push ok=" + BoolText(ok));
    }

    private void CleanupBall(const BML::ModContext &in ctx, const string &in reason) {
        ClearBallForce(ctx);

        if (ball is null) return;

        if (BML::CK::IsValid(ball)) {
            if (ballPhysical) {
                BML::Physics::Unphysicalize(ball);
            }
            BML::CK::Show(ball, CKHIDE, true);
        }

        @ball = null;
        ballPhysical = false;
        SetLast(ctx, "cleaned by " + reason);
    }

    // ==== 持续力 ====

    private void TickForce(const BML::ModContext &in ctx) {
        if (!forceActive) return;
        forceFramesLeft--;
        if (forceFramesLeft <= 0) {
            ClearBallForce(ctx);
        }
    }

    private void ClearBallForce(const BML::ModContext &in ctx) {
        if (!forceActive) return;
        if (ball !is null && BML::CK::IsValid(ball)) {
            BML::Physics::ClearForce(ball);
        }
        forceActive = false;
        forceFramesLeft = 0;
    }

    // ==== 工具函数 ====

    private CK3dEntity@ BorrowActiveBall(const BML::ModContext &in ctx) {
        CKDataArray@ currentLevel = ctx.BorrowDataArrayByName("CurrentLevel");
        if (currentLevel is null) return null;

        int col = BML::CK::FindColumn(currentLevel, "ActiveBall");
        if (col < 0) return null;

        CKObject@ object = currentLevel.GetElementObject(0, col);
        return cast<CK3dEntity>(object);
    }

    private CK3dEntity@ FindFirstEntity(BML::ObjectLoadResult@ result) {
        for (int i = 0; i < result.Count; i++) {
            CKObject@ object = result.BorrowObject(i);
            CK3dEntity@ entity = cast<CK3dEntity>(object);
            if (entity !is null) return entity;
        }
        return null;
    }

    private void SaveBoolean(BML::ConfigProperty@ property, bool value) {
        if (property !is null && property.IsValid) {
            property.SetBoolean(value);
        }
    }

    private void SetLast(const BML::ModContext &in ctx, const string &in text) {
        lastAction = text;
        ctx.LogInfo(text);
        BML::UI::AddMessage("CompleteMod: " + text);
    }

    private string BoolText(bool value) {
        return value ? "true" : "false";
    }

}

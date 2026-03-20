// ScriptHello - Full demo of BML AngelScript scripting

float g_SpeedMul = 1.0f;

void OnAttach() {
    bmlLog("Hello from AngelScript!");
    bmlSubscribe("BML/Input/KeyDown", "OnKeyDown");
    g_SpeedMul = bmlConfigGetFloat("Settings", "SpeedMultiplier", 1.0f);
    bmlPrint("ScriptHello loaded (speed=" + g_SpeedMul + "x)");
}

void OnDetach() {
    bmlConfigSetFloat("Settings", "SpeedMultiplier", g_SpeedMul);
}

void OnInit() {
    bmlLog("Virtools engine ready - starting intro sequence");
    IntroSequence();
}

// Coroutine: runs across multiple frames
void IntroSequence() {
    bmlPrint("Welcome to Ballance!");
    bmlWaitSeconds(2.0f);
    bmlPrint("Mod system ready.");
    bmlWaitSeconds(1.0f);

    CK3dObject@ ball = Find3dObject("Ball_Paper_01");
    if (ball !is null) {
        VxVector pos;
        ball.GetPosition(pos);
        bmlPrint("Ball position: " + pos.x + ", " + pos.y + ", " + pos.z);
    }
}

void OnKeyDown(int key) {
    if (key == CKKEY_F5) {
        ShowBallInfo();
    }
    if (key == CKKEY_F6) {
        g_SpeedMul += 0.5f;
        bmlConfigSetFloat("Settings", "SpeedMultiplier", g_SpeedMul);
        bmlPrint("Speed: " + g_SpeedMul + "x");
    }
    if (key == CKKEY_F7) {
        // Delayed action via coroutine
        DelayedAction();
    }
}

void ShowBallInfo() {
    CK3dObject@ ball = Find3dObject("Ball_Paper_01");
    if (ball !is null) {
        VxVector pos;
        ball.GetPosition(pos);
        bmlPrint("Ball at: " + pos.x + ", " + pos.y + ", " + pos.z);
    } else {
        bmlPrint("Ball not found");
    }
}

void DelayedAction() {
    bmlPrint("Action in 3...");
    bmlWaitSeconds(1.0f);
    bmlPrint("2...");
    bmlWaitSeconds(1.0f);
    bmlPrint("1...");
    bmlWaitSeconds(1.0f);
    bmlPrint("Go!");
    ShowBallInfo();
}

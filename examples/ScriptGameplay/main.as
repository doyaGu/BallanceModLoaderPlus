// ScriptGameplay - Gameplay modification example
// Demonstrates: object manipulation, periodic polling, config, coroutines

bool g_Enabled = true;
float g_BounceHeight = 5.0f;

void OnAttach() {
    bmlSubscribe("BML/Input/KeyDown", "OnKeyDown");
    g_Enabled = bmlConfigGetBool("Bounce", "Enabled", true);
    g_BounceHeight = bmlConfigGetFloat("Bounce", "Height", 5.0f);
}

void OnDetach() {
    bmlConfigSetBool("Bounce", "Enabled", g_Enabled);
    bmlConfigSetFloat("Bounce", "Height", g_BounceHeight);
}

void OnInit() {
    if (g_Enabled) {
        bmlPrint("[Gameplay] Bounce mod active (height=" + g_BounceHeight + ")");
        bmlPrint("[Gameplay] F8=Toggle, F9=Bounce ball, Shift+F9=Height+1");
    }
}

void OnKeyDown(int key) {
    if (key == CKKEY_F8) {
        g_Enabled = !g_Enabled;
        bmlPrint("[Gameplay] Bounce " + (g_Enabled ? "enabled" : "disabled"));
    }
    if (key == CKKEY_F9) {
        if (bmlIsKeyDown(CKKEY_LSHIFT)) {
            g_BounceHeight += 1.0f;
            bmlPrint("[Gameplay] Bounce height: " + g_BounceHeight);
        } else if (g_Enabled) {
            BounceBall();
        }
    }
}

void BounceBall() {
    CK3dObject@ ball = Find3dObject("Ball_Paper_01");
    if (ball is null) { bmlPrint("[Gameplay] Ball not found"); return; }

    VxVector pos;
    ball.GetPosition(pos);
    float startY = pos.y;
    float targetY = startY + g_BounceHeight;

    // Animate up over 30 frames
    for (int i = 0; i < 30; i++) {
        float t = float(i) / 30.0f;
        pos.y = startY + (targetY - startY) * t;
        ball.SetPosition(pos);
        bmlYield();
    }

    // Animate down over 30 frames
    for (int i = 0; i < 30; i++) {
        float t = float(i) / 30.0f;
        pos.y = targetY - (targetY - startY) * t;
        ball.SetPosition(pos);
        bmlYield();
    }

    // Restore exact position
    pos.y = startY;
    ball.SetPosition(pos);
}

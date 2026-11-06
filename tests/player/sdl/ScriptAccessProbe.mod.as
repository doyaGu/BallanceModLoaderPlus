[bml.mod id="SdlScriptAccessProbe" name="Script access regression" version="1.0.0"]
class ScriptAccessProbe {
    private CKObject@ borrowed;

    void OnLoad(const BML::ModContext &in ctx) {
        Mark(ctx, "bml_probe_script_access", BML::CommandCallback(this.OnCommand));
    }

    void OnCommand(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
        if (event.GetArg(0) == "deleted") {
            if (borrowed !is null && !BML::CK::IsValid(borrowed)) {
                @borrowed = null;
                Mark(ctx, "bml_probe_script_access_deleted", BML::CommandCallback(this.OnMarker));
            }
            return;
        }
        if (!CheckObjects(ctx)) {
            ctx.LogError("Script CK values or borrowing changed");
            return;
        }
        if (!CheckInput(ctx)) {
            ctx.LogError("Script input values, bounds or block ownership changed");
            return;
        }
        if (ctx.GetFrameCount() == 0 || ctx.GetTimeMs() < 0 ||
            ctx.GetAbsoluteTimeMs() < 0 || ctx.GetDeltaTimeMs() < 0) {
            ctx.LogError("Script time values changed");
            return;
        }
        Mark(ctx, "bml_probe_script_access_live", BML::CommandCallback(this.OnMarker));
    }

    bool CheckObjects(const BML::ModContext &in ctx) {
        if (BML::CK::IsValid(null) || BML::CK::GetId(null) != 0 ||
            BML::CK::GetName(null) != "" || BML::CK::GetClassId(null) != 0 ||
            BML::CK::IsVisible(null) || BML::CK::IsDynamic(null) ||
            BML::CK::GetPriority(null) != 0 || BML::CK::GetScriptCount(null) != 0 ||
            BML::CK::GetAttributeCount(null) != 0 || BML::CK::GetChildCount(null) != 0 ||
            BML::CK::BorrowParent(null) !is null)
            return false;
        VxVector zero = BML::CK::GetPosition(null);
        VxVector scale = BML::CK::GetScale(null);
        if (zero.x != 0 || zero.y != 0 || zero.z != 0 ||
            scale.x != 0 || scale.y != 0 || scale.z != 0)
            return false;
        CK3dEntity@ entity = ctx.Borrow3dEntityByName("BML_ScriptAccess_借用");
        @borrowed = entity;
        if (entity is null || !BML::CK::IsValid(entity) || BML::CK::GetId(entity) <= 0 ||
            BML::CK::GetName(entity) != "BML_ScriptAccess_借用" ||
            BML::CK::GetClassId(entity) <= 0 || BML::CK::GetPriority(entity) != 17 ||
            !BML::CK::IsVisible(entity) || !BML::CK::IsDynamic(entity) ||
            BML::CK::GetScriptCount(entity) != 0 || BML::CK::GetChildCount(entity) != 0 ||
            BML::CK::BorrowParent(entity) !is null)
            return false;
        VxVector position = BML::CK::GetPosition(entity);
        scale = BML::CK::GetScale(entity);
        return position.x == 1 && position.y == 2 && position.z == 3 &&
               scale.x == 2 && scale.y == 3 && scale.z == 4;
    }

    bool CheckInput(const BML::ModContext &in ctx) {
        BML::InputHook@ input = ctx.BorrowInputManager();
        if (input is null || !input.IsValid)
            return false;
        CKKEYBOARD invalid = CKKEYBOARD(-1);
        uint stamp = 123;
        if (input.IsKeyDown(invalid, stamp) || stamp != 0 || input.IsKeyUp(invalid) ||
            input.IsKeyPressed(invalid) || input.IsKeyReleased(invalid) ||
            input.GetKeyboardState(invalid) != 0 || input.IsKeyboardStateDown(invalid) ||
            input.GetKeyName(invalid) != "")
            return false;
        stamp = 123;
        if (input.IsKeyToggled(CKKEYBOARD(256), stamp) || stamp != 0 ||
            input.GetKeyFromName(input.GetKeyName(CKKEY_A)) != int(CKKEY_A) ||
            input.IsJoystickAttached(-1) || input.IsJoystickAttached(4) ||
            input.GetJoystickButtonsState(4) != 0 || input.IsJoystickButtonDown(-1, 0) ||
            input.IsJoystickButtonDown(0, 32) || input.GetJoystickPointOfViewAngle(4) != 0)
            return false;
        VxVector position = input.GetJoystickPosition(-1);
        VxVector rotation = input.GetJoystickRotation(4);
        Vx2DVector sliders = input.GetJoystickSliders(4);
        if (position.x != 0 || position.y != 0 || position.z != 0 ||
            rotation.x != 0 || rotation.y != 0 || rotation.z != 0 || sliders.x != 0 || sliders.y != 0)
            return false;
        int keyboard = input.IsBlocked(BML::INPUT_DEVICE_KEYBOARD);
        int mouse = input.IsBlocked(BML::INPUT_DEVICE_MOUSE);
        uint64 first = input.AcquireBlock(uint(BML::INPUT_BLOCK_KEYBOARD) | uint(BML::INPUT_BLOCK_MOUSE));
        uint64 second = input.AcquireBlock(uint(BML::INPUT_BLOCK_KEYBOARD));
        bool blocked = first != 0 && second != 0 && input.IsBlock() &&
                       input.IsBlocked(BML::INPUT_DEVICE_KEYBOARD) == keyboard + 2 &&
                       input.IsBlocked(BML::INPUT_DEVICE_MOUSE) == mouse + 1;
        CKKEYBOARD key = CKKEY_A;
        stamp = 123;
        blocked = input.GetKeyFromBuffer(0, key, stamp) == BML::INPUT_KEY_NONE &&
                  int(key) == 0 && stamp == 0 && input.GetNumberOfKeyInBuffer() == 0 && blocked;
        input.ReleaseBlock(first);
        input.ReleaseBlock(first);
        blocked = input.IsBlocked(BML::INPUT_DEVICE_KEYBOARD) == keyboard + 1 &&
                  input.IsBlocked(BML::INPUT_DEVICE_MOUSE) == mouse && blocked;
        input.ReleaseBlock(second);
        return input.IsBlocked(BML::INPUT_DEVICE_KEYBOARD) == keyboard &&
               input.IsBlocked(BML::INPUT_DEVICE_MOUSE) == mouse && blocked;
    }

    void Mark(const BML::ModContext &in ctx, const string &in name, BML::CommandCallback@ callback) {
        BML::CommandDefinition definition;
        definition.Name = name;
        BML::CommandRef@ registration = ctx.RegisterCommand(definition, callback, null);
    }

    void OnMarker(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {}
}

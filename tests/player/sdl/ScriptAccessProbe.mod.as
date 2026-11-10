[bml.mod id="SdlScriptAccessProbe" name="Script access regression" version="1.0.0"]
class ScriptAccessProbe {
    private CKObject@ borrowed;
    private BML::Config@ config;
    private BML::Input@ input;
    private BML::Timers@ timers;
    private BML::Commands@ commands;
    private BML::KeyboardState@ held;
    private BML::TimerRef@ once;
    private uint timerCalls = 0;


    void OnLoad(const BML::ModContext &in ctx) {
        @config = ctx.Config(); @input = ctx.Input(); @timers = ctx.Timers(); @commands = ctx.Commands();
        if (!CheckAuthoring(ctx)) { ctx.LogError("Script authoring contract failed"); return; }
        if (timers.SetTimeoutTicks(0, BML::TimerCallback(this.OnTimer), once, "zero") != BML::ERROR_OK || timerCalls != 0) {
            ctx.LogError("Zero delay timer was rejected or ran synchronously"); return;
        }
        Mark(ctx, "bml_probe_script_access", BML::CommandExecute(this.OnCommand));
    }

    uint notices = 0;
    void OnPostStartMenu(const BML::ModContext &in ctx) { Mark(ctx, "bml_probe_individual_event", BML::CommandExecute(this.OnMarker)); }
    void OnPreCommandExecute(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
        if (event.Name != "bml_probe_command") return;
        string argument;
        if (!event.IsPre || event.InvokedAs != "bpcheck" || event.ArgumentCount != 2 ||
            event.ReadArgument(1, argument) != BML::ERROR_OK || argument != "two words") throw("pre command event changed");
        ++notices;
    }
    void OnPostCommandExecute(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
        if (event.Name != "bml_probe_command") return;
        if (!event.IsPost) throw("post command event changed");
        if (++notices == 4) Mark(ctx, "bml_probe_individual_command", BML::CommandExecute(this.OnMarker));
    }

    void OnTimer(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
        BML::TimerInfo info;
        if (event.Read(info) == BML::ERROR_OK && info.Name == "zero" && info.Handle != 0) ++timerCalls;
    }

    bool CheckAuthoring(const BML::ModContext &in ctx) {
        BML::ConfigValue empty;
        string text; bool boolean = false; int integer = 0; float number = 0; CKKEYBOARD key = CKKEY_A;
        if (empty.Type != BML::CONFIG_PROPERTY_NONE || empty.ReadString(text) != BML::ERROR_TYPE_MISMATCH) return false;
        BML::ConfigValue stringValue("你好"); BML::ConfigValue boolValue(true);
        BML::ConfigValue intValue(42); BML::ConfigValue floatValue(1.25f); BML::ConfigValue keyValue(CKKEY_A);
        if (stringValue.ReadString(text) != BML::ERROR_OK || text != "你好" ||
            boolValue.ReadBoolean(boolean) != BML::ERROR_OK || !boolean ||
            intValue.ReadInteger(integer) != BML::ERROR_OK || integer != 42 ||
            floatValue.ReadFloat(number) != BML::ERROR_OK || number != 1.25f ||
            keyValue.ReadKey(key) != BML::ERROR_OK || key != CKKEY_A ||
            intValue.ReadKey(key) != BML::ERROR_TYPE_MISMATCH) return false;
        BML::ConfigPropertyDefinition definition;
        definition.Category = "Regression"; definition.Key = "Enabled";
        definition.DefaultValue = BML::ConfigValue(true); definition.Comment = "owned";
        definition.Choices.insertLast("a");
        BML::ConfigProperty@ property;
        if (config.DeclareProperty(definition, property) != BML::ERROR_OK) return false;
        BML::ConfigPropertyInfo info;
        if (property.Read(info) != BML::ERROR_OK || info.Definition.Category != "Regression" ||
            info.CurrentValue.ReadBoolean(boolean) != BML::ERROR_OK || !boolean ||
            info.Definition.Choices.length() != 1) return false;
        definition.Choices[0] = "changed";
        if (info.Definition.Choices[0] != "a" || property.Set(BML::ConfigValue(false)) != BML::ERROR_OK ||
            property.GetBoolean(true)) return false;
        @definition.Choices = null;
        array<BML::ModInfo> mods; array<BML::CommandInfo> list; array<int> wrong;
        array<BML::ModInfo>@ handle = array<BML::ModInfo>();
        return ctx.ReadMods(mods) == BML::ERROR_OK && mods.length() >= 3 &&
               commands.ReadCommands(list) == BML::ERROR_OK && list.length() > 0 &&
               ctx.ReadMods(wrong) == BML::ERROR_TYPE_MISMATCH &&
               ctx.ReadMods(@handle) == BML::ERROR_TYPE_MISMATCH &&
               commands.ReadCommands(wrong) == BML::ERROR_TYPE_MISMATCH;
    }

    int OnCommand(const BML::ModContext &in ctx, const BML::CommandInvocation &in event) {
        if (!config.IsValid() || !input.IsValid() || !timers.IsValid() || !commands.IsValid() || timerCalls != 1 ||
            once.Cancel() != BML::ERROR_OK || once.Cancel() != BML::ERROR_OK) { ctx.LogError("Cached service or completed ticket invalid"); return 1; }
        string argument;
        event.ReadArgument(0, argument);
        if (argument == "deleted") {
            if (borrowed !is null && !BML::CK::IsValid(borrowed)) {
                @borrowed = null;
                Mark(ctx, "bml_probe_script_access_deleted", BML::CommandExecute(this.OnMarker));
            }
            return 0;
        }
        if (!CheckObjects(ctx)) {
            ctx.LogError("Script CK values or borrowing changed");
            return 0;
        }
        if (!CheckInput(ctx)) {
            ctx.LogError("Script input values, bounds or block ownership changed");
            return 0;
        }
        BML::Time::Clock clock;
        if (BML::Time::ReadClock(clock) != BML::ERROR_OK || clock.MainTickCount == 0 ||
            clock.TimeMs < 0 || clock.AbsoluteMs < 0 || clock.DeltaMs < 0) {
            ctx.LogError("Script time values changed");
            return 0;
        }
        Mark(ctx, "bml_probe_script_access_live", BML::CommandExecute(this.OnMarker));
        return 0;
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
        BML::ObjectRef reference;
        CKObject@ object;
        if (BML::Scene::FindObject("BML_ScriptAccess_借用", reference) != BML::ERROR_OK ||
            BML::Engine::BorrowObject(ctx, reference, CKCID_3DENTITY, object) != BML::ERROR_OK) return false;
        CK3dEntity@ entity = cast<CK3dEntity>(object);
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
        BML::Input@ input = ctx.Input();
        if (input is null || !input.IsValid()) return false;
        string name; uint key = 0;
        if (input.GetKeyName(uint(CKKEY_A), name) != BML::ERROR_OK ||
            input.GetKeyFromName(name, key) != BML::ERROR_OK || key != uint(CKKEY_A)) return false;
        BML::JoystickState joystick;
        if (input.ReadJoystick(4, joystick) != BML::ERROR_INVALID_PARAMETER) return false;
        uint keyboard = 0, mouse = 0, count = 0;
        input.ReadBlockCount(BML::INPUT_DEVICE_KEYBOARD, keyboard);
        input.ReadBlockCount(BML::INPUT_DEVICE_MOUSE, mouse);
        BML::InputBlock@ first; BML::InputBlock@ second;
        if (input.AcquireBlock(uint(BML::INPUT_BLOCK_KEYBOARD) | uint(BML::INPUT_BLOCK_MOUSE), first) != BML::ERROR_OK ||
            input.AcquireBlock(uint(BML::INPUT_BLOCK_KEYBOARD), second) != BML::ERROR_OK) return false;
        BML::KeyboardState@ snapshot;
        if (input.ReadKeyboard(snapshot) != BML::ERROR_OK || snapshot.KeyCount != 256 || snapshot.EventCount != 0) return false;
        BML::KeyState state;
        if (snapshot.ReadKey(256, state) != BML::ERROR_INVALID_PARAMETER || !snapshot.IsKeyUp(256) || snapshot.IsKeyDown(256)) return false;
        input.ReadBlockCount(BML::INPUT_DEVICE_KEYBOARD, count);
        bool ok = count == keyboard + 2;
        if (first.Release() != BML::ERROR_OK || first.Release() != BML::ERROR_OK) return false;
        input.ReadBlockCount(BML::INPUT_DEVICE_MOUSE, count); ok = ok && count == mouse;
        if (second.Release() != BML::ERROR_OK) return false;
        input.ReadBlockCount(BML::INPUT_DEVICE_KEYBOARD, count);
        @held = snapshot;
        BML::KeyboardState@ current;
        input.ReadKeyboard(current, uint(BML::INPUT_RAW));
        return ok && count == keyboard && held.EventCount == 0;
    }

    void Mark(const BML::ModContext &in ctx, const string &in name, BML::CommandExecute@ callback) {
        BML::CommandDefinition definition;
        definition.Name = name;
        BML::CommandRef@ registration;
        ctx.Commands().Register(definition, callback, registration);
    }

    int OnMarker(const BML::ModContext &in ctx, const BML::CommandInvocation &in event) { return 0; }
}

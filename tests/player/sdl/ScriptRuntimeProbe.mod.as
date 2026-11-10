[bml.mod id="SdlRuntimeProbe" name="Runtime scope regression" version="1.0.0" reload="manual"]
class ScriptRuntimeProbe {
    bool FailCandidate = false;
    bool NewRuntime = false;
    BML::Config@ previous;
    BML::ModContext previousContext;
    BML::Commands@ previousCommands;
    BML::ConfigProperty@ property;
    BML::InputBlock@ block;
    BML::TimerRef@ timer;
    BML::KeyboardState@ keyboard;

    void OnLoad(const BML::ModContext &in ctx) {
        if (FailCandidate) throw("intentional candidate failure");
        if (previous !is null) {
            array<BML::ModInfo> mods(1);
            array<BML::CommandInfo> commands(1);
            if (previousContext.ReadMods(mods) != BML::ERROR_INVALID_HANDLE || mods.length() != 0 ||
                previousCommands.ReadCommands(commands) != BML::ERROR_INVALID_HANDLE || commands.length() != 0)
                throw("retired query kept stale output");
            BML::ConfigPropertyInfo info;
            if (previous.IsValid() || property.IsValid() || block.IsValid() || timer.IsValid() ||
                property.Read(info) != BML::ERROR_INVALID_HANDLE ||
                block.Release() != BML::ERROR_OK || timer.Cancel() != BML::ERROR_OK ||
                timer.Pause() != BML::ERROR_INVALID_HANDLE ||
                !keyboard.IsValid() || !keyboard.IsKeyUp(uint(CKKEY_A)))
                throw("retired runtime view was reactivated or lost its captured value");
            Mark(ctx, "bml_probe_rollback_scope");
        }
        @previous = ctx.Config();
        previousContext = ctx;
        @previousCommands = ctx.Commands();
        BML::ConfigPropertyDefinition definition;
        definition.Category = "Runtime";
        definition.Key = "Version";
        definition.DefaultValue = BML::ConfigValue(false);
        if (previous.DeclareProperty(definition, property) != BML::ERROR_OK)
            throw("new scope did not reset its declaration type");
        if (NewRuntime && property.GetInteger(-1) != 2) throw("new typed declaration failed");
        if (ctx.Input().AcquireBlock(uint(BML::INPUT_BLOCK_JOYSTICK), block) != BML::ERROR_OK ||
            ctx.Input().ReadKeyboard(keyboard) != BML::ERROR_OK ||
            ctx.Timers().SetIntervalTicks(100000, BML::TimerLoopCallback(this.Tick), timer) != BML::ERROR_OK)
            throw("runtime request registration failed");
        if (NewRuntime) {
            Mark(ctx, "bml_probe_new_scope");
            BML::CommandDefinition command;
            command.Name = "bml_probe_fail_provider";
            BML::CommandRef@ registration;
            ctx.Commands().Register(command, BML::CommandExecute(this.Fail), registration);
        }
    }
    bool Tick(const BML::ModContext &in ctx, const BML::TimerEvent &in event) { return true; }
    int Fail(const BML::ModContext &in ctx, const BML::CommandInvocation &in call) {
        throw("intentional provider failure");
        return 1;
    }
    int Marker(const BML::ModContext &in ctx, const BML::CommandInvocation &in call) { return 0; }
    void Mark(const BML::ModContext &in ctx, const string &in name) {
        BML::CommandDefinition definition; definition.Name = name;
        BML::CommandRef@ registration;
        if (ctx.Commands().Register(definition, BML::CommandExecute(this.Marker), registration) != BML::ERROR_OK)
            throw("runtime marker registration failed");
    }
    void OnUnload(const BML::ModContext &in ctx) {
        BML::ConfigPropertyDefinition definition;
        definition.Category = "Frozen"; definition.Key = "Rejected";
        BML::ConfigProperty@ rejectedProperty;
        if (ctx.Config().DeclareProperty(definition, rejectedProperty) != BML::ERROR_FROZEN ||
            rejectedProperty !is null) throw("unload admitted a new configuration declaration");
        BML::TimerRef@ rejected;
        if (ctx.Timers().SetTimeoutTicks(0, BML::TimerCallback(this.Timeout), rejected) != BML::ERROR_FROZEN ||
            rejected !is null) throw("unload admitted a new timer");
        // The host must retire these requests; retained Refs are used by rollback.
    }
    void Timeout(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {}
}

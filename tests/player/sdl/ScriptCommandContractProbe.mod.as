[bml.mod id="SdlScriptCommandContractProbe" name="Script command contracts" version="1.0.0"]
class ScriptCommandContractProbe {
    uint calls = 0;
    bool checked = false;
    bool cheatPre = false, cheatPost = false;
    void OnLoad(const BML::ModContext &in ctx) {
        BML::CommandDefinition definition;
        definition.Name = "bml_probe_text_contract";
        BML::CommandRef@ command;
        if (ctx.Commands().Register(definition, BML::CommandExecute(this.Text), command) != BML::ERROR_OK)
            throw("text command registration failed");
        array<string> invalid = {
            Bytes({255}), Bytes({195}), Bytes({192, 175}),
            Bytes({237, 160, 128}), Bytes({244, 144, 128, 128}), Bytes({0})
        };
        for (uint i = 0; i < invalid.length(); ++i) {
            for (uint field = 0; field < 3; ++field) {
                BML::CommandDefinition rejected;
                rejected.Name = "bml_probe_invalid_metadata";
                if (field == 0) rejected.Description = invalid[i];
                else if (field == 1) rejected.Usage = invalid[i];
                else rejected.Category = invalid[i];
                BML::CommandRef@ registration;
                int code = ctx.Commands().Register(rejected, BML::CommandExecute(this.Marker), registration);
                if (code != BML::ERROR_INVALID_PARAMETER || registration !is null)
                    throw("invalid command metadata accepted: " + i + " field=" + field + " code=" + code);
                rejected.Description = "中文"; rejected.Usage = "中文"; rejected.Category = "中文";
                if (ctx.Commands().Register(rejected, BML::CommandExecute(this.Marker), registration) != BML::ERROR_OK ||
                    registration.Unregister() != BML::ERROR_OK)
                    throw("rejected command metadata left a registration");
            }
        }
        array<string> invalidNames = invalid;
        invalidNames.insertLast(""); invalidNames.insertLast("two words"); invalidNames.insertLast("tab\tname");
        string oversized; oversized.resize(257);
        for (uint i = 0; i < oversized.length(); ++i) oversized[i] = 120;
        invalidNames.insertLast(oversized);
        BML::CommandInfo info;
        for (uint i = 0; i < invalidNames.length(); ++i) {
            if (ctx.Commands().Find(invalidNames[i], info) != BML::ERROR_INVALID_PARAMETER)
                throw("invalid command lookup was reported as missing: " + i);
        }
        if (ctx.Commands().Find("bml_probe_missing_valid_name", info) != BML::ERROR_NOT_FOUND ||
            ctx.Commands().Find(definition.Name, info) != BML::ERROR_OK)
            throw("valid command lookup changed");
        for (uint i = 0; i < invalid.length(); ++i) {
            int status = -1;
            int code = ctx.Commands().ExecuteLine("bml_probe_text_contract " + invalid[i], status);
            if (code != BML::ERROR_INVALID_PARAMETER || calls != 0)
                throw("invalid command text reached execution: " + i + " code=" + code);
        }
        int status = -1;
        if (ctx.Commands().ExecuteLine("bml_probe_text_contract 中文", status) != BML::ERROR_OK ||
            status != 13 || calls != 1) throw("valid UTF-8 or business status changed");
        definition.Name = "bml_probe_cheat_contract";
        definition.Cheat = true;
        if (ctx.Commands().Register(definition, BML::CommandExecute(this.Marker), command) != BML::ERROR_OK)
            throw("cheat command registration failed");
    }
    void OnProcess(const BML::ModContext &in ctx) {
        if (checked) return;
        checked = true;
        bool previous = false;
        if (BML::Gameplay::ReadCheatEnabled(previous) != BML::ERROR_OK) throw("read cheat state failed");
        int enabled = BML::Gameplay::EnableCheat(ctx, true);
        int status = -1;
        int code = ctx.Commands().ExecuteLine("bml_probe_cheat_contract", status);
        int restored = BML::Gameplay::EnableCheat(ctx, previous);
        if (enabled != BML::ERROR_OK || code != BML::ERROR_OK || status != 0 ||
            restored != BML::ERROR_OK || !cheatPre || !cheatPost)
            throw("cheat event payload or restoration failed");
        BML::CommandDefinition definition;
        definition.Name = "bml_probe_command_contracts";
        BML::CommandRef@ marker;
        if (ctx.Commands().Register(definition, BML::CommandExecute(this.Marker), marker) != BML::ERROR_OK)
            throw("command contracts marker registration failed");
    }
    void OnCommandEvent(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
        if (event.Name == "bml_probe_text_contract" && event.Cheat)
            throw("ordinary command was reported as cheat");
        if (event.Name != "bml_probe_cheat_contract") return;
        if (!event.Cheat) throw("cheat command lost its cheat flag");
        if (event.IsPre) cheatPre = true;
        if (event.IsPost) cheatPost = true;
    }
    int Text(const BML::ModContext &in ctx, const BML::CommandInvocation &in call) {
        ++calls;
        string argument;
        return call.ReadArgument(0, argument) == BML::ERROR_OK && argument == "中文" ? 13 : 99;
    }
    int Marker(const BML::ModContext &in ctx, const BML::CommandInvocation &in call) { return 0; }
    string Bytes(const array<uint8> &in bytes) {
        string result;
        result.resize(bytes.length());
        for (uint i = 0; i < bytes.length(); ++i) result[i] = bytes[i];
        return result;
    }
}

[bml.mod(id: "example.hello.script",
         name: "Hello Script",
         version: "1.0.0",
         author: "Template",
         description: "Minimal BML+ script mod template",
         bml: "0.3.11")]
class HelloScript {
    private BML::TimerRef@ heartbeat;
    private BML::CommandRef@ helloCommand;
    private bool enabled = true;
    private int counter = 3;
    private string search = "";

    void OnLoad(const BML::ModContext &in ctx) {
        ctx.LogInfo("HelloScript loaded: " + ctx.GetModId());

        string text = ctx.ReadModTextFileUtf8("Resources/hello.txt", "missing resource");
        ctx.LogInfo("HelloScript resource: " + text);

        @helloCommand = ctx.RegisterCommand(HelloCommand());

        @heartbeat = ctx.AddTimer(HeartbeatTimer());
    }

    void OnUnload(const BML::ModContext &in ctx) {
        if (heartbeat !is null && heartbeat.IsValid()) {
            heartbeat.Cancel();
        }
        if (helloCommand !is null && helloCommand.IsValid) {
            helloCommand.Unregister();
        }
        ctx.LogInfo("HelloScript unloaded");
    }

    void OnProcess(const BML::ModContext &in ctx) {
    }

    void OnRender(const BML::ModContext &in ctx, const BML::RenderEvent &in event) {
        BML::UI::Title("Hello Script");
        BML::UI::SetCursorCoord(0.4f, 0.35f);
        if (BML::UI::MainButton("Say Hello")) {
            ctx.SendIngameMessage("Hello from script UI");
        }
        BML::UI::YesNoButton("Enabled", enabled);
        BML::UI::InputIntButton("Counter", counter);
        BML::UI::SearchBar(search);
    }

    [bml.export(name: "Greeting", signature: "string(string)")]
    string Greeting(const string &in name) {
        return "Hello, " + name + "!";
    }
}

class HeartbeatTimer : BML::Timer {
    string get_Name() const { return "hello-script-heartbeat"; }
    float get_DelayMs() const { return 1000.0f; }
    bool get_Loop() const { return true; }

    bool Tick(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
        ctx.LogInfo("HelloScript heartbeat " + event.CompletedIterations);
        return event.CompletedIterations < 3;
    }
}

class HelloCommand : BML::Command {
    string get_Name() const { return "hello-script"; }
    string get_Alias() const { return "hs"; }
    string get_Description() const { return "Print a script greeting"; }

    void Execute(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
        string target = event.ArgCount > 0 ? event.GetArg(0) : "Ballance";
        ctx.SendIngameMessage("Hello, " + target + "!");
    }

    void Complete(const BML::ModContext &in ctx,
                  const BML::CommandEvent &in event,
                  BML::CommandCompletion &inout completions) {
        completions.Add("Ballance");
        completions.Add("BML");
        completions.Add(ctx.Id);
    }
}

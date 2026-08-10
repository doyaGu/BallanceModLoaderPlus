[bml.mod id="example.hello.script"
         name="Hello Script"
         version="1.0.0"
         author="Your Name"
         description="Minimal BML+ script mod"
         bml="0.3.13"]
class HelloScript {
    void OnLoad(const BML::ModContext &in ctx) {
        ctx.LogInfo("Hello Script loaded");
        BML::UI::AddMessage("Hello from your first script mod!");
    }
}

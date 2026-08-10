[bml.mod id="example.input.ui"
         name="Input and UI Example"
         version="1.0.0"
         author="BML+"
         description="Toggles a small ImGui window with F9"
         bml="0.3.13"]
class InputUiExample {
    private bool visible = true;

    void OnLoad(const BML::ModContext &in ctx) {
        ctx.LogInfo("Input and UI example loaded");
        BML::UI::AddMessage("Press F9 to toggle the example window.");
    }

    void OnProcess(const BML::ModContext &in ctx) {
        BML::InputHook@ input = ctx.BorrowInputManager();
        if (input !is null && input.IsKeyPressed(CKKEY_F9))
            visible = !visible;

        if (!visible)
            return;

        ImGui::SetNextWindowPos(ImVec2(10.0f, 60.0f), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(300.0f, 0.0f), ImGuiCond_Once);
        if (ImGui::Begin("BML+ Input Example")) {
            ImGui::TextUnformatted("The script is running.");
            ImGui::TextUnformatted("Press F9 to hide this window.");
        }
        ImGui::End();
    }
}

#include "../libs/BehaviorGraphImGui.as"

[bml.mod id="tutorial.behavior.graph.viewer" name="Behavior Graph Viewer Example" version="1.0.0" author="Tutorial" bml="0.3.12" description="ImGui behavior graph viewer example"]
class BehaviorGraphViewerExample {
    private BGImGui::Graph graph;
    private BGImGui::View view;
    private bool showWindow = true;
    private bool focusWindow = true;

    void OnLoad(const BML::ModContext &in ctx) {
        BuildSampleGraph();
        view.showInspector = false;
        view.showMiniMap = true;
        view.autoFitOnFirstDraw = true;
        LogInfo(ctx, "BehaviorGraphViewerExample loaded");
    }

    void OnProcess(const BML::ModContext &in ctx) {
        DrawWindow();
    }

    private void BuildSampleGraph() {
        graph.Clear();
        graph.title = "activate next Checkpoint";
        graph.subtitle = "Gameplay_Events";

        // 手工放置节点，让示例第一次打开就能看见完整流程。
        int parent = graph.AddNodeIndex("events", "Gameplay_Events", 20.0f, 185.0f);
        graph.nodes[parent].SetTitleLines("Gameplay", "Events");
        graph.nodes[parent].SetKind(BGImGui::NODE_KIND_GRAPH);
        graph.nodes[parent].subtitle = "event graph";
        graph.nodes[parent].AddOutput("checkpoint event");
        graph.nodes[parent].AddLine("Wait Message");
        graph.nodes[parent].AddLine("Send Message");

        int start = graph.AddNodeIndex("start", "Activate Script", 195.0f, 185.0f);
        graph.nodes[start].SetTitleLines("Activate", "Script");
        graph.nodes[start].subtitle = "enter subgraph";
        graph.nodes[start].AddInput("event");
        graph.nodes[start].AddOutput("write");
        graph.nodes[start].AddOutput("read");

        int current = graph.AddNodeIndex("current", "CurrentLevel", 370.0f, 65.0f);
        graph.nodes[current].SetKind(BGImGui::NODE_KIND_DATA);
        graph.nodes[current].subtitle = "current state";
        graph.nodes[current].AddInput("checkpoint");
        graph.nodes[current].AddOutput("state");

        int table = graph.AddNodeIndex("table", "Checkpoints", 370.0f, 260.0f);
        graph.nodes[table].SetKind(BGImGui::NODE_KIND_DATA);
        graph.nodes[table].subtitle = "checkpoint table";
        graph.nodes[table].AddInput("row");
        graph.nodes[table].AddOutput("matrix");
        graph.nodes[table].AddOutput("object");

        int setCurrent = graph.AddNodeIndex("set-current", "Set Cell", 545.0f, 65.0f);
        graph.nodes[setCurrent].subtitle = "write state";
        graph.nodes[setCurrent].AddInput("state");
        graph.nodes[setCurrent].AddOutput("updated");
        graph.nodes[setCurrent].AddLine("Current checkpoint");

        int getRow = graph.AddNodeIndex("get-row", "Get Row", 545.0f, 260.0f);
        graph.nodes[getRow].subtitle = "read table";
        graph.nodes[getRow].AddInput("row index");
        graph.nodes[getRow].AddOutput("checkpoint row");
        graph.nodes[getRow].AddLine("next checkpoint data");

        int matrix = graph.AddNodeIndex("matrix", "Set World Matrix", 720.0f, 260.0f);
        graph.nodes[matrix].SetTitleLines("Set World", "Matrix");
        graph.nodes[matrix].subtitle = "place helper";
        graph.nodes[matrix].AddInput("object");
        graph.nodes[matrix].AddInput("matrix");
        graph.nodes[matrix].AddOutput("placed");

        int show = graph.AddNodeIndex("show", "Show", 895.0f, 260.0f);
        graph.nodes[show].subtitle = "make visible";
        graph.nodes[show].AddInput("placed object");
        graph.nodes[show].AddOutput("visible");

        int message = graph.AddNodeIndex("message", "Send Message", 895.0f, 65.0f);
        graph.nodes[message].SetTitleLines("Send", "Message");
        graph.nodes[message].SetKind(BGImGui::NODE_KIND_MESSAGE);
        graph.nodes[message].subtitle = "notify gameplay";
        graph.nodes[message].AddInput("visible");
        graph.nodes[message].AddOutput("message");

        BGImGui::Edge@ e = null;
        graph.AddEdge(parent, start, "event");
        @e = graph.AddEdge(start, setCurrent, "exec");
        if (e !is null) {
            e.fromPort = 0;
        }
        graph.AddEdge(start, getRow, "exec");
        @e = graph.AddEdge(current, setCurrent, "state");
        if (e !is null) {
            e.parameter = true;
        }
        @e = graph.AddEdge(table, getRow, "row data");
        if (e !is null) {
            e.parameter = true;
            e.fromPort = 0;
        }
        @e = graph.AddEdge(getRow, matrix, "matrix");
        if (e !is null) {
            e.parameter = true;
            e.toPort = 1;
        }
        @e = graph.AddEdge(table, matrix, "object");
        if (e !is null) {
            e.parameter = true;
            e.fromPort = 1;
            e.toPort = 0;
        }
        graph.AddEdge(matrix, show, "exec");
        @e = graph.AddEdge(show, message, "done");
        if (e !is null) {
            e.toPort = 0;
        }
    }

    private void DrawWindow() {
        if (!showWindow) {
            return;
        }

        ImGui::SetNextWindowPos(ImVec2(20.0f, 38.0f), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(920.0f, 650.0f), ImGuiCond_Once);
        if (focusWindow) {
            ImGui::SetNextWindowFocus();
            focusWindow = false;
        }
        if (ImGui::Begin("Behavior Graph Viewer", showWindow)) {
            view.DrawToolbar(graph);
            view.Draw(graph, "behavior_graph_canvas", ImVec2(0.0f, 540.0f));
        }
        ImGui::End();
    }

    private void LogInfo(const BML::ModContext &in ctx, const string &in message) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Info(message);
        }
    }
}

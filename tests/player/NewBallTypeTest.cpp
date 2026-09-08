#include <BML/Behavior.hpp>
#include <BML/IBML.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>

#include "PlayerBallLocator.h"
#include "PlayerProbe.h"

#include <cstring>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace Behavior = BML::Behavior;

namespace {

bool ReadString(CKDataArray *array, int row, int column, std::string &value) {
    if (!array || row < 0 || column < 0 || row >= array->GetRowCount() ||
        column >= array->GetColumnCount())
        return false;
    const int required = array->GetElementStringValue(row, column, nullptr);
    if (required <= 0)
        return false;
    std::vector<char> text(static_cast<std::size_t>(required), '\0');
    if (array->GetElementStringValue(row, column, text.data()) <= 0)
        return false;
    value = text.data();
    return true;
}

bool HasRow(CKDataArray *array, int column, std::string_view expected) {
    if (!array)
        return false;
    for (int row = 0; row < array->GetRowCount(); ++row) {
        std::string value;
        if (ReadString(array, row, column, value) && value == expected)
            return true;
    }
    return false;
}

Behavior::Result<Behavior::Graph> Child(const Behavior::Graph &graph,
                                        std::string_view name) {
    auto node = graph.Find(Behavior::Named(name, 0));
    if (!node)
        return Behavior::Result<Behavior::Graph>::Failure(
            node.Code(), node.GetStatus());
    return graph.Inspect(node.Value());
}

bool HasStringPin(const Behavior::Graph &graph, std::string_view nodeName,
                  std::string_view expected) {
    auto node = graph.Find(Behavior::Named(nodeName, 0));
    if (!node)
        return false;
    for (Behavior::Port port : node->Ports()) {
        if (port.Kind() != Behavior::SlotKind::Pin)
            continue;
        auto value = graph.Read(port);
        const std::string *text = value
            ? std::get_if<std::string>(&value->Data) : nullptr;
        if (text && *text == expected)
            return true;
    }
    return false;
}

class NewBallTypeTest final : public IMod {
public:
    explicit NewBallTypeTest(IBML *bml) : IMod(bml) {
        AddDependency("BML");
    }

    const char *GetID() override { return "NewBallTypeTest"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "New Ball Type Test"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override {
        return "Validates a registered ball through the shipped level flow";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        BML::PlayerTest::ProbeReport::Reset();
        auto opened = Behavior::Session::Open(GetID());
        if (!opened) {
            Fail("behavior-session");
            return;
        }
        m_Behavior = opened.Take();

        m_BML->RegisterBallType(
            "Ball_Sticky.nmo", "sticky", "Sticky", "Ball_Sticky",
            10.0f, 0.0f, 1.4f, "Ball", 1.0f, 7.0f, 0.15f, 2.0f);
        m_BML->RegisterModulBall(
            "P_Ball_Sticky", false, 10.0f, 0.0f, 1.4f, "", false,
            true, false, 0.8f, 7.0f, 2.0f);
        m_BML->RegisterTrafo("P_Trafo_Sticky");
        GetLogger()->Info("New ball test registration: ball=sticky");
    }

    void OnLoadObject(const char *filename, CKBOOL, const char *, CK_CLASSID,
                      CKBOOL, CKBOOL, CKBOOL, CKBOOL, XObjectArray *,
                      CKObject *) override {
        if (!filename || std::strcmp(filename, "3D Entities\\Levelinit.nmo"))
            return;
        CKDataArray *levels = m_BML->GetArrayByName("AllLevel");
        if (!levels || levels->GetColumnCount() < 2) {
            Fail("all-level-unavailable");
            return;
        }
        for (int row = 0; row < levels->GetRowCount(); ++row) {
            std::string file;
            if (!ReadString(levels, row, 0, file) ||
                file.find("Level_01") == std::string::npos)
                continue;
            levels->SetElementStringValue(
                row, 1, const_cast<char *>("Ball_Sticky"));
            m_LevelConfigured = true;
            GetLogger()->Info(
                "New ball test level: file=%s start_ball=Ball_Sticky",
                file.c_str());
            return;
        }
        Fail("level-one-missing");
    }

    void OnProcess() override {
        using BML::PlayerTest::ProbeReport;
        if (!ProbeReport::Started() || ProbeReport::Reported())
            return;
        ++m_Frames;

        CK3dEntity *ball = BML::PlayerTest::ResolveRetailBall(m_BML);
        m_BallSelected = ball && ball->GetName() &&
            std::strcmp(ball->GetName(), "Ball_Sticky") == 0;
        m_BallPhysics = HasRow(
            m_BML->GetArrayByName("Physicalize_GameBall"), 0,
            "Ball_Sticky");
        m_ModulePhysics = HasRow(
            m_BML->GetArrayByName("Physicalize_Balls"), 0,
            "P_Ball_Sticky");
        m_ModuleGroup = HasRow(
            m_BML->GetArrayByName("PH_Groups"), 0, "P_Trafo_Sticky");
        m_GraphPatched = CheckGraph();

        if (m_LevelConfigured && m_BallSelected && m_BallPhysics &&
            m_ModulePhysics && m_ModuleGroup && m_GraphPatched) {
            Report(true, "complete", ball);
            return;
        }
        if (m_Frames >= 300)
            Report(false, "new-ball-incomplete", ball);
    }

private:
    bool CheckGraph() const {
        CKBehavior *script = m_BML->GetScriptByName("Gameplay_Ingame");
        auto gameplay = script
            ? m_Behavior.Inspect(script)
            : Behavior::Result<Behavior::Graph>::Failure(
                  BML_ERROR_NOT_FOUND);
        if (!gameplay)
            return false;
        auto ballManager = Child(gameplay.Value(), "BallManager");
        auto newBall = ballManager
            ? Child(ballManager.Value(), "New Ball")
            : Behavior::Result<Behavior::Graph>::Failure(
                  BML_ERROR_NOT_FOUND);
        auto physicalize = newBall
            ? Child(newBall.Value(), "physicalize new Ball")
            : Behavior::Result<Behavior::Graph>::Failure(
                  BML_ERROR_NOT_FOUND);
        if (!physicalize ||
            !HasStringPin(physicalize.Value(), "Switch On Parameter",
                          "Ball_Sticky"))
            return false;

        auto trafo = Child(gameplay.Value(), "Trafo Manager");
        auto setBall = trafo
            ? Child(trafo.Value(), "set new Ball")
            : Behavior::Result<Behavior::Graph>::Failure(
                  BML_ERROR_NOT_FOUND);
        return setBall &&
            HasStringPin(setBall.Value(), "Switch On Parameter", "sticky");
    }

    void Fail(const char *reason) {
        GetLogger()->Error("New ball type: status=fail reason=%s", reason);
        BML::PlayerTest::ProbeReport::Fail(reason);
    }

    void Report(bool passed, const char *reason, CK3dEntity *ball) {
        GetLogger()->Info(
            "New ball type: status=%s reason=%s level=%s active_ball=%s "
            "ball_table=%s module_table=%s module_group=%s graph=%s",
            passed ? "pass" : "fail", reason,
            m_LevelConfigured ? "true" : "false",
            m_BallSelected ? "Ball_Sticky"
                           : (ball && ball->GetName() ? ball->GetName()
                                                     : "unavailable"),
            m_BallPhysics ? "true" : "false",
            m_ModulePhysics ? "true" : "false",
            m_ModuleGroup ? "true" : "false",
            m_GraphPatched ? "true" : "false");
        if (passed)
            BML::PlayerTest::ProbeReport::Pass(reason);
        else
            BML::PlayerTest::ProbeReport::Fail(reason);
    }

    Behavior::Session m_Behavior;
    int m_Frames = 0;
    bool m_LevelConfigured = false;
    bool m_BallSelected = false;
    bool m_BallPhysics = false;
    bool m_ModulePhysics = false;
    bool m_ModuleGroup = false;
    bool m_GraphPatched = false;
};

} // namespace

BML_PLAYER_PROBE_EXPORTS()

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new NewBallTypeTest(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}

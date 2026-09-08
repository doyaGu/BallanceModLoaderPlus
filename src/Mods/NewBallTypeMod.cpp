#include "Mods/NewBallTypeMod.h"

#include "BML/IBML.h"
#include "BML/Guids/Logics.h"
#include "BML/Guids/Narratives.h"
#include "BML/Scene.h"

#include "BML/Behavior/Blocks/ObjectLoad.hpp"
#include "BML/Behavior/Blocks/Physicalize.hpp"

#include <cstdint>

namespace Behavior = BML::Behavior;

namespace {

struct BallBehavior {
    const BallTypeInfo *Info = nullptr;
    Behavior::ObjectRef Ball{};
    Behavior::ObjectRef Reset{};
    std::string Used;
};

Behavior::Edit::Node First(Behavior::Edit::Graph graph,
                           std::string_view name) {
    return graph.Require(Behavior::Named(name, 0));
}

bool AddPhysicalize(const Behavior::Session &session,
                    Behavior::Edit::Graph graph,
                    const std::vector<BallBehavior> &balls) {
    const auto physicalize = First(graph, "Physicalize");
    const auto switchOnBall = First(graph, "Switch On Parameter");
    const auto show = First(graph, "Show");
    const auto source = graph.Next(
        graph.Root().In(0), Behavior::NodePattern("Op"));

    for (const BallBehavior &ball : balls) {
        const auto ballName = graph.AppendPin(
            switchOnBall, "Pin", CKPGUID_STRING);
        graph.Bind(ballName, ball.Info->m_ObjName);
        const auto route = graph.AppendOut(switchOnBall, "Out");

        Behavior::Blocks::Physicalize::Options options;
        if (ball.Info->m_Radius > 0.0f) {
            options.Geometry = Behavior::Blocks::Physicalize::Shape::Ball;
            options.Radius = ball.Info->m_Radius;
        }
        auto made = Behavior::Blocks::Physicalize::Make(session, options);
        if (!made)
            return false;
        const auto added = graph.Add(made.Value());
        graph.Share(added.Target(), physicalize.Target());
        for (int pin = 0; pin != 11; ++pin)
            graph.Share(added.Pin(pin), physicalize.Pin(pin));
        if (ball.Info->m_Radius <= 0.0f)
            graph.Bind(added.Pin(11, CKPGUID_MESH), source.Pout(0));

        graph.Flow(route, added.In(0));
        graph.Flow(added.Out(0), show.In(0));
    }
    return true;
}

void ExtendResetBallPieces(Behavior::Edit::Graph graph,
                           const std::vector<BallBehavior> &balls) {
    if (balls.empty())
        return;

    const auto sequencer = First(graph, "Sequencer");
    const auto scripts = First(graph, "Parameter Selector");
    const auto originalExit = graph.Between(
        sequencer.Out(4), graph.Root().Out(0), 0);

    std::vector<Behavior::Edit::Port> inputs;
    std::vector<Behavior::Edit::Port> outputs;
    inputs.reserve(balls.size());
    outputs.reserve(balls.size());
    for (const BallBehavior &ball : balls) {
        const auto script = graph.AppendPin(
            scripts, "Pin", CKPGUID_STRING);
        graph.Bind(script, "Ball_ResetPieces_" + ball.Info->m_Name);
        inputs.push_back(graph.AppendIn(scripts, "In"));
        outputs.push_back(graph.AppendOut(sequencer, "Out"));
    }

    for (std::size_t index = 0; index < inputs.size(); ++index) {
        graph.Flow(index == 0 ? sequencer.Out(4) : outputs[index - 1],
                   inputs[index]);
    }
    graph.Reconnect(originalExit, outputs.back(), graph.Root().Out(0));
}

void RemoveBallAttributes(const Behavior::Session &session,
                          Behavior::Edit::Graph graph,
                          const std::vector<BallBehavior> &balls) {
    const auto original = First(graph, "Remove Attribute");
    const auto entry = graph.Leaving(graph.Root().In(0));
    for (const BallBehavior &ball : balls) {
        auto remove = session.Use(VT_LOGICS_REMOVEATTRIBUTE);
        remove.Target(CKPGUID_BEOBJECT, ball.Ball);
        const auto added = graph.Add(remove);
        graph.Share(added.Pin(0), original.Pin(0));
        graph.Splice(entry, added);
    }
}

bool BuildEventHandlerEdit(const Behavior::Session &session,
                           const std::vector<BallBehavior> &balls,
                           Behavior::Edit &edit) {
    auto root = edit.Root();

    auto resetLevel = First(root, "reset Level").Graph();
    ExtendResetBallPieces(
        First(resetLevel, "reset Ballpieces").Graph(), balls);
    auto nestedReset = First(resetLevel, "reset  Level").Graph();
    RemoveBallAttributes(
        session, First(nestedReset, "reset Level").Graph(), balls);

    auto exitLevel = First(root, "Exit Level").Graph();
    ExtendResetBallPieces(
        First(exitLevel, "reset Ballpieces").Graph(), balls);
    RemoveBallAttributes(
        session, First(exitLevel, "reset Level").Graph(), balls);
    return true;
}

bool BuildGameplayEdit(const Behavior::Session &session,
                       CKAttributeType trafoType,
                       const std::vector<BallBehavior> &balls,
                       Behavior::Edit &edit) {
    auto root = edit.Root();
    auto ballManager = First(root, "BallManager").Graph();
    auto newBall = First(ballManager, "New Ball").Graph();
    if (!AddPhysicalize(
            session, First(newBall, "physicalize new Ball").Graph(), balls))
        return false;
    auto deactivate = First(ballManager, "Deactivate Ball").Graph();
    ExtendResetBallPieces(
        First(deactivate, "reset Ballpieces").Graph(), balls);

    auto init = First(root, "Init Ingame").Graph();
    auto setTrafo = First(init, "set Trafo-Attribute").Graph();
    const auto setTrafoExit = setTrafo.Entering(setTrafo.Root().Out(0));
    for (const BallBehavior &ball : balls) {
        auto set = session.Use(VT_LOGICS_SETATTRIBUTE);
        set.Target(CKPGUID_BEOBJECT, ball.Ball)
            .Pins({{Behavior::At(0), Behavior::Value::As(
                CKPGUID_ATTRIBUTE, static_cast<std::int32_t>(trafoType))}});
        const auto added = setTrafo.Add(set);
        const auto value = setTrafo.AppendPin(
            added, "Attribute Value", CKPGUID_STRING);
        setTrafo.Bind(value, ball.Info->m_ID);
        setTrafo.Splice(setTrafoExit, added);
    }

    auto trafo = First(root, "Trafo Manager").Graph();
    const auto piecesNode = First(trafo, "set Piecesflag");
    auto pieces = piecesNode.Graph();
    const auto pieceSwitch = First(pieces, "Switch On Parameter");
    const auto fadeoutNode = First(trafo, "Fadeout Manager");
    auto fadeout = fadeoutNode.Graph();
    Behavior::NodePattern usedIdentity;
    usedIdentity.Prototype(VT_LOGICS_IDENTITY).Pins(3);
    const auto currentUsed = fadeout.Require(std::move(usedIdentity));
    auto boolIdentity = session.Use(VT_LOGICS_IDENTITY);
    boolIdentity.PinType(Behavior::At(0), CKPGUID_BOOL)
        .PoutType(Behavior::At(0), CKPGUID_BOOL);

    for (const BallBehavior &ball : balls) {
        const auto used = trafo.AppendLocal(ball.Used, CKPGUID_BOOL);

        const auto pieceResult = trafo.AppendPout(
            piecesNode, ball.Used, CKPGUID_BOOL);
        trafo.Push(pieceResult, used);
        const auto pieceInput = pieces.AppendPin(
            pieceSwitch, "Pin", CKPGUID_STRING);
        pieces.Bind(pieceInput, ball.Info->m_ID);
        const auto pieceRoute = pieces.AppendOut(pieceSwitch, "Out");
        const auto setUsed = pieces.Add(boolIdentity);
        pieces.Bind(setUsed.Pin(0), true);
        pieces.Push(setUsed.Pout(0),
                    pieces.Root().Pout(ball.Used, CKPGUID_BOOL));
        pieces.Flow(pieceRoute, setUsed.In());
        pieces.Flow(setUsed.Out(), pieces.Root().Out(0));

        const auto usedInput = trafo.AppendPin(
            fadeoutNode, ball.Used, CKPGUID_BOOL);
        const auto usedResult = trafo.AppendPout(
            fadeoutNode, ball.Used, CKPGUID_BOOL);
        trafo.Bind(usedInput, used);
        trafo.Push(usedResult, used);

        const auto firstSwitch = fadeout.Add(
            session.Use(VT_LOGICS_BINARYSWITCH));
        const auto secondSwitch = fadeout.Add(
            session.Use(VT_LOGICS_BINARYSWITCH));
        const auto clearUsed = fadeout.Add(boolIdentity);
        fadeout.Bind(clearUsed.Pin(0), false);

        auto timerBlock = session.Use(VT_LOGICS_TIMER);
        timerBlock.Pins({{Behavior::At(0), Behavior::Value::As(
            CKPGUID_TIME, 20000.0f)}});
        const auto timer = fadeout.Add(timerBlock);

        auto activateBlock = session.Use(VT_NARRATIVES_ACTIVATESCRIPT);
        activateBlock.Pins({
            {Behavior::At(0), true},
            {Behavior::At(1),
             Behavior::Value::Object(CKPGUID_SCRIPT, ball.Reset)}});
        const auto activate = fadeout.Add(activateBlock);

        const auto state = fadeout.Root().Pin(ball.Used, CKPGUID_BOOL);
        const auto changed = fadeout.Root().Pout(ball.Used, CKPGUID_BOOL);
        fadeout.Push(currentUsed.Pout(0), changed);
        fadeout.Share(firstSwitch.Pin(0), state);
        fadeout.Share(secondSwitch.Pin(0), state);
        fadeout.Push(clearUsed.Pout(0), changed);

        fadeout.Flow(currentUsed.Out(), firstSwitch.In());
        fadeout.FlowCycle(firstSwitch.Out(1), firstSwitch.In(), 1);
        fadeout.Flow(firstSwitch.Out(0), clearUsed.In());
        fadeout.Flow(clearUsed.Out(), timer.In(0));
        fadeout.Flow(timer.Out(1), secondSwitch.In());
        fadeout.FlowCycle(secondSwitch.Out(1), timer.In(1), 1);
        fadeout.Flow(timer.Out(0), activate.In());
        fadeout.Flow(secondSwitch.Out(0), activate.In());
        fadeout.FlowCycle(activate.Out(), firstSwitch.In(), 1);
    }

    if (!AddPhysicalize(
            session, First(trafo, "physicalize new Ball").Graph(), balls))
        return false;

    auto explosion = First(trafo, "start Explosion").Graph();
    const auto explosionSwitch = First(explosion, "Switch On Parameter");
    const auto explosionScripts = First(explosion, "Parameter Selector");
    auto setBall = First(trafo, "set new Ball").Graph();
    const auto ballSwitch = First(setBall, "Switch On Parameter");
    const auto ballObjects = First(setBall, "Parameter Selector");
    for (const BallBehavior &ball : balls) {
        const auto explosionId = explosion.AppendPin(
            explosionSwitch, "Pin", CKPGUID_STRING);
        explosion.Bind(explosionId, ball.Info->m_ID);
        const auto explosionRoute = explosion.AppendOut(
            explosionSwitch, "Out");
        const auto explosionName = explosion.AppendPin(
            explosionScripts, "Pin", CKPGUID_STRING);
        explosion.Bind(
            explosionName, "Ball_Explosion_" + ball.Info->m_Name);
        const auto explosionIn = explosion.AppendIn(explosionScripts, "In");
        explosion.Flow(explosionRoute, explosionIn);

        const auto ballId = setBall.AppendPin(
            ballSwitch, "Pin", CKPGUID_STRING);
        setBall.Bind(ballId, ball.Info->m_ID);
        const auto ballRoute = setBall.AppendOut(ballSwitch, "Out");
        const auto ballObject = setBall.AppendPin(
            ballObjects, "Pin", CKPGUID_3DENTITY);
        setBall.Bind(
            ballObject,
            Behavior::Value::Object(CKPGUID_3DENTITY, ball.Ball));
        const auto ballIn = setBall.AppendIn(ballObjects, "In");
        setBall.Flow(ballRoute, ballIn);
    }

    return true;
}

} // namespace

void NewBallTypeMod::OnLoad() {
    auto opened = Behavior::Session::Open(GetID());
    if (!opened) {
        GetLogger()->Error("Behavior authoring is unavailable for new ball types: %s",
                           opened.GetStatus().Message.empty()
                               ? "could not open the owner session"
                               : opened.GetStatus().Message.c_str());
        return;
    }
    m_Behavior = opened.Take();
}

void NewBallTypeMod::OnUnload() {
    (void) m_BallPatch.Close();
    m_Behavior.Reset();
    m_GameplayScript = {};
    m_EventHandler = {};
    m_BallPatchPending = false;
}

void NewBallTypeMod::OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                                  CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                                  XObjectArray *objArray, CKObject *masterObj) {
    if (!strcmp(filename, "3D Entities\\Balls.nmo"))
        OnLoadBalls(objArray);

    if (!strcmp(filename, "3D Entities\\Levelinit.nmo"))
        OnLoadLevelinit(objArray);

    if (!strcmp(filename, "3D Entities\\Sound.nmo"))
        OnLoadSounds(objArray);
}

void NewBallTypeMod::OnLoadScript(const char *filename, CKBehavior *script) {
    if (!m_Behavior || !script || !script->GetName())
        return;
    Behavior::ObjectRef *target = nullptr;
    if (!strcmp(script->GetName(), "Gameplay_Ingame"))
        target = &m_GameplayScript;
    else if (!strcmp(script->GetName(), "Event_handler"))
        target = &m_EventHandler;
    if (!target)
        return;

    auto reference = m_Behavior.Reference(script);
    if (!reference) {
        GetLogger()->Error("Cannot identify %s for Behavior authoring: %s",
                           script->GetName(),
                           reference.GetStatus().Message.empty()
                               ? "object reference creation failed"
                               : reference.GetStatus().Message.c_str());
        return;
    }
    *target = reference.Value();
    m_BallPatchPending = true;
}

void NewBallTypeMod::OnProcess() {
    InstallBallBehaviorPatch();
}

void NewBallTypeMod::OnExitGame() {
    (void) m_BallPatch.Close();
    m_GameplayScript = {};
    m_EventHandler = {};
    m_BallPatchPending = false;
}

void NewBallTypeMod::RegisterBallType(const char *ballFile, const char *ballId, const char *ballName,
                                      const char *objName, float friction, float elasticity, float mass,
                                      const char *collGroup, float linearDamp, float rotDamp, float force,
                                      float radius) {
    m_BallTypes.emplace_back();
    BallTypeInfo &info = m_BallTypes.back();

    info.m_File = ballFile;
    info.m_ID = ballId;
    info.m_Name = ballName;
    info.m_ObjName = objName;

    info.m_Radius = radius;

    info.m_Friction = friction;
    info.m_Elasticity = elasticity;
    info.m_Mass = mass;
    info.m_CollGroup = collGroup;
    info.m_LinearDamp = linearDamp;
    info.m_RotDamp = rotDamp;
    info.m_Force = force;

    GetLogger()->Info("Registered New Ball Type: %s", ballName);
}

void NewBallTypeMod::RegisterFloorType(const char *floorName, float friction, float elasticity, float mass,
                                       const char *collGroup, bool enableColl) {
    m_FloorTypes.emplace_back();
    FloorTypeInfo &info = m_FloorTypes.back();

    info.m_Name = floorName;

    info.m_Friction = friction;
    info.m_Elasticity = elasticity;
    info.m_Mass = mass;
    info.m_CollGroup = collGroup;
    info.m_EnableColl = enableColl;

    GetLogger()->Info("Registered New Floor Type: %s", floorName);
}

void NewBallTypeMod::RegisterModulBall(const char *modulName, bool fixed, float friction, float elasticity, float mass,
                                       const char *collGroup, bool frozen, bool enableColl, bool calcMassCenter,
                                       float linearDamp, float rotDamp, float radius) {
    m_ModulBalls.emplace_back();
    ModulBallInfo &info = m_ModulBalls.back();

    info.m_Name = modulName;

    info.m_Fixed = fixed;
    info.m_Friction = friction;
    info.m_Elasticity = elasticity;
    info.m_Mass = mass;
    info.m_CollGroup = collGroup;
    info.m_Frozen = frozen;
    info.m_EnableColl = enableColl;
    info.m_MassCenter = calcMassCenter;
    info.m_LinearDamp = linearDamp;
    info.m_RotDamp = rotDamp;
    info.m_Radius = radius;

    GetLogger()->Info("Registered New Modul Ball Type: %s", modulName);
}

void NewBallTypeMod::RegisterModulConvex(const char *modulName, bool fixed, float friction, float elasticity,
                                         float mass, const char *collGroup, bool frozen, bool enableColl,
                                         bool calcMassCenter, float linearDamp, float rotDamp) {
    m_ModulConvexes.emplace_back();
    ModulConvexInfo &info = m_ModulConvexes.back();

    info.m_Name = modulName;

    info.m_Fixed = fixed;
    info.m_Friction = friction;
    info.m_Elasticity = elasticity;
    info.m_Mass = mass;
    info.m_CollGroup = collGroup;
    info.m_Frozen = frozen;
    info.m_EnableColl = enableColl;
    info.m_MassCenter = calcMassCenter;
    info.m_LinearDamp = linearDamp;
    info.m_RotDamp = rotDamp;

    GetLogger()->Info("Registered New Modul Convex Type: %s", modulName);
}

void NewBallTypeMod::RegisterTrafo(const char *modulName) {
    m_Moduls.emplace_back();
    ModulInfo &info = m_Moduls.back();

    info.m_Name = modulName;
    info.m_Type = 0;

    GetLogger()->Info("Registered New Ball Transformer Type: %s", modulName);
}

void NewBallTypeMod::RegisterModul(const char *modulName) {
    m_Moduls.emplace_back();
    ModulInfo &info = m_Moduls.back();

    info.m_Name = modulName;
    info.m_Type = 1;

    GetLogger()->Info("Registered New Modul Type: %s", modulName);
}

void NewBallTypeMod::OnLoadBalls(XObjectArray *objArray) {
    m_PhysicsBall = m_BML->GetArrayByName("Physicalize_GameBall");
    m_AllBalls = m_BML->GetGroupByName("All_Balls");
    std::string path = "3D Entities\\";
    CK3dEntity *ballMF = m_BML->Get3dEntityByName("Balls_MF");
    CKContext *context = m_BML->GetCKContext();
    if (!m_Behavior || !context || !m_PhysicsBall || !m_AllBalls || !ballMF) {
        GetLogger()->Error(
            "Cannot load registered ball types: the Behavior session or base ball objects are unavailable");
        return;
    }

    for (BallTypeInfo &info: m_BallTypes) {
        Behavior::Blocks::ObjectLoad::Options options;
        options.File = path + info.m_File;
        auto block = Behavior::Blocks::ObjectLoad::Make(m_Behavior, options);
        if (!block) {
            GetLogger()->Error(
                "Cannot configure Object Load for ball type %s: %s",
                info.m_Name.c_str(), block.GetStatus().Message.empty()
                    ? "the Block was rejected"
                    : block.GetStatus().Message.c_str());
            return;
        }
        auto called = block->Call("Load", Behavior::Signals(1).Pouts());
        if (!called) {
            GetLogger()->Error(
                "Cannot load ball type %s: %s", info.m_Name.c_str(),
                called.GetStatus().Message.empty()
                    ? "Object Load could not execute"
                    : called.GetStatus().Message.c_str());
            return;
        }
        Behavior::Call call = called.Take();
        auto captured = call.TakeFrames();
        if (!captured || captured->Empty()) {
            GetLogger()->Error(
                "Cannot load ball type %s: Object Load produced no Frame",
                info.m_Name.c_str());
            return;
        }
        Behavior::Frames frames = captured.Take();
        const Behavior::Frame frame = frames[frames.Size() - 1];
        if (!frame.HasOut("Loaded") || frame.HasOut("Failed")) {
            GetLogger()->Error(
                "Cannot load ball type %s: Object Load did not activate Loaded",
                info.m_Name.c_str());
            return;
        }
        auto loaded = frame.Pout<Behavior::ObjectList>("Loaded Objects");
        if (!loaded) {
            GetLogger()->Error(
                "Cannot read the objects loaded for ball type %s: %s",
                info.m_Name.c_str(), loaded.GetStatus().Message.empty()
                    ? "Loaded Objects is unavailable"
                    : loaded.GetStatus().Message.c_str());
            return;
        }

        info.m_AllGroup = nullptr;
        info.m_BallObj = nullptr;
        info.m_PiecesGroup = nullptr;
        info.m_PiecesFrame = nullptr;
        info.m_Explosion = nullptr;
        info.m_Reset = nullptr;

        std::string allGroup = "All_" + info.m_ObjName;
        std::string piecesGroup = info.m_ObjName + "_Pieces";
        std::string piecesFrame = info.m_ObjName + "Pieces_Frame";
        std::string explosion = "Ball_Explosion_" + info.m_Name;
        std::string reset = "Ball_ResetPieces_" + info.m_Name;

        for (Behavior::ObjectRef reference : loaded.Value()) {
            BML::Scene::ObjectInfo observed;
            if (BML::Scene::ReadObject(reference, observed) != BML_OK)
                continue;
            CKObject *obj = context->GetObject(observed.Id);
            if (!obj || !obj->GetName())
                continue;
            const char *name = obj->GetName();
            if (allGroup == name && CKIsChildClassOf(obj, CKCID_GROUP))
                info.m_AllGroup = static_cast<CKGroup *>(obj);
            if (info.m_ObjName == name &&
                CKIsChildClassOf(obj, CKCID_3DOBJECT))
                info.m_BallObj = static_cast<CK3dObject *>(obj);
            if (piecesGroup == name && CKIsChildClassOf(obj, CKCID_GROUP))
                info.m_PiecesGroup = static_cast<CKGroup *>(obj);
            if (piecesFrame == name &&
                CKIsChildClassOf(obj, CKCID_3DENTITY))
                info.m_PiecesFrame = static_cast<CK3dEntity *>(obj);
            if (explosion == name && CKIsChildClassOf(obj, CKCID_BEHAVIOR))
                info.m_Explosion = static_cast<CKBehavior *>(obj);
            if (reset == name && CKIsChildClassOf(obj, CKCID_BEHAVIOR))
                info.m_Reset = static_cast<CKBehavior *>(obj);
        }

        if (!info.m_AllGroup ||
            !info.m_BallObj ||
            !info.m_PiecesGroup ||
            !info.m_PiecesFrame ||
            !info.m_Explosion ||
            !info.m_Reset) {
            GetLogger()->Error(
                "Cannot register ball type %s: its loaded object set is incomplete",
                info.m_Name.c_str());
            return;
        }

        info.m_BallObj->SetParent(ballMF);
        info.m_PiecesFrame->SetParent(ballMF);

        m_PhysicsBall->AddRow();
        int row = m_PhysicsBall->GetRowCount() - 1;

        m_PhysicsBall->SetElementStringValue(row, 0, (CKSTRING) info.m_ObjName.c_str());
        m_PhysicsBall->SetElementValue(row, 1, &info.m_Friction);
        m_PhysicsBall->SetElementValue(row, 2, &info.m_Elasticity);
        m_PhysicsBall->SetElementValue(row, 3, &info.m_Mass);
        m_PhysicsBall->SetElementStringValue(row, 4, (CKSTRING) info.m_CollGroup.c_str());
        m_PhysicsBall->SetElementValue(row, 5, &info.m_LinearDamp);
        m_PhysicsBall->SetElementValue(row, 6, &info.m_RotDamp);
        m_PhysicsBall->SetElementValue(row, 7, &info.m_Force);

        for (int i = 0; i < info.m_AllGroup->GetObjectCount(); i++)
            m_AllBalls->AddObject(info.m_AllGroup->GetObject(i));
    }

    m_BallPatchPending = true;
    GetLogger()->Info("New Ball Types Registered");
}

void NewBallTypeMod::OnLoadLevelinit(XObjectArray *objArray) {
    CKDataArray *phGroups = m_BML->GetArrayByName("PH_Groups");
    CKDataArray *physBalls = m_BML->GetArrayByName("Physicalize_Balls");
    CKDataArray *physConvexs = m_BML->GetArrayByName("Physicalize_Convex");
    CKDataArray *physFloors = m_BML->GetArrayByName("Physicalize_Floors");

    for (ModulInfo &info: m_Moduls) {
        phGroups->AddRow();
        int row = phGroups->GetRowCount() - 1;
        phGroups->SetElementStringValue(row, 0, (CKSTRING) info.m_Name.c_str());
        int activation = 1;
        phGroups->SetElementValue(row, 2, &activation);
        phGroups->SetElementValue(row, 3, &info.m_Type);
    }

    for (FloorTypeInfo &info: m_FloorTypes) {
        physFloors->AddRow();
        int row = physFloors->GetRowCount() - 1;
        physFloors->SetElementStringValue(row, 0, (CKSTRING) info.m_Name.c_str());
        physFloors->SetElementValue(row, 1, &info.m_Friction);
        physFloors->SetElementValue(row, 2, &info.m_Elasticity);
        physFloors->SetElementValue(row, 3, &info.m_Mass);
        physFloors->SetElementStringValue(row, 4, (CKSTRING) info.m_CollGroup.c_str());
        physFloors->SetElementValue(row, 5, &info.m_EnableColl);
    }

    for (ModulConvexInfo &info: m_ModulConvexes) {
        physConvexs->AddRow();
        int row = physConvexs->GetRowCount() - 1;
        physConvexs->SetElementStringValue(row, 0, (CKSTRING) info.m_Name.c_str());
        physConvexs->SetElementValue(row, 1, &info.m_Fixed);
        physConvexs->SetElementValue(row, 2, &info.m_Friction);
        physConvexs->SetElementValue(row, 3, &info.m_Elasticity);
        physConvexs->SetElementValue(row, 4, &info.m_Mass);
        physConvexs->SetElementStringValue(row, 5, (CKSTRING) info.m_CollGroup.c_str());
        physConvexs->SetElementValue(row, 6, &info.m_Frozen);
        physConvexs->SetElementValue(row, 7, &info.m_EnableColl);
        physConvexs->SetElementValue(row, 8, &info.m_MassCenter);
        physConvexs->SetElementValue(row, 9, &info.m_LinearDamp);
        physConvexs->SetElementValue(row, 10, &info.m_RotDamp);

        phGroups->AddRow();
        row = phGroups->GetRowCount() - 1;
        phGroups->SetElementStringValue(row, 0, (CKSTRING) info.m_Name.c_str());
        int activation = 2, reset = 2;
        phGroups->SetElementValue(row, 2, &activation);
        phGroups->SetElementValue(row, 3, &reset);
    }

    for (ModulBallInfo &info: m_ModulBalls) {
        physBalls->AddRow();
        int row = physBalls->GetRowCount() - 1;
        physBalls->SetElementStringValue(row, 0, (CKSTRING) info.m_Name.c_str());
        physBalls->SetElementValue(row, 1, &info.m_Fixed);
        physBalls->SetElementValue(row, 2, &info.m_Friction);
        physBalls->SetElementValue(row, 3, &info.m_Elasticity);
        physBalls->SetElementValue(row, 4, &info.m_Mass);
        physBalls->SetElementStringValue(row, 5, (CKSTRING) info.m_CollGroup.c_str());
        physBalls->SetElementValue(row, 6, &info.m_Frozen);
        physBalls->SetElementValue(row, 7, &info.m_EnableColl);
        physBalls->SetElementValue(row, 8, &info.m_MassCenter);
        physBalls->SetElementValue(row, 9, &info.m_LinearDamp);
        physBalls->SetElementValue(row, 10, &info.m_RotDamp);
        physBalls->SetElementValue(row, 11, &info.m_Radius);

        phGroups->AddRow();
        row = phGroups->GetRowCount() - 1;
        phGroups->SetElementStringValue(row, 0, (CKSTRING) info.m_Name.c_str());
        int activation = 3, reset = 2;
        phGroups->SetElementValue(row, 2, &activation);
        phGroups->SetElementValue(row, 3, &reset);
    }

    m_BML->SetIC(phGroups);

    GetLogger()->Info("New Modul & Floor Types Registered");
}

void NewBallTypeMod::OnLoadSounds(XObjectArray *objArray) {
    CKDataArray *ballSound = m_BML->GetArrayByName("BallSound");

    for (BallTypeInfo &info: m_BallTypes) {
        std::string roll = "Roll_" + info.m_Name;
        std::string hit = "Hit_" + info.m_Name;

        ballSound->AddRow();
        int row = ballSound->GetRowCount() - 1;
        ballSound->SetElementStringValue(row, 0, (CKSTRING) info.m_ObjName.c_str());
        ballSound->SetElementStringValue(row, 1, (CKSTRING) ((roll + "_Stone").c_str()));
        ballSound->SetElementStringValue(row, 2, (CKSTRING) ((roll + "_Wood").c_str()));
        ballSound->SetElementStringValue(row, 3, (CKSTRING) ((roll + "_Metal").c_str()));
        ballSound->SetElementStringValue(row, 4, (CKSTRING) ((hit + "_Stone").c_str()));
        ballSound->SetElementStringValue(row, 5, (CKSTRING) ((hit + "_Wood").c_str()));
        ballSound->SetElementStringValue(row, 6, (CKSTRING) ((hit + "_Metal").c_str()));
        ballSound->SetElementStringValue(row, 7, (CKSTRING) ((hit + "_Dome").c_str()));
    }

    GetLogger()->Info("New Ball Sounds Registered");
}

void NewBallTypeMod::InstallBallBehaviorPatch() {
    if (!m_BallPatchPending || !m_Behavior ||
        !m_GameplayScript.Domain || !m_EventHandler.Domain)
        return;

    if (m_BallPatch) {
        auto closed = m_BallPatch.Close();
        if (!closed) {
            GetLogger()->Error(
                "Cannot replace the new-ball Behavior Patch: %s",
                closed.GetStatus().Message.empty()
                    ? "the previous Patch could not be restored"
                    : closed.GetStatus().Message.c_str());
            m_BallPatchPending = false;
            return;
        }
        if (closed.Value() == Behavior::CloseState::Closing)
            return;
    }

    if (m_BallTypes.empty()) {
        m_BallPatchPending = false;
        return;
    }

    std::vector<BallBehavior> balls;
    balls.reserve(m_BallTypes.size());
    for (const BallTypeInfo &info : m_BallTypes) {
        if (!info.m_BallObj || !info.m_Reset) {
            GetLogger()->Error(
                "Cannot author the new-ball graphs before ball type %s is loaded",
                info.m_Name.c_str());
            m_BallPatchPending = false;
            return;
        }
        auto ball = m_Behavior.Reference(info.m_BallObj);
        auto reset = m_Behavior.Reference(info.m_Reset);
        if (!ball || !reset) {
            const Behavior::Status &status = !ball
                ? ball.GetStatus() : reset.GetStatus();
            GetLogger()->Error(
                "Cannot identify the objects for ball type %s: %s",
                info.m_Name.c_str(), status.Message.empty()
                    ? "object reference creation failed"
                    : status.Message.c_str());
            m_BallPatchPending = false;
            return;
        }
        balls.push_back(
            {&info, ball.Value(), reset.Value(),
             "__BML_NewBall_Used_" + info.m_ID});
    }

    auto eventGraph = m_Behavior.Inspect(m_EventHandler);
    auto gameplayGraph = m_Behavior.Inspect(m_GameplayScript);
    if (!eventGraph || !gameplayGraph) {
        const Behavior::Status &status = !eventGraph
            ? eventGraph.GetStatus() : gameplayGraph.GetStatus();
        GetLogger()->Error(
            "Cannot inspect the ball gameplay graphs: %s",
            status.Message.empty() ? "graph inspection failed"
                                   : status.Message.c_str());
        m_BallPatchPending = false;
        return;
    }

    CKAttributeManager *attributes = m_BML->GetAttributeManager();
    const CKAttributeType trafoType = attributes
        ? attributes->GetAttributeTypeByName("TrafoType") : -1;
    if (trafoType < 0) {
        GetLogger()->Error(
            "Cannot author the new-ball graphs: TrafoType is unavailable");
        m_BallPatchPending = false;
        return;
    }

    Behavior::Edit eventEdit;
    Behavior::Edit gameplayEdit;
    if (!BuildEventHandlerEdit(m_Behavior, balls, eventEdit) ||
        !BuildGameplayEdit(m_Behavior, trafoType, balls, gameplayEdit)) {
        GetLogger()->Error(
            "Cannot describe the new-ball graph changes with Behavior authoring");
        m_BallPatchPending = false;
        return;
    }

    auto applied = m_Behavior.Apply(
        "New ball types",
        Behavior::On(eventGraph.Value(), eventEdit),
        Behavior::On(gameplayGraph.Value(), gameplayEdit));
    if (!applied) {
        GetLogger()->Error(
            "Cannot apply the new-ball graph changes: %s",
            applied.GetStatus().Message.empty()
                ? "the Patch was rejected"
                : applied.GetStatus().Message.c_str());
        m_BallPatchPending = false;
        return;
    }

    m_BallPatch = applied.Take();
    m_BallPatchPending = false;
    GetLogger()->Info("Installed the new-ball Behavior Patch");
}

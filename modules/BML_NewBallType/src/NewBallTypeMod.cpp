#include "NewBallTypeMod.h"

#include "BML/ScriptGraph.h"
#include "BML/Guids/Logics.h"
#include "BML/Guids/Narratives.h"
#include "BML/Guids/physics_RT.h"

void NewBallTypeMod::SetServices(const bml::ModuleServices *services) {
    m_Services = services;
}

void NewBallTypeMod::SetContext(CKContext *context) {
    m_Context = context;
}

bool NewBallTypeMod::InitObjectLoader(CKContext *ctx) {
    if (!ctx) return false;
    auto *owner = static_cast<CKBehavior *>(
        ctx->GetObjectByNameAndClass("Level_Init", CKCID_BEHAVIOR));
    if (!owner) return false;
    m_ObjectLoader = bml::ObjectLoadCache(ctx);
    return m_ObjectLoader.Init(owner);
}

void NewBallTypeMod::SetInitialConditions(CKBeObject *object, bool hierarchy) const {
    if (!m_Context || !object) {
        return;
    }

    m_Context->GetCurrentScene()->SetObjectInitialValue(object, CKSaveObjectState(object));

    if (!hierarchy) {
        return;
    }

    if (CKIsChildClassOf(object, CKCID_2DENTITY)) {
        auto *entity = static_cast<CK2dEntity *>(object);
        for (int index = 0; index < entity->GetChildrenCount(); ++index) {
            SetInitialConditions(entity->GetChild(index), true);
        }
    }

    if (CKIsChildClassOf(object, CKCID_3DENTITY)) {
        auto *entity = static_cast<CK3dEntity *>(object);
        for (int index = 0; index < entity->GetChildrenCount(); ++index) {
            SetInitialConditions(entity->GetChild(index), true);
        }
    }
}

void NewBallTypeMod::HandleLoadObject(const char *filename, CKBOOL isMap, CK_CLASSID filterClass,
                                      const CK_ID *objectIds, uint32_t objectCount) {
    (void)isMap;
    (void)filterClass;

    if (!strcmp(filename, "3D Entities\\Balls.nmo"))
        OnLoadBalls(objectIds, objectCount);

    if (!strcmp(filename, "3D Entities\\Levelinit.nmo"))
        OnLoadLevelinit();

    if (!strcmp(filename, "3D Entities\\Sound.nmo"))
        OnLoadSounds();
}

void NewBallTypeMod::HandleLoadScript(CKBehavior *script) {
    if (!script) {
        return;
    }

    if (!strcmp(script->GetName(), "Gameplay_Ingame"))
        OnEditScript_Gameplay_Ingame(script);

    if (!strcmp(script->GetName(), "Event_handler"))
        OnEditScript_Base_EventHandler(script);
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

    if (m_Services) m_Services->Log().Info("Registered New Ball Type: %s", ballName ? ballName : "");
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

    if (m_Services) m_Services->Log().Info("Registered New Floor Type: %s", floorName ? floorName : "");
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

    if (m_Services) m_Services->Log().Info("Registered New Modul Ball Type: %s", modulName ? modulName : "");
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

    if (m_Services) m_Services->Log().Info("Registered New Modul Convex Type: %s", modulName ? modulName : "");
}

void NewBallTypeMod::RegisterTrafo(const char *modulName) {
    m_Moduls.emplace_back();
    ModulInfo &info = m_Moduls.back();

    info.m_Name = modulName;
    info.m_Type = 0;

    if (m_Services) m_Services->Log().Info("Registered New Ball Transformer Type: %s", modulName ? modulName : "");
}

void NewBallTypeMod::RegisterModul(const char *modulName) {
    m_Moduls.emplace_back();
    ModulInfo &info = m_Moduls.back();

    info.m_Name = modulName;
    info.m_Type = 1;

    if (m_Services) m_Services->Log().Info("Registered New Modul Type: %s", modulName ? modulName : "");
}

void NewBallTypeMod::OnLoadBalls(const CK_ID *objectIds, uint32_t objectCount) {
    m_PhysicsBall.Reset(m_Context, static_cast<CKDataArray *>(m_Context->GetObjectByNameAndClass((CKSTRING)"Physicalize_GameBall", CKCID_DATAARRAY)));
    m_AllBalls.Reset(m_Context, static_cast<CKGroup *>(m_Context->GetObjectByNameAndClass((CKSTRING)"All_Balls", CKCID_GROUP)));
    std::string path = "3D Entities\\";
    CK3dEntity *ballMF = static_cast<CK3dEntity *>(m_Context->GetObjectByNameAndClass((CKSTRING)"Balls_MF", CKCID_3DENTITY));

    for (BallTypeInfo &info: m_BallTypes) {
        auto loadResult = m_ObjectLoader.Load({.file = (path + info.m_File).c_str()});
        XObjectArray *objects = loadResult.objects;

        std::string allGroup = "All_" + info.m_ObjName;
        std::string piecesGroup = info.m_ObjName + "_Pieces";
        std::string piecesFrame = info.m_ObjName + "Pieces_Frame";
        std::string explosion = "Ball_Explosion_" + info.m_Name;
        std::string reset = "Ball_ResetPieces_" + info.m_Name;

        for (CK_ID *id = objects->Begin(); id != objects->End(); id++) {
            CKObject *obj = m_Context->GetObject(*id);
            const char *name = obj->GetName();
            if (name) {
                if (allGroup == name)
                    info.m_AllGroup.Reset(m_Context, (CKGroup *) obj);
                if (info.m_ObjName == name)
                    info.m_BallObj.Reset(m_Context, (CK3dObject *) obj);
                if (piecesGroup == name)
                    info.m_PiecesGroup.Reset(m_Context, (CKGroup *) obj);
                if (piecesFrame == name)
                    info.m_PiecesFrame.Reset(m_Context, (CK3dEntity *) obj);
                if (explosion == name)
                    info.m_Explosion.Reset(m_Context, (CKBehavior *) obj);
                if (reset == name)
                    info.m_Reset.Reset(m_Context, (CKBehavior *) obj);
            }
        }

        auto *allGroup_p = info.m_AllGroup.Get();
        auto *ballObj_p = info.m_BallObj.Get();
        auto *piecesGroup_p = info.m_PiecesGroup.Get();
        auto *piecesFrame_p = info.m_PiecesFrame.Get();
        auto *explosion_p = info.m_Explosion.Get();
        auto *reset_p = info.m_Reset.Get();
        if (!allGroup_p || !ballObj_p || !piecesGroup_p ||
            !piecesFrame_p || !explosion_p || !reset_p) {
            if (m_Services) m_Services->Log().Error("Ball type '%s' incomplete -- skipping", info.m_Name.c_str());
            continue;
        }

        auto *ballParam_p = info.m_BallParam.Get();
        auto *resetParam_p = info.m_ResetParam.Get();
        if (ballParam_p) bml::SetParam(ballParam_p, static_cast<CKObject *>(ballObj_p));
        if (resetParam_p) bml::SetParam(resetParam_p, static_cast<CKObject *>(reset_p));
        ballObj_p->SetParent(ballMF);
        piecesFrame_p->SetParent(ballMF);

        auto *physicsBall_p = m_PhysicsBall.Get();
        auto *allBalls_p = m_AllBalls.Get();
        if (!physicsBall_p || !allBalls_p) {
            if (m_Services) m_Services->Log().Error("PhysicsBall or AllBalls array unavailable");
            continue;
        }
        physicsBall_p->AddRow();
        int row = physicsBall_p->GetRowCount() - 1;

        physicsBall_p->SetElementStringValue(row, 0, (CKSTRING) info.m_ObjName.c_str());
        physicsBall_p->SetElementValue(row, 1, &info.m_Friction);
        physicsBall_p->SetElementValue(row, 2, &info.m_Elasticity);
        physicsBall_p->SetElementValue(row, 3, &info.m_Mass);
        physicsBall_p->SetElementStringValue(row, 4, (CKSTRING) info.m_CollGroup.c_str());
        physicsBall_p->SetElementValue(row, 5, &info.m_LinearDamp);
        physicsBall_p->SetElementValue(row, 6, &info.m_RotDamp);
        physicsBall_p->SetElementValue(row, 7, &info.m_Force);

        for (int i = 0; i < allGroup_p->GetObjectCount(); i++)
            allBalls_p->AddObject(allGroup_p->GetObject(i));
    }

    if (m_Services) m_Services->Log().Info("New Ball Types Registered");
}

void NewBallTypeMod::OnLoadLevelinit() {
    CKDataArray *phGroups = static_cast<CKDataArray *>(m_Context->GetObjectByNameAndClass((CKSTRING)"PH_Groups", CKCID_DATAARRAY));
    CKDataArray *physBalls = static_cast<CKDataArray *>(m_Context->GetObjectByNameAndClass((CKSTRING)"Physicalize_Balls", CKCID_DATAARRAY));
    CKDataArray *physConvexs = static_cast<CKDataArray *>(m_Context->GetObjectByNameAndClass((CKSTRING)"Physicalize_Convex", CKCID_DATAARRAY));
    CKDataArray *physFloors = static_cast<CKDataArray *>(m_Context->GetObjectByNameAndClass((CKSTRING)"Physicalize_Floors", CKCID_DATAARRAY));

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

    SetInitialConditions(phGroups);

    if (m_Services) m_Services->Log().Info("New Modul & Floor Types Registered");
}

void NewBallTypeMod::OnLoadSounds() {
    CKDataArray *ballSound = static_cast<CKDataArray *>(m_Context->GetObjectByNameAndClass((CKSTRING)"BallSound", CKCID_DATAARRAY));

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

    if (m_Services) m_Services->Log().Info("New Ball Sounds Registered");
}

void NewBallTypeMod::OnEditScript_Gameplay_Ingame(CKBehavior *script) {
    if (m_Services) m_Services->Log().Info("Modify Ingame script to accommodate new ball types");
    bml::Graph graph(script);

    auto ballManager = graph.Find("BallManager");
    if (!ballManager) {
        if (m_Services) m_Services->Log().Error("Expected 'BallManager' not found in script");
        return;
    }

    auto initIngame = graph.Find("Init Ingame");
    if (!initIngame) {
        if (m_Services) m_Services->Log().Error("Expected 'Init Ingame' not found in script");
        return;
    }

    auto trafoManager = graph.Find("Trafo Manager");
    if (!trafoManager) {
        if (m_Services) m_Services->Log().Error("Expected 'Trafo Manager' not found in script");
        return;
    }

    {
        auto phyNewBall = ballManager.Find("New Ball").Find("physicalize new Ball");
        OnEditScript_PhysicalizeNewBall(phyNewBall);

        auto resetPieces = ballManager.Find("Deactivate Ball").Find("reset Ballpieces");
        OnEditScript_ResetBallPieces(resetPieces);
    }

    {
        auto trafoAttr = initIngame.Find("set Trafo-Attribute");
        bml::Graph tg(trafoAttr);
        CKAttributeManager *am = m_Context->GetAttributeManager();
        CKAttributeType trafoType = am->GetAttributeTypeByName("TrafoType");
        for (BallTypeInfo &info: m_BallTypes) {
            CKBehavior *setAttr = tg.CreateBehavior(VT_LOGICS_SETATTRIBUTE, true);
            CKParameter *attr = tg.Param("Attr", CKPGUID_ATTRIBUTE, trafoType);
            CKParameter *attrParam = tg.ParamString("Param", info.m_ID.c_str());
            setAttr->GetTargetParameter()->SetDirectSource(info.m_BallParam.Get());
            setAttr->GetInputParameter(0)->SetDirectSource(attr);
            setAttr->CreateInputParameter("Param", CKPGUID_STRING)->SetDirectSource(attrParam);
            tg.Insert(tg.From(trafoAttr->GetOutput(0)).PrevLink(), setAttr);
        }
    }

    {
        {
            auto pieceFlag = trafoManager.Find("set Piecesflag");
            bml::Graph pf(pieceFlag);
            CKBehavior *sop = pieceFlag.Find("Switch On Parameter");
            CKParameterType booltype = m_Context->GetParameterManager()->ParameterGuidToType(CKPGUID_BOOL);
            for (BallTypeInfo &info: m_BallTypes) {
                CKParameter *id = pf.ParamString("Pin", info.m_ID.c_str());
                CKParameter *boolTrue = pf.Param("True", CKPGUID_BOOL, TRUE);
                CKBehavior *identity = pf.CreateBehavior(VT_LOGICS_IDENTITY);
                identity->GetInputParameter(0)->SetType(booltype);
                identity->GetOutputParameter(0)->SetType(booltype);

                identity->GetInputParameter(0)->SetDirectSource(boolTrue);
                identity->GetOutputParameter(0)->AddDestination(info.m_UsedParam.Get(), false);
                sop->CreateInputParameter("Pin", CKPGUID_STRING)->SetDirectSource(id);
                pf.Link(sop->CreateOutput("Out"), identity);
                pf.Link(identity, pieceFlag->GetOutput(0));
            }
        }

        {
            OnEditScript_PhysicalizeNewBall(trafoManager.Find("physicalize new Ball"));
        }

        {
            auto explode = trafoManager.Find("start Explosion");
            bml::Graph eg(explode);
            CKBehavior *sop = explode.Find("Switch On Parameter");
            CKBehavior *ps = explode.Find("Parameter Selector");
            for (BallTypeInfo &info: m_BallTypes) {
                std::string explosion = "Ball_Explosion_" + info.m_Name;
                CKParameter *id = eg.ParamString("Pin", info.m_ID.c_str());
                CKParameter *scr = eg.ParamString("Pin", explosion.c_str());
                sop->CreateInputParameter("Pin", CKPGUID_STRING)->SetDirectSource(id);
                ps->CreateInputParameter("Pin", CKPGUID_STRING)->SetDirectSource(scr);
                eg.Link(sop->CreateOutput("Out"), ps->CreateInput("In"));
            }
        }

        {
            auto setNewBall = trafoManager.Find("set new Ball");
            bml::Graph sg(setNewBall);
            CKBehavior *sop = setNewBall.Find("Switch On Parameter");
            CKBehavior *ps = setNewBall.Find("Parameter Selector");
            for (BallTypeInfo &info: m_BallTypes) {
                CKParameter *id = sg.ParamString("Pin", info.m_ID.c_str());
                sop->CreateInputParameter("Pin", CKPGUID_STRING)->SetDirectSource(id);
                ps->CreateInputParameter("Pin", CKPGUID_3DENTITY)->SetDirectSource(info.m_BallParam.Get());
                sg.Link(sop->CreateOutput("Out"), ps->CreateInput("In"));
            }
        }

        {
            auto fadeoutNode = trafoManager.Find("Fadeout Manager");
            bml::Graph fade(fadeoutNode);
            CKBehavior *identity = nullptr;
            fadeoutNode.FindAll([&identity](CKBehavior *beh) {
                if (beh->GetInputParameterCount() == 3) {
                    identity = beh;
                    return false;
                }
                return true;
            }, "Identity");
            CKParameterType booltype = m_Context->GetParameterManager()->ParameterGuidToType(CKPGUID_BOOL);
            CKParameter *time = fade.Param("Time", CKPGUID_TIME, 20000.0f);
            CKParameter *reset = fade.Param("Reset", CKPGUID_BOOL, TRUE);
            CKParameter *setfalse = fade.Param("False", CKPGUID_BOOL, FALSE);
            for (BallTypeInfo &info: m_BallTypes) {
                CKBehavior *binswitch[2] = {fade.CreateBehavior(VT_LOGICS_BINARYSWITCH), fade.CreateBehavior(VT_LOGICS_BINARYSWITCH)};
                CKBehavior *seton = fade.CreateBehavior(VT_LOGICS_IDENTITY);
                seton->GetInputParameter(0)->SetType(booltype);
                seton->GetOutputParameter(0)->SetType(booltype);
                CKBehavior *timer = fade.CreateBehavior(VT_LOGICS_TIMER);
                CKBehavior *activate = fade.CreateBehavior(VT_NARRATIVES_ACTIVATESCRIPT);
                info.m_Timer.Reset(m_Context, timer);
                info.m_BinarySwitch[0].Reset(m_Context, binswitch[0]);
                info.m_BinarySwitch[1].Reset(m_Context, binswitch[1]);

                identity->GetOutputParameter(0)->AddDestination(info.m_UsedParam.Get(), false);
                binswitch[0]->GetInputParameter(0)->SetDirectSource(info.m_UsedParam.Get());
                binswitch[1]->GetInputParameter(0)->SetDirectSource(info.m_UsedParam.Get());
                seton->GetInputParameter(0)->SetDirectSource(setfalse);
                seton->GetOutputParameter(0)->AddDestination(info.m_UsedParam.Get(), false);
                timer->GetInputParameter(0)->SetDirectSource(time);
                activate->GetInputParameter(0)->SetDirectSource(reset);
                activate->GetInputParameter(1)->SetDirectSource(info.m_ResetParam.Get());

                fade.Link(identity, binswitch[0]);
                fade.Link(binswitch[0], binswitch[0], 1, 0, 1);
                fade.Link(binswitch[0], seton);
                fade.Link(seton, timer);
                fade.Link(timer, binswitch[1], 1);
                fade.Link(binswitch[1], timer, 1, 1, 1);
                fade.Link(timer, activate);
                fade.Link(binswitch[1], activate);
                fade.Link(activate, binswitch[0], 0, 0, 1);
            }
        }
    }
}

void NewBallTypeMod::OnEditScript_Base_EventHandler(CKBehavior *script) {
    if (m_Services) m_Services->Log().Info("Reset ball pieces for new ball types");
    bml::Graph graph(script);

    auto resetLevel = graph.Find("reset Level");
    if (!resetLevel) {
        if (m_Services) m_Services->Log().Error("Expected 'reset Level' not found in script");
        return;
    }

    auto exitLevel = graph.Find("Exit Level");
    if (!exitLevel) {
        if (m_Services) m_Services->Log().Error("Expected 'Exit Level' not found in script");
        return;
    }

    for (BallTypeInfo &info: m_BallTypes) {
        info.m_BallParam.Reset(m_Context, graph.Param("Target", CKPGUID_BEOBJECT));
        info.m_UsedParam.Reset(m_Context, graph.Param("Used", CKPGUID_BOOL));
        info.m_ResetParam.Reset(m_Context, graph.Param("Script", CKPGUID_SCRIPT));
    }

    auto addResetAttr = [this](CKBehavior *behGraph) {
        bml::Graph g(behGraph);
        CKBehavior *remAttr = g.Find("Remove Attribute");
        for (BallTypeInfo &info: m_BallTypes) {
            CKBehavior *attr = g.CreateBehavior(VT_LOGICS_REMOVEATTRIBUTE, true);
            attr->GetTargetParameter()->SetDirectSource(info.m_BallParam.Get());
            attr->GetInputParameter(0)->ShareSourceWith(remAttr->GetInputParameter(0));
            g.Insert(g.From(behGraph->GetInput(0)).NextLink(), attr);
        }
    };

    OnEditScript_ResetBallPieces(resetLevel.Find("reset Ballpieces"));
    CKBehavior *innerReset = resetLevel.Find("reset  Level").Find("reset Level");
    addResetAttr(innerReset);

    OnEditScript_ResetBallPieces(exitLevel.Find("reset Ballpieces"));
    addResetAttr(exitLevel.Find("reset Level"));
}

void NewBallTypeMod::OnEditScript_PhysicalizeNewBall(CKBehavior *behGraph) {
    bml::Graph g(behGraph);
    CKBehavior *physicalize = g.Find("Physicalize");
    CKBehavior *sop = g.Find("Switch On Parameter");
    CKBehavior *show = g.Find("Show");
    CKBehavior *op = g.From(behGraph->GetInput(0)).Next();

    for (BallTypeInfo &info: m_BallTypes) {
        CKParameter *ballName = g.ParamString("Pin", info.m_ObjName.c_str());
        sop->CreateInputParameter("Pin", CKPGUID_STRING)->SetDirectSource(ballName);
        CKBehavior *newPhy;
        if (info.m_Radius > 0) {
            bml::BehaviorFactory factory(behGraph, PHYSICS_RT_PHYSICALIZE, true);
            factory.Local(0, 0).Local(1, 1).ApplySettings();
            factory.Input(11, VxVector(0, 0, 0));
            factory.Input(12, info.m_Radius);
            newPhy = factory.Build();
        } else {
            bml::BehaviorFactory factory(behGraph, PHYSICS_RT_PHYSICALIZE, true);
            newPhy = factory.Build();
            newPhy->GetInputParameter(11)->SetDirectSource(op->GetOutputParameter(0));
        }

        newPhy->GetTargetParameter()->ShareSourceWith(physicalize->GetTargetParameter());
        for (int i = 0; i < 11; i++)
            newPhy->GetInputParameter(i)->ShareSourceWith(physicalize->GetInputParameter(i));
        g.Link(sop->CreateOutput("Out"), newPhy);
        g.Link(newPhy, show);
    }
}

void NewBallTypeMod::OnEditScript_ResetBallPieces(CKBehavior *behGraph) {
    bml::Graph g(behGraph);
    CKBehavior *seq = g.Find("Sequencer");
    CKBehavior *ps = g.Find("Parameter Selector");

    for (BallTypeInfo &info: m_BallTypes) {
        std::string reset = "Ball_ResetPieces_" + info.m_Name;
        CKParameter *param = g.ParamString("Pin", reset.c_str());
        ps->CreateInputParameter("Pin", CKPGUID_STRING)->SetDirectSource(param);

        int cnt = seq->GetOutputCount() - 1;
        g.From(seq->GetOutput(cnt)).NextLink()->SetInBehaviorIO(seq->CreateOutput("Out"));
        g.Link(seq->GetOutput(seq->GetOutputCount() - 2), ps->CreateInput("In"));
    }
}

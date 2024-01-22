#include "NewBallTypeMod.h"

#include "BML/IBML.h"
#include "BML/ExecuteBB.h"
#include "BML/ScriptHelper.h"

using namespace ScriptHelper;

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

    for (BallTypeInfo &info: m_BallTypes) {
        XObjectArray *objects = ExecuteBB::ObjectLoad((path + info.m_File).c_str(), false).first;

        std::string allGroup = "All_" + info.m_ObjName;
        std::string piecesGroup = info.m_ObjName + "_Pieces";
        std::string piecesFrame = info.m_ObjName + "Pieces_Frame";
        std::string explosion = "Ball_Explosion_" + info.m_Name;
        std::string reset = "Ball_ResetPieces_" + info.m_Name;

        for (CK_ID *id = objects->Begin(); id != objects->End(); id++) {
            CKObject *obj = m_BML->GetCKContext()->GetObject(*id);
            const char *name = obj->GetName();
            if (name) {
                if (allGroup == name)
                    info.m_AllGroup = (CKGroup *) obj;
                if (info.m_ObjName == name)
                    info.m_BallObj = (CK3dObject *) obj;
                if (piecesGroup == name)
                    info.m_PiecesGroup = (CKGroup *) obj;
                if (piecesFrame == name)
                    info.m_PiecesFrame = (CK3dEntity *) obj;
                if (explosion == name)
                    info.m_Explosion = (CKBehavior *) obj;
                if (reset == name)
                    info.m_Reset = (CKBehavior *) obj;
            }
        }

        if (!info.m_AllGroup ||
            !info.m_BallObj ||
            !info.m_PiecesGroup ||
            !info.m_PiecesFrame ||
            !info.m_Explosion ||
            !info.m_Reset) {
            GetLogger()->Info("Register New Ball Types Failed");
            return;
        }

        SetParamObject(info.m_BallParam, info.m_BallObj);
        SetParamObject(info.m_ResetParam, info.m_Reset);
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

void NewBallTypeMod::OnEditScript_Gameplay_Ingame(CKBehavior *script) {
    GetLogger()->Info("Modify Ingame script to accommodate new ball types");

    {
        CKBehavior *ballMgr = FindFirstBB(script, "BallManager");
        CKBehavior *newBall = FindFirstBB(ballMgr, "New Ball");
        CKBehavior *phyNewBall = FindFirstBB(newBall, "physicalize new Ball");
        OnEditScript_PhysicalizeNewBall(phyNewBall);

        CKBehavior *deactBall = FindFirstBB(ballMgr, "Deactivate Ball");
        CKBehavior *resetPieces = FindFirstBB(deactBall, "reset Ballpieces");
        OnEditScript_ResetBallPieces(resetPieces);
    }

    {
        CKBehavior *init = FindFirstBB(script, "Init Ingame");
        CKBehavior *trafoAttr = FindFirstBB(init, "set Trafo-Attribute");
        CKAttributeManager *am = m_BML->GetAttributeManager();
        CKAttributeType trafoType = am->GetAttributeTypeByName("TrafoType");
        for (BallTypeInfo &info: m_BallTypes) {
            CKBehavior *setAttr = CreateBB(trafoAttr, VT_LOGICS_SETATTRIBUTE, true);
            CKParameter *attr = CreateParamValue(trafoAttr, "Attr", CKPGUID_ATTRIBUTE, trafoType);
            CKParameter *attrParam = CreateParamString(trafoAttr, "Param", info.m_ID.c_str());
            setAttr->GetTargetParameter()->SetDirectSource(info.m_BallParam);
            setAttr->GetInputParameter(0)->SetDirectSource(attr);
            setAttr->CreateInputParameter("Param", CKPGUID_STRING)->SetDirectSource(attrParam);
            InsertBB(trafoAttr, FindPreviousLink(trafoAttr, trafoAttr->GetOutput(0)), setAttr);
        }
    }

    {
        CKBehavior *trafoMgr = FindFirstBB(script, "Trafo Manager");
        {
            CKBehavior *pieceFlag = FindFirstBB(trafoMgr, "set Piecesflag");
            CKBehavior *sop = FindFirstBB(pieceFlag, "Switch On Parameter");
            CKParameterType booltype = m_BML->GetParameterManager()->ParameterGuidToType(CKPGUID_BOOL);
            for (BallTypeInfo &info: m_BallTypes) {
                CKParameter *id = CreateParamString(pieceFlag, "Pin", info.m_ID.c_str());
                CKParameter *boolTrue = CreateParamValue(pieceFlag, "True", CKPGUID_BOOL, TRUE);
                CKBehavior *identity = CreateBB(pieceFlag, VT_LOGICS_IDENTITY);
                identity->GetInputParameter(0)->SetType(booltype);
                identity->GetOutputParameter(0)->SetType(booltype);

                identity->GetInputParameter(0)->SetDirectSource(boolTrue);
                identity->GetOutputParameter(0)->AddDestination(info.m_UsedParam, false);
                sop->CreateInputParameter("Pin", CKPGUID_STRING)->SetDirectSource(id);
                CreateLink(pieceFlag, sop->CreateOutput("Out"), identity);
                CreateLink(pieceFlag, identity, pieceFlag->GetOutput(0));
            }
        }

        {
            CKBehavior *phyNewBall = FindFirstBB(trafoMgr, "physicalize new Ball");
            OnEditScript_PhysicalizeNewBall(phyNewBall);
        }

        {
            CKBehavior *explode = FindFirstBB(trafoMgr, "start Explosion");
            CKBehavior *sop = FindFirstBB(explode, "Switch On Parameter");
            CKBehavior *ps = FindFirstBB(explode, "Parameter Selector");
            for (BallTypeInfo &info: m_BallTypes) {
                std::string explosion = "Ball_Explosion_" + info.m_Name;
                CKParameter *id = CreateParamString(explode, "Pin", info.m_ID.c_str());
                CKParameter *scr = CreateParamString(explode, "Pin", explosion.c_str());
                sop->CreateInputParameter("Pin", CKPGUID_STRING)->SetDirectSource(id);
                ps->CreateInputParameter("Pin", CKPGUID_STRING)->SetDirectSource(scr);
                CreateLink(explode, sop->CreateOutput("Out"), ps->CreateInput("In"));
            }
        }

        {
            CKBehavior *setNewBall = FindFirstBB(trafoMgr, "set new Ball");
            CKBehavior *sop = FindFirstBB(setNewBall, "Switch On Parameter");
            CKBehavior *ps = FindFirstBB(setNewBall, "Parameter Selector");
            for (BallTypeInfo &info: m_BallTypes) {
                CKParameter *id = CreateParamString(setNewBall, "Pin", info.m_ID.c_str());
                sop->CreateInputParameter("Pin", CKPGUID_STRING)->SetDirectSource(id);
                ps->CreateInputParameter("Pin", CKPGUID_3DENTITY)->SetDirectSource(info.m_BallParam);
                CreateLink(setNewBall, sop->CreateOutput("Out"), ps->CreateInput("In"));
            }
        }

        {
            CKBehavior *fadeout = FindFirstBB(trafoMgr, "Fadeout Manager");
            CKBehavior *identity = nullptr;
            FindBB(
                fadeout, [&identity](CKBehavior *beh) {
                    if (beh->GetInputParameterCount() == 3) {
                        identity = beh;
                        return false;
                    } else return true;
                },
                "Identity");
            CKParameterType booltype = m_BML->GetParameterManager()->ParameterGuidToType(CKPGUID_BOOL);
            CKParameter *time = CreateParamValue(fadeout, "Time", CKPGUID_TIME, 20000.0f);
            CKParameter *reset = CreateParamValue(fadeout, "Reset", CKPGUID_BOOL, TRUE);
            CKParameter *setfalse = CreateParamValue(fadeout, "False", CKPGUID_BOOL, FALSE);
            for (BallTypeInfo &info: m_BallTypes) {
                CKBehavior *binswitch[2] = {CreateBB(fadeout, VT_LOGICS_BINARYSWITCH), CreateBB(fadeout, VT_LOGICS_BINARYSWITCH)};
                CKBehavior *seton = CreateBB(fadeout, VT_LOGICS_IDENTITY);
                seton->GetInputParameter(0)->SetType(booltype);
                seton->GetOutputParameter(0)->SetType(booltype);
                CKBehavior *timer = CreateBB(fadeout, VT_LOGICS_TIMER);
                CKBehavior *activate = CreateBB(fadeout, VT_NARRATIVES_ACTIVATESCRIPT);
                info.m_Timer = timer;
                info.m_BinarySwitch[0] = binswitch[0];
                info.m_BinarySwitch[1] = binswitch[1];

                identity->GetOutputParameter(0)->AddDestination(info.m_UsedParam, false);
                binswitch[0]->GetInputParameter(0)->SetDirectSource(info.m_UsedParam);
                binswitch[1]->GetInputParameter(0)->SetDirectSource(info.m_UsedParam);
                seton->GetInputParameter(0)->SetDirectSource(setfalse);
                seton->GetOutputParameter(0)->AddDestination(info.m_UsedParam, false);
                timer->GetInputParameter(0)->SetDirectSource(time);
                activate->GetInputParameter(0)->SetDirectSource(reset);
                activate->GetInputParameter(1)->SetDirectSource(info.m_ResetParam);

                CreateLink(fadeout, identity, binswitch[0]);
                CreateLink(fadeout, binswitch[0], binswitch[0], 1, 0, 1);
                CreateLink(fadeout, binswitch[0], seton);
                CreateLink(fadeout, seton, timer);
                CreateLink(fadeout, timer, binswitch[1], 1);
                CreateLink(fadeout, binswitch[1], timer, 1, 1, 1);
                CreateLink(fadeout, timer, activate);
                CreateLink(fadeout, binswitch[1], activate);
                CreateLink(fadeout, activate, binswitch[0], 0, 0, 1);
            }
        }
    }
}

void NewBallTypeMod::OnEditScript_Base_EventHandler(CKBehavior *script) {
    GetLogger()->Info("Reset ball pieces for new ball types");

    for (BallTypeInfo &info: m_BallTypes) {
        info.m_BallParam = CreateLocalParameter(script, "Target", CKPGUID_BEOBJECT);
        info.m_UsedParam = CreateLocalParameter(script, "Used", CKPGUID_BOOL);
        info.m_ResetParam = CreateLocalParameter(script, "Script", CKPGUID_SCRIPT);
    }

    auto addResetAttr = [this](CKBehavior *graph) {
        CKBehavior *remAttr = FindFirstBB(graph, "Remove Attribute");
        for (BallTypeInfo &info: m_BallTypes) {
            CKBehavior *attr = CreateBB(graph, VT_LOGICS_REMOVEATTRIBUTE, true);
            attr->GetTargetParameter()->SetDirectSource(info.m_BallParam);
            attr->GetInputParameter(0)->ShareSourceWith(remAttr->GetInputParameter(0));
            InsertBB(graph, FindNextLink(graph, graph->GetInput(0)), attr);
        }
    };

    CKBehavior *resetLevel = FindFirstBB(script, "reset Level");
    CKBehavior *resetPieces = FindFirstBB(resetLevel, "reset Ballpieces");
    OnEditScript_ResetBallPieces(resetPieces);
    resetLevel = FindFirstBB(resetLevel, "reset  Level");
    resetLevel = FindFirstBB(resetLevel, "reset Level");
    addResetAttr(resetLevel);

    CKBehavior *exitLevel = FindFirstBB(script, "Exit Level");
    resetPieces = FindFirstBB(exitLevel, "reset Ballpieces");
    OnEditScript_ResetBallPieces(resetPieces);
    resetLevel = FindFirstBB(exitLevel, "reset Level");
    addResetAttr(resetLevel);
}

void NewBallTypeMod::OnEditScript_PhysicalizeNewBall(CKBehavior *graph) {
    CKBehavior *physicalize = FindFirstBB(graph, "Physicalize");
    CKBehavior *sop = FindFirstBB(graph, "Switch On Parameter");
    CKBehavior *show = FindFirstBB(graph, "Show");
    CKBehavior *op = FindNextBB(graph, graph->GetInput(0));

    for (BallTypeInfo &info: m_BallTypes) {
        CKParameter *ballName = CreateParamString(graph, "Pin", info.m_ObjName.c_str());
        sop->CreateInputParameter("Pin", CKPGUID_STRING)->SetDirectSource(ballName);
        CKBehavior *newPhy;
        if (info.m_Radius > 0) {
            newPhy = ExecuteBB::CreatePhysicalizeBall(graph);
            SetParamValue(newPhy->GetInputParameter(12)->GetDirectSource(), info.m_Radius);
        } else {
            newPhy = ExecuteBB::CreatePhysicalizeConvex(graph);
            newPhy->GetInputParameter(11)->SetDirectSource(op->GetOutputParameter(0));
        }

        newPhy->GetTargetParameter()->ShareSourceWith(physicalize->GetTargetParameter());
        for (int i = 0; i < 11; i++)
            newPhy->GetInputParameter(i)->ShareSourceWith(physicalize->GetInputParameter(i));
        CreateLink(graph, sop->CreateOutput("Out"), newPhy);
        CreateLink(graph, newPhy, show);
    }
}

void NewBallTypeMod::OnEditScript_ResetBallPieces(CKBehavior *graph) {
    CKBehavior *seq = FindFirstBB(graph, "Sequencer");
    CKBehavior *ps = FindFirstBB(graph, "Parameter Selector");

    for (BallTypeInfo &info: m_BallTypes) {
        std::string reset = "Ball_ResetPieces_" + info.m_Name;
        CKParameter *script = CreateParamString(graph, "Pin", reset.c_str());
        ps->CreateInputParameter("Pin", CKPGUID_STRING)->SetDirectSource(script);

        int cnt = seq->GetOutputCount() - 1;
        FindNextLink(graph, seq->GetOutput(cnt))->SetInBehaviorIO(seq->CreateOutput("Out"));
        CreateLink(graph, seq->GetOutput(seq->GetOutputCount() - 2), ps->CreateInput("In"));
    }
}
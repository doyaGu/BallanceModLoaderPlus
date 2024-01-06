#include "Summoner.h"

#include "BML/ScriptHelper.h"

IMod *BMLEntry(IBML *bml) {
    return new Summoner(bml);
}

void BMLExit(IMod *mod) {
    delete mod;
}

void Summoner::OnLoad() {
    GetConfig()->SetCategoryComment("Auxiliaries", "Temporal Auxiliary Moduls");

    m_AddBall[0] = GetConfig()->GetProperty("Auxiliaries", "PaperBall");
    m_AddBall[0]->SetComment("Add a Paper Ball");
    m_AddBall[0]->SetDefaultKey(CKKEY_J);

    m_AddBall[1] = GetConfig()->GetProperty("Auxiliaries", "WoodBall");
    m_AddBall[1]->SetComment("Add a Wood Ball");
    m_AddBall[1]->SetDefaultKey(CKKEY_K);

    m_AddBall[2] = GetConfig()->GetProperty("Auxiliaries", "StoneBall");
    m_AddBall[2]->SetComment("Add a Stone Ball");
    m_AddBall[2]->SetDefaultKey(CKKEY_N);

    m_AddBall[3] = GetConfig()->GetProperty("Auxiliaries", "Box");
    m_AddBall[3]->SetComment("Add a Box");
    m_AddBall[3]->SetDefaultKey(CKKEY_M);

    m_MoveKeys[0] = GetConfig()->GetProperty("Auxiliaries", "MoveFront");
    m_MoveKeys[0]->SetComment("Move Front");
    m_MoveKeys[0]->SetDefaultKey(CKKEY_UP);

    m_MoveKeys[1] = GetConfig()->GetProperty("Auxiliaries", "MoveBack");
    m_MoveKeys[1]->SetComment("Move Back");
    m_MoveKeys[1]->SetDefaultKey(CKKEY_DOWN);

    m_MoveKeys[2] = GetConfig()->GetProperty("Auxiliaries", "MoveLeft");
    m_MoveKeys[2]->SetComment("Move Left");
    m_MoveKeys[2]->SetDefaultKey(CKKEY_LEFT);

    m_MoveKeys[3] = GetConfig()->GetProperty("Auxiliaries", "MoveRight");
    m_MoveKeys[3]->SetComment("Move Right");
    m_MoveKeys[3]->SetDefaultKey(CKKEY_RIGHT);

    m_MoveKeys[4] = GetConfig()->GetProperty("Auxiliaries", "MoveUp");
    m_MoveKeys[4]->SetComment("Move Up");
    m_MoveKeys[4]->SetDefaultKey(CKKEY_RSHIFT);

    m_MoveKeys[5] = GetConfig()->GetProperty("Auxiliaries", "MoveDown");
    m_MoveKeys[5]->SetComment("Move Down");
    m_MoveKeys[5]->SetDefaultKey(CKKEY_RCONTROL);

    m_Balls[0] = (CK3dEntity *) ExecuteBB::ObjectLoad("3D Entities\\PH\\P_Ball_Paper.nmo", true, "P_Ball_Paper_MF").second;
    m_Balls[1] = (CK3dEntity *) ExecuteBB::ObjectLoad("3D Entities\\PH\\P_Ball_Wood.nmo", true, "P_Ball_Wood_MF").second;
    m_Balls[2] = (CK3dEntity *) ExecuteBB::ObjectLoad("3D Entities\\PH\\P_Ball_Stone.nmo", true, "P_Ball_Stone_MF").second;
    m_Balls[3] = (CK3dEntity *) ExecuteBB::ObjectLoad("3D Entities\\PH\\P_Box.nmo", true, "P_Box_MF").second;

    m_InputHook = m_BML->GetInputManager();
}

void Summoner::OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass, CKBOOL addToScene, CKBOOL reuseMeshes,
                              CKBOOL reuseMaterials, CKBOOL dynamic, XObjectArray *objArray, CKObject *masterObj) {
    if (!strcmp(filename, "3D Entities\\Camera.nmo")) {
        m_CamOrientRef = m_BML->Get3dEntityByName("Cam_OrientRef");
        m_CamTarget = m_BML->Get3dEntityByName("Cam_Target");
    }
}

void Summoner::OnLoadScript(const char *filename, CKBehavior *script) {
    if (!strcmp(script->GetName(), "Gameplay_Events"))
        OnEditScript_Gameplay_Events(script);
}

void Summoner::OnProcess() {
    m_DeltaTime = m_BML->GetTimeManager()->GetLastDeltaTime() / 10;

    if (!(m_BML->IsPlaying() && m_BML->IsCheatEnabled()))
        return;

    VxVector vector;

    if (m_CurSel < 0) {
        for (int i = 0; i < 4; i++) {
            if (m_InputHook->IsKeyDown(m_AddBall[i]->GetKey())) {
                m_CurSel = i;
                m_InputHook->SetBlock(true);
            }
        }

        if (m_CurSel >= 0) {
            m_CurObj = (CK3dEntity *) m_BML->GetCKContext()->CopyObject(m_Balls[m_CurSel]);
            vector = VxVector(0, 5, 0);
            m_CurObj->SetPosition(&vector, m_CamTarget);
            m_CurObj->Show();
        }
    } else if (m_InputHook->oIsKeyDown(m_AddBall[m_CurSel]->GetKey())) {
        if (m_InputHook->oIsKeyDown(m_MoveKeys[0]->GetKey())) {
            vector = VxVector(0, 0, 0.1f * m_DeltaTime);
            m_CurObj->Translate(&vector, m_CamOrientRef);
        }
        if (m_InputHook->oIsKeyDown(m_MoveKeys[1]->GetKey())) {
            vector = VxVector(0, 0, -0.1f * m_DeltaTime);
            m_CurObj->Translate(&vector, m_CamOrientRef);
        }
        if (m_InputHook->oIsKeyDown(m_MoveKeys[2]->GetKey())) {
            vector = VxVector(-0.1f * m_DeltaTime, 0, 0);
            m_CurObj->Translate(&vector, m_CamOrientRef);
        }
        if (m_InputHook->oIsKeyDown(m_MoveKeys[3]->GetKey())) {
            vector = VxVector(0.1f * m_DeltaTime, 0, 0);
            m_CurObj->Translate(&vector, m_CamOrientRef);
        }
        if (m_InputHook->oIsKeyDown(m_MoveKeys[4]->GetKey())) {
            vector = VxVector(0, 0.1f * m_DeltaTime, 0);
            m_CurObj->Translate(&vector, m_CamOrientRef);
        }
        if (m_InputHook->oIsKeyDown(m_MoveKeys[5]->GetKey())) {
            vector = VxVector(0, -0.1f * m_DeltaTime, 0);
            m_CurObj->Translate(&vector, m_CamOrientRef);
        }
    } else {
        CKMesh *mesh = m_CurObj->GetMesh(0);
        switch (m_CurSel) {
            case 0:
                ExecuteBB::PhysicalizeConvex(m_CurObj, false, 0.5f, 0.4f, 0.2f, "", false, true, false, 1.5f, 0.1f, mesh->GetName(), VxVector(0, 0, 0), mesh);
                break;
            case 1:
                ExecuteBB::PhysicalizeBall(m_CurObj, false, 0.6f, 0.2f, 2.0f, "", false, true, false, 0.6f, 0.1f, mesh->GetName());
                break;
            case 2:
                ExecuteBB::PhysicalizeBall(m_CurObj, false, 0.7f, 0.1f, 10.0f, "", false, true, false, 0.2f, 0.1f, mesh->GetName());
                break;
            default:
                ExecuteBB::PhysicalizeConvex(m_CurObj, false, 0.7f, 0.3f, 1.0f, "", false, true, false, 0.1f, 0.1f, mesh->GetName(), VxVector(0, 0, 0), mesh);
                break;
        }

        CKDataArray *ph = m_BML->GetArrayByName("PH");
        ph->AddRow();
        int index = ph->GetRowCount() - 1;
        ph->SetElementValueFromParameter(index, 0, m_CurSector);
        static char P_BALL_NAMES[4][13] = {"P_Ball_Paper", "P_Ball_Wood", "P_Ball_Stone", "P_Box"};
        ph->SetElementStringValue(index, 1, P_BALL_NAMES[m_CurSel]);
        VxMatrix matrix = m_CurObj->GetWorldMatrix();
        ph->SetElementValue(index, 2, &matrix);
        ph->SetElementObject(index, 3, m_CurObj);
        CKBOOL set = false;
        ph->SetElementValue(index, 4, &set);

        CKGroup *depth = m_BML->GetGroupByName("DepthTest");
        depth->AddObject(m_CurObj);
        m_TempBalls.emplace_back(index, m_CurObj);

        m_CurSel = -1;
        m_CurObj = nullptr;
        m_InputHook->SetBlock(false);

        GetLogger()->Info("Summoned a %s", m_CurSel < 2 ? m_CurSel == 0 ? "Paper Ball" : "Wood Ball" : m_CurSel == 2 ? "Stone Ball" : "Box");
    }
}

void Summoner::OnPostResetLevel() {
    CKDataArray *ph = m_BML->GetArrayByName("PH");
    for (auto iter = m_TempBalls.rbegin(); iter != m_TempBalls.rend(); iter++) {
        ph->RemoveRow(iter->first);
        m_BML->GetCKContext()->DestroyObject(iter->second);
    }
    m_TempBalls.clear();
}

void Summoner::OnEditScript_Gameplay_Events(CKBehavior *script) {
    CKBehavior *id = ScriptHelper::FindNextBB(script, script->GetInput(0));
    m_CurSector = id->GetOutputParameter(0)->GetDestination(0);
}
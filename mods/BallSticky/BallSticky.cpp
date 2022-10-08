#include "BallSticky.h"

using namespace ScriptHelper;

IMod* BMLEntry(IBML* bml) {
	return new BallSticky(bml);
}

void BMLExit(IMod *mod) {
    delete mod;
}

void BallSticky::OnLoad() {
    m_StickyImpulse = 90.0f;
    m_BML->RegisterBallType("Ball_Sticky.nmo", "sticky", "Sticky", "Ball_Sticky", 10.0f, 0.0f, 1.4f, "Ball", 1.0f, 7.0f, 0.15f, 2.0f);
    m_BML->RegisterModulBall("P_Ball_Sticky", false, 10.0f, 0.0f, 1.4f, "", false, true, false, 0.8f, 7.0f, 2.0f);
    m_BML->RegisterTrafo("P_Trafo_Sticky");

	for (int i = 0; i < 2; i++) {
        m_BallRef[i] = (CK3dEntity*)m_BML->GetCKContext()->CreateObject(CKCID_3DENTITY, "Ball_Sticky_Ref");
        m_BML->GetCKContext()->GetCurrentScene()->AddObjectToScene(m_BallRef[i]);
	}
}

void BallSticky::OnProcess() {
	if (m_BML->IsIngame() && m_CurLevel) {
		auto* curBall = (CK3dEntity*)m_CurLevel->GetElementObject(0, 1);
		if (curBall) {
			VxVector pos;
			curBall->GetPosition(&pos);
			pos.y += 1;
			m_BallRef[0]->SetPosition(&pos);
			pos.y -= 2;
			m_BallRef[1]->SetPosition(&pos);

			bool isSticky = !strcmp(curBall->GetName(), "Ball_Sticky");
			SetParamValue(m_StickyForce[0], isSticky ? m_StickyImpulse : 0);
			SetParamValue(m_StickyForce[1], isSticky ? -m_StickyImpulse : 0);
		}
	}
}

void BallSticky::OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName,
                              CK_CLASSID filterClass, CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials,
                              CKBOOL dynamic, XObjectArray *objArray, CKObject *masterObj) {
	if (!strcmp(filename, "3D Entities\\Levelinit.nmo"))
		OnLoadLevelinit(objArray);
}

void BallSticky::OnLoadScript(const char *filename, CKBehavior* script) {
	if (!strcmp(script->GetName(), "Gameplay_Ingame"))
        OnEditScript_Gameplay_Ingame(script);
}

void BallSticky::OnLoadLevelinit(XObjectArray* objArray) {
	// CKDataArray* allLevel = m_BML->GetArrayByName("AllLevel");
	// for (int i = 0; i < allLevel->GetRowCount(); i++)
	// 	allLevel->SetElementStringValue(i, 1, "Ball_Sticky");
}

void BallSticky::OnEditScript_Gameplay_Ingame(CKBehavior* script) {
    m_CurLevel = m_BML->GetArrayByName("CurrentLevel");

	CKBehavior* ballNav = FindFirstBB(script, "Ball Navigation");
	CKBehavior* oForce = FindFirstBB(ballNav, "SetPhysicsForce");
	CKBehavior* forces[8], * keepActive[8], * perSecond[8];
    m_StickyForce[0] = CreateParamValue(ballNav, "Force", CKPGUID_FLOAT, m_StickyImpulse);
    m_StickyForce[1] = CreateParamValue(ballNav, "Force", CKPGUID_FLOAT, -m_StickyImpulse);
	CKParameter* posRef[2] = { CreateParamObject(ballNav, "PosRef", CKPGUID_3DENTITY, m_BallRef[0]),
		CreateParamObject(ballNav, "PosRef", CKPGUID_3DENTITY, m_BallRef[1]) };
	for (int i = 0; i < 8; i++) {
		keepActive[i] = CreateBB(ballNav, VT_LOGICS_KEEPACTIVE);
		perSecond[i] = CreateBB(ballNav, VT_LOGICS_PERSECOND);
		perSecond[i]->GetInputParameter(0)->SetDirectSource(m_StickyForce[i % 2]);
		CreateLink(ballNav, keepActive[i], perSecond[i], 1);
		forces[i] = CreateBB(ballNav, PHYSICS_RT_PHYSICSIMPULSE, true);
		forces[i]->GetTargetParameter()->ShareSourceWith(oForce->GetTargetParameter());
		forces[i]->GetInputParameter(0)->ShareSourceWith(oForce->GetInputParameter(0));
		forces[i]->GetInputParameter(1)->SetDirectSource(posRef[i % 2]);
		forces[i]->GetInputParameter(3)->ShareSourceWith(oForce->GetInputParameter(3));
		forces[i]->GetInputParameter(4)->SetDirectSource(perSecond[i]->GetOutputParameter(0));
		CreateLink(ballNav, perSecond[i], forces[i]);
	}

	int cnt = 0;
	FindBB(ballNav, [ballNav, &forces, &keepActive, &cnt](CKBehavior* beh) {
		VxVector dir;
		beh->GetInputParameterValue(2, &dir);
		int idx = -1;
		if (dir.x == 1) idx = 0;
		if (dir.x == -1) idx = 2;
		if (dir.z == 1) idx = 4;
		if (dir.z == -1) idx = 6;
		if (idx >= 0) {
			cnt++;
			for (int i = idx; i < idx + 2; i++) {
				forces[i]->GetInputParameter(2)->ShareSourceWith(beh->GetInputParameter(2));
				CreateLink(ballNav, beh, keepActive[i], 0, 1);
				CreateLink(ballNav, beh, keepActive[i], 1, 0);
			}
		}
		return cnt < 4;
	}, "SetPhysicsForce");
}
#include "TravelMode.h"
#include "TravelCommand.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

IMod *BMLEntry(IBML *bml) {
    return new TravelMode(bml);
}

void BMLExit(IMod *mod) {
    delete mod;
}

void TravelMode::OnLoad() {
    GetConfig()->SetCategoryComment("Camera", "Camera Utilities");

    m_CamOn = GetConfig()->GetProperty("Camera", "Enable");
    m_CamOn->SetComment("Enable Camera Utilities");
    m_CamOn->SetDefaultBoolean(false);

    m_CamReset = GetConfig()->GetProperty("Camera", "Reset");
    m_CamReset->SetComment("Reset Camera");
    m_CamReset->SetDefaultKey(CKKEY_D);

    m_Cam45 = GetConfig()->GetProperty("Camera", "Rotate45");
    m_Cam45->SetComment("Set to 45 degrees");
    m_Cam45->SetDefaultKey(CKKEY_W);

    m_CamRot[0] = GetConfig()->GetProperty("Camera", "RotateLeft");
    m_CamRot[0]->SetComment("Rotate the camera");
    m_CamRot[0]->SetDefaultKey(CKKEY_Q);

    m_CamRot[1] = GetConfig()->GetProperty("Camera", "RotateRight");
    m_CamRot[1]->SetComment("Rotate the camera");
    m_CamRot[1]->SetDefaultKey(CKKEY_E);

    m_CamY[0] = GetConfig()->GetProperty("Camera", "MoveUp");
    m_CamY[0]->SetComment("Move the camera");
    m_CamY[0]->SetDefaultKey(CKKEY_A);

    m_CamY[1] = GetConfig()->GetProperty("Camera", "MoveDown");
    m_CamY[1]->SetComment("Move the camera");
    m_CamY[1]->SetDefaultKey(CKKEY_Z);

    m_CamZ[0] = GetConfig()->GetProperty("Camera", "MoveFront");
    m_CamZ[0]->SetComment("Move the camera");
    m_CamZ[0]->SetDefaultKey(CKKEY_S);

    m_CamZ[1] = GetConfig()->GetProperty("Camera", "MoveBack");
    m_CamZ[1]->SetComment("Move the camera");
    m_CamZ[1]->SetDefaultKey(CKKEY_X);

    m_BML->RegisterCommand(new TravelCommand(this));

    m_RenderContext = m_BML->GetRenderContext();
    m_InputHook = m_BML->GetInputManager();
    m_TravelCam = (CKCamera *) m_BML->GetCKContext()->CreateObject(CKCID_CAMERA, "TravelCam");
}

void TravelMode::OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass, CKBOOL addToScene, CKBOOL reuseMeshes,
                              CKBOOL reuseMaterials, CKBOOL dynamic, XObjectArray *objArray, CKObject *masterObj) {
    if (!strcmp(filename, "3D Entities\\Camera.nmo")) {
        m_CamPos = m_BML->Get3dEntityByName("Cam_Pos");
        m_CamOrient = m_BML->Get3dEntityByName("Cam_Orient");
        m_CamOrientRef = m_BML->Get3dEntityByName("Cam_OrientRef");
        m_CamTarget = m_BML->Get3dEntityByName("Cam_Target");
    }
}

void TravelMode::OnProcess() {
    m_DeltaTime = m_BML->GetTimeManager()->GetLastDeltaTime() / 10;

    VxVector vect;
    VxQuaternion quat;

    if (!m_Paused && IsInTravelCam()) {
        if (m_InputHook->IsKeyDown(CKKEY_1)) {
            m_TravelSpeed = 0.2f;
        } else if (m_InputHook->IsKeyDown(CKKEY_2)) {
            m_TravelSpeed = 0.4f;
        } else if (m_InputHook->IsKeyDown(CKKEY_3)) {
            m_TravelSpeed = 0.8f;
        } else if (m_InputHook->IsKeyDown(CKKEY_4)) {
            m_TravelSpeed = 1.6f;
        } else if (m_InputHook->IsKeyDown(CKKEY_5)) {
            m_TravelSpeed = 2.4f;
        }

        if (m_InputHook->IsKeyDown(CKKEY_W)) {
            vect = VxVector(0, 0, m_TravelSpeed * m_DeltaTime);
            m_TravelCam->Translate(&vect, m_TravelCam);
        }
        if (m_InputHook->IsKeyDown(CKKEY_S)) {
            vect = VxVector(0, 0, -m_TravelSpeed * m_DeltaTime);
            m_TravelCam->Translate(&vect, m_TravelCam);
        }
        if (m_InputHook->IsKeyDown(CKKEY_A)) {
            vect = VxVector(-m_TravelSpeed * m_DeltaTime, 0, 0);
            m_TravelCam->Translate(&vect, m_TravelCam);
        }
        if (m_InputHook->IsKeyDown(CKKEY_D)) {
            vect = VxVector(m_TravelSpeed * m_DeltaTime, 0, 0);
            m_TravelCam->Translate(&vect, m_TravelCam);
        }
        if (m_InputHook->IsKeyDown(CKKEY_SPACE)) {
            vect = VxVector(0, m_TravelSpeed * m_DeltaTime, 0);
            m_TravelCam->Translate(&vect);
        }
        if (m_InputHook->IsKeyDown(CKKEY_LSHIFT)) {
            vect = VxVector(0, -m_TravelSpeed * m_DeltaTime, 0);
            m_TravelCam->Translate(&vect);
        }

        VxVector delta;
        m_InputHook->GetMouseRelativePosition(delta);
        if (delta.x != 0) {
            vect = VxVector(0, 1, 0);
            m_TravelCam->Rotate(&vect, (-delta.x / (float)::GetSystemMetrics(SM_CXSCREEN)) * 180.0f / PI);
        }
        if (delta.y != 0) {
            vect = VxVector(1, 0, 0);
            m_TravelCam->Rotate(&vect, (-delta.y / (float)::GetSystemMetrics(SM_CYSCREEN)) * 180.0f / PI, m_TravelCam);
        }
    } else if (m_CamOn->GetBoolean()) {
        if (m_InputHook->IsKeyPressed(m_Cam45->GetKey())) {
            vect = VxVector(0, 1, 0);
            m_CamOrientRef->Rotate(&vect, PI / 4, m_CamOrientRef);
            m_CamOrient->SetQuaternion(&quat, m_CamOrientRef);
        }
        if (m_InputHook->IsKeyDown(m_CamRot[0]->GetKey())) {
            vect = VxVector(0, 1, 0);
            m_CamOrientRef->Rotate(&vect, -0.01f * m_DeltaTime, m_CamOrientRef);
            m_CamOrient->SetQuaternion(&quat, m_CamOrientRef);
        }
        if (m_InputHook->IsKeyDown(m_CamRot[1]->GetKey())) {
            vect = VxVector(0, 1, 0);
            m_CamOrientRef->Rotate(&vect, 0.01f * m_DeltaTime, m_CamOrientRef);
            m_CamOrient->SetQuaternion(&quat, m_CamOrientRef);
        }
        if (m_InputHook->IsKeyDown(m_CamY[0]->GetKey())) {
            vect = VxVector(0, 0.15f * m_DeltaTime, 0);
            m_CamPos->Translate(&vect, m_CamOrientRef);
        }
        if (m_InputHook->IsKeyDown(m_CamY[1]->GetKey())) {
            vect = VxVector(0, -0.15f * m_DeltaTime, 0);
            m_CamPos->Translate(&vect, m_CamOrientRef);
        }
        if (m_InputHook->IsKeyDown(m_CamZ[0]->GetKey())) {
            VxVector position;
            m_CamPos->GetPosition(&position, m_CamOrientRef);
            position.z = (std::min)(position.z + 0.1f * m_DeltaTime, -0.1f);
            m_CamPos->SetPosition(&position, m_CamOrientRef);
        }
        if (m_InputHook->IsKeyDown(m_CamZ[1]->GetKey())) {
            vect = VxVector(0, 0, -0.1f * m_DeltaTime);
            m_CamPos->Translate(&vect, m_CamOrientRef);
        }
        if (m_InputHook->IsKeyDown(m_CamReset->GetKey())) {
            VxQuaternion rotation;
            m_CamOrientRef->GetQuaternion(&rotation, m_CamTarget);
            if (rotation.angle > 0.9f)
                rotation = VxQuaternion();
            else {
                rotation = rotation + VxQuaternion();
                rotation *= 0.5f;
            }
            m_CamOrientRef->SetQuaternion(&rotation, m_CamTarget);
            m_CamOrient->SetQuaternion(&quat, m_CamOrientRef);
            vect = VxVector(0, 35, -22);
            m_CamPos->SetPosition(&vect, m_CamOrient);
        }
    }
}

void TravelMode::OnExitGame() {
    OnPreExitLevel();
}

void TravelMode::OnPauseLevel() {
    m_Paused = true;
}

void TravelMode::OnUnpauseLevel() {
    m_Paused = false;
}

void TravelMode::OnPreExitLevel() {
    if (m_Once) {
        m_BML->ExecuteCommand("hud on");
        m_Once = false;
    }
}

void TravelMode::EnterTravelCam() {
    CKCamera *cam = m_BML->GetTargetCameraByName("InGameCam");
    m_TravelCam->SetWorldMatrix(cam->GetWorldMatrix());
    int width, height;
    cam->GetAspectRatio(width, height);
    m_TravelCam->SetAspectRatio(width, height);
    m_TravelCam->SetFov(cam->GetFov());
    m_RenderContext->AttachViewpointToCamera(m_TravelCam);
    m_BML->ExecuteCommand("hud off");
    m_Once = true;
}

void TravelMode::ExitTravelCam() {
    CKCamera *cam = m_BML->GetTargetCameraByName("InGameCam");
    m_RenderContext->AttachViewpointToCamera(cam);
    m_BML->ExecuteCommand("hud on");
}

bool TravelMode::IsInTravelCam() {
    return m_RenderContext->GetAttachedCamera() == m_TravelCam;
}

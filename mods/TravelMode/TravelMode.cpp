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
    m_BML->RegisterCommand(new TravelCommand(this));

    m_RenderContext = m_BML->GetRenderContext();
    m_InputHook = m_BML->GetInputManager();
}

void TravelMode::OnUnload() {
    ExitTravelCam();
}

void TravelMode::OnProcess() {
    m_DeltaTime = m_BML->GetTimeManager()->GetLastDeltaTime() / 10;

    VxVector vect;
    VxQuaternion quat;

    if (!m_BML->IsPaused() && IsInTravelCam()) {
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

        if (m_InputHook->IsKeyDown(CKKEY_LCONTROL)) {
            if (m_InputHook->IsKeyDown(CKKEY_1)) {
                m_RotateSpeed = 0.2f;
            } else if (m_InputHook->IsKeyDown(CKKEY_2)) {
                m_RotateSpeed = 0.4f;
            } else if (m_InputHook->IsKeyDown(CKKEY_3)) {
                m_RotateSpeed = 0.6f;
            } else if (m_InputHook->IsKeyDown(CKKEY_4)) {
                m_RotateSpeed = 0.8f;
            } else if (m_InputHook->IsKeyDown(CKKEY_5)) {
                m_RotateSpeed = 1.0f;
            }
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
        const float factor = (180.0f / PI) * m_RotateSpeed * m_DeltaTime;
        if (delta.x != 0) {
            vect = VxVector(0, 1, 0);
            m_TravelCam->Rotate(&vect, (-delta.x / (float)::GetSystemMetrics(SM_CXSCREEN)) * factor);
        }
        if (delta.y != 0) {
            vect = VxVector(1, 0, 0);
            m_TravelCam->Rotate(&vect, (-delta.y / (float)::GetSystemMetrics(SM_CYSCREEN)) * factor, m_TravelCam);
        }
    }
}

void TravelMode::OnExitGame() {
    ExitTravelCam();
}

void TravelMode::OnPreExitLevel() {
    ExitTravelCam();
}

void TravelMode::EnterTravelCam() {
    if (IsInTravelCam())
        return;

    CKCamera *cam = m_BML->GetTargetCameraByName("InGameCam");
    if (!cam)
        return;

    m_TravelCam = (CKCamera *) m_BML->GetCKContext()->CreateObject(CKCID_CAMERA, "TravelCam");
    m_TravelCam->SetWorldMatrix(cam->GetWorldMatrix());
    int width, height;
    cam->GetAspectRatio(width, height);
    m_TravelCam->SetAspectRatio(width, height);
    m_TravelCam->SetFov(cam->GetFov());
    m_RenderContext->AttachViewpointToCamera(m_TravelCam);

    m_BML->ExecuteCommand("hud off");
}

void TravelMode::ExitTravelCam() {
    if (m_TravelCam) {
        CKCamera *cam = m_BML->GetTargetCameraByName("InGameCam");
        if (cam)
            m_RenderContext->AttachViewpointToCamera(cam);
        m_BML->GetCKContext()->DestroyObject(m_TravelCam);
        m_TravelCam = nullptr;

        m_BML->ExecuteCommand("hud on");
    }
}

bool TravelMode::IsInTravelCam() {
    return m_TravelCam && m_RenderContext->GetAttachedCamera() == m_TravelCam;
}
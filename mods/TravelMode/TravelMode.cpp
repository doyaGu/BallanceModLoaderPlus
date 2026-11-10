#include "TravelMode.h"
#include "TravelCommand.h"

BML_DEFINE_MOD(TravelMode, {
    .Id = "TravelMode",
    .Name = "Travel Mode",
    .Version = BML_VERSION,
    .Author = "Gamepiaynmo & Kakuty",
    .Description = "Travel Mode for Ballance.",
    .MinimumLoaderVersion = BML_VERSION,
})

void TravelMode::OnLoad(const BML::Mod::Context &context) {
    m_Context = context;
    m_Timers = ModSupport::Timers(context);
    ModSupport::RegisterCommand(m_Context, m_Commands, std::make_shared<TravelCommand>(this));

    m_RenderContext = ModSupport::Render(m_Context);
}

void TravelMode::OnUnload(const BML::Mod::Context &) { ExitTravelCam(); }

void TravelMode::OnProcess(const BML::Mod::Context &) {
    m_DeltaTime = ModSupport::Engine(m_Context)->GetTimeManager()->GetLastDeltaTime() / 10;

    VxVector vect;
    VxQuaternion quat;

    if (!ModSupport::GameState().Paused && IsInTravelCam()) {
        if (ModSupport::Keyboard(m_Context).IsKeyDown(CKKEY_1)) {
            m_TravelSpeed = 0.2f;
        } else if (ModSupport::Keyboard(m_Context).IsKeyDown(CKKEY_2)) {
            m_TravelSpeed = 0.4f;
        } else if (ModSupport::Keyboard(m_Context).IsKeyDown(CKKEY_3)) {
            m_TravelSpeed = 0.8f;
        } else if (ModSupport::Keyboard(m_Context).IsKeyDown(CKKEY_4)) {
            m_TravelSpeed = 1.6f;
        } else if (ModSupport::Keyboard(m_Context).IsKeyDown(CKKEY_5)) {
            m_TravelSpeed = 2.4f;
        }

        if (ModSupport::Keyboard(m_Context).IsKeyDown(CKKEY_LCONTROL)) {
            if (ModSupport::Keyboard(m_Context).IsKeyDown(CKKEY_1)) {
                m_RotateSpeed = 0.2f;
            } else if (ModSupport::Keyboard(m_Context).IsKeyDown(CKKEY_2)) {
                m_RotateSpeed = 0.4f;
            } else if (ModSupport::Keyboard(m_Context).IsKeyDown(CKKEY_3)) {
                m_RotateSpeed = 0.6f;
            } else if (ModSupport::Keyboard(m_Context).IsKeyDown(CKKEY_4)) {
                m_RotateSpeed = 0.8f;
            } else if (ModSupport::Keyboard(m_Context).IsKeyDown(CKKEY_5)) {
                m_RotateSpeed = 1.0f;
            }
        }

        if (ModSupport::Keyboard(m_Context).IsKeyDown(CKKEY_W)) {
            vect = VxVector(0, 0, m_TravelSpeed * m_DeltaTime);
            m_TravelCam->Translate(&vect, m_TravelCam);
        }
        if (ModSupport::Keyboard(m_Context).IsKeyDown(CKKEY_S)) {
            vect = VxVector(0, 0, -m_TravelSpeed * m_DeltaTime);
            m_TravelCam->Translate(&vect, m_TravelCam);
        }
        if (ModSupport::Keyboard(m_Context).IsKeyDown(CKKEY_A)) {
            vect = VxVector(-m_TravelSpeed * m_DeltaTime, 0, 0);
            m_TravelCam->Translate(&vect, m_TravelCam);
        }
        if (ModSupport::Keyboard(m_Context).IsKeyDown(CKKEY_D)) {
            vect = VxVector(m_TravelSpeed * m_DeltaTime, 0, 0);
            m_TravelCam->Translate(&vect, m_TravelCam);
        }
        if (ModSupport::Keyboard(m_Context).IsKeyDown(CKKEY_SPACE)) {
            vect = VxVector(0, m_TravelSpeed * m_DeltaTime, 0);
            m_TravelCam->Translate(&vect);
        }
        if (ModSupport::Keyboard(m_Context).IsKeyDown(CKKEY_LSHIFT)) {
            vect = VxVector(0, -m_TravelSpeed * m_DeltaTime, 0);
            m_TravelCam->Translate(&vect);
        }

        VxVector delta;
        delta = BML::Convert::ToVxVector(m_Context.Input().ReadMouse().Value().RelativePosition);
        const float factor = (180.0f / PI) * m_RotateSpeed * m_DeltaTime;
        const int width = m_RenderContext->GetWidth();
        const int height = m_RenderContext->GetHeight();
        if (delta.x != 0 && width > 0) {
            vect = VxVector(0, 1, 0);
            m_TravelCam->Rotate(&vect, (-delta.x / static_cast<float>(width)) * factor);
        }
        if (delta.y != 0 && height > 0) {
            vect = VxVector(1, 0, 0);
            m_TravelCam->Rotate(&vect, (-delta.y / static_cast<float>(height)) * factor,
                                m_TravelCam);
        }
    }
}

void TravelMode::OnExitGame(const BML::Mod::Context &) { ExitTravelCam(); }

void TravelMode::OnPreExitLevel(const BML::Mod::Context &) { ExitTravelCam(); }

void TravelMode::EnterTravelCam() {
    if (IsInTravelCam())
        return;

    CKCamera *cam = ModSupport::Find<CKTargetCamera>(m_Context, "InGameCam");
    if (!cam)
        return;

    m_TravelCam =
        (CKCamera *)ModSupport::Engine(m_Context)->CreateObject(CKCID_CAMERA, "TravelCam");
    m_TravelCam->SetWorldMatrix(cam->GetWorldMatrix());
    int width, height;
    cam->GetAspectRatio(width, height);
    m_TravelCam->SetAspectRatio(width, height);
    m_TravelCam->SetFov(cam->GetFov());
    m_RenderContext->AttachViewpointToCamera(m_TravelCam);

    ModSupport::ExecuteCommand(m_Context, "hud off");
}

void TravelMode::ExitTravelCam() {
    if (m_TravelCam) {
        CKCamera *cam = ModSupport::Find<CKTargetCamera>(m_Context, "InGameCam");
        if (cam)
            m_RenderContext->AttachViewpointToCamera(cam);
        ModSupport::Engine(m_Context)->DestroyObject(m_TravelCam);
        m_TravelCam = nullptr;

        ModSupport::ExecuteCommand(m_Context, "hud on");
    }
}

bool TravelMode::IsInTravelCam() {
    return m_TravelCam && m_RenderContext->GetAttachedCamera() == m_TravelCam;
}

#include "CameraUtilities.h"

BML_DEFINE_MOD(CameraUtilities, {
    .Id = "CameraUtilities",
    .Name = "Camera Utilities",
    .Version = BML_VERSION,
    .Author = "Gamepiaynmo & Kakuty",
    .Description = "Camera utilities for Ballance.",
    .MinimumLoaderVersion = BML_VERSION,
})

void CameraUtilities::OnLoad(const BML::Mod::Context &context) {
    m_Context = context;
    m_Timers = ModSupport::Timers(context);
    m_Context.Config().SetCategoryComment("Camera", "Camera Utilities").Take();

    m_CamOn = m_Context.Config()
                  .DeclareProperty({"Camera", "Enable", bool(false),
                                    "Enable Camera Utilities [Cheat Mode Only]"})
                  .Take();

    m_CamReset = m_Context.Config()
                     .DeclareProperty({"Camera", "Reset", uint32_t(CKKEY_D), "Reset Camera"})
                     .Take();

    m_Cam45 = m_Context.Config()
                  .DeclareProperty({"Camera", "Rotate45", uint32_t(CKKEY_W), "Set to 45 degrees"})
                  .Take();

    m_CamRot[0] =
        m_Context.Config()
            .DeclareProperty({"Camera", "RotateLeft", uint32_t(CKKEY_Q), "Rotate the camera"})
            .Take();

    m_CamRot[1] =
        m_Context.Config()
            .DeclareProperty({"Camera", "RotateRight", uint32_t(CKKEY_E), "Rotate the camera"})
            .Take();

    m_CamY[0] = m_Context.Config()
                    .DeclareProperty({"Camera", "MoveUp", uint32_t(CKKEY_A), "Move the camera"})
                    .Take();

    m_CamY[1] = m_Context.Config()
                    .DeclareProperty({"Camera", "MoveDown", uint32_t(CKKEY_Z), "Move the camera"})
                    .Take();

    m_CamZ[0] = m_Context.Config()
                    .DeclareProperty({"Camera", "MoveFront", uint32_t(CKKEY_S), "Move the camera"})
                    .Take();

    m_CamZ[1] = m_Context.Config()
                    .DeclareProperty({"Camera", "MoveBack", uint32_t(CKKEY_X), "Move the camera"})
                    .Take();
}

void CameraUtilities::OnLoadObject(const BML::Mod::Context &,
                                   const BML::Mod::LoadObjectEvent &event) {
    const char *filename = event.Filename.data();
    const bool isMap = event.IsMap;
    const char *masterName = event.MasterName.data();
    const auto filterClass = static_cast<CK_CLASSID>(event.FilterClass);
    const bool addToScene = event.AddToScene, addtoscene = event.AddToScene,
               reuseMeshes = event.ReuseMeshes, reuseMaterials = event.ReuseMaterials,
               dynamic = event.Dynamic;
    XObjectArray objects;
    for (auto ref : event.Objects) {
        auto p = BML::Engine::BorrowObject<CKObject>(m_Context, ref);
        if (p)
            objects.PushBack(p.Value()->GetID());
    }
    auto *objArray = &objects;
    auto master = BML::Engine::BorrowObject<CKObject>(m_Context, event.MasterObject);
    CKObject *masterObj = master ? master.Value() : nullptr;

    if (ModSupport::ResourceIs(filename, "3D Entities\\Camera.nmo")) {
        m_CamPos = ModSupport::Find<CK3dEntity>(m_Context, "Cam_Pos");
        m_CamOrient = ModSupport::Find<CK3dEntity>(m_Context, "Cam_Orient");
        m_CamOrientRef = ModSupport::Find<CK3dEntity>(m_Context, "Cam_OrientRef");
        m_CamTarget = ModSupport::Find<CK3dEntity>(m_Context, "Cam_Target");
    }
}

void CameraUtilities::OnProcess(const BML::Mod::Context &) {
    m_DeltaTime = ModSupport::Engine(m_Context)->GetTimeManager()->GetLastDeltaTime() / 10;

    if (!(m_CamOn.Get<bool>() && ModSupport::GameState().Playing))
        return;

    VxVector vect;
    VxQuaternion quat;

    if (ModSupport::Keyboard(m_Context).IsKeyPressed(m_Cam45.Get<uint32_t>())) {
        vect = VxVector(0, 1, 0);
        m_CamOrientRef->Rotate(&vect, PI / 4, m_CamOrientRef);
        m_CamOrient->SetQuaternion(&quat, m_CamOrientRef);
    }
    if (ModSupport::Keyboard(m_Context).IsKeyDown(m_CamRot[0].Get<uint32_t>())) {
        vect = VxVector(0, 1, 0);
        m_CamOrientRef->Rotate(&vect, -0.01f * m_DeltaTime, m_CamOrientRef);
        m_CamOrient->SetQuaternion(&quat, m_CamOrientRef);
    }
    if (ModSupport::Keyboard(m_Context).IsKeyDown(m_CamRot[1].Get<uint32_t>())) {
        vect = VxVector(0, 1, 0);
        m_CamOrientRef->Rotate(&vect, 0.01f * m_DeltaTime, m_CamOrientRef);
        m_CamOrient->SetQuaternion(&quat, m_CamOrientRef);
    }
    if (ModSupport::Keyboard(m_Context).IsKeyDown(m_CamY[0].Get<uint32_t>())) {
        vect = VxVector(0, 0.15f * m_DeltaTime, 0);
        m_CamPos->Translate(&vect, m_CamOrientRef);
    }
    if (ModSupport::Keyboard(m_Context).IsKeyDown(m_CamY[1].Get<uint32_t>())) {
        vect = VxVector(0, -0.15f * m_DeltaTime, 0);
        m_CamPos->Translate(&vect, m_CamOrientRef);
    }
    if (ModSupport::Keyboard(m_Context).IsKeyDown(m_CamZ[0].Get<uint32_t>())) {
        VxVector position;
        m_CamPos->GetPosition(&position, m_CamOrientRef);
        position.z = (std::min)(position.z + 0.1f * m_DeltaTime, -0.1f);
        m_CamPos->SetPosition(&position, m_CamOrientRef);
    }
    if (ModSupport::Keyboard(m_Context).IsKeyDown(m_CamZ[1].Get<uint32_t>())) {
        vect = VxVector(0, 0, -0.1f * m_DeltaTime);
        m_CamPos->Translate(&vect, m_CamOrientRef);
    }
    if (ModSupport::Keyboard(m_Context).IsKeyDown(m_CamReset.Get<uint32_t>())) {
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
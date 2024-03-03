#include "Physics.h"

#include <BML/Bui.h>

PhysicsMod *g_Mod = nullptr;

static bool CheckPhysicsRTVersion() {
    CKPluginManager *pm = CKGetPluginManager();

    CKPluginEntry *entry = pm->FindComponent(CKGUID(0x6BED328B, 0x141F5148));
    if (!entry)
        return false;

    if (entry->m_PluginInfo.m_Version != 0x000002)
        return false;

    return true;
}

IMod *BMLEntry(IBML *bml) {
    if (!CheckPhysicsRTVersion())
        return nullptr;

    g_Mod = new PhysicsMod(bml);
    return g_Mod;
}

void BMLExit(IMod *mod) {
    delete mod;
}

void PhysicsMod::OnLoad() {
    GetConfig()->SetCategoryComment("Misc", "Miscellaneous");

    m_Enabled = GetConfig()->GetProperty("Misc", "Enable");
    m_Enabled->SetComment("Enable Display Info");
    m_Enabled->SetDefaultBoolean(false);

    m_IpionManager = (CKIpionManager *) m_BML->GetCKContext()->GetManagerByGuid(CKGUID(0x6bed328b, 0x141f5148));
    m_InputHook = m_BML->GetInputManager();
}

void PhysicsMod::OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                              CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                              XObjectArray *objArray, CKObject *masterObj) {
    if (!strcmp(filename, "3D Entities\\Gameplay.nmo")) {
        m_CurLevel = m_BML->GetArrayByName("CurrentLevel");
    }

    if (isMap) {
        m_MapName = filename;
        m_MapName = m_MapName.substr(m_MapName.find_last_of('\\') + 1);
        m_MapName = m_MapName.substr(0, m_MapName.find_last_of('.'));
    }
}

void PhysicsMod::OnLoadScript(const char *filename, CKBehavior *script) {
    if (!strcmp(script->GetName(), "Gameplay_Ingame")) {
        for (int i = 0; i < script->GetLocalParameterCount(); ++i) {
            CKParameter *param = script->GetLocalParameter(i);
            if (!strcmp(param->GetName(), "ActiveBall")) {
                m_ActiveBall = param;
                break;
            }
        }
    }
}

void PhysicsMod::OnProcess() {
    if (m_Enabled->GetBoolean()) {
        auto *ball = GetPhysicsBall();
        m_BallData.Acquire(ball);
        if (m_BallData.valid && m_BallReset) {
            m_BallDataOrig = m_BallData;
            m_BallReset = false;
        }

        {
            Bui::ImGuiContextScope scope;
            OnDraw();
        }

        if (m_BallData.valid) {
            m_BallDataLast.ApplyDiff(ball, m_BallData);
            m_BallDataLast = m_BallData;
        } else if (m_BallDataLast.valid) {
            m_BallReset = true;
            m_BallDataLast.valid = false;
        }
    }
}

void PhysicsMod::OnStartLevel() {
    if (m_Enabled->GetBoolean())
        m_ShowWindow = true;
}

void PhysicsMod::OnPreResetLevel() {
    m_ShowWindow = false;
}

void PhysicsMod::OnPreExitLevel() {
    m_ShowWindow = false;
}

void PhysicsMod::OnDraw() {
    constexpr ImGuiWindowFlags WinFlags = ImGuiWindowFlags_NoNav |
                                          ImGuiWindowFlags_NoFocusOnAppearing |
                                          ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (ImGui::IsKeyChordPressed(ImGuiMod_Shift | ImGuiMod_Alt | ImGuiKey_F6))
        m_ShowWindow = !m_ShowWindow;

    if (!m_ShowWindow)
        return;

    if (ImGui::Begin("Physics Info", &m_ShowWindow, WinFlags)) {
        ImGui::Text("Active Ball: %s", m_BallData.name);

        ImGui::Checkbox("CollisionEnabled", &m_BallData.collisionEnabled);
        ImGui::Checkbox("GravityEnabled", &m_BallData.gravityEnabled);
        ImGui::Checkbox("MotionEnabled", &m_BallData.motionEnabled);

        ImGui::Text("Mass:");
        ImGui::InputFloat("##Mass", &m_BallData.mass);
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            m_BallData.mass = m_BallDataOrig.mass;
        }

        ImGui::Text("Inertia:");
        ImGui::InputFloat3("##Inertia", &m_BallData.inertia[0]);
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            m_BallData.inertia = m_BallDataOrig.inertia;
        }

        ImGui::Text("Speed Damping:");
        ImGui::InputFloat("##SpeedDamping", &m_BallData.speedDamping);
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            m_BallData.speedDamping = m_BallDataOrig.speedDamping;
        }

        ImGui::Text("Rot Damping:");
        ImGui::InputFloat("##RotDamping", &m_BallData.rotDamping);
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            m_BallData.rotDamping = m_BallDataOrig.rotDamping;
        }

        ImGui::Text("Position:");
        ImGui::Text("(%.3f, %.3f, %.3f)", m_BallData.position.x, m_BallData.position.y, m_BallData.position.z);
        ImGui::Text("Pitch: %.3f", m_BallData.angles.x);
        ImGui::Text("Yaw: %.3f", m_BallData.angles.y);
        ImGui::Text("Roll: %.3f", m_BallData.angles.z);

        ImGui::Text("Velocity:");
        ImGui::Text("(%.3f, %.3f, %.3f)", m_BallData.velocity.x, m_BallData.velocity.y, m_BallData.velocity.z);
        ImGui::Text("Angular Velocity:");
        ImGui::Text("(%.3f, %.3f, %.3f)", m_BallData.angularVelocity.x, m_BallData.angularVelocity.y, m_BallData.angularVelocity.z);
    }
    ImGui::End();
}
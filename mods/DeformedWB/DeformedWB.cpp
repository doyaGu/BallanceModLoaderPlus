#include "DeformedWB.h"

#include <random>

std::default_random_engine g_RandomEngine;

BML_DEFINE_MOD(DeformedWB, {
    .Id = "DeformedWB",
    .Name = "Deformed Wooden Ball",
    .Version = BML_VERSION,
    .Author = "Gamepiaynmo",
    .Description = "Randomly deform the player wooden ball. This does not affect the physical volume.",
    .MinimumLoaderVersion = BML_VERSION,
})

void DeformedWB::OnLoad(const BML::Mod::Context &context) {
    m_Context = context;
    m_Timers = ModSupport::Timers(context);
    m_Enabled =
        m_Context.Config()
            .DeclareProperty({"Misc", "Enable", bool(false), "Enable deforming player wooden ball"})
            .Take();

    m_Extent =
        m_Context.Config()
            .DeclareProperty({"Misc", "Extent", float(0.25),
                              "A float ranged from 0 to 1 representing the extent of deformation"})
            .Take();
}

void DeformedWB::OnLoadObject(const BML::Mod::Context &, const BML::Mod::LoadObjectEvent &event) {
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

    if (ModSupport::ResourceIs(filename, "3D Entities\\Balls.nmo")) {
        m_BallMesh = ModSupport::Find<CKMesh>(m_Context, "Ball_Wood_Mesh");
        m_Vertices.resize(m_BallMesh->GetVertexCount());
        m_Normals.resize(m_BallMesh->GetVertexCount());
        for (int i = 0; i < m_BallMesh->GetVertexCount(); i++) {
            m_BallMesh->GetVertexPosition(i, &m_Vertices[i]);
            m_BallMesh->GetVertexNormal(i, &m_Normals[i]);
        }
    }
}

void DeformedWB::OnStartLevel(const BML::Mod::Context &) {
    if (m_Enabled.Get<bool>()) {
        std::uniform_real_distribution<float> rnd(-1, 1);
        VxMatrix proj, invp, scale, invs;
        proj.SetIdentity();
        scale.SetIdentity();
        invs.SetIdentity();
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++)
                proj[i][j] = rnd(g_RandomEngine);
            for (int j = 0; j < i; j++)
                proj[i] -= DotProduct(proj[i], proj[j]) * proj[j];
            proj[i] = Normalize(proj[i]);
            scale[i][i] = rnd(g_RandomEngine) * m_Extent.Get<float>() + 1;
        }

        float slen = scale[0][0] + scale[1][1] + scale[2][2];
        for (int i = 0; i < 3; i++) {
            scale[i][i] *= 3 / slen;
            invs[i][i] = 1 / scale[i][i];
        }

        Vx3DTransposeMatrix(invp, proj);
        VxMatrix def = proj * scale * invp, invd = proj * invs * invp;
        for (int i = 0; i < m_BallMesh->GetVertexCount(); i++) {
            VxVector vertex = def * m_Vertices[i];
            VxVector normal = invd * m_Normals[i];
            m_BallMesh->SetVertexPosition(i, &vertex);
            m_BallMesh->SetVertexNormal(i, &normal);
        }
    } else {
        VxVector tmp;
        m_BallMesh->GetVertexPosition(0, &tmp);
        if (tmp != m_Vertices[0]) {
            for (int i = 0; i < m_BallMesh->GetVertexCount(); i++) {
                m_BallMesh->SetVertexPosition(i, &m_Vertices[i]);
                m_BallMesh->SetVertexNormal(i, &m_Normals[i]);
            }
        }
    }
};
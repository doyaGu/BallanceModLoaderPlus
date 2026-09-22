#include "DeformedWB.h"

#include <random>

std::default_random_engine g_RandomEngine;

IMod *BMLEntry(IBML *bml) {
    return new DeformedWB(bml);
}

void BMLExit(IMod *mod) {
    delete mod;
}

void DeformedWB::OnLoad() {
    m_Enabled = GetConfig()->GetProperty("Misc", "Enable");
    m_Enabled->SetComment("Enable deforming player wooden ball");
    m_Enabled->SetDefaultBoolean(false);

    m_Extent = GetConfig()->GetProperty("Misc", "Extent");
    m_Extent->SetComment("A float ranged from 0 to 1 representing the extent of deformation");
    m_Extent->SetDefaultFloat(0.25);
}

void DeformedWB::OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                              CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                              XObjectArray *objArray, CKObject *masterObj) {
    if (!strcmp(filename, "3D Entities\\Balls.nmo")) {
        m_BallMesh = m_BML->GetMeshByName("Ball_Wood_Mesh");
        m_Vertices.resize(m_BallMesh->GetVertexCount());
        m_Normals.resize(m_BallMesh->GetVertexCount());
        for (int i = 0; i < m_BallMesh->GetVertexCount(); i++) {
            m_BallMesh->GetVertexPosition(i, &m_Vertices[i]);
            m_BallMesh->GetVertexNormal(i, &m_Normals[i]);
        }
    }
}

void DeformedWB::OnStartLevel() {
    if (m_Enabled->GetBoolean()) {
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
            scale[i][i] = rnd(g_RandomEngine) * m_Extent->GetFloat() + 1;
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
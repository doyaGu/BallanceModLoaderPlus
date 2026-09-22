#include "SpiritTrail.h"

#include <stdio.h>
#include <sys/stat.h>

IMod *BMLEntry(IBML *bml) {
    return new SpiritTrail(bml);
}

void BMLExit(IMod *mod) {
    delete mod;
}

std::pair<char *, int> ReadDataFromFile(const char *filename) {
    if (!filename) return {nullptr, 0};
    FILE *fp = fopen(filename, "rb");
    if (!fp) return {nullptr, 0};

    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buffer = new char[size];
    fread(buffer, size, 1, fp);
    fclose(fp);

    return {buffer, size};
}

std::string GetFileHash(const char *filename) {
    if (!filename) return "";
    auto data = ReadDataFromFile(filename);
    if (!data.first) return "";

    std::string res;
    int segm[] = {0, data.second / 4, data.second / 2, data.second * 3 / 4, data.second};
    for (int i = 0; i < 4; i++) {
        char hash[10];
        CKDWORD crc = CKComputeDataCRC(data.first + segm[i], segm[i + 1] - segm[i]);
        sprintf(hash, "%08lx", crc);
        res += hash;
    }

    delete[] data.first;
    return res;
}

void WriteDataToFile(char *data, int size, const char *filename) {
    if (!data || size == 0 || !filename)
        return;

    FILE *fp = fopen(filename, "wb");
    fwrite(data, size, 1, fp);
    fclose(fp);
}

std::pair<char *, int> UncompressDataFromFile(const char *filename) {
    if (!filename) return {nullptr, 0};

    FILE *fp = fopen(filename, "rb");
    if (!fp) return {nullptr, 0};

    fseek(fp, 0, SEEK_END);
    int nsize = ftell(fp) - 4;
    fseek(fp, 0, SEEK_SET);

    int size;
    char *buffer = new char[nsize];
    fread(&size, 4, 1, fp);
    fread(buffer, nsize, 1, fp);
    fclose(fp);

    char *res = CKUnPackData(size, buffer, nsize);
    delete[] buffer;
    return {res, size};
}

void CompressDataToFile(char *data, int size, const char *filename) {
    if (!data || size == 0 || !filename)
        return;

    int nsize;
    char *res = CKPackData(data, size, nsize, 9);

    FILE *fp = fopen(filename, "wb");
    if (!fp) return;

    fwrite(&size, 4, 1, fp);
    fwrite(res, nsize, 1, fp);
    fclose(fp);

    CKDeletePointer(res);
}

void SpiritTrail::OnLoad() {
    VxMakeDirectory("..\\ModLoader\\Trails\\");

    GetConfig()->SetCategoryComment("Misc", "Miscellaneous");
    m_Enabled = GetConfig()->GetProperty("Misc", "Enable");
    m_Enabled->SetComment("Enable Spirit Trail");
    m_Enabled->SetDefaultBoolean(false);

    m_HSSR = GetConfig()->GetProperty("Misc", "HS_SR");
    m_HSSR->SetComment("Play HS(No) or SR(Yes) record");
    m_HSSR->SetDefaultBoolean(false);

    m_DeathReset = GetConfig()->GetProperty("Misc", "DeathReset");
    m_DeathReset->SetComment("Reset record on Death");
    m_DeathReset->SetDefaultBoolean(true);
}

void SpiritTrail::OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                               CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                               XObjectArray *objArray, CKObject *masterObj) {
    if (isMap) {
        m_CurMap = filename;
        XString filepath = filename;
        m_BML->GetPathManager()->ResolveFileName(filepath, DATA_PATH_IDX);
        m_RecordDir = "..\\ModLoader\\Trails\\" + GetFileHash(filepath.CStr()) + "\\";
        VxMakeDirectory((CKSTRING) m_RecordDir.c_str());
    }

    if (!strcmp(filename, "3D Entities\\Balls.nmo")) {
        CKDataArray *physBall = m_BML->GetArrayByName("Physicalize_GameBall");
        for (int i = 0; i < physBall->GetRowCount(); i++) {
            SpiritBall ball;
            ball.name.resize(physBall->GetElementStringValue(i, 0, nullptr), '\0');
            physBall->GetElementStringValue(i, 0, &ball.name[0]);
            ball.name.pop_back();
            ball.obj = m_BML->Get3dObjectByName(ball.name.c_str());

            CKDependencies dep;
            dep.Resize(40);
            dep.Fill(0);
            dep.m_Flags = CK_DEPENDENCIES_CUSTOM;
            dep[CKCID_OBJECT] = CK_DEPENDENCIES_COPY_OBJECT_NAME | CK_DEPENDENCIES_COPY_OBJECT_UNIQUENAME;
            dep[CKCID_MESH] = CK_DEPENDENCIES_COPY_MESH_MATERIAL;
            dep[CKCID_3DENTITY] = CK_DEPENDENCIES_COPY_3DENTITY_MESH;
            ball.obj = (CK3dObject *) m_BML->GetCKContext()->CopyObject(ball.obj, &dep, "_Spirit");
            for (int j = 0; j < ball.obj->GetMeshCount(); j++) {
                CKMesh *mesh = ball.obj->GetMesh(j);
                for (int k = 0; k < mesh->GetMaterialCount(); k++) {
                    CKMaterial *mat = mesh->GetMaterial(k);
                    mat->EnableAlphaBlend();
                    mat->SetSourceBlend(VXBLEND_SRCALPHA);
                    mat->SetDestBlend(VXBLEND_INVSRCALPHA);
                    VxColor color = mat->GetDiffuse();
                    color.a = 0.5f;
                    mat->SetDiffuse(color);
                    ball.materials.push_back(mat);
                    m_BML->SetIC(mat);
                }
            }
            m_DualBalls.push_back(ball);
        }
        GetLogger()->Info("Created Spirit Balls");
    }

    if (!strcmp(filename, "3D Entities\\Gameplay.nmo")) {
        m_Energy = m_BML->GetArrayByName("Energy");
        m_CurLevel = m_BML->GetArrayByName("CurrentLevel");
        m_IngameParam = m_BML->GetArrayByName("IngameParameter");
    }
}

void SpiritTrail::OnProcess() {
    if (m_SRActivated)
        m_SRTimer += m_BML->GetTimeManager()->GetLastDeltaTime();

    const float delta = 1000.0f / TICK_SPEED;
    if (m_IsRecording && !m_RecordPaused) {
        m_RecordTimer += m_BML->GetTimeManager()->GetLastDeltaTime();
        m_RecordTimer = (std::min)(m_RecordTimer, 1000.0f);

        while (m_RecordTimer > 0) {
            m_RecordTimer -= delta;

            int curBall = GetCurrentBall();
            if (curBall != m_CurBall)
                m_Record.trafo.emplace_back(m_Record.states.size(), curBall);
            m_CurBall = curBall;

            auto *ball = (CK3dObject *) m_CurLevel->GetElementObject(0, 1);
            if (ball) {
                Record::State state;
                ball->GetPosition(&state.pos);
                ball->GetQuaternion(&state.rot);
                m_Record.states.push_back(state);
            }
        }

        if (m_BML->IsCheatEnabled()) {
            StopRecording();
        }

        if (GetSRScore() > 1000 * 3600) {
            GetLogger()->Info("Record is longer than 1 hour, stop recording");
            StopRecording();
        }
    }

    if (m_IsPlaying && !m_PlayPaused) {
        m_PlayTimer += m_BML->GetTimeManager()->GetLastDeltaTime();
        m_PlayTimer = std::min(m_PlayTimer, 1000.0f);

        while (m_PlayTimer > 0) {
            m_PlayTimer -= delta;
            m_PlayFrame++;

            auto &trafos = m_Play[m_PlayHSSR].trafo;
            if (m_PlayTrafo < trafos.size()) {
                auto &trafo = trafos[m_PlayTrafo];
                if (m_PlayFrame == trafo.first) {
                    SetCurrentBall(trafo.second);
                    m_PlayTrafo++;
                }
            }
        }

        auto &states = m_Play[m_PlayHSSR].states;
        if (m_PlayFrame >= 0 && m_PlayFrame < states.size() - 1) {
            if (m_PlayBall >= 0 && m_PlayBall < m_DualBalls.size()) {
                CKObject *playerBall = m_CurLevel->GetElementObject(0, 1);
                CK3dObject *ball = m_DualBalls[m_PlayBall].obj;
                ball->Show(playerBall->IsVisible() ? CKSHOW : CKHIDE);

                float portion = (m_PlayTimer / delta + 1);
                Record::State &cur = states[m_PlayFrame], &next = states[m_PlayFrame + 1];
                VxVector position = (next.pos - cur.pos) * portion + cur.pos;
                VxQuaternion rotation = Slerp(portion, cur.rot, next.rot);
                ball->SetPosition(&position);
                ball->SetQuaternion(&rotation);
            }
        } else {
            StopPlaying();
        }
    }
}

int SpiritTrail::GetHSScore() {
    int points, lifes;
    m_Energy->GetElementValue(0, 0, &points);
    m_Energy->GetElementValue(0, 1, &lifes);
    return points + lifes * 200;
}

int SpiritTrail::GetCurrentBall() {
    CKObject *ball = m_CurLevel->GetElementObject(0, 1);
    if (ball) {
        std::string ballName = ball->GetName();
        for (size_t i = 0; i < m_DualBalls.size(); i++) {
            if (m_DualBalls[i].name == ballName)
                return i;
        }
    }

    return 0;
}

int SpiritTrail::GetCurrentSector() {
    int res;
    m_IngameParam->GetElementValue(0, 1, &res);
    return res;
}

void SpiritTrail::SetCurrentBall(int curBall) {
    m_PlayBall = curBall;
    for (auto &ball: m_DualBalls)
        ball.obj->Show(CKHIDE);
    if (m_PlayBall >= 0 && m_PlayBall < m_DualBalls.size())
        m_DualBalls[m_PlayBall].obj->Show();
}

void SpiritTrail::PreparePlaying() {
    if (!m_IsPlaying && m_Enabled->GetBoolean() && !m_WaitPlaying) {
        m_WaitPlaying = true;

        m_LoadPlay = std::thread([this]() {
            std::string recfile[2] = {(m_RecordDir + "hs" + std::to_string(m_CurSector) + ".rec"),
                                      (m_RecordDir + "sr" + std::to_string(m_CurSector) + ".rec")};
            m_PlayHSSR = m_HSSR->GetBoolean();

            struct stat buf = {0};
            for (int i = 0; i < 2; i++) {
                if (stat(recfile[i].c_str(), &buf) == 0) {
                    std::pair<char *, int> data = UncompressDataFromFile(recfile[i].c_str());
                    m_Play[i].hsscore = *reinterpret_cast<int *>(data.first);
                    m_Play[i].srscore = *reinterpret_cast<float *>(data.first + 4);
                    if (bool(i) == m_PlayHSSR) {
                        int ssize = *reinterpret_cast<int *>(data.first + 8);
                        int tsize = *reinterpret_cast<int *>(data.first + 12);
                        m_Play[i].states.resize(ssize);
                        memcpy(&m_Play[i].states[0], data.first + 16, ssize * sizeof(Record::State));
                        m_Play[i].trafo.resize(tsize);
                        memcpy(&m_Play[i].trafo[0], data.first + 16 + ssize * sizeof(Record::State),
                               tsize * sizeof(std::pair<int, int>));
                    }

                    CKDeletePointer(data.first);
                } else {
                    m_Play[i].hsscore = INT_MIN;
                    m_Play[i].srscore = FLT_MAX;
                }
            }
        });
    }
}

void SpiritTrail::StartPlaying() {
    if (!m_IsPlaying && m_Enabled->GetBoolean() && m_WaitPlaying) {
        m_WaitPlaying = false;

        if (m_LoadPlay.joinable())
            m_LoadPlay.join();

        if (m_Play[m_PlayHSSR].hsscore > INT_MIN) {
            m_IsPlaying = true;
            m_PlayPaused = false;
            m_PlayTimer = -1000.0f / TICK_SPEED;
            m_PlayFrame = 0;
            m_PlayTrafo = 1;
            SetCurrentBall(m_Play[m_PlayHSSR].trafo[0].second);
        }
    }
}

void SpiritTrail::PausePlaying() {
    m_PlayPaused = true;
}

void SpiritTrail::UnpausePlaying() {
    m_PlayPaused = false;
}

void SpiritTrail::StopPlaying() {
    if (m_IsPlaying) {
        m_IsPlaying = false;
        for (int i = 0; i < 2; i++) {
            m_Play[i].states.clear();
            m_Play[i].states.shrink_to_fit();
            m_Play[i].trafo.clear();
            m_Play[i].trafo.shrink_to_fit();
        }

        for (auto &ball: m_DualBalls)
            ball.obj->Show(CKHIDE);
    }
}

void SpiritTrail::PrepareRecording() {
    if (!m_IsRecording && !m_BML->IsCheatEnabled() && m_Enabled->GetBoolean() && !m_WaitRecording) {
        m_WaitRecording = true;
    }
}

void SpiritTrail::StartRecording() {
    if (!m_IsRecording && !m_BML->IsCheatEnabled() && m_Enabled->GetBoolean() && m_WaitRecording) {
        m_WaitRecording = false;
        m_IsRecording = true;
        m_RecordPaused = false;

        m_StartHS = GetHSScore();
        m_RecordTimer = 0;
        m_SRTimer = 0;
        m_CurBall = GetCurrentBall();
        m_Record.trafo.emplace_back(0, m_CurBall);
    }
}

void SpiritTrail::PauseRecording() {
    m_RecordPaused = true;
}

void SpiritTrail::UnpauseRecording() {
    m_RecordPaused = false;
}

void SpiritTrail::StopRecording() {
    if (m_IsRecording) {
        m_IsRecording = false;
        m_Record.trafo.clear();
        m_Record.trafo.shrink_to_fit();
        m_Record.states.clear();
        m_Record.states.shrink_to_fit();
    }
}

static bool CopyFile(const char *src, const char *dst) {
    FILE *fin = fopen(src, "rb");
    if (!fin) return false;
    FILE *fout = fopen(dst, "wb");
    if (!fout) return false;

    int ch;
    while ((ch = fgetc(fin)) != EOF)
        fputc(ch, fout);

    fclose(fin);
    fclose(fout);
    return true;
}

void SpiritTrail::EndRecording() {
    if (m_IsRecording) {
        m_Record.hsscore = GetHSScore() - m_StartHS;
        m_Record.srscore = GetSRScore();

        bool savehs = m_Record.hsscore > m_Play[0].hsscore,
            savesr = m_Record.srscore < m_Play[1].srscore;
        if (savehs || savesr) {
            int ssize = sizeof(Record::State) * m_Record.states.size();
            int tsize = sizeof(std::pair<int, int>) * m_Record.trafo.size();
            int size = 16 + ssize + tsize;

            char *buffer = new char[size];
            *reinterpret_cast<int *>(buffer) = m_Record.hsscore;
            *reinterpret_cast<float *>(buffer + 4) = m_Record.srscore;
            *reinterpret_cast<int *>(buffer + 8) = m_Record.states.size();
            *reinterpret_cast<int *>(buffer + 12) = m_Record.trafo.size();
            memcpy(buffer + 16, &m_Record.states[0], ssize);
            memcpy(buffer + 16 + ssize, &m_Record.trafo[0], tsize);

            std::string hspath = m_RecordDir + "hs" + std::to_string(m_CurSector) + ".rec",
                srpath = m_RecordDir + "sr" + std::to_string(m_CurSector) + ".rec";

            std::thread([savehs, savesr, hspath, srpath, buffer, size]() {
                if (savehs) {
                    CompressDataToFile(buffer, size, hspath.c_str());
                    if (savesr) {
                        CopyFile(hspath.c_str(), srpath.c_str());
                    }
                } else {
                    CompressDataToFile(buffer, size, srpath.c_str());
                }
                delete[] buffer;
            }).detach();

            if (savehs)
                GetLogger()->Info("HS of sector %d has updated", m_CurSector);
            if (savesr)
                GetLogger()->Info("SR of sector %d has updated", m_CurSector);
        }

        StopRecording();
    }
}
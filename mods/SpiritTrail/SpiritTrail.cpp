#include "SpiritTrail.h"

#include <stdio.h>
#include <bit>
#include <cstring>
#include <limits>

BML_DEFINE_MOD(SpiritTrail, {
    .Id = "SpiritTrail",
    .Name = "Spirit Trail",
    .Version = BML_VERSION,
    .Author = "Gamepiaynmo",
    .Description = "Play the historical best record as a translucent ball.",
    .MinimumLoaderVersion = BML_VERSION,
})

std::pair<char *, int> ReadDataFromFile(const char *filename) {
    uint8_t *bytes = nullptr;
    size_t size = 0;
    if (!filename || !BML_ReadBinaryFileUtf8(filename, &bytes, &size))
        return {nullptr, 0};
    std::unique_ptr<uint8_t, decltype(&BML_Free)> owned(bytes, BML_Free);
    if (size > INT_MAX)
        return {nullptr, 0};
    auto copy = std::make_unique<char[]>(size);
    if (size)
        std::memcpy(copy.get(), bytes, size);
    return {copy.release(), static_cast<int>(size)};
}
namespace {
uint32_t ReadWord(const char *p) {
    const auto *b = reinterpret_cast<const unsigned char *>(p);
    return uint32_t(b[0]) | (uint32_t(b[1]) << 8) | (uint32_t(b[2]) << 16) | (uint32_t(b[3]) << 24);
}
void WriteWord(char *p, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        p[i] = static_cast<char>(value >> (i * 8));
}
float ReadFloat(const char *p) { return std::bit_cast<float>(ReadWord(p)); }
void WriteFloat(char *p, float value) { WriteWord(p, std::bit_cast<uint32_t>(value)); }
} // namespace

std::string GetFileHash(const char *filename) {
    if (!filename)
        return "";
    auto data = ReadDataFromFile(filename);
    if (!data.first)
        return "";

    std::string res;
    int segm[] = {0, data.second / 4, data.second / 2, data.second * 3 / 4, data.second};
    for (int i = 0; i < 4; i++) {
        char hash[10];
        CKDWORD crc = CKComputeDataCRC(data.first + segm[i], segm[i + 1] - segm[i]);
        snprintf(hash, sizeof(hash), "%08x", static_cast<unsigned>(crc));
        res += hash;
    }

    delete[] data.first;
    return res;
}

std::pair<char *, int> UncompressDataFromFile(const char *filename) {
    auto [bytes, length] = ReadDataFromFile(filename);
    std::unique_ptr<char[]> owned(bytes);
    if (!bytes || length <= 4)
        return {nullptr, 0};
    const uint32_t size = ReadWord(bytes);
    if (size < 16 || size > INT_MAX)
        return {nullptr, 0};
    char *data = CKUnPackData(static_cast<int>(size), bytes + 4, length - 4);
    return {data, data ? static_cast<int>(size) : 0};
}
bool CompressDataToFile(const char *data, int size, const char *filename) {
    if (!data || size <= 0 || !filename)
        return false;
    int packedSize = 0;
    char *packed = CKPackData(const_cast<char *>(data), size, packedSize, 9);
    if (!packed || packedSize <= 0)
        return false;
    std::unique_ptr<char, void (*)(void *)> owned(packed, CKDeletePointer);
    std::vector<uint8_t> bytes(static_cast<size_t>(packedSize) + 4);
    WriteWord(reinterpret_cast<char *>(bytes.data()), static_cast<uint32_t>(size));
    std::memcpy(bytes.data() + 4, packed, packedSize);
    return BML_WriteBinaryFileUtf8(filename, bytes.data(), bytes.size()) != 0;
}
void SpiritTrail::OnUnload(const BML::Mod::Context &) {
    StopPlaying();
    StopRecording();
    if (m_SaveRecord.joinable())
        m_SaveRecord.join();
}

void SpiritTrail::OnLoad(const BML::Mod::Context &context) {
    m_Context = context;
    m_Timers = ModSupport::Timers(context);
    m_TrailsRoot = m_Context.GetDirectory(BML_DIR_LOADER).Value() + "/Trails/";
    if (!BML_CreateFileTreeUtf8(m_TrailsRoot.c_str()))
        throw std::runtime_error("Cannot create Spirit Trail directory");

    m_Context.Config().SetCategoryComment("Misc", "Miscellaneous").Take();
    m_Enabled = m_Context.Config()
                    .DeclareProperty({"Misc", "Enable", bool(false), "Enable Spirit Trail"})
                    .Take();

    m_HSSR = m_Context.Config()
                 .DeclareProperty({"Misc", "HS_SR", bool(false), "Play HS(No) or SR(Yes) record"})
                 .Take();

    m_DeathReset = m_Context.Config()
                       .DeclareProperty({"Misc", "DeathReset", bool(true), "Reset record on Death"})
                       .Take();
}

void SpiritTrail::OnLoadObject(const BML::Mod::Context &, const BML::Mod::LoadObjectEvent &event) {
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

    if (isMap) {
        m_CurMap = filename;
        XString filepath = filename;
        ModSupport::Engine(m_Context)->GetPathManager()->ResolveFileName(filepath, DATA_PATH_IDX);
        StopPlaying();
        m_RecordDir = m_TrailsRoot + GetFileHash(filepath.CStr()) + "/";
        if (!BML_CreateFileTreeUtf8(m_RecordDir.c_str()))
            throw std::runtime_error("Cannot create map record directory");
    }

    if (ModSupport::ResourceIs(filename, "3D Entities\\Balls.nmo")) {
        CKDataArray *physBall = ModSupport::Find<CKDataArray>(m_Context, "Physicalize_GameBall");
        for (int i = 0; i < physBall->GetRowCount(); i++) {
            const char *name = ModSupport::StringCell(*physBall, i, 0);
            if (!name || !*name)
                continue;
            SpiritBall ball(m_Context);
            ball.name = name;
            ball.obj = ModSupport::Find<CK3dObject>(m_Context, name);

            CKDependencies dep;
            dep.Resize(40);
            dep.Fill(0);
            dep.m_Flags = CK_DEPENDENCIES_CUSTOM;
            dep[CKCID_OBJECT] =
                CK_DEPENDENCIES_COPY_OBJECT_NAME | CK_DEPENDENCIES_COPY_OBJECT_UNIQUENAME;
            dep[CKCID_MESH] = CK_DEPENDENCIES_COPY_MESH_MATERIAL;
            dep[CKCID_3DENTITY] = CK_DEPENDENCIES_COPY_3DENTITY_MESH;
            ball.obj =
                (CK3dObject *)ModSupport::Engine(m_Context)->CopyObject(ball.obj, &dep, "_Spirit");
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
                    ball.materials.emplace_back(m_Context, mat);
                    ModSupport::SetIC(m_Context, mat);
                }
            }
            m_DualBalls.push_back(ball);
        }
        ModSupport::Log(m_Context, BML_LOG_INFO, "Created Spirit Balls");
    }

    if (ModSupport::ResourceIs(filename, "3D Entities\\Gameplay.nmo")) {
        m_Energy = ModSupport::Find<CKDataArray>(m_Context, "Energy");
        m_CurLevel = ModSupport::Find<CKDataArray>(m_Context, "CurrentLevel");
        m_IngameParam = ModSupport::Find<CKDataArray>(m_Context, "IngameParameter");
    }
}

void SpiritTrail::OnProcess(const BML::Mod::Context &) {
    if (m_SRActivated)
        m_SRTimer += ModSupport::Engine(m_Context)->GetTimeManager()->GetLastDeltaTime();

    const float delta = 1000.0f / TICK_SPEED;
    if (m_IsRecording && !m_RecordPaused) {
        m_RecordTimer += ModSupport::Engine(m_Context)->GetTimeManager()->GetLastDeltaTime();
        m_RecordTimer = (std::min)(m_RecordTimer, 1000.0f);

        while (m_RecordTimer > 0) {
            m_RecordTimer -= delta;

            int curBall = GetCurrentBall();
            if (curBall != m_CurBall)
                m_Record.trafo.emplace_back(m_Record.states.size(), curBall);
            m_CurBall = curBall;

            auto *ball = (CK3dObject *)m_CurLevel->GetElementObject(0, 1);
            if (ball) {
                Record::State state;
                ball->GetPosition(&state.pos);
                ball->GetQuaternion(&state.rot);
                m_Record.states.push_back(state);
            }
        }

        if (ModSupport::CheatEnabled()) {
            StopRecording();
        }

        if (GetSRScore() > 1000 * 3600) {
            ModSupport::Log(m_Context, BML_LOG_INFO,
                            "Record is longer than 1 hour, stop recording");
            StopRecording();
        }
    }

    if (m_IsPlaying && !m_PlayPaused) {
        m_PlayTimer += ModSupport::Engine(m_Context)->GetTimeManager()->GetLastDeltaTime();
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
    for (auto &ball : m_DualBalls)
        ball.obj->Show(CKHIDE);
    if (m_PlayBall >= 0 && m_PlayBall < m_DualBalls.size())
        m_DualBalls[m_PlayBall].obj->Show();
}

void SpiritTrail::PreparePlaying() {
    if (!m_IsPlaying && m_Enabled.Get<bool>() && !m_WaitPlaying) {
        m_WaitPlaying = true;

        m_PlayHSSR = m_HSSR.Get<bool>();
        const std::string paths[2] = {m_RecordDir + "hs" + std::to_string(m_CurSector) + ".rec",
                                      m_RecordDir + "sr" + std::to_string(m_CurSector) + ".rec"};
        const auto ballCount = m_DualBalls.size();
        const bool selected = m_PlayHSSR;
        m_LoadPlay = std::jthread([this, paths, ballCount, selected] {
            for (int i = 0; i < 2; ++i) {
                m_Play[i] = {};
                try {
                    auto [data, length] = UncompressDataFromFile(paths[i].c_str());
                    std::unique_ptr<char, void (*)(void *)> owned(data, CKDeletePointer);
                    if (!data || length < 16)
                        continue;
                    const uint32_t states = ReadWord(data + 8), transforms = ReadWord(data + 12);
                    if (uint64_t(states) * 28 + uint64_t(transforms) * 8 + 16 != uint64_t(length) ||
                        !states || !transforms || !std::isfinite(ReadFloat(data + 4)))
                        continue;
                    Record record;
                    record.hsscore = std::bit_cast<int32_t>(ReadWord(data));
                    record.srscore = ReadFloat(data + 4);
                    if (bool(i) == selected) {
                        record.states.resize(states);
                        const char *p = data + 16;
                        for (auto &state : record.states) {
                            state.pos = VxVector(ReadFloat(p), ReadFloat(p + 4), ReadFloat(p + 8));
                            state.rot = VxQuaternion(ReadFloat(p + 12), ReadFloat(p + 16),
                                                     ReadFloat(p + 20), ReadFloat(p + 24));
                            p += 28;
                        }
                        record.trafo.reserve(transforms);
                        for (uint32_t j = 0; j < transforms; ++j, p += 8) {
                            const uint32_t frame = ReadWord(p), ball = ReadWord(p + 4);
                            if (frame >= states || ball >= ballCount)
                                throw std::runtime_error("Invalid record transform");
                            record.trafo.emplace_back(static_cast<int>(frame),
                                                      static_cast<int>(ball));
                        }
                    }
                    m_Play[i] = std::move(record);
                } catch (...) {
                    m_Play[i] = {};
                }
            }
        });
    }
}

void SpiritTrail::StartPlaying() {
    if (!m_IsPlaying && m_Enabled.Get<bool>() && m_WaitPlaying) {
        m_WaitPlaying = false;

        if (m_LoadPlay.joinable())
            m_LoadPlay.join();

        if (m_Play[m_PlayHSSR].hsscore > INT_MIN && !m_Play[m_PlayHSSR].states.empty() &&
            !m_Play[m_PlayHSSR].trafo.empty()) {
            m_IsPlaying = true;
            m_PlayPaused = false;
            m_PlayTimer = -1000.0f / TICK_SPEED;
            m_PlayFrame = 0;
            m_PlayTrafo = 1;
            SetCurrentBall(m_Play[m_PlayHSSR].trafo[0].second);
        }
    }
}

void SpiritTrail::PausePlaying() { m_PlayPaused = true; }

void SpiritTrail::UnpausePlaying() { m_PlayPaused = false; }

void SpiritTrail::StopPlaying() {
    if (m_LoadPlay.joinable())
        m_LoadPlay.join();
    m_WaitPlaying = false;
    if (m_IsPlaying) {
        m_IsPlaying = false;
        for (int i = 0; i < 2; i++) {
            m_Play[i].states.clear();
            m_Play[i].states.shrink_to_fit();
            m_Play[i].trafo.clear();
            m_Play[i].trafo.shrink_to_fit();
        }

        for (auto &ball : m_DualBalls)
            ball.obj->Show(CKHIDE);
    }
}

void SpiritTrail::PrepareRecording() {
    if (!m_IsRecording && !ModSupport::CheatEnabled() && m_Enabled.Get<bool>() &&
        !m_WaitRecording) {
        m_WaitRecording = true;
    }
}

void SpiritTrail::StartRecording() {
    if (!m_IsRecording && !ModSupport::CheatEnabled() && m_Enabled.Get<bool>() && m_WaitRecording) {
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

void SpiritTrail::PauseRecording() { m_RecordPaused = true; }

void SpiritTrail::UnpauseRecording() { m_RecordPaused = false; }

void SpiritTrail::StopRecording() {
    if (m_IsRecording) {
        m_IsRecording = false;
        m_Record.trafo.clear();
        m_Record.trafo.shrink_to_fit();
        m_Record.states.clear();
        m_Record.states.shrink_to_fit();
    }
}

void SpiritTrail::EndRecording() {
    if (m_IsRecording) {
        m_Record.hsscore = GetHSScore() - m_StartHS;
        m_Record.srscore = GetSRScore();

        bool savehs = m_Record.hsscore > m_Play[0].hsscore,
             savesr = m_Record.srscore < m_Play[1].srscore;
        if (savehs || savesr) {
            const uint64_t bytes =
                16 + uint64_t(m_Record.states.size()) * 28 + uint64_t(m_Record.trafo.size()) * 8;
            if (bytes > INT_MAX)
                throw std::runtime_error("Spirit Trail record exceeds the format capacity");
            std::vector<char> buffer(static_cast<size_t>(bytes));
            WriteWord(buffer.data(), static_cast<uint32_t>(m_Record.hsscore));
            WriteFloat(buffer.data() + 4, m_Record.srscore);
            WriteWord(buffer.data() + 8, static_cast<uint32_t>(m_Record.states.size()));
            WriteWord(buffer.data() + 12, static_cast<uint32_t>(m_Record.trafo.size()));
            char *p = buffer.data() + 16;
            for (const auto &state : m_Record.states) {
                WriteFloat(p, state.pos.x);
                WriteFloat(p + 4, state.pos.y);
                WriteFloat(p + 8, state.pos.z);
                WriteFloat(p + 12, state.rot.x);
                WriteFloat(p + 16, state.rot.y);
                WriteFloat(p + 20, state.rot.z);
                WriteFloat(p + 24, state.rot.w);
                p += 28;
            }
            for (const auto &[frame, ball] : m_Record.trafo) {
                WriteWord(p, static_cast<uint32_t>(frame));
                WriteWord(p + 4, static_cast<uint32_t>(ball));
                p += 8;
            }
            const auto hspath = m_RecordDir + "hs" + std::to_string(m_CurSector) + ".rec";
            const auto srpath = m_RecordDir + "sr" + std::to_string(m_CurSector) + ".rec";
            if (m_SaveRecord.joinable())
                m_SaveRecord.join();
            m_SaveRecord =
                std::jthread([savehs, savesr, hspath, srpath, buffer = std::move(buffer)] {
                    try {
                        if (savehs) {
                            if (CompressDataToFile(buffer.data(), static_cast<int>(buffer.size()),
                                                   hspath.c_str()) &&
                                savesr)
                                BML_CopyFileUtf8(hspath.c_str(), srpath.c_str());
                        } else
                            CompressDataToFile(buffer.data(), static_cast<int>(buffer.size()),
                                               srpath.c_str());
                    } catch (...) {
                    }
                });

            if (savehs)
                ModSupport::Log(m_Context, BML_LOG_INFO, "HS of sector %d has updated",
                                m_CurSector);
            if (savesr)
                ModSupport::Log(m_Context, BML_LOG_INFO, "SR of sector %d has updated",
                                m_CurSector);
        }

        StopRecording();
    }
}
#pragma once
#include "Common/ModSupport.hpp"
#include "Common/Commands.hpp"

#define TICK_SPEED 8

class NewSpawn;

std::unordered_map<std::string, int> ball_name_to_id{
    {"Ball_Paper", 0}, {"Ball_Wood", 1}, {"Ball_Stone", 2}};

class CommandNewSpawn {
  public:
    static BML::Command::Definition Define() {
        BML::Command::Definition d;
        d.Name = "newspawn";
        d.Alias = "nsp";
        d.Description = "Sets your spawn point to tp without reset prop";
        d.Cheat = true;
        return d;
    }

    explicit CommandNewSpawn(NewSpawn *np) : mod(np) {}

    void Execute(const BML::Mod::Context &context, const std::vector<std::string> &args);

    const std::vector<std::string> GetTabCompletion(const BML::Mod::Context &context,
                                                    const std::vector<std::string> &args);

    const VxMatrix &get_matrix() const { return matrix; }
    void set_matrix(const VxMatrix &mat) { matrix = mat; }
    int get_curSector() const { return curSector; }
    void set_curSector(const int sec) { curSector = sec; }

    int get_ball_type() const { return ball_type; }
    void set_ball_type(const int type) { ball_type = type; }

    bool get_is_spawned() const { return is_spawned; }
    void set_is_spawned(const bool type) { is_spawned = type; }

    const std::string &get_map_filename_setlevel() const { return map_filename; }
    void set_map_filename_setlevel(const std::string &type) { map_filename = type; }

  private:
    NewSpawn *mod = nullptr;

    VxMatrix matrix = {};
    int curSector = {};
    int ball_type = {};

    bool is_spawned = false;

    std::string map_filename;
};

class NewSpawn : public BML::Mod::Instance {
    BML::Mod::Context m_Context;
    ModSupport::Timers m_Timers;
    BML::Behavior::Session m_Behaviors;
    std::vector<BML::Command::Registration> m_Commands;

  public:


    //[transport]
    void OnLoad(const BML::Mod::Context &) override;
    void OnProcess(const BML::Mod::Context &) override;
    void OnModifyConfig(const BML::Mod::Context &, const BML::Mod::ConfigEvent &event) override;
    void OnLoadScript(const BML::Mod::Context &, const BML::Mod::LoadScriptEvent &event) override;
    void OnBallNavActive(const BML::Mod::Context &) override;
    void OnBallNavInactive(const BML::Mod::Context &) override;
    void OnPostStartMenu(const BML::Mod::Context &) override;
    void OnLoadObject(const BML::Mod::Context &, const BML::Mod::LoadObjectEvent &event) override;

    //[spirit]
    void OnPreResetLevel(const BML::Mod::Context &) override {
        StopRecording();
        StopPlaying();
        record_save_enable = false;
    }

    void OnPreExitLevel(const BML::Mod::Context &) override {
        StopRecording();
        StopPlaying();
        record_save_enable = false;
    }

    void OnPreEndLevel(const BML::Mod::Context &) override {
        StopRecording();
        StopPlaying();
        record_save_enable = false;
    }

    CK3dEntity *get_curBall() const {
        return static_cast<CK3dEntity *>(m_curLevel->GetElementObject(0, 1));
    }
    std::string get_map_filename_curlevel() { return map_filename_curlevel; }
    void set_map_filename_curlevel(const std::string &type) { map_filename_curlevel = type; }

  private:
    int GetCurrentBall();
    void SetCurrentBall(int curBall);

    void StopPlaying();
    void StopRecording(bool save_data = false);

    void StartRecording();
    void StartPlaying();

    static void SetParamString(CKParameter *param, CKSTRING value) { param->SetStringValue(value); }

    //[transport]
    bool init = false;
    bool Ball_Active = false;
    BML::Config::Property prop_enabled = {};
    BML::Config::Property reset_prop_enabled = {};
    BML::Config::Property prop_key = {};
    BML::Config::Property reset_spirit_enabled = {};
    BML::Config::Property prop_key_record = {};
    uint32_t key_prop = {};
    uint32_t key_record = {};
    bool enabled = false;
    bool reset_enabled = false;
    bool spirit_enabled = false;
    CommandNewSpawn *cmdspawnp_ptr = nullptr;

    ModSupport::Object<CKBehavior> m_dynamicPos{};
    ModSupport::Object<CKBehavior> m_phyNewBall{};
    ModSupport::Object<CKDataArray> m_curLevel{};
    ModSupport::Object<CKDataArray> m_checkpoints{};
    ModSupport::Object<CKDataArray> m_resetpoints{};
    ModSupport::Object<CKDataArray> m_ingameParam{};
    ModSupport::Object<CKParameter> m_curSector{};

    //[transport] Ball type
    ModSupport::Object<CKParameter> m_curTrafo{};
    ModSupport::Object<CKBehavior> m_setNewBall{};

    //[transport] Map filename
    std::string map_filename_curlevel;

    //[spirit]
    bool m_isRecording = false;
    bool m_isPlaying = false;

    bool record_exist = false;
    bool record_save_enable = false;

    float m_recordTimer = {};
    float m_playTimer = {};
    float m_srtimer = {};

    struct SpiritBall {
        std::string name;
        CK3dObject *obj;
        std::vector<CKMaterial *> materials;
    };

    std::vector<SpiritBall> m_dualBalls;

    struct Record {
        struct State {
            VxVector pos;
            VxQuaternion rot;
        };

        std::vector<State> states;
        std::vector<std::pair<int, int>> trafo;
    };

    Record m_record = {};
    Record m_play = {};

    size_t m_playBall = {};
    size_t m_curBall = {};
    size_t m_playFrame = {};
    size_t m_playTrafo = {};
};
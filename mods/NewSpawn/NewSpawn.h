#pragma once
#include <BML/BMLAll.h>

#define TICK_SPEED 8

MOD_EXPORT IMod *BMLEntry(IBML *bml);
MOD_EXPORT void BMLExit(IMod *mod);

class NewSpawn;

std::unordered_map<std::string, int> ball_name_to_id{{"Ball_Paper", 0}, {"Ball_Wood", 1}, {"Ball_Stone", 2}};

class CommandNewSpawn : public ICommand {
public:
    explicit CommandNewSpawn(NewSpawn *np) : mod(np) {}
    std::string GetName() override { return "newspawn"; }
    std::string GetAlias() override { return "nsp"; }
    std::string GetDescription() override { return "Sets your spawn point to tp without reset prop"; }
    bool IsCheat() override { return true; }

    void Execute(IBML *bml, const std::vector<std::string> &args) override;

    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override;

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

class NewSpawn : public IMod {
public:
    explicit NewSpawn(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "NewSpawn"; }
    const char *GetVersion() override { return "0.0.2"; }
    const char *GetName() override { return "NewSpawn"; }
    const char *GetAuthor() override { return "fluoresce"; }
    const char *GetDescription() override {
        return "\"nsp\" to set a position. shortcut key to tp to the position. "
                " Thanks for the help of BallanceBug and Swung ";
    }
    DECLARE_BML_VERSION;

    //[transport]
    void OnLoad() override;
    void OnProcess() override;
    void OnModifyConfig(const char *category, const char *key, IProperty *prop) override;
    void OnLoadScript(const char *filename, CKBehavior *script) override;
    void OnBallNavActive() override;
    void OnBallNavInactive() override;
    void OnPostStartMenu() override;
    void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName,
                      CK_CLASSID filterClass, CKBOOL addtoscene, CKBOOL reuseMeshes, CKBOOL reuseMaterials,
                      CKBOOL dynamic, XObjectArray *objArray, CKObject *masterObj) override;

    //[spirit]
    void OnPreResetLevel() override {
        StopRecording();
        StopPlaying();
        record_save_enable = false;
    }

    void OnPreExitLevel() override {
        StopRecording();
        StopPlaying();
        record_save_enable = false;
    }

    void OnPreEndLevel() override {
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

    static void SetParamString(CKParameter *param, CKSTRING value) {
        param->SetStringValue(value);
    }

    //[transport]
    bool init = false;
    bool Ball_Active = false;
    IProperty *prop_enabled = nullptr;
    IProperty *reset_prop_enabled = nullptr;
    IProperty *prop_key = nullptr;
    IProperty *reset_spirit_enabled = nullptr;
    IProperty *prop_key_record = nullptr;
    InputHook *input_manager = nullptr;
    CKKEYBOARD key_prop = {};
    CKKEYBOARD key_record = {};
    bool enabled = false;
    bool reset_enabled = false;
    bool spirit_enabled = false;
    CommandNewSpawn *cmdspawnp_ptr = nullptr;

    CKBehavior *m_dynamicPos = nullptr;
    CKBehavior *m_phyNewBall = nullptr;
    CKDataArray *m_curLevel = nullptr;
    CKDataArray *m_checkpoints = nullptr;
    CKDataArray *m_resetpoints = nullptr;
    CKDataArray *m_ingameParam = nullptr;
    CKParameter *m_curSector = nullptr;

    //[transport] Ball type
    CKParameter *m_curTrafo = nullptr;
    CKBehavior *m_setNewBall = nullptr;

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

    size_t m_playBall= {};
    size_t m_curBall= {};
    size_t m_playFrame = {};
    size_t m_playTrafo = {};
};
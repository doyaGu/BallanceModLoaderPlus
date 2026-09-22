#pragma once
#include <vector>
#include <unordered_map>
#include <memory>
#include <sstream>
#include <fstream>

#include <BML/BMLAll.h>

#include "PerLevelSegmentState.h"
#include "SegmentGui.h"

#include "picojson.h"

constexpr int SEG_MAJOR_VER = 2;
constexpr int SEG_MINOR_VER = 1;
constexpr int SEG_PATCH_VER = 6;

MOD_EXPORT IMod *BMLEntry(IBML *bml);
MOD_EXPORT void BMLExit(IMod *mod);

class Segment : public IMod {
public:
    explicit Segment(IBML *bml);

    const char *GetID() override { return "Segment"; }
    const char *GetVersion() override { return SEG_VERSION.c_str(); }
    const char *GetName() override { return "Segment"; }
    const char *GetAuthor() override { return "Swung0x48"; }
    const char *GetDescription() override {
        return "A mod to display your gameplay performance split into each segment.";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override;
    void OnModifyConfig(const char *category, const char *key, IProperty *prop) override;
    void OnPreEndLevel() override;
    void OnCounterActive() override;
    void OnCounterInactive() override;
    void OnPauseLevel() override;
    void OnUnpauseLevel() override;
    void OnProcess() override;
    void OnStartLevel() override;
    void OnPreResetLevel() override;
    void OnPostCheckpointReached() override;
    void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                              CKBOOL addtoscene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                              XObjectArray *objArray, CKObject *masterObj) override;
    void OnPreExitLevel() override;
    void OnGameOver() override;
    void OnCheatEnabled(bool enable) override;

    void ClearHistory() const {
        if (session_)
            session_->state.clear_history();
    }

    void Show() const {
        if (session_)
            session_->gui.set_visible(true);
    }

    void Hide() const {
        if (session_)
            session_->gui.set_visible(false);
    }

private:
    const static inline std::string SEG_VERSION = std::format("{}.{}.{}", SEG_MAJOR_VER, SEG_MINOR_VER, SEG_PATCH_VER);
    const static inline std::string RECORD_SAVE_PATH = "../ModLoader/Configs/SegmentRecords.json";

    struct session {
        session(const int current_level, const int sector_count): state(sector_count), gui(state, current_level) {
        }

        session(const std::string_view current_level_name, const int sector_count) : state(sector_count),
            gui(state, current_level_name) {
        }

        PerLevelSegmentState state;
        SegmentGui gui;
    };

    std::unordered_map<std::string, std::shared_ptr<session>> sessions_;
    std::shared_ptr<session> session_;
    bool cheat_enabled_once_ = false;

    IProperty *font_scale_ = nullptr;
    IProperty *show_settings_ = nullptr;

    enum class serialize_from_t {
        Current,
        Target
    };

    picojson::value serialize_sessions_to_pico(serialize_from_t serialize_from = serialize_from_t::Target) const;

    void save_pico_to_file(const picojson::value &v);

    bool load_sessions_from_file();

    static bool is_custom_map(const std::string_view filename) {
        return filename.substr(0, 11) != "3D Entities";
    }

    int get_current_level() const {
        int ret = 0;
        m_BML->GetArrayByName("CurrentLevel")->GetElementValue(0, 0, &ret);
        return ret;
    }

    int get_current_sector() const {
        int next_sector;
        CKDataArray *array = m_BML->GetArrayByName("IngameParameter");
        if (!array)
            return -1;
        array->GetElementValue(0, 1, &next_sector);
        return next_sector - 1;
    }

    // TODO: more elegant way to retrieve sector groups
    int get_sector_count() const {
        std::string name;

        int sector_count = 0;
        for (int i = 1; i <= 9; i++) {
            if (i == 9)
                name = "Sector_9";
            else
                name = std::format("Sector_{:02d}", i);

            if (m_BML->GetGroupByName(name.c_str()) == nullptr)
                break;

            sector_count = i;
        }
        return sector_count;
    }

    static std::vector<double> split(const std::string &s, const char delim = ' ') {
        std::vector<double> vec;
        std::istringstream iss(s);
        std::string temp;

        while (getline(iss, temp, delim)) {
            vec.push_back(stof(temp));
        }
        return vec;
    }
};

#pragma once
#include <vector>
#include <unordered_map>
#include <memory>
#include <sstream>
#include <fstream>

#include "Common/ModSupport.hpp"
#include "Common/Commands.hpp"

#include "PerLevelSegmentState.h"
#include "SegmentGui.h"

#include "picojson.h"

constexpr int SEG_MAJOR_VER = 2;
constexpr int SEG_MINOR_VER = 1;
constexpr int SEG_PATCH_VER = 6;

class Segment : public BML::Mod::Instance {
    BML::Mod::Context m_Context;
    ModSupport::Timers m_Timers;
    BML::Behavior::Session m_Behaviors;
    std::vector<BML::Command::Registration> m_Commands;

  public:
    Segment();


    void OnLoad(const BML::Mod::Context &) override;
    void OnModifyConfig(const BML::Mod::Context &, const BML::Mod::ConfigEvent &event) override;
    void OnPreEndLevel(const BML::Mod::Context &) override;
    void OnCounterActive(const BML::Mod::Context &) override;
    void OnCounterInactive(const BML::Mod::Context &) override;
    void OnPauseLevel(const BML::Mod::Context &) override;
    void OnUnpauseLevel(const BML::Mod::Context &) override;
    void OnProcess(const BML::Mod::Context &) override;
    void OnStartLevel(const BML::Mod::Context &) override;
    void OnPreResetLevel(const BML::Mod::Context &) override;
    void OnPostCheckpointReached(const BML::Mod::Context &) override;
    void OnLoadObject(const BML::Mod::Context &, const BML::Mod::LoadObjectEvent &event) override;
    void OnPreExitLevel(const BML::Mod::Context &) override;
    void OnGameOver(const BML::Mod::Context &) override;
    void OnCheatEnabled(const BML::Mod::Context &, const BML::Mod::CheatEvent &event) override;

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
    std::string m_RecordSavePath;

    struct session {
        session(const int current_level, const int sector_count)
            : state(sector_count), gui(state, current_level) {}

        session(const std::string_view current_level_name, const int sector_count)
            : state(sector_count), gui(state, current_level_name) {}

        PerLevelSegmentState state;
        SegmentGui gui;
    };

    std::unordered_map<std::string, std::shared_ptr<session>> sessions_;
    std::shared_ptr<session> session_;
    bool cheat_enabled_once_ = false;

    BML::Config::Property font_scale_ = {};
    BML::Config::Property show_settings_ = {};

    enum class serialize_from_t { Current, Target };

    picojson::value
    serialize_sessions_to_pico(serialize_from_t serialize_from = serialize_from_t::Target) const;

    void save_pico_to_file(const picojson::value &v);

    bool load_sessions_from_file();

    static bool is_custom_map(const std::string_view filename) {
        return filename.substr(0, 11) != "3D Entities";
    }

    int get_current_level() const {
        int ret = 0;
        ModSupport::Find<CKDataArray>(m_Context, "CurrentLevel")->GetElementValue(0, 0, &ret);
        return ret;
    }

    int get_current_sector() const {
        int next_sector;
        CKDataArray *array = ModSupport::Find<CKDataArray>(m_Context, "IngameParameter");
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

            if (ModSupport::Find<CKGroup>(m_Context, name.c_str()) == nullptr)
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

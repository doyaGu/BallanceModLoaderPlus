#include "Segment.h"

#include "CommandSeg.h"

IMod *BMLEntry(IBML *bml) {
    return new Segment(bml);
}

void BMLExit(IMod *mod) {
    delete mod;
}

Segment::Segment(IBML *bml) : IMod(bml) {}

void Segment::OnLoad() {
    GetConfig()->SetCategoryComment("GUI", "GUI Settings");

    font_scale_ = GetConfig()->GetProperty("GUI", "FontScale");
    font_scale_->SetComment("The font scale");
    font_scale_->SetDefaultFloat(0.7f);

    show_settings_ = GetConfig()->GetProperty("GUI", "ShowSettings");
    show_settings_->SetComment("Show settings in window");
    show_settings_->SetDefaultBoolean(false);

    m_BML->RegisterCommand(new CommandSeg(this));

    load_sessions_from_file();
}

void Segment::OnModifyConfig(const char *category, const char *key, IProperty *prop) {
    if (prop == font_scale_) {
        if (session_)
            session_->gui.set_font_scale(font_scale_->GetFloat());
	} else if (prop == show_settings_) {
		if (session_)
			session_->gui.set_settings_visible(show_settings_->GetBoolean());
	}
}

void Segment::OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                           CKBOOL addtoscene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                           XObjectArray *objArray, CKObject *masterObj) {
    if (!isMap)
        return;
    const int current_level = get_current_level();
    const int sector_count = get_sector_count();

    // TODO: S/L states
    //state_ = std::make_unique<PerLevelSegmentState>(sector_count);
    //gui_ = std::make_unique<SegmentGui>(*state_, get_current_level());

    if (sessions_.find(filename) == sessions_.end()) {
        if (!is_custom_map(filename))
            sessions_[filename] = std::make_shared<session>(current_level, sector_count);
        else {
            CKPathSplitter splitter(const_cast<char*>(filename));
            std::string name = std::format("\"{}\"", splitter.GetName());
            sessions_[filename] = std::make_shared<session>(name, sector_count);
        }
    }

    session_ = sessions_[filename];
    session_->gui.set_cursor_visible(true);
    session_->gui.set_settings_visible(show_settings_->GetBoolean());
    session_->gui.set_font_scale(font_scale_->GetFloat());
}

void Segment::OnPreExitLevel() {
    session_->gui.set_cursor_visible(false);
    session_->state.enable_counting(false);
    if (!cheat_enabled_once_)
        session_->state.update_target_figures();
    save_pico_to_file(serialize_sessions_to_pico());
    cheat_enabled_once_ = false;
}

void Segment::OnGameOver() {
    session_->state.enable_counting(false);
}

void Segment::OnCheatEnabled(bool enable) {
    if (enable) {
        cheat_enabled_once_ = true;
        m_BML->SendIngameMessage("Cheat mode enabled. Segment will stop recording.");
        m_BML->SendIngameMessage("Disable cheat mode to restore.");
    } else {
        m_BML->SendIngameMessage("Cheat mode disabled. Segment will now start recording.");
    }
}

void Segment::OnPreEndLevel() {
    session_->state.enable_counting(false);
    session_->state.change_segment(get_current_sector() + 1);
    if (!cheat_enabled_once_)
        session_->state.update_target_figures();
    save_pico_to_file(serialize_sessions_to_pico());
    cheat_enabled_once_ = false;
}

void Segment::OnCounterActive() {
    session_->state.enable_counting(true);
}

void Segment::OnCounterInactive() {
    session_->state.enable_counting(false);
}

void Segment::OnPauseLevel() {
    session_->state.enable_counting(false);
}

void Segment::OnUnpauseLevel() {
    session_->state.enable_counting(true);
}

void Segment::OnProcess() {
    if (!m_BML->IsIngame())
        return;

    const float delta = m_BML->GetTimeManager()->GetLastDeltaTime();
    session_->state.update(delta / 1000.0f);
    session_->gui.update();
}

void Segment::OnStartLevel() {
    cheat_enabled_once_ = m_BML->IsCheatEnabled();
    session_->state.reset();
    session_->gui.set_cursor_visible(true);
}

void Segment::OnPreResetLevel() {
    session_->gui.set_cursor_visible(false);
    session_->state.enable_counting(false);
    if (!cheat_enabled_once_)
        session_->state.update_target_figures();
    save_pico_to_file(serialize_sessions_to_pico());
    cheat_enabled_once_ = false;
}

void Segment::OnPostCheckpointReached() {
    session_->state.change_segment(get_current_sector());
}

picojson::value Segment::serialize_sessions_to_pico(serialize_from_t serialize_from) const {
    picojson::array records;
    for (const auto &[path, session]: sessions_) {
        picojson::object map_obj;

        picojson::object level_obj;
        picojson::array arr;
        const auto &state = session->state;
        switch (serialize_from) {
            case serialize_from_t::Target:
                for (size_t i = 0; i < state.size(); ++i) {
                    arr.emplace_back(state.segment_target(i));
                }
                break;
            case serialize_from_t::Current:
                for (size_t i = 0; i < state.size(); ++i) {
                    arr.emplace_back(state.segment(i));
                }
                break;
            default:
                assert(false);
        }

        level_obj.emplace("name", session->gui.current_level_name_);
        level_obj.emplace("segments", arr);
        map_obj.emplace(path, level_obj);

        records.emplace_back(map_obj);
    }
    picojson::value v(records);
    return v;
}

void Segment::save_pico_to_file(const picojson::value &v) {
    //std::string path = (std::filesystem::current_path() / RECORD_SAVE_PATH).lexically_normal().string();
    std::ofstream fs(RECORD_SAVE_PATH, std::ios::trunc);
    fs << v.serialize();
    fs.close();
}

bool Segment::load_sessions_from_file() {
    picojson::value v;
    std::ifstream fs(RECORD_SAVE_PATH);
    fs >> v;
    std::string err = picojson::get_last_error();
    if (!err.empty()) {
        GetLogger()->Warn("Error loading sessions from file.");
        GetLogger()->Warn(err.c_str());

        sessions_.clear();
        return false;
    }

    if (!v.is<picojson::array>()) {
        GetLogger()->Error("Outermost array in illegal form.");

        sessions_.clear();
        return false;
    }
    picojson::array records = v.get<picojson::array>();

    for (const auto &record: records) {
        if (!record.is<picojson::object>()) {
            GetLogger()->Error("Record object in illegal form.");

            sessions_.clear();
            return false;
        }
        picojson::object map_obj = record.get<picojson::object>();

        for (picojson::value::object::const_iterator i = map_obj.begin(); i != map_obj.end(); ++i) {
            if (!i->second.is<picojson::object>()) {
                GetLogger()->Error("Level object in illegal form.");

                sessions_.clear();
                return false;
            }
            picojson::object obj = i->second.get<picojson::object>();

            if (!obj.contains("name") || !obj["name"].is<std::string>()) {
                GetLogger()->Error("Level object does not have \'name\' field.");

                sessions_.clear();
                return false;
            }
            std::string name = obj["name"].get<std::string>();

            if (!obj.contains("segments") || !obj["segments"].is<picojson::array>()) {
                GetLogger()->Error("Level object does not have \'segments\' field.");

                sessions_.clear();
                return false;
            }
            const auto &arr = obj["segments"].get<picojson::array>();

            sessions_[i->first] = std::make_shared<session>(name, arr.size());
            auto &state = sessions_[i->first]->state;
            for (size_t j = 0; j < state.size(); ++j) {
                state.segment_target(j) = static_cast<float>(arr[j].get<double>());
            }
        }
    }

    return true;
}

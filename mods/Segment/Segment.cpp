#include "Segment.h"

#include "CommandSeg.h"

BML_DEFINE_MOD(Segment, {
    .Id = "Segment",
    .Name = "Segment",
    .Version = std::format("{}.{}.{}", SEG_MAJOR_VER, SEG_MINOR_VER, SEG_PATCH_VER),
    .Author = "Swung0x48",
    .Description = "A mod to display your gameplay performance split into each segment.",
    .MinimumLoaderVersion = BML_VERSION,
})

Segment::Segment() = default;

void Segment::OnLoad(const BML::Mod::Context &context) {
    m_Context = context;
    m_Timers = ModSupport::Timers(context);
    m_Context.Config().SetCategoryComment("GUI", "GUI Settings");

    font_scale_ = m_Context.Config()
                      .DeclareProperty({"GUI", "FontScale", float(0.7f), "The font scale"})
                      .Take();

    show_settings_ =
        m_Context.Config()
            .DeclareProperty({"GUI", "ShowSettings", bool(false), "Show settings in window"})
            .Take();

    ModSupport::RegisterCommand(m_Context, m_Commands, std::make_shared<CommandSeg>(this));

    m_RecordSavePath = m_Context.GetDirectory(BML_DIR_CONFIG).Value() + "/SegmentRecords.json";
    load_sessions_from_file();
}

void Segment::OnModifyConfig(const BML::Mod::Context &, const BML::Mod::ConfigEvent &event) {
    const auto *category = event.Category.data();
    const auto *key = event.Key.data();
    auto found = m_Context.Config().FindProperty(event.Category, event.Key);
    if (!found)
        return;
    auto prop = found.Value();

    if ((event.Category == "GUI" && event.Key == "FontScale")) {
        if (session_)
            session_->gui.set_font_scale(font_scale_.Get<float>());
    } else if ((event.Category == "GUI" && event.Key == "ShowSettings")) {
        if (session_)
            session_->gui.set_settings_visible(show_settings_.Get<bool>());
    }
}

void Segment::OnLoadObject(const BML::Mod::Context &, const BML::Mod::LoadObjectEvent &event) {
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

    if (!isMap)
        return;
    const int current_level = get_current_level();
    const int sector_count = get_sector_count();

    // TODO: S/L states
    // state_ = std::make_unique<PerLevelSegmentState>(sector_count);
    // gui_ = std::make_unique<SegmentGui>(*state_, get_current_level());

    if (sessions_.find(filename) == sessions_.end()) {
        if (!is_custom_map(filename))
            sessions_[filename] = std::make_shared<session>(current_level, sector_count);
        else {
            CKPathSplitter splitter(const_cast<char *>(filename));
            std::string name = std::format("\"{}\"", splitter.GetName());
            sessions_[filename] = std::make_shared<session>(name, sector_count);
        }
    }

    session_ = sessions_[filename];
    session_->gui.set_cursor_visible(true);
    session_->gui.set_settings_visible(show_settings_.Get<bool>());
    session_->gui.set_font_scale(font_scale_.Get<float>());
}

void Segment::OnPreExitLevel(const BML::Mod::Context &) {
    session_->gui.set_cursor_visible(false);
    session_->state.enable_counting(false);
    if (!cheat_enabled_once_)
        session_->state.update_target_figures();
    save_pico_to_file(serialize_sessions_to_pico());
    cheat_enabled_once_ = false;
}

void Segment::OnGameOver(const BML::Mod::Context &) { session_->state.enable_counting(false); }

void Segment::OnCheatEnabled(const BML::Mod::Context &, const BML::Mod::CheatEvent &event) {
    const bool enable = event.Enabled;

    if (enable) {
        cheat_enabled_once_ = true;
        BML::UI::AddMessage("Cheat mode enabled. Segment will stop recording.");
        BML::UI::AddMessage("Disable cheat mode to restore.");
    } else {
        BML::UI::AddMessage("Cheat mode disabled. Segment will now start recording.");
    }
}

void Segment::OnPreEndLevel(const BML::Mod::Context &) {
    session_->state.enable_counting(false);
    session_->state.change_segment(get_current_sector() + 1);
    if (!cheat_enabled_once_)
        session_->state.update_target_figures();
    save_pico_to_file(serialize_sessions_to_pico());
    cheat_enabled_once_ = false;
}

void Segment::OnCounterActive(const BML::Mod::Context &) { session_->state.enable_counting(true); }

void Segment::OnCounterInactive(const BML::Mod::Context &) {
    session_->state.enable_counting(false);
}

void Segment::OnPauseLevel(const BML::Mod::Context &) { session_->state.enable_counting(false); }

void Segment::OnUnpauseLevel(const BML::Mod::Context &) { session_->state.enable_counting(true); }

void Segment::OnProcess(const BML::Mod::Context &) {
    if (!ModSupport::GameState().Ingame)
        return;

    const float delta = ModSupport::Engine(m_Context)->GetTimeManager()->GetLastDeltaTime();
    session_->state.update(delta / 1000.0f);
    session_->gui.update();
}

void Segment::OnStartLevel(const BML::Mod::Context &) {
    cheat_enabled_once_ = ModSupport::CheatEnabled();
    session_->state.reset();
    session_->gui.set_cursor_visible(true);
}

void Segment::OnPreResetLevel(const BML::Mod::Context &) {
    session_->gui.set_cursor_visible(false);
    session_->state.enable_counting(false);
    if (!cheat_enabled_once_)
        session_->state.update_target_figures();
    save_pico_to_file(serialize_sessions_to_pico());
    cheat_enabled_once_ = false;
}

void Segment::OnPostCheckpointReached(const BML::Mod::Context &) {
    session_->state.change_segment(get_current_sector());
}

picojson::value Segment::serialize_sessions_to_pico(serialize_from_t serialize_from) const {
    picojson::array records;
    for (const auto &[path, session] : sessions_) {
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
    if (!BML_WriteTextFileUtf8(m_RecordSavePath.c_str(), v.serialize().c_str()))
        (void)m_Context.LogError("Failed to save Segment records");
}

bool Segment::load_sessions_from_file() {
    picojson::value v;
    char *text = BML_ReadTextFileUtf8(m_RecordSavePath.c_str());
    if (!text)
        return false;
    std::string data(text);
    BML_FreeString(text);
    std::istringstream fs(data);
    fs >> v;
    std::string err = picojson::get_last_error();
    if (!err.empty()) {
        ModSupport::Log(m_Context, BML_LOG_WARN, "Error loading sessions from file.");
        ModSupport::Log(m_Context, BML_LOG_WARN, "%s", err.c_str());

        sessions_.clear();
        return false;
    }

    if (!v.is<picojson::array>()) {
        ModSupport::Log(m_Context, BML_LOG_ERROR, "Outermost array in illegal form.");

        sessions_.clear();
        return false;
    }
    picojson::array records = v.get<picojson::array>();

    for (const auto &record : records) {
        if (!record.is<picojson::object>()) {
            ModSupport::Log(m_Context, BML_LOG_ERROR, "Record object in illegal form.");

            sessions_.clear();
            return false;
        }
        picojson::object map_obj = record.get<picojson::object>();

        for (picojson::value::object::const_iterator i = map_obj.begin(); i != map_obj.end(); ++i) {
            if (!i->second.is<picojson::object>()) {
                ModSupport::Log(m_Context, BML_LOG_ERROR, "Level object in illegal form.");

                sessions_.clear();
                return false;
            }
            picojson::object obj = i->second.get<picojson::object>();

            if (!obj.contains("name") || !obj["name"].is<std::string>()) {
                ModSupport::Log(m_Context, BML_LOG_ERROR,
                                "Level object does not have \'name\' field.");

                sessions_.clear();
                return false;
            }
            std::string name = obj["name"].get<std::string>();

            if (!obj.contains("segments") || !obj["segments"].is<picojson::array>()) {
                ModSupport::Log(m_Context, BML_LOG_ERROR,
                                "Level object does not have \'segments\' field.");

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

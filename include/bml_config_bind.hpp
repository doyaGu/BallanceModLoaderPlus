/**
 * @file bml_config_bind.hpp
 * @brief Declarative config binding for BML modules
 *
 * Eliminates the repeated EnsureDefaultConfig/RefreshConfig boilerplate.
 * Bind config keys to member fields once, then call Sync() to ensure
 * defaults exist and read current values in one step.
 *
 * Usage:
 * @code
 *   class MyMod : public bml::HookModule {
 *       bool m_Widescreen = false;
 *       int32_t m_MaxFps = 0;
 *       float m_Volume = 1.0f;
 *
 *       bml::ConfigBindings m_Cfg;
 *
 *       BML_Result OnModuleAttach(bml::ModuleServices &services) override {
 *           m_Cfg.Bind("Graphics", "Widescreen", m_Widescreen, false);
 *           m_Cfg.Bind("Graphics", "MaxFPS",     m_MaxFps,     0);
 *           m_Cfg.Bind("Audio",    "Volume",     m_Volume,     1.0f);
 *           m_Cfg.Sync(Services().Config());
 *           return BML_RESULT_OK;
 *       }
 *   };
 * @endcode
 */

#ifndef BML_CONFIG_BIND_HPP
#define BML_CONFIG_BIND_HPP

#include "bml_config.hpp"

#include <string>
#include <variant>
#include <vector>

namespace bml {

class ConfigBindings {
    enum Type : uint8_t { Bool, Int, Float, String };

    struct Slot {
        const char *section;
        const char *key;
        Type type;
        void *target;
        std::variant<bool, int32_t, float, std::string> defaultVal;

        void EnsureDefault(Config &cfg) const {
            switch (type) {
            case Bool:
                if (!cfg.GetBool(section, key).has_value())
                    cfg.SetBool(section, key, std::get<bool>(defaultVal));
                break;
            case Int:
                if (!cfg.GetInt(section, key).has_value())
                    cfg.SetInt(section, key, std::get<int32_t>(defaultVal));
                break;
            case Float:
                if (!cfg.GetFloat(section, key).has_value())
                    cfg.SetFloat(section, key, std::get<float>(defaultVal));
                break;
            case String:
                if (!cfg.GetString(section, key).has_value())
                    cfg.SetString(section, key, std::get<std::string>(defaultVal).c_str());
                break;
            }
        }

        void Read(const Config &cfg) const {
            switch (type) {
            case Bool:
                *static_cast<bool *>(target) = cfg.GetBool(section, key).value_or(std::get<bool>(defaultVal));
                break;
            case Int:
                *static_cast<int32_t *>(target) = cfg.GetInt(section, key).value_or(std::get<int32_t>(defaultVal));
                break;
            case Float:
                *static_cast<float *>(target) = cfg.GetFloat(section, key).value_or(std::get<float>(defaultVal));
                break;
            case String:
                *static_cast<std::string *>(target) = cfg.GetString(section, key).value_or(std::get<std::string>(defaultVal));
                break;
            }
        }
    };

    std::vector<Slot> m_Slots;

public:
    void Bind(const char *section, const char *key, bool &field, bool def) {
        m_Slots.push_back({section, key, Bool, &field, def});
    }
    void Bind(const char *section, const char *key, int32_t &field, int32_t def) {
        m_Slots.push_back({section, key, Int, &field, def});
    }
    void Bind(const char *section, const char *key, float &field, float def) {
        m_Slots.push_back({section, key, Float, &field, def});
    }
    void Bind(const char *section, const char *key, std::string &field, std::string def) {
        m_Slots.push_back({section, key, String, &field, std::move(def)});
    }

    /** @brief Ensure defaults exist, then read all values. Call once on attach. */
    void Sync(Config cfg) {
        for (auto &s : m_Slots) { s.EnsureDefault(cfg); s.Read(cfg); }
    }

    /** @brief Read all values without writing defaults. Call on refresh events. */
    void Refresh(const Config &cfg) {
        for (auto &s : m_Slots) s.Read(cfg);
    }

    size_t Count() const noexcept { return m_Slots.size(); }
    bool Empty() const noexcept { return m_Slots.empty(); }
    void Clear() { m_Slots.clear(); }
};

} // namespace bml

#endif /* BML_CONFIG_BIND_HPP */

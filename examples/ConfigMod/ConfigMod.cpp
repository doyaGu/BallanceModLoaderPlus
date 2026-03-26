/**
 * ConfigMod - BML v0.4 Configuration Example
 *
 * Demonstrates:
 * - Reading config values with typed defaults (string, int, bool)
 * - Logging loaded configuration on engine init
 * - Writing a config value on detach
 * - Using Services().Config() for all operations
 */

#define BML_LOADER_IMPLEMENTATION
#include "bml_module.hpp"
#include "bml_topics.h"

#include <ctime>

class ConfigMod : public bml::Module {
    bml::imc::SubscriptionManager m_Subs;

    // Cached config values read during attach
    std::string m_PlayerName;
    int32_t     m_MaxRetries = 3;
    bool        m_Verbose    = false;

public:
    BML_Result OnAttach(bml::ModuleServices &services) override {
        m_Subs = services.CreateSubscriptions();

        // Read config with typed defaults -- missing keys return the default
        auto cfg = Services().Config();
        m_PlayerName = cfg.GetString("general", "player_name", "Player");
        m_MaxRetries = cfg.GetInt("general", "max_retries", 3);
        m_Verbose    = cfg.GetBool("general", "verbose", false);

        Services().Log().Info("Config loaded: name=\"%s\", retries=%d, verbose=%s",
                              m_PlayerName.c_str(), m_MaxRetries,
                              m_Verbose ? "true" : "false");

        // Log config values again when the engine finishes initialising
        m_Subs.Add(BML_TOPIC_ENGINE_INIT, [this](const bml::imc::Message &) {
            Services().Log().Info("[Engine Init] player_name = %s", m_PlayerName.c_str());
            Services().Log().Info("[Engine Init] max_retries = %d", m_MaxRetries);
            Services().Log().Info("[Engine Init] verbose     = %s",
                                  m_Verbose ? "true" : "false");
        });

        return m_Subs.Empty() ? BML_RESULT_FAIL : BML_RESULT_OK;
    }

    void OnDetach() override {
        // Write a "last_run" timestamp so the next session can read it
        char buf[64];
        std::time_t now = std::time(nullptr);
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

        Services().Config().SetString("general", "last_run", buf);
        Services().Log().Info("Saved last_run = %s", buf);
    }
};

BML_DEFINE_MODULE(ConfigMod)

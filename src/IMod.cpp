#include "BML/IMod.h"
#include "Logger.h"
#include "Config.h"
#include "ModLoader.h"

ILogger *IMod::GetLogger() {
    if (m_Logger == nullptr)
        m_Logger = new Logger(GetID());
    return m_Logger;
}

IConfig *IMod::GetConfig() {
    if (m_Config == nullptr) {
        Config *config = new Config(this);
        m_Config = config;
        ModLoader::GetInstance().AddConfig(config);
    }
    return m_Config;
}

IMod::~IMod() {
    if (m_Logger)
        delete m_Logger;
    if (m_Config)
        delete m_Config;
}
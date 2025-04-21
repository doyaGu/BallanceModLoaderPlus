#include "BML/IMod.h"
#include "Logger.h"
#include "Config.h"
#include "ModContext.h"

ILogger *IMod::GetLogger() {
    if (m_Logger == nullptr)
        m_Logger = new Logger(GetID());
    return m_Logger;
}

IConfig *IMod::GetConfig() {
    if (m_Config == nullptr) {
        auto *config = new Config(this);
        m_Config = config;
        BML_GetModContext()->AddConfig(config);
    }
    return m_Config;
}

IMod::~IMod() {
    if (m_Logger)
        delete m_Logger;
    if (m_Config)
        delete m_Config;
    m_BML->ClearDependencies(this);
}
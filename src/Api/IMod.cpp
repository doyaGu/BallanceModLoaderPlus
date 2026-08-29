#include "BML/IMod.h"

#include <memory>

#include "Logging/Logger.h"
#include "Config/Config.h"
#include "Loader/ModContext.h"

ILogger *IMod::GetLogger() {
    if (m_Logger == nullptr)
        m_Logger = new Logger(GetID());
    return m_Logger;
}

IConfig *IMod::GetConfig() {
    if (m_Config == nullptr) {
        auto config = std::make_unique<Config>(this);
        m_Config = BML_GetModContext()->AddConfig(std::move(config));
    }
    return m_Config;
}

IMod::~IMod() {
    if (m_Logger)
        delete m_Logger;
    m_Config = nullptr;
    m_BML->ClearDependencies(this);
}

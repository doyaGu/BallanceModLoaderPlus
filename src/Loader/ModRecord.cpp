#include "Loader/ModRecord.h"
#include "Loader/ModContext.h"

Logger *ModRecord::GetLogger() {
    if (!m_Logger)
        m_Logger = std::make_unique<Logger>(GetID());
    return m_Logger.get();
}

Config *ModRecord::GetConfig() {
    if (!m_Config && m_Context)
        m_Config = m_Context->AddConfig(std::make_unique<Config>(this));
    return m_Config;
}

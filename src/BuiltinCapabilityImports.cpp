#include "BuiltinCapabilityImports.h"

void BuiltinCapabilityImports::SendIngameMessage(const char *message) {
    BindIfNeeded(m_SendMessage, "ui.message.add", "void(string)");
    m_SendMessage.Invoke(message);
}

void BuiltinCapabilityImports::ClearIngameMessages() {
    BindIfNeeded(m_ClearMessages, "ui.message.clear", "void()");
    m_ClearMessages.Invoke();
}

void BuiltinCapabilityImports::OpenModsMenu() {
    BindIfNeeded(m_OpenModsMenu, "ui.menu.mods.open", "void()");
    m_OpenModsMenu.Invoke();
}

void BuiltinCapabilityImports::CloseModsMenu() {
    BindIfNeeded(m_CloseModsMenu, "ui.menu.mods.close", "void()");
    m_CloseModsMenu.Invoke();
}

void BuiltinCapabilityImports::OpenMapMenu() {
    BindIfNeeded(m_OpenMapMenu, "ui.menu.maps.open", "void()");
    m_OpenMapMenu.Invoke();
}

void BuiltinCapabilityImports::CloseMapMenu() {
    BindIfNeeded(m_CloseMapMenu, "ui.menu.maps.close", "void()");
    m_CloseMapMenu.Invoke();
}

int BuiltinCapabilityImports::GetHUD() {
    BindIfNeeded(m_GetHUD, "ui.hud.get", "int()");
    return m_GetHUD.InvokeOr(0);
}

void BuiltinCapabilityImports::SetHUD(int mode) {
    BindIfNeeded(m_SetHUD, "ui.hud.set", "void(int)");
    m_SetHUD.Invoke(mode);
}

void BuiltinCapabilityImports::ShowTitle(bool show) {
    BindIfNeeded(m_ShowTitle, "ui.hud.title.show", "void(bool)");
    m_ShowTitle.Invoke(show);
}

void BuiltinCapabilityImports::ShowFPS(bool show) {
    BindIfNeeded(m_ShowFPS, "ui.hud.fps.show", "void(bool)");
    m_ShowFPS.Invoke(show);
}

void BuiltinCapabilityImports::ShowSRTimer(bool show) {
    BindIfNeeded(m_ShowSRTimer, "ui.hud.sr.show", "void(bool)");
    m_ShowSRTimer.Invoke(show);
}

void BuiltinCapabilityImports::StartSRTimer() {
    BindIfNeeded(m_StartSRTimer, "ui.hud.sr.start", "void()");
    m_StartSRTimer.Invoke();
}

void BuiltinCapabilityImports::PauseSRTimer() {
    BindIfNeeded(m_PauseSRTimer, "ui.hud.sr.pause", "void()");
    m_PauseSRTimer.Invoke();
}

void BuiltinCapabilityImports::ResetSRTimer() {
    BindIfNeeded(m_ResetSRTimer, "ui.hud.sr.reset", "void()");
    m_ResetSRTimer.Invoke();
}

float BuiltinCapabilityImports::GetSRTime() {
    BindIfNeeded(m_GetSRTime, "ui.hud.sr.time", "float()");
    return m_GetSRTime.InvokeOr(0.0f);
}

#include "Gameplay/GameSession.h"

namespace BML {

void GameSession::ActivateLevel() noexcept {
    m_Phase = GamePhase::LevelActive;
}

void GameSession::BeginTransition() noexcept {
    m_Phase = GamePhase::Transitioning;
}

void GameSession::ReturnToFrontEnd() noexcept {
    m_Phase = GamePhase::FrontEnd;
}

void GameSession::PauseLevel() noexcept {
    if (m_Phase == GamePhase::LevelActive)
        m_Phase = GamePhase::LevelPaused;
}

void GameSession::ResumeLevel() noexcept {
    if (m_Phase == GamePhase::LevelPaused)
        m_Phase = GamePhase::LevelActive;
}

} // namespace BML

#include "RecorderEngine.h"

bool RecorderEngine::StartRecording(const std::string &path, uint32_t levelId) {
    if (m_State != RecorderState::Idle) return false;

    if (!m_Writer.Open(path, levelId)) return false;

    m_FilePath = path;
    m_FrameCounter = 0;
    m_FrameEvents.clear();
    m_CurrentLevelId = levelId;
    m_State = RecorderState::Recording;
    return true;
}

bool RecorderEngine::StopRecording() {
    if (m_State != RecorderState::Recording) return false;

    m_Writer.Finalize();
    m_FrameEvents.clear();
    m_State = RecorderState::Idle;
    return true;
}

bool RecorderEngine::StartPlayback(const std::string &path) {
    if (m_State != RecorderState::Idle) return false;

    if (!m_Reader.Open(path)) return false;

    m_FilePath = path;
    m_FrameCounter = 0;
    m_HasPendingFrame = false;
    PreloadNextFrame();
    m_State = RecorderState::Playing;
    return true;
}

bool RecorderEngine::StopPlayback() {
    if (m_State != RecorderState::Playing && m_State != RecorderState::Paused)
        return false;

    m_Reader = RecordingReader{};
    m_HasPendingFrame = false;
    m_State = RecorderState::Idle;
    return true;
}

bool RecorderEngine::PausePlayback() {
    if (m_State != RecorderState::Playing) return false;
    m_State = RecorderState::Paused;
    return true;
}

bool RecorderEngine::ResumePlayback() {
    if (m_State != RecorderState::Paused) return false;
    m_State = RecorderState::Playing;
    return true;
}

void RecorderEngine::AccumulateEvent(const EventRecord &event) {
    if (m_State != RecorderState::Recording) return;
    m_FrameEvents.push_back(event);
}

bool RecorderEngine::OnPostProcess(float deltaTime, std::vector<EventRecord> *out_events) {
    if (out_events) out_events->clear();

    if (m_State == RecorderState::Recording) {
        m_Writer.WriteFrame(m_FrameCounter, deltaTime, m_FrameEvents);
        m_FrameEvents.clear();
        ++m_FrameCounter;
        return true;
    }

    if (m_State == RecorderState::Paused) {
        return true;
    }

    if (m_State == RecorderState::Playing) {
        if (m_HasPendingFrame) {
            if (out_events) {
                *out_events = std::move(m_PendingFrame.events);
            }
            ++m_FrameCounter;
            m_HasPendingFrame = false;
            PreloadNextFrame(); // best-effort; EOF handled on next tick
            return true;
        } else {
            // No more frames
            m_Reader = RecordingReader{};
            m_State = RecorderState::Idle;
            return false;
        }
    }

    return true;
}

void RecorderEngine::OnLevelEnd() {
    if (m_State == RecorderState::Recording) {
        StopRecording();
    } else if (m_State == RecorderState::Playing || m_State == RecorderState::Paused) {
        StopPlayback();
    }
}

bool RecorderEngine::PreloadNextFrame() {
    m_HasPendingFrame = m_Reader.ReadNextFrame(m_PendingFrame);
    return m_HasPendingFrame;
}

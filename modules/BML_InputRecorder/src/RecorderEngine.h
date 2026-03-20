#ifndef BML_INPUT_RECORDER_ENGINE_H
#define BML_INPUT_RECORDER_ENGINE_H

#include "RecordingFormat.h"

#include <string>

enum class RecorderState : uint8_t {
    Idle,
    Recording,
    Playing,
    Paused
};

class RecorderEngine {
public:
    RecorderState State() const { return m_State; }

    bool StartRecording(const std::string &path, uint32_t levelId);
    bool StopRecording();

    bool StartPlayback(const std::string &path);
    bool StopPlayback();
    bool PausePlayback();
    bool ResumePlayback();

    void AccumulateEvent(const EventRecord &event);

    // Returns false when playback has ended (EOF reached).
    // out_events receives the events to inject this frame (during playback).
    bool OnPostProcess(float deltaTime, std::vector<EventRecord> *out_events);

    void OnLevelEnd();

    uint32_t FrameCount() const { return m_FrameCounter; }
    const std::string &FilePath() const { return m_FilePath; }

private:
    bool PreloadNextFrame();

    RecorderState m_State = RecorderState::Idle;
    RecordingWriter m_Writer;
    RecordingReader m_Reader;
    std::vector<EventRecord> m_FrameEvents;
    uint32_t m_FrameCounter = 0;
    uint32_t m_CurrentLevelId = 0;
    std::string m_FilePath;

    FrameData m_PendingFrame;
    bool m_HasPendingFrame = false;
};

#endif // BML_INPUT_RECORDER_ENGINE_H

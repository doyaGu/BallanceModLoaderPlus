#include <gtest/gtest.h>

#include "RecordingFormat.h"
#include "RecorderEngine.h"

#include <cstring>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace {

class TempDir {
public:
    TempDir() {
        m_Path = fs::temp_directory_path() / "bml_input_recorder_test";
        fs::create_directories(m_Path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(m_Path, ec);
    }
    std::string Path() const { return m_Path.string(); }
    std::string File(const char *name) const { return (m_Path / name).string(); }
private:
    fs::path m_Path;
};

EventRecord MakeKeyDown(uint32_t key, uint32_t scan = 0, uint32_t ts = 0, bool repeat = false) {
    EventRecord r{};
    r.type = EVENT_KEY_DOWN;
    r.key_down.key_code = key;
    r.key_down.scan_code = scan;
    r.key_down.timestamp = ts;
    r.key_down.repeat = repeat ? 1 : 0;
    return r;
}

EventRecord MakeKeyUp(uint32_t key, uint32_t scan = 0, uint32_t ts = 0) {
    EventRecord r{};
    r.type = EVENT_KEY_UP;
    r.key_up.key_code = key;
    r.key_up.scan_code = scan;
    r.key_up.timestamp = ts;
    return r;
}

EventRecord MakeMouseBtn(uint32_t button, bool down, uint32_t ts = 0) {
    EventRecord r{};
    r.type = EVENT_MOUSE_BTN;
    r.mouse_btn.button = button;
    r.mouse_btn.down = down ? 1 : 0;
    r.mouse_btn.timestamp = ts;
    return r;
}

EventRecord MakeMouseMove(float x, float y, float rx, float ry, bool abs = false) {
    EventRecord r{};
    r.type = EVENT_MOUSE_MOVE;
    r.mouse_move.x = x;
    r.mouse_move.y = y;
    r.mouse_move.rel_x = rx;
    r.mouse_move.rel_y = ry;
    r.mouse_move.absolute = abs ? 1 : 0;
    return r;
}

} // namespace

// ============================================================================
// RecordingFormat Tests
// ============================================================================

TEST(RecordingFormat, WriteAndReadHeader) {
    TempDir tmp;
    std::string path = tmp.File("test.bmlr");

    {
        RecordingWriter writer;
        ASSERT_TRUE(writer.Open(path, 42));
        ASSERT_TRUE(writer.Finalize());
    }

    RecordingReader reader;
    ASSERT_TRUE(reader.Open(path));

    const auto &h = reader.Header();
    EXPECT_EQ(std::memcmp(h.magic, "BMLR", 4), 0);
    EXPECT_EQ(h.format_version, 1u);
    EXPECT_EQ(h.level_id, 42u);
    EXPECT_EQ(h.total_frames, 0u);
}

TEST(RecordingFormat, WriteAndReadFrames) {
    TempDir tmp;
    std::string path = tmp.File("frames.bmlr");

    std::vector<EventRecord> f1_events = {MakeKeyDown(0x41, 30, 100), MakeKeyUp(0x41, 30, 200)};
    std::vector<EventRecord> f2_events = {MakeMouseBtn(0, true, 300)};
    std::vector<EventRecord> f3_events = {MakeMouseMove(1.0f, 2.0f, 0.5f, -0.5f, true)};

    {
        RecordingWriter writer;
        ASSERT_TRUE(writer.Open(path, 0));
        ASSERT_TRUE(writer.WriteFrame(0, 0.016f, f1_events));
        ASSERT_TRUE(writer.WriteFrame(1, 0.017f, f2_events));
        ASSERT_TRUE(writer.WriteFrame(2, 0.016f, f3_events));
        ASSERT_TRUE(writer.Finalize());
    }

    RecordingReader reader;
    ASSERT_TRUE(reader.Open(path));
    EXPECT_EQ(reader.Header().total_frames, 3u);

    FrameData frame;

    // Frame 0
    ASSERT_TRUE(reader.ReadNextFrame(frame));
    EXPECT_EQ(frame.frame_number, 0u);
    EXPECT_NEAR(frame.delta_time, 0.016f, 1e-5f);
    ASSERT_EQ(frame.events.size(), 2u);
    EXPECT_EQ(frame.events[0].type, EVENT_KEY_DOWN);
    EXPECT_EQ(frame.events[0].key_down.key_code, 0x41u);
    EXPECT_EQ(frame.events[0].key_down.timestamp, 100u);
    EXPECT_EQ(frame.events[1].type, EVENT_KEY_UP);
    EXPECT_EQ(frame.events[1].key_up.key_code, 0x41u);

    // Frame 1
    ASSERT_TRUE(reader.ReadNextFrame(frame));
    EXPECT_EQ(frame.frame_number, 1u);
    ASSERT_EQ(frame.events.size(), 1u);
    EXPECT_EQ(frame.events[0].type, EVENT_MOUSE_BTN);
    EXPECT_EQ(frame.events[0].mouse_btn.button, 0u);
    EXPECT_EQ(frame.events[0].mouse_btn.down, 1);

    // Frame 2
    ASSERT_TRUE(reader.ReadNextFrame(frame));
    EXPECT_EQ(frame.frame_number, 2u);
    ASSERT_EQ(frame.events.size(), 1u);
    EXPECT_EQ(frame.events[0].type, EVENT_MOUSE_MOVE);
    EXPECT_NEAR(frame.events[0].mouse_move.x, 1.0f, 1e-5f);
    EXPECT_NEAR(frame.events[0].mouse_move.y, 2.0f, 1e-5f);
    EXPECT_EQ(frame.events[0].mouse_move.absolute, 1);

    // EOF
    EXPECT_FALSE(reader.ReadNextFrame(frame));
}

TEST(RecordingFormat, InvalidMagic) {
    TempDir tmp;
    std::string path = tmp.File("bad.bmlr");

    // Write garbage
    {
        std::ofstream f(path, std::ios::binary);
        const char garbage[32] = "NOT_A_RECORDING_FILE_AT_ALL!!";
        f.write(garbage, sizeof(garbage));
    }

    RecordingReader reader;
    EXPECT_FALSE(reader.Open(path));
}

TEST(RecordingFormat, EmptyFrames) {
    TempDir tmp;
    std::string path = tmp.File("empty.bmlr");

    {
        RecordingWriter writer;
        ASSERT_TRUE(writer.Open(path, 0));
        ASSERT_TRUE(writer.WriteFrame(0, 0.016f, {}));
        ASSERT_TRUE(writer.WriteFrame(1, 0.017f, {}));
        ASSERT_TRUE(writer.Finalize());
    }

    RecordingReader reader;
    ASSERT_TRUE(reader.Open(path));
    EXPECT_EQ(reader.Header().total_frames, 2u);

    FrameData frame;
    ASSERT_TRUE(reader.ReadNextFrame(frame));
    EXPECT_EQ(frame.events.size(), 0u);
    ASSERT_TRUE(reader.ReadNextFrame(frame));
    EXPECT_EQ(frame.events.size(), 0u);
    EXPECT_FALSE(reader.ReadNextFrame(frame));
}

TEST(RecordingFormat, FinalizeUpdatesFrameCount) {
    TempDir tmp;
    std::string path = tmp.File("finalize.bmlr");

    {
        RecordingWriter writer;
        ASSERT_TRUE(writer.Open(path, 0));
        writer.WriteFrame(0, 0.016f, {MakeKeyDown(1)});
        writer.WriteFrame(1, 0.016f, {MakeKeyDown(2)});
        writer.WriteFrame(2, 0.016f, {MakeKeyDown(3)});
        EXPECT_EQ(writer.FrameCount(), 3u);
        writer.Finalize();
    }

    RecordingReader reader;
    ASSERT_TRUE(reader.Open(path));
    EXPECT_EQ(reader.Header().total_frames, 3u);
}

TEST(RecordingFormat, ReaderReset) {
    TempDir tmp;
    std::string path = tmp.File("reset.bmlr");

    {
        RecordingWriter writer;
        ASSERT_TRUE(writer.Open(path, 0));
        writer.WriteFrame(0, 0.016f, {MakeKeyDown(0x41)});
        writer.WriteFrame(1, 0.017f, {MakeKeyUp(0x41)});
        writer.Finalize();
    }

    RecordingReader reader;
    ASSERT_TRUE(reader.Open(path));

    FrameData frame;
    ASSERT_TRUE(reader.ReadNextFrame(frame));
    ASSERT_TRUE(reader.ReadNextFrame(frame));
    EXPECT_FALSE(reader.ReadNextFrame(frame));

    // Reset and read again
    reader.Reset();
    ASSERT_TRUE(reader.ReadNextFrame(frame));
    EXPECT_EQ(frame.frame_number, 0u);
    EXPECT_EQ(frame.events[0].key_down.key_code, 0x41u);
}

// ============================================================================
// RecorderEngine State Machine Tests
// ============================================================================

TEST(RecorderEngine, RecordingTransitions) {
    TempDir tmp;
    RecorderEngine engine;

    EXPECT_EQ(engine.State(), RecorderState::Idle);

    // Can't stop when idle
    EXPECT_FALSE(engine.StopRecording());

    // Can't play when idle (without valid file)
    EXPECT_FALSE(engine.StartPlayback(tmp.File("nonexistent.bmlr")));

    // Start recording
    ASSERT_TRUE(engine.StartRecording(tmp.File("rec.bmlr"), 0));
    EXPECT_EQ(engine.State(), RecorderState::Recording);

    // Can't start recording again
    EXPECT_FALSE(engine.StartRecording(tmp.File("rec2.bmlr"), 0));

    // Can't start playback while recording
    EXPECT_FALSE(engine.StartPlayback(tmp.File("rec.bmlr")));

    // Stop recording
    ASSERT_TRUE(engine.StopRecording());
    EXPECT_EQ(engine.State(), RecorderState::Idle);
}

TEST(RecorderEngine, PlaybackTransitions) {
    TempDir tmp;
    std::string path = tmp.File("play.bmlr");

    // Create a recording first
    {
        RecorderEngine rec;
        ASSERT_TRUE(rec.StartRecording(path, 0));
        rec.AccumulateEvent(MakeKeyDown(0x41));
        rec.OnPostProcess(0.016f, nullptr);
        rec.AccumulateEvent(MakeKeyUp(0x41));
        rec.OnPostProcess(0.016f, nullptr);
        rec.StopRecording();
    }

    RecorderEngine engine;

    // Start playback
    ASSERT_TRUE(engine.StartPlayback(path));
    EXPECT_EQ(engine.State(), RecorderState::Playing);

    // Can't record while playing
    EXPECT_FALSE(engine.StartRecording(tmp.File("rec.bmlr"), 0));

    // Pause
    ASSERT_TRUE(engine.PausePlayback());
    EXPECT_EQ(engine.State(), RecorderState::Paused);

    // Can't pause again
    EXPECT_FALSE(engine.PausePlayback());

    // Resume
    ASSERT_TRUE(engine.ResumePlayback());
    EXPECT_EQ(engine.State(), RecorderState::Playing);

    // Stop
    ASSERT_TRUE(engine.StopPlayback());
    EXPECT_EQ(engine.State(), RecorderState::Idle);
}

TEST(RecorderEngine, AccumulateAndFlush) {
    TempDir tmp;
    std::string path = tmp.File("accum.bmlr");

    RecorderEngine engine;
    ASSERT_TRUE(engine.StartRecording(path, 5));

    engine.AccumulateEvent(MakeKeyDown(0x41, 30, 100));
    engine.AccumulateEvent(MakeKeyDown(0x42, 48, 110));
    engine.OnPostProcess(0.016f, nullptr);

    engine.AccumulateEvent(MakeMouseMove(10.0f, 20.0f, 1.0f, -1.0f));
    engine.OnPostProcess(0.017f, nullptr);

    EXPECT_EQ(engine.FrameCount(), 2u);
    engine.StopRecording();

    // Verify the file
    RecordingReader reader;
    ASSERT_TRUE(reader.Open(path));
    EXPECT_EQ(reader.Header().level_id, 5u);
    EXPECT_EQ(reader.Header().total_frames, 2u);

    FrameData frame;
    ASSERT_TRUE(reader.ReadNextFrame(frame));
    EXPECT_EQ(frame.events.size(), 2u);
    EXPECT_EQ(frame.events[0].key_down.key_code, 0x41u);
    EXPECT_EQ(frame.events[1].key_down.key_code, 0x42u);

    ASSERT_TRUE(reader.ReadNextFrame(frame));
    EXPECT_EQ(frame.events.size(), 1u);
    EXPECT_EQ(frame.events[0].type, EVENT_MOUSE_MOVE);
}

TEST(RecorderEngine, PlaybackInjectsEvents) {
    TempDir tmp;
    std::string path = tmp.File("inject.bmlr");

    // Record
    {
        RecorderEngine rec;
        ASSERT_TRUE(rec.StartRecording(path, 0));
        rec.AccumulateEvent(MakeKeyDown(0x41));
        rec.AccumulateEvent(MakeKeyUp(0x41));
        rec.OnPostProcess(0.016f, nullptr);
        rec.AccumulateEvent(MakeMouseBtn(0, true));
        rec.OnPostProcess(0.017f, nullptr);
        rec.StopRecording();
    }

    // Play back
    RecorderEngine engine;
    ASSERT_TRUE(engine.StartPlayback(path));

    std::vector<EventRecord> injected;

    // Frame 0
    EXPECT_TRUE(engine.OnPostProcess(0.016f, &injected));
    ASSERT_EQ(injected.size(), 2u);
    EXPECT_EQ(injected[0].type, EVENT_KEY_DOWN);
    EXPECT_EQ(injected[0].key_down.key_code, 0x41u);
    EXPECT_EQ(injected[1].type, EVENT_KEY_UP);

    // Frame 1 (last frame -- should return true this tick, then false next)
    EXPECT_TRUE(engine.OnPostProcess(0.017f, &injected));
    ASSERT_EQ(injected.size(), 1u);
    EXPECT_EQ(injected[0].type, EVENT_MOUSE_BTN);

    // EOF -- transitions to Idle
    EXPECT_FALSE(engine.OnPostProcess(0.016f, &injected));
    EXPECT_EQ(engine.State(), RecorderState::Idle);
    EXPECT_TRUE(injected.empty());
}

TEST(RecorderEngine, LevelEndStopsRecording) {
    TempDir tmp;
    RecorderEngine engine;
    ASSERT_TRUE(engine.StartRecording(tmp.File("levelend.bmlr"), 0));
    EXPECT_EQ(engine.State(), RecorderState::Recording);

    engine.OnLevelEnd();
    EXPECT_EQ(engine.State(), RecorderState::Idle);
}

TEST(RecorderEngine, LevelEndStopsPlayback) {
    TempDir tmp;
    std::string path = tmp.File("levelend_play.bmlr");

    {
        RecorderEngine rec;
        ASSERT_TRUE(rec.StartRecording(path, 0));
        rec.AccumulateEvent(MakeKeyDown(1));
        rec.OnPostProcess(0.016f, nullptr);
        rec.StopRecording();
    }

    RecorderEngine engine;
    ASSERT_TRUE(engine.StartPlayback(path));
    EXPECT_EQ(engine.State(), RecorderState::Playing);

    engine.OnLevelEnd();
    EXPECT_EQ(engine.State(), RecorderState::Idle);
}

TEST(RecorderEngine, AccumulateIgnoredWhenNotRecording) {
    RecorderEngine engine;
    // Should not crash or change state
    engine.AccumulateEvent(MakeKeyDown(0x41));
    EXPECT_EQ(engine.State(), RecorderState::Idle);
}

TEST(RecorderEngine, PausedFrameNotAdvanced) {
    TempDir tmp;
    std::string path = tmp.File("pause.bmlr");

    {
        RecorderEngine rec;
        ASSERT_TRUE(rec.StartRecording(path, 0));
        rec.AccumulateEvent(MakeKeyDown(0x41));
        rec.OnPostProcess(0.016f, nullptr);
        rec.AccumulateEvent(MakeKeyDown(0x42));
        rec.OnPostProcess(0.016f, nullptr);
        rec.StopRecording();
    }

    RecorderEngine engine;
    ASSERT_TRUE(engine.StartPlayback(path));

    std::vector<EventRecord> injected;

    // Get frame 0
    ASSERT_TRUE(engine.OnPostProcess(0.016f, &injected));
    EXPECT_EQ(injected.size(), 1u);
    EXPECT_EQ(engine.FrameCount(), 1u);

    // Pause
    engine.PausePlayback();

    // Tick while paused -- should not advance
    ASSERT_TRUE(engine.OnPostProcess(0.016f, &injected));
    EXPECT_TRUE(injected.empty());
    EXPECT_EQ(engine.FrameCount(), 1u);

    // Resume
    engine.ResumePlayback();

    // Get frame 1
    ASSERT_TRUE(engine.OnPostProcess(0.016f, &injected));
    EXPECT_EQ(injected.size(), 1u);
    EXPECT_EQ(injected[0].key_down.key_code, 0x42u);
}

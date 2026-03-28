#ifndef BML_INPUT_RECORDER_FORMAT_H
#define BML_INPUT_RECORDER_FORMAT_H

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

// ============================================================================
// On-disk binary format structures (packed, little-endian)
// ============================================================================

#pragma pack(push, 1)

struct RecordingHeader {
    char magic[4];          // "BMLR"
    uint32_t format_version;
    uint32_t bml_version;   // (major << 16) | (minor << 8) | patch
    uint32_t level_id;
    uint64_t start_timestamp;
    uint32_t total_frames;  // written on finalize
    uint32_t reserved;
};

static_assert(sizeof(RecordingHeader) == 32, "Header must be 32 bytes");

enum RecordedEventType : uint8_t {
    EVENT_KEY_DOWN   = 0,
    EVENT_KEY_UP     = 1,
    EVENT_MOUSE_BTN  = 2,
    EVENT_MOUSE_MOVE = 3,
};

struct RecordedKeyDown {
    uint32_t key_code;
    uint32_t scan_code;
    uint32_t timestamp;
    uint8_t repeat;
};

struct RecordedKeyUp {
    uint32_t key_code;
    uint32_t scan_code;
    uint32_t timestamp;
};

struct RecordedMouseBtn {
    uint32_t button;
    uint8_t down;
    uint32_t timestamp;
};

struct RecordedMouseMove {
    float x;
    float y;
    float rel_x;
    float rel_y;
    uint8_t absolute;
    uint32_t timestamp;
};

#pragma pack(pop)

// ============================================================================
// In-memory event record (type tag + union payload)
// ============================================================================

struct EventRecord {
    RecordedEventType type;
    union {
        RecordedKeyDown key_down;
        RecordedKeyUp key_up;
        RecordedMouseBtn mouse_btn;
        RecordedMouseMove mouse_move;
    };
};

// ============================================================================
// Decoded frame (used by both reader and state machine)
// ============================================================================

struct FrameData {
    uint32_t frame_number;
    float delta_time;
    std::vector<EventRecord> events;
};

// ============================================================================
// RecordingWriter -- writes header + appends frames, finalizes on close
// ============================================================================

class RecordingWriter {
public:
    RecordingWriter() = default;
    ~RecordingWriter();

    RecordingWriter(const RecordingWriter &) = delete;
    RecordingWriter &operator=(const RecordingWriter &) = delete;
    RecordingWriter(RecordingWriter &&other) noexcept;
    RecordingWriter &operator=(RecordingWriter &&other) noexcept;

    bool Open(const std::string &path, uint32_t levelId);
    bool WriteFrame(uint32_t frameNum, float dt, const std::vector<EventRecord> &events);
    bool Finalize();
    bool IsOpen() const { return m_File.is_open(); }
    uint32_t FrameCount() const { return m_FrameCount; }

private:
    void WriteEvent(const EventRecord &event);

    std::ofstream m_File;
    uint32_t m_FrameCount = 0;
    bool m_Finalized = false;
};

// ============================================================================
// RecordingReader -- reads header + iterates frames
// ============================================================================

class RecordingReader {
public:
    RecordingReader() = default;

    bool Open(const std::string &path);
    const RecordingHeader &Header() const { return m_Header; }
    bool ReadNextFrame(FrameData &out);
    void Reset();
    bool IsOpen() const { return m_File.is_open(); }

private:
    bool ReadEvent(EventRecord &out);

    std::ifstream m_File;
    RecordingHeader m_Header{};
    std::streampos m_DataStart = 0;
};

#endif // BML_INPUT_RECORDER_FORMAT_H

#include "RecordingFormat.h"

#include <algorithm>
#include <cstring>
#include <utility>

static constexpr char kMagic[4] = {'B', 'M', 'L', 'R'};
static constexpr uint32_t kFormatVersion = 1;

// Pack BML version as (major << 16) | (minor << 8) | patch
static uint32_t PackBmlVersion() {
    // 0.4.0
    return (0u << 16) | (4u << 8) | 0u;
}

// ============================================================================
// RecordingWriter
// ============================================================================

RecordingWriter::~RecordingWriter() {
    if (IsOpen() && !m_Finalized) {
        Finalize();
    }
}

RecordingWriter::RecordingWriter(RecordingWriter &&other) noexcept
    : m_File(std::move(other.m_File)),
      m_FrameCount(other.m_FrameCount),
      m_Finalized(other.m_Finalized) {
    other.m_FrameCount = 0;
    other.m_Finalized = true;
}

RecordingWriter &RecordingWriter::operator=(RecordingWriter &&other) noexcept {
    if (this != &other) {
        if (IsOpen() && !m_Finalized) {
            Finalize();
        }
        m_File = std::move(other.m_File);
        m_FrameCount = other.m_FrameCount;
        m_Finalized = other.m_Finalized;
        other.m_FrameCount = 0;
        other.m_Finalized = true;
    }
    return *this;
}

bool RecordingWriter::Open(const std::string &path, uint32_t levelId) {
    if (IsOpen()) return false;

    m_File.open(path, std::ios::binary | std::ios::trunc);
    if (!m_File.is_open()) return false;

    RecordingHeader header{};
    std::memcpy(header.magic, kMagic, 4);
    header.format_version = kFormatVersion;
    header.bml_version = PackBmlVersion();
    header.level_id = levelId;
    header.start_timestamp = 0; // filled by caller if needed
    header.total_frames = 0;
    header.reserved = 0;

    m_File.write(reinterpret_cast<const char *>(&header), sizeof(header));
    if (!m_File.good()) {
        m_File.close();
        return false;
    }

    m_FrameCount = 0;
    m_Finalized = false;
    return true;
}

bool RecordingWriter::WriteFrame(uint32_t frameNum, float dt, const std::vector<EventRecord> &events) {
    if (!IsOpen() || m_Finalized) return false;

    m_File.write(reinterpret_cast<const char *>(&frameNum), sizeof(frameNum));
    m_File.write(reinterpret_cast<const char *>(&dt), sizeof(dt));

    uint16_t eventCount = static_cast<uint16_t>(std::min(events.size(), size_t{UINT16_MAX}));
    m_File.write(reinterpret_cast<const char *>(&eventCount), sizeof(eventCount));

    for (uint16_t i = 0; i < eventCount; ++i) {
        WriteEvent(events[i]);
    }

    ++m_FrameCount;
    return m_File.good();
}

void RecordingWriter::WriteEvent(const EventRecord &event) {
    m_File.write(reinterpret_cast<const char *>(&event.type), sizeof(event.type));

    switch (event.type) {
    case EVENT_KEY_DOWN:
        m_File.write(reinterpret_cast<const char *>(&event.key_down), sizeof(RecordedKeyDown));
        break;
    case EVENT_KEY_UP:
        m_File.write(reinterpret_cast<const char *>(&event.key_up), sizeof(RecordedKeyUp));
        break;
    case EVENT_MOUSE_BTN:
        m_File.write(reinterpret_cast<const char *>(&event.mouse_btn), sizeof(RecordedMouseBtn));
        break;
    case EVENT_MOUSE_MOVE:
        m_File.write(reinterpret_cast<const char *>(&event.mouse_move), sizeof(RecordedMouseMove));
        break;
    }
}

bool RecordingWriter::Finalize() {
    if (!IsOpen() || m_Finalized) return false;

    // Seek back to total_frames field in header (offset 24)
    m_File.seekp(offsetof(RecordingHeader, total_frames));
    m_File.write(reinterpret_cast<const char *>(&m_FrameCount), sizeof(m_FrameCount));
    m_File.close();
    m_Finalized = true;
    return true;
}

// ============================================================================
// RecordingReader
// ============================================================================

bool RecordingReader::Open(const std::string &path) {
    m_File.open(path, std::ios::binary);
    if (!m_File.is_open()) return false;

    m_File.read(reinterpret_cast<char *>(&m_Header), sizeof(m_Header));
    if (!m_File.good()) {
        m_File.close();
        return false;
    }

    if (std::memcmp(m_Header.magic, kMagic, 4) != 0) {
        m_File.close();
        return false;
    }

    if (m_Header.format_version != kFormatVersion) {
        m_File.close();
        return false;
    }

    m_DataStart = m_File.tellg();
    return true;
}

bool RecordingReader::ReadNextFrame(FrameData &out) {
    if (!IsOpen()) return false;

    uint32_t frameNum = 0;
    float dt = 0.0f;
    uint16_t eventCount = 0;

    m_File.read(reinterpret_cast<char *>(&frameNum), sizeof(frameNum));
    if (!m_File.good()) return false;

    m_File.read(reinterpret_cast<char *>(&dt), sizeof(dt));
    m_File.read(reinterpret_cast<char *>(&eventCount), sizeof(eventCount));
    if (!m_File.good()) return false;

    out.frame_number = frameNum;
    out.delta_time = dt;
    out.events.clear();
    out.events.reserve(eventCount);

    for (uint16_t i = 0; i < eventCount; ++i) {
        EventRecord event{};
        if (!ReadEvent(event)) return false;
        out.events.push_back(event);
    }

    return true;
}

bool RecordingReader::ReadEvent(EventRecord &out) {
    m_File.read(reinterpret_cast<char *>(&out.type), sizeof(out.type));
    if (!m_File.good()) return false;

    switch (out.type) {
    case EVENT_KEY_DOWN:
        m_File.read(reinterpret_cast<char *>(&out.key_down), sizeof(RecordedKeyDown));
        break;
    case EVENT_KEY_UP:
        m_File.read(reinterpret_cast<char *>(&out.key_up), sizeof(RecordedKeyUp));
        break;
    case EVENT_MOUSE_BTN:
        m_File.read(reinterpret_cast<char *>(&out.mouse_btn), sizeof(RecordedMouseBtn));
        break;
    case EVENT_MOUSE_MOVE:
        m_File.read(reinterpret_cast<char *>(&out.mouse_move), sizeof(RecordedMouseMove));
        break;
    default:
        return false;
    }

    return m_File.good();
}

void RecordingReader::Reset() {
    if (IsOpen()) {
        m_File.clear();
        m_File.seekg(m_DataStart);
    }
}

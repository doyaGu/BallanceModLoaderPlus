#ifndef BML_UI_AUTOMATION_SESSION_H
#define BML_UI_AUTOMATION_SESSION_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace UiAutomationSession {

inline constexpr std::string_view Format = "bml-ui-session-v1";

enum class CheckpointKind {
    Input,
    Capture,
};

struct Checkpoint {
    std::uint32_t Sequence = 0;
    CheckpointKind Kind = CheckpointKind::Input;
    std::string Name;
};

enum class AcknowledgementState {
    Waiting,
    Succeeded,
    Failed,
    ProtocolError,
};

struct Acknowledgement {
    AcknowledgementState State = AcknowledgementState::Waiting;
    std::uint32_t Sequence = 0;
    std::string Reason;
};

class PlayerEndpoint {
  public:
    explicit PlayerEndpoint(std::filesystem::path directory);

    bool Publish(CheckpointKind kind, std::string_view name);
    Acknowledgement PollAcknowledgement();

    bool HasPendingCheckpoint() const;
    std::uint32_t PendingSequence() const;
    const std::string &LastError() const;

  private:
    std::filesystem::path m_Directory;
    std::uint32_t m_NextSequence = 1;
    std::uint32_t m_PendingSequence = 0;
    std::string m_LastError;
};

class RunnerEndpoint {
  public:
    explicit RunnerEndpoint(std::filesystem::path directory);

    std::optional<Checkpoint> ReceiveNext();
    bool Acknowledge(const Checkpoint &checkpoint, bool succeeded, std::string_view reason = {});

    std::uint32_t NextSequence() const;
    const std::string &LastError() const;

  private:
    std::filesystem::path m_Directory;
    std::uint32_t m_NextSequence = 1;
    std::string m_LastError;
};

const char *ToString(CheckpointKind kind);

} // namespace UiAutomationSession

#endif // BML_UI_AUTOMATION_SESSION_H

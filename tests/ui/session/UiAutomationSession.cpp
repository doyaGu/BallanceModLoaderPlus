#include "UiAutomationSession.h"

#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace UiAutomationSession {
namespace {

using Properties = std::map<std::string, std::string>;

fs::path MessagePath(const fs::path &directory, const char *prefix, std::uint32_t sequence) {
    std::ostringstream name;
    name << prefix << '-' << std::setfill('0') << std::setw(6) << sequence << ".msg";
    return directory / name.str();
}

bool IsToken(std::string_view value) {
    if (value.empty() || value.front() == '-' || value.back() == '-')
        return false;
    for (char character : value) {
        const bool alpha = character >= 'a' && character <= 'z';
        const bool digit = character >= '0' && character <= '9';
        if (!alpha && !digit && character != '-')
            return false;
    }
    return true;
}

bool WriteAtomically(const fs::path &path, const std::string &content, std::string &error) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec) {
        error = "cannot-create-session-directory";
        return false;
    }
    if (fs::exists(path, ec)) {
        error = "message-already-exists";
        return false;
    }

    fs::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            error = "cannot-open-temporary-message";
            return false;
        }
        output << content;
        if (!output.good()) {
            error = "cannot-write-temporary-message";
            return false;
        }
    }
    fs::rename(temporary, path, ec);
    if (ec) {
        fs::remove(temporary);
        error = "cannot-publish-message";
        return false;
    }
    return true;
}

std::optional<Properties> ReadProperties(const fs::path &path, std::string &error) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return std::nullopt;

    Properties properties;
    std::string line;
    while (std::getline(input, line)) {
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos || separator == 0 || separator + 1 >= line.size()) {
            error = "invalid-message-line";
            return Properties{};
        }
        std::string key = line.substr(0, separator);
        std::string value = line.substr(separator + 1);
        if (!properties.emplace(std::move(key), std::move(value)).second) {
            error = "duplicate-message-key";
            return Properties{};
        }
    }
    if (!input.eof()) {
        error = "cannot-read-message";
        return Properties{};
    }
    return properties;
}

bool HasOnlyKeys(const Properties &properties, const std::set<std::string> &allowed,
                 std::string &error) {
    for (const auto &[key, value] : properties) {
        (void)value;
        if (!allowed.contains(key)) {
            error = "unknown-message-key";
            return false;
        }
    }
    return true;
}

const std::string *Find(const Properties &properties, const char *key) {
    const auto found = properties.find(key);
    return found == properties.end() ? nullptr : &found->second;
}

bool ParseSequence(const std::string &value, std::uint32_t &sequence) {
    try {
        std::size_t consumed = 0;
        const unsigned long parsed = std::stoul(value, &consumed);
        if (consumed != value.size() || parsed == 0 ||
            parsed > std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }
        sequence = static_cast<std::uint32_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

std::optional<Checkpoint> ParseCheckpoint(const Properties &properties, std::string &error) {
    if (!HasOnlyKeys(properties, {"format", "sequence", "kind", "name"}, error))
        return std::nullopt;
    const std::string *format = Find(properties, "format");
    const std::string *sequenceText = Find(properties, "sequence");
    const std::string *kindText = Find(properties, "kind");
    const std::string *name = Find(properties, "name");
    if (!format || !sequenceText || !kindText || !name || *format != Format) {
        error = "invalid-checkpoint-fields";
        return std::nullopt;
    }

    Checkpoint checkpoint;
    if (!ParseSequence(*sequenceText, checkpoint.Sequence) || !IsToken(*name)) {
        error = "invalid-checkpoint-value";
        return std::nullopt;
    }
    if (*kindText == "input")
        checkpoint.Kind = CheckpointKind::Input;
    else if (*kindText == "capture")
        checkpoint.Kind = CheckpointKind::Capture;
    else {
        error = "invalid-checkpoint-kind";
        return std::nullopt;
    }
    checkpoint.Name = *name;
    return checkpoint;
}

Acknowledgement ParseAcknowledgement(const Properties &properties, std::uint32_t expected,
                                     std::string &error) {
    Acknowledgement acknowledgement;
    acknowledgement.State = AcknowledgementState::ProtocolError;
    if (!HasOnlyKeys(properties, {"format", "sequence", "status", "reason"}, error))
        return acknowledgement;
    const std::string *format = Find(properties, "format");
    const std::string *sequenceText = Find(properties, "sequence");
    const std::string *status = Find(properties, "status");
    if (!format || !sequenceText || !status || *format != Format ||
        !ParseSequence(*sequenceText, acknowledgement.Sequence) ||
        acknowledgement.Sequence != expected) {
        error = "invalid-acknowledgement-fields";
        return acknowledgement;
    }

    const std::string *reason = Find(properties, "reason");
    if (*status == "passed" && !reason) {
        acknowledgement.State = AcknowledgementState::Succeeded;
        return acknowledgement;
    }
    if (*status == "failed" && reason && IsToken(*reason)) {
        acknowledgement.State = AcknowledgementState::Failed;
        acknowledgement.Reason = *reason;
        return acknowledgement;
    }
    error = "invalid-acknowledgement-status";
    return acknowledgement;
}

} // namespace

PlayerEndpoint::PlayerEndpoint(fs::path directory) : m_Directory(std::move(directory)) {}

bool PlayerEndpoint::Publish(CheckpointKind kind, std::string_view name) {
    m_LastError.clear();
    if (m_PendingSequence != 0) {
        m_LastError = "checkpoint-already-pending";
        return false;
    }
    if (!IsToken(name)) {
        m_LastError = "invalid-checkpoint-name";
        return false;
    }

    std::ostringstream content;
    content << "format=" << Format << '\n'
            << "sequence=" << m_NextSequence << '\n'
            << "kind=" << ToString(kind) << '\n'
            << "name=" << name << '\n';
    if (!WriteAtomically(MessagePath(m_Directory, "checkpoint", m_NextSequence), content.str(),
                         m_LastError)) {
        return false;
    }
    m_PendingSequence = m_NextSequence++;
    return true;
}

Acknowledgement PlayerEndpoint::PollAcknowledgement() {
    if (m_PendingSequence == 0) {
        m_LastError = "no-checkpoint-pending";
        return {AcknowledgementState::ProtocolError, 0, m_LastError};
    }

    m_LastError.clear();
    const auto properties =
        ReadProperties(MessagePath(m_Directory, "ack", m_PendingSequence), m_LastError);
    if (!properties)
        return {AcknowledgementState::Waiting, m_PendingSequence, {}};
    if (!m_LastError.empty())
        return {AcknowledgementState::ProtocolError, m_PendingSequence, m_LastError};

    Acknowledgement acknowledgement =
        ParseAcknowledgement(*properties, m_PendingSequence, m_LastError);
    if (acknowledgement.State != AcknowledgementState::Waiting)
        m_PendingSequence = 0;
    if (acknowledgement.State == AcknowledgementState::ProtocolError)
        acknowledgement.Reason = m_LastError;
    return acknowledgement;
}

bool PlayerEndpoint::HasPendingCheckpoint() const { return m_PendingSequence != 0; }

std::uint32_t PlayerEndpoint::PendingSequence() const { return m_PendingSequence; }

const std::string &PlayerEndpoint::LastError() const { return m_LastError; }

RunnerEndpoint::RunnerEndpoint(fs::path directory) : m_Directory(std::move(directory)) {}

std::optional<Checkpoint> RunnerEndpoint::ReceiveNext() {
    m_LastError.clear();
    const auto properties =
        ReadProperties(MessagePath(m_Directory, "checkpoint", m_NextSequence), m_LastError);
    if (!properties)
        return std::nullopt;
    if (!m_LastError.empty())
        return std::nullopt;
    std::optional<Checkpoint> checkpoint = ParseCheckpoint(*properties, m_LastError);
    if (checkpoint && checkpoint->Sequence != m_NextSequence) {
        m_LastError = "unexpected-checkpoint-sequence";
        return std::nullopt;
    }
    return checkpoint;
}

bool RunnerEndpoint::Acknowledge(const Checkpoint &checkpoint, bool succeeded,
                                 std::string_view reason) {
    m_LastError.clear();
    if (checkpoint.Sequence != m_NextSequence) {
        m_LastError = "unexpected-checkpoint-sequence";
        return false;
    }
    if ((!succeeded && !IsToken(reason)) || (succeeded && !reason.empty())) {
        m_LastError = "invalid-acknowledgement-reason";
        return false;
    }

    std::ostringstream content;
    content << "format=" << Format << '\n'
            << "sequence=" << checkpoint.Sequence << '\n'
            << "status=" << (succeeded ? "passed" : "failed") << '\n';
    if (!succeeded)
        content << "reason=" << reason << '\n';
    if (!WriteAtomically(MessagePath(m_Directory, "ack", checkpoint.Sequence), content.str(),
                         m_LastError)) {
        return false;
    }
    ++m_NextSequence;
    return true;
}

std::uint32_t RunnerEndpoint::NextSequence() const { return m_NextSequence; }

const std::string &RunnerEndpoint::LastError() const { return m_LastError; }

const char *ToString(CheckpointKind kind) {
    switch (kind) {
    case CheckpointKind::Input:
        return "input";
    case CheckpointKind::Capture:
        return "capture";
    }
    return "unknown";
}

} // namespace UiAutomationSession

#include "Behavior/FrameStore.h"

#include <algorithm>
#include <utility>

namespace BML::Behavior {
namespace {

ExecutionFault QueueFullFault(int nativeCode) {
    return {ExecutionError::FrameQueueFull, nativeCode,
            "Behavior frame retention is full; execution was stopped."};
}

} // namespace

FrameStore::FrameStore(FrameRetention retention)
    : m_Retention(retention) {}

FrameAppendResult FrameStore::Retain(RunFrame frame) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_Retention.Kind == RetentionKind::Latest) {
        if (frame.Terminal)
            StoreTerminal(std::move(frame));
        else if (frame.Fault)
            m_LastError = std::move(frame);
        else
            m_Latest = std::move(frame);
        return {};
    }

    if (m_Retention.Kind == RetentionKind::Ignore) {
        if (frame.Terminal)
            StoreTerminal(std::move(frame));
        else if (frame.Fault)
            m_LastError = std::move(frame);
        return {};
    }

    if (!ShouldRetain(frame))
        return {};
    if (m_Frames.size() < m_Retention.Capacity) {
        m_Frames.push_back(std::move(frame));
        return {};
    }

    RunFrame terminal = frame;
    terminal.Terminal = true;
    terminal.NativeContinuation = false;
    terminal.QueuedInput = false;
    terminal.Overflow = FrameOverflow{1, m_Retention.Kind,
                                        m_Retention.Capacity,
                                        terminal.Fault};
    terminal.Fault = QueueFullFault(frame.ReturnCode);
    const ExecutionFault fault = terminal.Fault;
    StoreTerminal(std::move(terminal));
    return {true, fault};
}

std::vector<RunFrame> FrameStore::Read() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return ReadLocked();
}

bool FrameStore::Consume(std::span<const std::uint64_t> sequences) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::vector<RunFrame> current = ReadLocked();
    if (current.size() != sequences.size())
        return false;
    for (std::size_t index = 0; index < current.size(); ++index) {
        if (current[index].Sequence != sequences[index])
            return false;
    }
    for (std::uint64_t sequence : sequences) {
        if (!EraseSequence(sequence))
            return false;
    }
    return true;
}

std::vector<RunFrame> FrameStore::Take() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::vector<RunFrame> drained = ReadLocked();
    m_Frames.clear();
    m_Latest.reset();
    m_LastError.reset();
    m_TerminalFrame.reset();
    return drained;
}

bool FrameStore::ShouldRetain(const RunFrame &frame) const noexcept {
    switch (m_Retention.Kind) {
    case RetentionKind::Signals:
        return frame.Sequence == 1 || !frame.ActiveOutputs.empty() ||
               frame.Terminal || static_cast<bool>(frame.Fault);
    case RetentionKind::EachFrame:
        return true;
    case RetentionKind::Latest:
    case RetentionKind::Ignore:
        return false;
    }
    return false;
}

void FrameStore::StoreTerminal(RunFrame frame) {
    m_TerminalFrame = std::move(frame);
}

std::vector<RunFrame> FrameStore::ReadLocked() const {
    std::vector<RunFrame> frames;
    frames.reserve(m_Frames.size() + (m_Latest ? 1u : 0u) +
                     (m_LastError ? 1u : 0u) +
                     (m_TerminalFrame ? 1u : 0u));
    frames.insert(frames.end(), m_Frames.begin(), m_Frames.end());
    if (m_Latest)
        frames.push_back(*m_Latest);
    if (m_LastError)
        frames.push_back(*m_LastError);
    if (m_TerminalFrame)
        frames.push_back(*m_TerminalFrame);
    std::sort(frames.begin(), frames.end(),
              [](const RunFrame &left, const RunFrame &right) {
                  return left.Sequence < right.Sequence;
              });
    frames.erase(
        std::unique(frames.begin(), frames.end(),
                    [](const RunFrame &left,
                       const RunFrame &right) {
                        return left.Sequence == right.Sequence;
                    }),
        frames.end());
    return frames;
}

bool FrameStore::EraseSequence(std::uint64_t sequence) {
    const auto eraseOptional = [sequence](std::optional<RunFrame> &entry) {
        if (entry && entry->Sequence == sequence) {
            entry.reset();
            return true;
        }
        return false;
    };
    auto iterator = std::find_if(
        m_Frames.begin(), m_Frames.end(),
        [sequence](const RunFrame &frame) {
            return frame.Sequence == sequence;
        });
    if (iterator != m_Frames.end()) {
        m_Frames.erase(iterator);
        return true;
    }
    return eraseOptional(m_Latest) || eraseOptional(m_LastError) ||
           eraseOptional(m_TerminalFrame);
}

} // namespace BML::Behavior

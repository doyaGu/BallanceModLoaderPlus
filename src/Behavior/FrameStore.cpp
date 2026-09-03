#include "Behavior/FrameStore.h"

#include <algorithm>
#include <utility>

namespace BML::Behavior {
namespace {

ExecutionFault QueueFullFault(int nativeCode) {
    return {ExecutionError::FrameQueueFull, nativeCode,
            "Behavior frame retention is full; execution was stopped."};
}

bool HasNoContinuation(const RunFrame &frame) noexcept {
    return !frame.NativeContinuation && !frame.QueuedInput;
}

} // namespace

FrameStore::FrameStore(FrameRetention retention)
    : m_Retention(retention) {}

bool FrameStore::KeepsPouts(const RunFrame &frame) const noexcept {
    switch (m_Retention.Kind) {
    case RetentionKind::Latest:
        return true;
    case RetentionKind::Ignore:
        return false;
    case RetentionKind::Signals:
    case RetentionKind::EachFrame:
        return ShouldRetain(frame);
    }
    return false;
}

FrameAppendResult FrameStore::Retain(RunFrame frame) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_Retention.Kind == RetentionKind::Latest) {
        if (HasNoContinuation(frame)) {
            if (frame.Fault && m_LastError &&
                m_LastError->Sequence < frame.Sequence) {
                m_LastError.reset();
            }
            StoreNonContinuing(std::move(frame));
        } else if (frame.Fault) {
            m_LastError = std::move(frame);
        } else {
            m_Latest = std::move(frame);
        }
        return {};
    }

    if (m_Retention.Kind == RetentionKind::Ignore) {
        if (HasNoContinuation(frame)) {
            if (frame.Fault && m_LastError &&
                m_LastError->Sequence < frame.Sequence) {
                m_LastError.reset();
            }
            StoreNonContinuing(std::move(frame));
        } else if (frame.Fault) {
            m_LastError = std::move(frame);
        }
        return {};
    }

    if (!ShouldRetain(frame))
        return {};
    if (m_Frames.size() < m_Retention.Capacity) {
        m_Frames.push_back(std::move(frame));
        return {};
    }

    RunFrame failureFrame = frame;
    failureFrame.NativeContinuation = false;
    failureFrame.QueuedInput = false;
    failureFrame.Overflow = FrameOverflow{1, m_Retention.Kind,
                                         m_Retention.Capacity,
                                         failureFrame.Fault};
    failureFrame.Fault = QueueFullFault(frame.ReturnCode);
    const ExecutionFault fault = failureFrame.Fault;
    const std::optional<FrameOverflow> overflow = failureFrame.Overflow;
    StoreNonContinuing(std::move(failureFrame));
    return {true, fault, overflow};
}

std::vector<RunFrame> FrameStore::Read() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return ReadLocked();
}

bool FrameStore::Consume(std::span<const std::uint64_t> sequences) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!MatchesLocked(sequences))
        return false;
    m_Frames.clear();
    m_Latest.reset();
    m_LastError.reset();
    m_NonContinuing.reset();
    return true;
}

std::vector<RunFrame> FrameStore::Take() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::vector<RunFrame> drained = ReadLocked();
    m_Frames.clear();
    m_Latest.reset();
    m_LastError.reset();
    m_NonContinuing.reset();
    return drained;
}

bool FrameStore::ShouldRetain(const RunFrame &frame) const noexcept {
    switch (m_Retention.Kind) {
    case RetentionKind::Signals:
        return frame.Sequence == 1 || !frame.ActiveOutputs.empty() ||
               HasNoContinuation(frame) || static_cast<bool>(frame.Fault);
    case RetentionKind::EachFrame:
        return true;
    case RetentionKind::Latest:
    case RetentionKind::Ignore:
        return false;
    }
    return false;
}

void FrameStore::StoreNonContinuing(RunFrame frame) {
    // A failed non-continuing Frame is both the last failure and the last
    // completion. Keep one owned copy while it is current; when a later
    // completion displaces it, preserve it in the failure role.
    if (!frame.Fault && m_NonContinuing && m_NonContinuing->Fault &&
        (!m_LastError ||
         m_NonContinuing->Sequence > m_LastError->Sequence)) {
        m_LastError = std::move(*m_NonContinuing);
    }
    m_NonContinuing = std::move(frame);
}

std::vector<RunFrame> FrameStore::ReadLocked() const {
    std::vector<const RunFrame *> visible;
    visible.reserve(m_Frames.size() + (m_Latest ? 1u : 0u) +
                    (m_LastError ? 1u : 0u) +
                    (m_NonContinuing ? 1u : 0u));
    for (const RunFrame &frame : m_Frames)
        visible.push_back(&frame);
    if (m_Latest)
        visible.push_back(&*m_Latest);
    if (m_LastError)
        visible.push_back(&*m_LastError);
    if (m_NonContinuing)
        visible.push_back(&*m_NonContinuing);
    std::sort(visible.begin(), visible.end(),
              [](const RunFrame *left, const RunFrame *right) {
                  return left->Sequence < right->Sequence;
              });
    visible.erase(
        std::unique(visible.begin(), visible.end(),
                    [](const RunFrame *left, const RunFrame *right) {
                        return left->Sequence == right->Sequence;
                    }),
        visible.end());

    std::vector<RunFrame> frames;
    frames.reserve(visible.size());
    for (const RunFrame *frame : visible)
        frames.push_back(*frame);
    return frames;
}

bool FrameStore::MatchesLocked(
    std::span<const std::uint64_t> sequences) const {
    std::vector<std::uint64_t> current;
    current.reserve(m_Frames.size() + (m_Latest ? 1u : 0u) +
                    (m_LastError ? 1u : 0u) +
                    (m_NonContinuing ? 1u : 0u));
    for (const RunFrame &frame : m_Frames)
        current.push_back(frame.Sequence);
    if (m_Latest)
        current.push_back(m_Latest->Sequence);
    if (m_LastError)
        current.push_back(m_LastError->Sequence);
    if (m_NonContinuing)
        current.push_back(m_NonContinuing->Sequence);
    std::sort(current.begin(), current.end());
    current.erase(std::unique(current.begin(), current.end()), current.end());
    return current.size() == sequences.size() &&
        std::equal(current.begin(), current.end(), sequences.begin());
}

} // namespace BML::Behavior

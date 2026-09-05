#ifndef BML_BEHAVIOR_FRAMESTORE_H
#define BML_BEHAVIOR_FRAMESTORE_H

#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

#include "Behavior/Execution.h"

namespace BML::Behavior {

struct FrameAppendResult {
    bool Overflowed = false;
    ExecutionFault Failure;
    // The overflow record the store attached to the Frame it kept, so the
    // caller can report the same Frame it stored.
    std::optional<FrameOverflow> Overflow;
};

enum class FrameBatchResult {
    Complete,
    Insufficient,
    Failed,
};

// A destination for one atomic read of the Frames currently owned by a Run.
// FrameStore holds its lock across both passes, so the measured set is exactly
// the set written and is consumed only after every Frame was accepted.
class FrameBatch {
public:
    virtual ~FrameBatch() = default;
    virtual bool Measure(const RunFrame &frame) = 0;
    virtual FrameBatchResult Ready() = 0;
    virtual bool Write(const RunFrame &frame) = 0;
};

// Owns copied RunFrames independently of the native Behavior instance. Ending
// an input activation does not end that instance: its Run retains the native
// Behavior until explicit Run/session/owner/world teardown.
class FrameStore final {
public:
    explicit FrameStore(FrameRetention retention);

    [[nodiscard]] bool KeepsPouts(const RunFrame &frame) const noexcept;
    [[nodiscard]] FrameAppendResult Retain(RunFrame frame);
    [[nodiscard]] std::vector<RunFrame> Read() const;
    bool Consume(std::span<const std::uint64_t> sequences);
    [[nodiscard]] std::vector<RunFrame> Take();
    [[nodiscard]] FrameBatchResult Take(FrameBatch &batch);

private:
    [[nodiscard]] bool ShouldRetain(
        const RunFrame &frame) const noexcept;
    void StoreNonContinuing(RunFrame frame);
    [[nodiscard]] std::vector<RunFrame> ReadLocked() const;
    [[nodiscard]] bool MatchesLocked(
        std::span<const std::uint64_t> sequences) const;
    [[nodiscard]] const RunFrame *NextLocked(
        std::uint64_t after, bool first) const noexcept;
    [[nodiscard]] bool VisitLocked(FrameBatch &batch, bool write) const;
    void ClearLocked() noexcept;

    FrameRetention m_Retention;
    mutable std::mutex m_Mutex;
    std::vector<RunFrame> m_Frames;
    std::optional<RunFrame> m_Latest;
    std::optional<RunFrame> m_LastError;
    std::optional<RunFrame> m_NonContinuing;
};

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_FRAMESTORE_H

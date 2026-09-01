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
    ExecutionFault TerminalFault;
};

// Owns copied RunFrames independently of the native Behavior instance. A Run
// can therefore tear down its CK objects as soon as it becomes terminal while
// preserving transport data until the author takes it or closes the handle.
class FrameStore final {
public:
    explicit FrameStore(FrameRetention retention);

    [[nodiscard]] FrameAppendResult Retain(RunFrame frame);
    [[nodiscard]] std::vector<RunFrame> Read() const;
    bool Consume(std::span<const std::uint64_t> sequences);
    [[nodiscard]] std::vector<RunFrame> Take();

private:
    [[nodiscard]] bool ShouldRetain(
        const RunFrame &frame) const noexcept;
    void StoreTerminal(RunFrame frame);
    [[nodiscard]] std::vector<RunFrame> ReadLocked() const;
    bool EraseSequence(std::uint64_t sequence);

    FrameRetention m_Retention;
    mutable std::mutex m_Mutex;
    std::vector<RunFrame> m_Frames;
    std::optional<RunFrame> m_Latest;
    std::optional<RunFrame> m_LastError;
    std::optional<RunFrame> m_TerminalFrame;
};

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_FRAMESTORE_H

#ifndef BML_BEHAVIOR_OUTCOMESTORE_H
#define BML_BEHAVIOR_OUTCOMESTORE_H

#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

#include "Behavior/Execution.h"

namespace BML::Behavior {

struct OutcomeRetainResult {
    bool Overflowed = false;
    ExecutionFault TerminalFault;
};

// Owns copied Outcomes independently of the native Behavior instance. A Run
// can therefore tear down its CK objects as soon as it becomes terminal while
// preserving transport data until the author drains or closes the handle.
class OutcomeStore final {
public:
    explicit OutcomeStore(OutcomeRetention retention);

    [[nodiscard]] OutcomeRetainResult Retain(ExecutionOutcome outcome);
    [[nodiscard]] std::vector<ExecutionOutcome> Read() const;
    bool Consume(std::span<const std::uint64_t> sequences);
    [[nodiscard]] std::vector<ExecutionOutcome> Drain();

private:
    [[nodiscard]] bool ShouldRetain(
        const ExecutionOutcome &outcome) const noexcept;
    void StoreTerminal(ExecutionOutcome outcome);
    [[nodiscard]] std::vector<ExecutionOutcome> ReadLocked() const;
    bool EraseSequence(std::uint64_t sequence);

    OutcomeRetention m_Retention;
    mutable std::mutex m_Mutex;
    std::vector<ExecutionOutcome> m_Outcomes;
    std::optional<ExecutionOutcome> m_Latest;
    std::optional<ExecutionOutcome> m_LastError;
    std::optional<ExecutionOutcome> m_TerminalOutcome;
};

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_OUTCOMESTORE_H

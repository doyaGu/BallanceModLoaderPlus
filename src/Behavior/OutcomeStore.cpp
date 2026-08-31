#include "Behavior/OutcomeStore.h"

#include <algorithm>
#include <utility>

namespace BML::Behavior {
namespace {

ExecutionFault QueueFullFault(int nativeCode) {
    return {ExecutionError::OutcomeQueueFull, nativeCode,
            "Behavior outcome retention is full; execution was stopped."};
}

} // namespace

OutcomeStore::OutcomeStore(OutcomeRetention retention)
    : m_Retention(retention) {}

OutcomeRetainResult OutcomeStore::Retain(ExecutionOutcome outcome) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_Retention.Kind == RetentionKind::Latest) {
        if (outcome.Terminal)
            StoreTerminal(std::move(outcome));
        else if (outcome.Fault)
            m_LastError = std::move(outcome);
        else
            m_Latest = std::move(outcome);
        return {};
    }

    if (m_Retention.Kind == RetentionKind::Ignore) {
        if (outcome.Terminal)
            StoreTerminal(std::move(outcome));
        else if (outcome.Fault)
            m_LastError = std::move(outcome);
        return {};
    }

    if (!ShouldRetain(outcome))
        return {};
    if (m_Outcomes.size() < m_Retention.Capacity) {
        m_Outcomes.push_back(std::move(outcome));
        return {};
    }

    ExecutionOutcome terminal = outcome;
    terminal.Terminal = true;
    terminal.NativeContinuation = false;
    terminal.QueuedInput = false;
    terminal.Overflow = OutcomeOverflow{1, m_Retention.Kind,
                                        m_Retention.Capacity,
                                        terminal.Fault};
    terminal.Fault = QueueFullFault(outcome.ReturnCode);
    const ExecutionFault fault = terminal.Fault;
    StoreTerminal(std::move(terminal));
    return {true, fault};
}

std::vector<ExecutionOutcome> OutcomeStore::Read() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return ReadLocked();
}

bool OutcomeStore::Consume(std::span<const std::uint64_t> sequences) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::vector<ExecutionOutcome> current = ReadLocked();
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

std::vector<ExecutionOutcome> OutcomeStore::Drain() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::vector<ExecutionOutcome> drained = ReadLocked();
    m_Outcomes.clear();
    m_Latest.reset();
    m_LastError.reset();
    m_TerminalOutcome.reset();
    return drained;
}

bool OutcomeStore::ShouldRetain(const ExecutionOutcome &outcome) const noexcept {
    switch (m_Retention.Kind) {
    case RetentionKind::Signals:
        return outcome.Sequence == 1 || !outcome.ActiveOutputs.empty() ||
               outcome.Terminal || static_cast<bool>(outcome.Fault);
    case RetentionKind::EachFrame:
        return true;
    case RetentionKind::Latest:
    case RetentionKind::Ignore:
        return false;
    }
    return false;
}

void OutcomeStore::StoreTerminal(ExecutionOutcome outcome) {
    m_TerminalOutcome = std::move(outcome);
}

std::vector<ExecutionOutcome> OutcomeStore::ReadLocked() const {
    std::vector<ExecutionOutcome> outcomes;
    outcomes.reserve(m_Outcomes.size() + (m_Latest ? 1u : 0u) +
                     (m_LastError ? 1u : 0u) +
                     (m_TerminalOutcome ? 1u : 0u));
    outcomes.insert(outcomes.end(), m_Outcomes.begin(), m_Outcomes.end());
    if (m_Latest)
        outcomes.push_back(*m_Latest);
    if (m_LastError)
        outcomes.push_back(*m_LastError);
    if (m_TerminalOutcome)
        outcomes.push_back(*m_TerminalOutcome);
    std::sort(outcomes.begin(), outcomes.end(),
              [](const ExecutionOutcome &left, const ExecutionOutcome &right) {
                  return left.Sequence < right.Sequence;
              });
    outcomes.erase(
        std::unique(outcomes.begin(), outcomes.end(),
                    [](const ExecutionOutcome &left,
                       const ExecutionOutcome &right) {
                        return left.Sequence == right.Sequence;
                    }),
        outcomes.end());
    return outcomes;
}

bool OutcomeStore::EraseSequence(std::uint64_t sequence) {
    const auto eraseOptional = [sequence](std::optional<ExecutionOutcome> &entry) {
        if (entry && entry->Sequence == sequence) {
            entry.reset();
            return true;
        }
        return false;
    };
    auto iterator = std::find_if(
        m_Outcomes.begin(), m_Outcomes.end(),
        [sequence](const ExecutionOutcome &outcome) {
            return outcome.Sequence == sequence;
        });
    if (iterator != m_Outcomes.end()) {
        m_Outcomes.erase(iterator);
        return true;
    }
    return eraseOptional(m_Latest) || eraseOptional(m_LastError) ||
           eraseOptional(m_TerminalOutcome);
}

} // namespace BML::Behavior

#ifndef BML_CONSOLE_COMPLETION_STATE_H
#define BML_CONSOLE_COMPLETION_STATE_H

#include <cstddef>
#include <vector>

namespace BML::Console {

enum class CompletionTriggerAction {
    RebuildCandidates,
    SelectNextCandidate,
    SelectPreviousCandidate,
};

inline CompletionTriggerAction GetCompletionTriggerAction(bool hasCandidates, bool shiftHeld) {
    if (!hasCandidates) {
        return CompletionTriggerAction::RebuildCandidates;
    }
    return shiftHeld
        ? CompletionTriggerAction::SelectPreviousCandidate
        : CompletionTriggerAction::SelectNextCandidate;
}

inline int FindCandidatePageForIndex(const std::vector<int> &pageStarts, int candidateIndex) {
    if (pageStarts.empty() || candidateIndex < 0) {
        return 0;
    }

    for (int i = static_cast<int>(pageStarts.size()) - 1; i >= 0; --i) {
        if (candidateIndex >= pageStarts[static_cast<size_t>(i)]) {
            return i;
        }
    }
    return 0;
}

inline void NextCandidate(int candidateCount, const std::vector<int> &pageStarts,
    int &candidateIndex, int &candidatePage) {
    if (candidateCount <= 0) {
        return;
    }

    candidateIndex = (candidateIndex + 1) % candidateCount;
    candidatePage = FindCandidatePageForIndex(pageStarts, candidateIndex);
}

inline void PrevCandidate(int candidateCount, const std::vector<int> &pageStarts,
    int &candidateIndex, int &candidatePage) {
    if (candidateCount <= 0) {
        return;
    }

    if (candidateIndex == 0) {
        candidateIndex = candidateCount;
    }
    candidateIndex = (candidateIndex - 1) % candidateCount;
    candidatePage = FindCandidatePageForIndex(pageStarts, candidateIndex);
}

inline void NextCandidatePage(int candidateCount, const std::vector<int> &pageStarts,
    int &candidateIndex, int &candidatePage) {
    if (candidateCount <= 0 || pageStarts.empty()) {
        return;
    }
    if (pageStarts.size() == 1) {
        candidateIndex = candidateCount - 1;
        candidatePage = 0;
        return;
    }

    const int nextPage = (candidatePage + 1) % static_cast<int>(pageStarts.size());
    const int nextIndex = nextPage > 0
        ? pageStarts[static_cast<size_t>(nextPage)] - 1
        : candidateCount - 1;
    if (candidateIndex == nextIndex) {
        candidateIndex = pageStarts[static_cast<size_t>(nextPage)];
        candidatePage = nextPage;
    } else {
        candidateIndex = nextIndex;
        candidatePage = FindCandidatePageForIndex(pageStarts, candidateIndex);
    }
}

inline void PrevCandidatePage(int candidateCount, const std::vector<int> &pageStarts,
    int &candidateIndex, int &candidatePage) {
    if (candidateCount <= 0 || pageStarts.empty()) {
        return;
    }
    if (pageStarts.size() == 1) {
        candidateIndex = 0;
        candidatePage = 0;
        return;
    }

    const int prevPage = candidatePage > 0
        ? (candidatePage - 1) % static_cast<int>(pageStarts.size())
        : static_cast<int>(pageStarts.size()) - 1;
    const int prevIndex = pageStarts[static_cast<size_t>(candidatePage)];
    if (candidateIndex == prevIndex) {
        candidateIndex = candidatePage > 0
            ? pageStarts[static_cast<size_t>(prevPage + 1)] - 1
            : candidateCount - 1;
        candidatePage = prevPage;
    } else {
        candidateIndex = prevIndex;
        candidatePage = FindCandidatePageForIndex(pageStarts, candidateIndex);
    }
}

} // namespace BML::Console

#endif // BML_CONSOLE_COMPLETION_STATE_H

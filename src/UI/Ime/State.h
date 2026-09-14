#ifndef BML_UI_IME_STATE_H
#define BML_UI_IME_STATE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Overlay::Ime {
    constexpr std::size_t MaxCandidateLists = 32;
    constexpr std::size_t MaxImmPayloadBytes = 1024 * 1024;
    constexpr std::size_t MaxCandidateCount = 1024;

    struct TextRange {
        std::uint32_t begin = 0;
        std::uint32_t end = 0;

        bool operator==(const TextRange &) const = default;
    };

    struct CandidatePagePosition {
        std::uint32_t index = 0;
        std::uint32_t count = 0;

        bool operator==(const CandidatePagePosition &) const = default;
    };

    struct CandidateListSnapshot {
        std::vector<std::u16string> items;
        std::uint32_t style = 0;
        std::uint32_t selection = 0;
        std::uint32_t pageStart = 0;
        std::uint32_t pageSize = 0;
        std::optional<CandidatePagePosition> pagePosition;
        bool hasSelection = false;

        bool SetIndexedPage(std::span<const std::uint32_t> pageStarts,
                            std::uint32_t pageIndex) noexcept;
        bool operator==(const CandidateListSnapshot &) const = default;
    };

    struct Snapshot {
        bool composing = false;
        std::u16string composition;
        std::vector<TextRange> targetRanges;
        std::vector<std::uint32_t> clauseBoundaries;
        std::uint32_t cursor = 0;
        std::array<std::optional<CandidateListSnapshot>, MaxCandidateLists> candidateLists;

        bool HasCandidates() const noexcept;
        bool IsActive() const noexcept;
        bool HasContent() const noexcept;
    };

    enum class CandidateDirection {
        Previous,
        Next,
    };

    struct CandidateSelectionRequest {
        std::size_t listIndex = 0;
        std::uint32_t itemIndex = 0;
    };

    struct CompositionUpdate {
        bool hasResult = false;
        bool hasText = false;
        bool hasTargetRanges = false;
        bool hasClauseBoundaries = false;
        bool hasCursor = false;
        std::u16string text;
        std::vector<TextRange> targetRanges;
        std::vector<std::uint32_t> clauseBoundaries;
        std::uint32_t cursor = 0;
    };

    class State {
    public:
        void Reset() noexcept;
        void StartComposition() noexcept;
        void ApplyComposition(CompositionUpdate update);
        bool SetCandidateList(std::size_t index, CandidateListSnapshot list);
        void CloseCandidateLists(std::uint32_t mask) noexcept;

        const Snapshot &GetSnapshot() const noexcept { return m_Snapshot; }
        std::uint64_t GetRevision() const noexcept { return m_Revision; }
        std::uint64_t GetCandidateRevision() const noexcept { return m_CandidateRevision; }

    private:
        bool ClearComposition() noexcept;
        bool ClearAll() noexcept;
        void Touch() noexcept { ++m_Revision; }
        void TouchCandidates() noexcept {
            ++m_CandidateRevision;
            Touch();
        }

        Snapshot m_Snapshot;
        std::uint64_t m_Revision = 0;
        std::uint64_t m_CandidateRevision = 0;
    };

    bool ParseUtf16(std::span<const std::byte> bytes, std::u16string &output);
    bool ParseClauses(std::span<const std::byte> bytes, std::size_t compositionLength,
                      std::vector<std::uint32_t> &output);
    bool ParseCandidateList(std::span<const std::byte> bytes, CandidateListSnapshot &output);
    std::optional<std::uint32_t> StepCandidateIndex(
        std::uint32_t count, std::optional<std::uint32_t> selection,
        CandidateDirection direction) noexcept;
    std::optional<CandidateSelectionRequest> PlanCandidateSelection(
        const Snapshot &snapshot, CandidateDirection direction) noexcept;
}

#endif // BML_UI_IME_STATE_H

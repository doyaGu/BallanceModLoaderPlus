#include "UI/Ime/State.h"

#include <algorithm>
#include <limits>
#include <string_view>
#include <utility>

namespace Overlay::Ime {
    namespace {
        struct CandidateListHeader {
            std::uint32_t Size;
            std::uint32_t Style;
            std::uint32_t Count;
            std::uint32_t Selection;
            std::uint32_t PageStart;
            std::uint32_t PageSize;
        };

        constexpr std::size_t CandidateFixedHeaderSize = sizeof(CandidateListHeader);
        constexpr std::uint32_t ImeCandidateCodeStyle = 0x0002;

        bool ReadU32(std::span<const std::byte> bytes, std::size_t offset, std::uint32_t &value) {
            if (offset > bytes.size() || bytes.size() - offset < sizeof(value))
                return false;

            value = 0;
            for (unsigned int shift = 0; shift < 32; shift += 8) {
                value |= static_cast<std::uint32_t>(
                             std::to_integer<unsigned char>(bytes[offset++]))
                         << shift;
            }
            return true;
        }

        bool ReadTerminatedUtf16(std::span<const std::byte> bytes, std::size_t offset, std::u16string &output) {
            if (offset < CandidateFixedHeaderSize || offset >= bytes.size() ||
                (offset & 1u) != 0) {
                return false;
            }

            output.clear();
            for (std::size_t cursor = offset; cursor + 1 < bytes.size(); cursor += 2) {
                const auto low = std::to_integer<unsigned char>(bytes[cursor]);
                const auto high = std::to_integer<unsigned char>(bytes[cursor + 1]);
                const char16_t character =
                    static_cast<char16_t>(low | (static_cast<unsigned int>(high) << 8));
                if (character == u'\0')
                    return true;
                output.push_back(character);
            }
            return false;
        }

        std::uint32_t ClampCursor(std::u16string_view text, std::uint32_t cursor) {
            cursor = std::min<std::uint32_t>(cursor, static_cast<std::uint32_t>(text.size()));
            if (cursor != 0 && cursor < text.size() &&
                text[cursor] >= 0xdc00 && text[cursor] <= 0xdfff &&
                text[cursor - 1] >= 0xd800 &&
                text[cursor - 1] <= 0xdbff) {
                --cursor;
            }
            return cursor;
        }
    }

    bool Snapshot::HasCandidates() const noexcept {
        for (const auto &list : candidateLists) {
            if (list.has_value() && !list->items.empty())
                return true;
        }
        return false;
    }

    bool Snapshot::IsActive() const noexcept {
        return composing || HasCandidates();
    }

    bool Snapshot::HasContent() const noexcept {
        return !composition.empty() || HasCandidates();
    }

    bool State::ClearComposition() noexcept {
        const bool changed = m_Snapshot.composing || !m_Snapshot.composition.empty() ||
                             !m_Snapshot.targetRanges.empty() || !m_Snapshot.clauseBoundaries.empty() ||
                             m_Snapshot.cursor != 0;
        m_Snapshot.composing = false;
        m_Snapshot.composition.clear();
        m_Snapshot.targetRanges.clear();
        m_Snapshot.clauseBoundaries.clear();
        m_Snapshot.cursor = 0;
        return changed;
    }

    bool State::ClearAll() noexcept {
        bool changed = ClearComposition();
        for (auto &list : m_Snapshot.candidateLists) {
            changed |= list.has_value();
            list.reset();
        }
        return changed;
    }

    void State::Reset() noexcept {
        const bool candidatesChanged = m_Snapshot.HasCandidates();
        if (ClearAll()) {
            if (candidatesChanged)
                ++m_CandidateRevision;
            Touch();
        }
    }

    void State::StartComposition() noexcept {
        const bool candidatesChanged = m_Snapshot.HasCandidates();
        bool changed = ClearAll();
        changed |= !m_Snapshot.composing;
        m_Snapshot.composing = true;
        if (changed) {
            if (candidatesChanged)
                ++m_CandidateRevision;
            Touch();
        }
    }

    void State::ApplyComposition(CompositionUpdate update) {
        bool changed = false;
        if (update.hasResult) {
            const bool candidatesChanged = m_Snapshot.HasCandidates();
            changed = ClearAll();
            if (candidatesChanged)
                ++m_CandidateRevision;
            if (!update.hasText) {
                if (changed)
                    Touch();
                return;
            }
        }

        if (update.hasText || update.hasTargetRanges ||
            update.hasClauseBoundaries || update.hasCursor) {
            if (!m_Snapshot.composing) {
                m_Snapshot.composing = true;
                changed = true;
            }
        }

        if (update.hasText) {
            changed |= m_Snapshot.composition != update.text;
            m_Snapshot.composition = std::move(update.text);
            changed |= !m_Snapshot.targetRanges.empty();
            m_Snapshot.targetRanges.clear();
            changed |= !m_Snapshot.clauseBoundaries.empty();
            m_Snapshot.clauseBoundaries.clear();
            const std::uint32_t cursor = ClampCursor(m_Snapshot.composition, m_Snapshot.cursor);
            changed |= m_Snapshot.cursor != cursor;
            m_Snapshot.cursor = cursor;
        }
        if (update.hasTargetRanges) {
            changed |= m_Snapshot.targetRanges != update.targetRanges;
            m_Snapshot.targetRanges = std::move(update.targetRanges);
        }
        if (update.hasClauseBoundaries) {
            changed |= m_Snapshot.clauseBoundaries != update.clauseBoundaries;
            m_Snapshot.clauseBoundaries = std::move(update.clauseBoundaries);
        }
        if (update.hasCursor) {
            const std::uint32_t cursor = ClampCursor(m_Snapshot.composition, update.cursor);
            changed |= m_Snapshot.cursor != cursor;
            m_Snapshot.cursor = cursor;
        }
        if (changed)
            Touch();
    }

    bool State::SetCandidateList(std::size_t index, CandidateListSnapshot list) {
        if (index >= m_Snapshot.candidateLists.size())
            return false;
        if (list.items.empty()) {
            if (!m_Snapshot.candidateLists[index])
                return true;
            m_Snapshot.candidateLists[index].reset();
        } else {
            if (m_Snapshot.candidateLists[index] == list)
                return true;
            m_Snapshot.candidateLists[index] = std::move(list);
        }
        TouchCandidates();
        return true;
    }

    void State::CloseCandidateLists(std::uint32_t mask) noexcept {
        bool changed = false;
        if (mask == 0) {
            for (auto &list : m_Snapshot.candidateLists) {
                changed |= list.has_value();
                list.reset();
            }
            if (changed)
                TouchCandidates();
            return;
        }

        for (std::size_t index = 0; index < m_Snapshot.candidateLists.size();
             ++index) {
            if ((mask & (std::uint32_t{1} << index)) != 0) {
                changed |= m_Snapshot.candidateLists[index].has_value();
                m_Snapshot.candidateLists[index].reset();
            }
        }
        if (changed)
            TouchCandidates();
    }

    bool ParseUtf16(std::span<const std::byte> bytes, std::u16string &output) {
        if (bytes.size() > MaxImmPayloadBytes || (bytes.size() & 1u) != 0)
            return false;

        std::u16string parsed;
        parsed.reserve(bytes.size() / 2);
        for (std::size_t offset = 0; offset < bytes.size(); offset += 2) {
            const auto low = std::to_integer<unsigned char>(bytes[offset]);
            const auto high = std::to_integer<unsigned char>(bytes[offset + 1]);
            parsed.push_back(static_cast<char16_t>(
                low | (static_cast<unsigned int>(high) << 8)));
        }
        output = std::move(parsed);
        return true;
    }

    bool ParseClauses(std::span<const std::byte> bytes, std::size_t compositionLength,
                      std::vector<std::uint32_t> &output) {
        if (bytes.size() > MaxImmPayloadBytes || bytes.empty() ||
            bytes.size() % sizeof(std::uint32_t) != 0 ||
            compositionLength > std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }

        std::vector<std::uint32_t> parsed;
        parsed.reserve(bytes.size() / sizeof(std::uint32_t));
        for (std::size_t offset = 0; offset < bytes.size(); offset += sizeof(std::uint32_t)) {
            std::uint32_t value = 0;
            if (!ReadU32(bytes, offset, value) ||
                value > compositionLength ||
                (!parsed.empty() && value < parsed.back())) {
                return false;
            }
            parsed.push_back(value);
        }

        if (parsed.front() != 0 || parsed.back() != compositionLength)
            return false;
        output = std::move(parsed);
        return true;
    }

    bool ParseCandidateList(std::span<const std::byte> bytes, CandidateListSnapshot &output) {
        if (bytes.size() < CandidateFixedHeaderSize ||
            bytes.size() > MaxImmPayloadBytes) {
            return false;
        }

        std::uint32_t declaredSize = 0;
        std::uint32_t style = 0;
        std::uint32_t count = 0;
        std::uint32_t selection = 0;
        std::uint32_t pageStart = 0;
        std::uint32_t pageSize = 0;
        if (!ReadU32(bytes, offsetof(CandidateListHeader, Size), declaredSize) ||
            !ReadU32(bytes, offsetof(CandidateListHeader, Style), style) ||
            !ReadU32(bytes, offsetof(CandidateListHeader, Count), count) ||
            !ReadU32(bytes, offsetof(CandidateListHeader, Selection), selection) ||
            !ReadU32(bytes, offsetof(CandidateListHeader, PageStart), pageStart) ||
            !ReadU32(bytes, offsetof(CandidateListHeader, PageSize), pageSize) ||
            declaredSize < CandidateFixedHeaderSize ||
            declaredSize > bytes.size() || (declaredSize & 1u) != 0 ||
            count > MaxCandidateCount ||
            count > (declaredSize - CandidateFixedHeaderSize) /
                        sizeof(std::uint32_t)) {
            return false;
        }

        // IME_CAND_CODE with one entry stores a packed legacy DBCS value in
        // dwOffset[0], not a UTF-16 string offset. It is outside the Unicode
        // candidate-list model and must never be misread as an address.
        if (style == ImeCandidateCodeStyle && count == 1)
            return false;

        const auto declaredBytes = bytes.first(declaredSize);
        const std::size_t stringsBegin =
            CandidateFixedHeaderSize + count * sizeof(std::uint32_t);
        std::vector<std::uint32_t> offsets;
        offsets.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index) {
            std::uint32_t stringOffset = 0;
            if (!ReadU32(declaredBytes,
                         CandidateFixedHeaderSize +
                             index * sizeof(std::uint32_t),
                         stringOffset) ||
                stringOffset < stringsBegin || stringOffset >= declaredSize ||
                (stringOffset & 1u) != 0) {
                return false;
            }
            offsets.push_back(stringOffset);
        }

        std::vector<std::uint32_t> sortedOffsets = offsets;
        std::sort(sortedOffsets.begin(), sortedOffsets.end());
        sortedOffsets.erase(
            std::unique(sortedOffsets.begin(), sortedOffsets.end()),
            sortedOffsets.end());

        CandidateListSnapshot parsed;
        parsed.style = style;
        parsed.items.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index) {
            const std::uint32_t stringOffset = offsets[index];
            const auto nextOffset = std::upper_bound(
                sortedOffsets.begin(), sortedOffsets.end(), stringOffset);
            const std::size_t stringLimit = nextOffset == sortedOffsets.end()
                ? declaredSize
                : *nextOffset;
            std::u16string item;
            if (!ReadTerminatedUtf16(declaredBytes.first(stringLimit),
                                     stringOffset, item)) {
                return false;
            }
            parsed.items.push_back(std::move(item));
        }

        parsed.selection = selection;
        parsed.hasSelection = selection < count;
        parsed.pageStart = count == 0 ? 0 : std::min(pageStart, count - 1);
        parsed.pageSize = std::min(pageSize, count - parsed.pageStart);
        output = std::move(parsed);
        return true;
    }

    std::optional<CandidateSelectionRequest> PlanCandidateSelection(
        const Snapshot &snapshot, CandidateDirection direction) noexcept {
        const CandidateListSnapshot *activeList = nullptr;
        std::size_t activeListIndex = 0;

        for (std::size_t index = 0; index < snapshot.candidateLists.size(); ++index) {
            const auto &candidateList = snapshot.candidateLists[index];
            if (!candidateList || candidateList->items.empty())
                continue;

            if (!activeList) {
                activeList = &*candidateList;
                activeListIndex = index;
            }
            if (candidateList->hasSelection &&
                candidateList->selection < candidateList->items.size()) {
                activeList = &*candidateList;
                activeListIndex = index;
                break;
            }
        }

        if (!activeList)
            return std::nullopt;

        const std::uint32_t count = static_cast<std::uint32_t>(
            std::min<std::size_t>(activeList->items.size(),
                                  std::numeric_limits<std::uint32_t>::max()));
        const std::optional<std::uint32_t> selection =
            activeList->hasSelection
                ? std::optional<std::uint32_t>(activeList->selection)
                : std::nullopt;
        const std::optional<std::uint32_t> itemIndex =
            StepCandidateIndex(count, selection, direction);
        if (!itemIndex)
            return std::nullopt;
        return CandidateSelectionRequest{activeListIndex, *itemIndex};
    }

    std::optional<std::uint32_t> StepCandidateIndex(std::uint32_t count, std::optional<std::uint32_t> selection,
                                               CandidateDirection direction) noexcept {
        if (count == 0)
            return std::nullopt;
        if (!selection || *selection >= count)
            return direction == CandidateDirection::Previous ? count - 1 : 0;
        if (direction == CandidateDirection::Next)
            return *selection + 1 == count ? 0 : *selection + 1;
        return *selection == 0 ? count - 1 : *selection - 1;
    }
}

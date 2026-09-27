#include "Api/Behavior/Codec.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

#include "BML/ImcWire.hpp"

namespace BML::Api::Behavior {
namespace {

using BML::Behavior::Internal::Pout;
using BML::Behavior::Internal::PoutKind;
using BML::Behavior::Internal::RunFrame;

std::uint32_t PublicPoutKind(PoutKind kind) noexcept {
    return static_cast<std::uint32_t>(kind) + 1u;
}

class WireFrames final : public BML::Behavior::Internal::FrameBatch {
public:
    WireFrames(BML_BehaviorRunFrame *headers,
               std::uint32_t headerCapacity,
               std::uint32_t headerStride,
               void *payload,
               std::uint32_t payloadCapacity,
               std::uint32_t *headerCount,
               std::uint32_t *payloadSize) noexcept
        : m_Headers(reinterpret_cast<std::uint8_t *>(headers)),
          m_HeaderCapacity(headerCapacity), m_HeaderStride(headerStride),
          m_Payload(static_cast<std::uint8_t *>(payload)),
          m_PayloadCapacity(payloadCapacity), m_OutHeaderCount(headerCount),
          m_OutPayloadSize(payloadSize) {}

    bool Measure(const RunFrame &frame) override {
        return !m_Writing && Add(frame);
    }

    BML::Behavior::Internal::FrameBatchResult Ready() override {
        if (m_HeaderCount > UINT32_MAX || m_PayloadSize > UINT32_MAX ||
            !FitsStrided(m_HeaderCount, m_HeaderStride,
                         sizeof(BML_BehaviorRunFrame)))
            return BML::Behavior::Internal::FrameBatchResult::Failed;
        *m_OutHeaderCount = static_cast<std::uint32_t>(m_HeaderCount);
        *m_OutPayloadSize = static_cast<std::uint32_t>(m_PayloadSize);
        if (m_HeaderCapacity < m_HeaderCount ||
            m_PayloadCapacity < m_PayloadSize)
            return BML::Behavior::Internal::FrameBatchResult::Insufficient;
        m_ExpectedHeaders = m_HeaderCount;
        m_ExpectedPayload = m_PayloadSize;
        m_HeaderCount = 0;
        m_PayloadSize = 0;
        m_Writing = true;
        return BML::Behavior::Internal::FrameBatchResult::Complete;
    }

    bool Write(const RunFrame &frame) override {
        return m_Writing && Add(frame) &&
            m_HeaderCount <= m_ExpectedHeaders &&
            m_PayloadSize <= m_ExpectedPayload;
    }

private:
    bool Add(const RunFrame &frame) {
        if (m_Writing &&
            (m_HeaderCount >= m_ExpectedHeaders ||
             m_HeaderCount >= m_HeaderCapacity || !m_Headers))
            return false;
        BML_BehaviorRunFrame header{};
        header.StructSize = sizeof(header);
        header.Sequence = frame.Sequence;
        header.Frame = frame.Frame;
        header.NativeResult = frame.ReturnCode;
        if (frame.NativeContinuation)
            header.Continuation |= BML_BEHAVIOR_CONTINUATION_NATIVE;
        if (frame.QueuedInput)
            header.Continuation |= BML_BEHAVIOR_CONTINUATION_QUEUED_INPUT;
        header.Error = PublicError(frame.Fault.Code);

        if (!AddOuts(frame, header) || !AddPouts(frame, header) ||
            !AddDiagnostic(frame, header))
            return false;
        if (m_Writing)
            std::memcpy(m_Headers + m_HeaderCount * m_HeaderStride,
                        &header, sizeof(header));
        ++m_HeaderCount;
        return true;
    }
    bool Align(std::size_t alignment) {
        const std::size_t remainder = m_PayloadSize % alignment;
        if (!remainder)
            return true;
        const std::size_t padding = alignment - remainder;
        if (m_PayloadSize > UINT32_MAX ||
            padding > UINT32_MAX - m_PayloadSize)
            return false;
        if (!CanWrite(padding))
            return false;
        if (m_Writing)
            std::memset(m_Payload + m_PayloadSize, 0, padding);
        m_PayloadSize += padding;
        return true;
    }

    bool Append(const void *data, std::size_t size, std::uint32_t &offset) {
        if ((!data && size) || m_PayloadSize > UINT32_MAX ||
            size > UINT32_MAX || size > UINT32_MAX - m_PayloadSize)
            return false;
        if (!CanWrite(size))
            return false;
        offset = static_cast<std::uint32_t>(m_PayloadSize);
        if (m_Writing && size)
            std::memcpy(m_Payload + m_PayloadSize, data, size);
        m_PayloadSize += size;
        return true;
    }

    template <typename T>
    bool ReserveRecords(std::size_t count, std::uint32_t &offset) {
        if (!Align(alignof(T)) || count > UINT32_MAX / sizeof(T))
            return false;
        const std::size_t bytes = count * sizeof(T);
        if (m_PayloadSize > UINT32_MAX ||
            bytes > UINT32_MAX - m_PayloadSize)
            return false;
        if (!CanWrite(bytes))
            return false;
        offset = static_cast<std::uint32_t>(m_PayloadSize);
        if (m_Writing)
            std::memset(m_Payload + m_PayloadSize, 0, bytes);
        m_PayloadSize += bytes;
        return true;
    }

    template <typename T>
    bool StoreRecord(std::uint32_t base, std::size_t index,
                     const T &record) {
        if (!m_Writing)
            return true;
        const std::size_t at = static_cast<std::size_t>(base) +
            index * sizeof(T);
        if (!m_Payload || at > m_ExpectedPayload ||
            sizeof(T) > m_ExpectedPayload - at ||
            at > m_PayloadCapacity ||
            sizeof(T) > m_PayloadCapacity - at)
            return false;
        std::memcpy(m_Payload + at, &record, sizeof(record));
        return true;
    }

    bool AddOuts(const RunFrame &frame,
                 BML_BehaviorRunFrame &header) {
        if (frame.ActiveOutputs.empty())
            return true;
        header.OutCount = static_cast<std::uint32_t>(frame.ActiveOutputs.size());
        if (!ReserveRecords<BML_BehaviorOutRecord>(
                frame.ActiveOutputs.size(), header.OutOffset))
            return false;
        for (std::size_t index = 0; index < frame.ActiveOutputs.size(); ++index) {
            const auto &out = frame.ActiveOutputs[index];
            BML_BehaviorOutRecord record{};
            record.StructSize = sizeof(record);
            record.Index = out.Index;
            record.Occurrence = out.Occurrence;
            std::string storage;
            const std::string_view name = Utf8Text(out.Name, storage);
            record.NameLength = static_cast<std::uint32_t>(name.size());
            if (!Append(name.data(), name.size(), record.NameOffset))
                return false;
            if (!StoreRecord(header.OutOffset, index, record))
                return false;
        }
        return true;
    }

    bool AddPouts(const RunFrame &frame,
                  BML_BehaviorRunFrame &header) {
        if (frame.Pouts.empty())
            return true;
        header.PoutCount = static_cast<std::uint32_t>(frame.Pouts.size());
        if (!ReserveRecords<BML_BehaviorPoutRecord>(
                frame.Pouts.size(), header.PoutOffset))
            return false;
        for (std::size_t index = 0; index < frame.Pouts.size(); ++index) {
            const Pout &pout = frame.Pouts[index];
            BML_BehaviorPoutRecord record{};
            record.StructSize = sizeof(record);
            record.Index = pout.Index;
            record.Occurrence = pout.Occurrence;
            record.Type = {pout.TypeGuid1, pout.TypeGuid2};
            record.Kind = PublicPoutKind(pout.Kind);
            std::string storage;
            const std::string_view name = Utf8Text(pout.Name, storage);
            record.NameLength = static_cast<std::uint32_t>(name.size());
            if (!Append(name.data(), name.size(), record.NameOffset) ||
                !AddPoutValue(pout, record))
                return false;
            if (!StoreRecord(header.PoutOffset, index, record))
                return false;
        }
        return true;
    }

    bool AddPoutValue(const Pout &pout, BML_BehaviorPoutRecord &record) {
        if (!Align(BML_BEHAVIOR_VALUE_ALIGNMENT))
            return false;
        std::uint8_t bytes[64]{};
        std::size_t size = 0;
        switch (pout.Kind) {
        case PoutKind::Bool:
            BML::Imc::Wire::Detail::Store32(bytes, pout.Int32 ? 1u : 0u);
            size = 4;
            break;
        case PoutKind::Int32:
            BML::Imc::Wire::Detail::Store32(
                bytes, static_cast<std::uint32_t>(pout.Int32));
            size = 4;
            break;
        case PoutKind::Float32:
            BML::Imc::Wire::Detail::Store32(
                bytes, std::bit_cast<std::uint32_t>(pout.Float32));
            size = 4;
            break;
        case PoutKind::Utf8: {
            std::string storage;
            const std::string_view text = Utf8Text(pout.Text, storage);
            record.ValueSize = static_cast<std::uint32_t>(text.size());
            return Append(text.data(), text.size(), record.ValueOffset);
        }
        case PoutKind::Object:
            BML::Imc::Wire::Detail::Store32(bytes, pout.ObjectDomain);
            BML::Imc::Wire::Detail::Store32(bytes + 4, pout.ObjectSlot);
            BML::Imc::Wire::Detail::Store32(bytes + 8, pout.ObjectGeneration);
            size = 12;
            break;
        case PoutKind::ObjectList: {
            if (pout.Objects.size() > UINT32_MAX / 12u)
                return false;
            record.ValueSize = static_cast<std::uint32_t>(
                pout.Objects.size() * 12u);
            record.ValueOffset = static_cast<std::uint32_t>(m_PayloadSize);
            for (const auto &object : pout.Objects) {
                std::uint8_t encoded[12]{};
                BML::Imc::Wire::Detail::Store32(encoded, object.Domain);
                BML::Imc::Wire::Detail::Store32(encoded + 4, object.Slot);
                BML::Imc::Wire::Detail::Store32(encoded + 8,
                                                object.Generation);
                std::uint32_t ignored = 0;
                if (!Append(encoded, sizeof(encoded), ignored))
                    return false;
            }
            return true;
        }
        default:
            if (pout.ComponentCount > pout.Components.size())
                return false;
            size = static_cast<std::size_t>(pout.ComponentCount) * 4u;
            for (std::uint32_t index = 0; index < pout.ComponentCount; ++index) {
                BML::Imc::Wire::Detail::Store32(
                    bytes + index * 4,
                    std::bit_cast<std::uint32_t>(pout.Components[index]));
            }
            break;
        }
        record.ValueSize = static_cast<std::uint32_t>(size);
        return Append(bytes, size, record.ValueOffset);
    }

    bool AddDiagnostic(const RunFrame &frame,
                       BML_BehaviorRunFrame &header) {
        if (!frame.Fault)
            return true;
        header.DiagnosticCount = 1;
        if (!ReserveRecords<BML_BehaviorDiagnosticRecord>(
                1, header.DiagnosticOffset))
            return false;
        BML_BehaviorDiagnosticRecord record{};
        record.StructSize = sizeof(record);
        record.Error = PublicError(frame.Fault.Code);
        record.Phase = BML_BEHAVIOR_PHASE_EXECUTION;
        record.NativeResult = frame.Fault.NativeCode;
        std::string storage;
        const std::string_view message = Utf8Text(frame.Fault.Message, storage);
        record.MessageLength = static_cast<std::uint32_t>(message.size());
        if (!Append(message.data(), message.size(), record.MessageOffset))
            return false;
        return StoreRecord(header.DiagnosticOffset, 0, record);
    }

    [[nodiscard]] bool CanWrite(std::size_t size) const noexcept {
        if (!m_Writing)
            return true;
        return m_Payload && m_PayloadSize <= m_ExpectedPayload &&
            size <= m_ExpectedPayload - m_PayloadSize &&
            m_PayloadSize <= m_PayloadCapacity &&
            size <= m_PayloadCapacity - m_PayloadSize;
    }

    std::uint8_t *m_Headers = nullptr;
    std::size_t m_HeaderCapacity = 0;
    std::size_t m_HeaderStride = 0;
    std::uint8_t *m_Payload = nullptr;
    std::size_t m_PayloadCapacity = 0;
    std::uint32_t *m_OutHeaderCount = nullptr;
    std::uint32_t *m_OutPayloadSize = nullptr;
    bool m_Writing = false;
    std::size_t m_HeaderCount = 0;
    std::size_t m_PayloadSize = 0;
    std::size_t m_ExpectedHeaders = 0;
    std::size_t m_ExpectedPayload = 0;
};

} // namespace

int WriteFrames(FrameStore &store, BML_BehaviorRunFrame *headers,
                std::uint32_t headerCapacity, std::uint32_t headerStride,
                void *payload, std::uint32_t payloadCapacity,
                std::uint32_t *outHeaderCount,
                std::uint32_t *outPayloadSize) {
    WireFrames batch(headers, headerCapacity, headerStride,
                     payload, payloadCapacity,
                     outHeaderCount, outPayloadSize);
    switch (store.Take(batch)) {
    case BML::Behavior::Internal::FrameBatchResult::Complete:
        return BML_OK;
    case BML::Behavior::Internal::FrameBatchResult::Insufficient:
        return BML_ERROR_BUFFER_TOO_SMALL;
    case BML::Behavior::Internal::FrameBatchResult::Failed:
        return BML_ERROR_OUT_OF_MEMORY;
    }
    return BML_ERROR_FAIL;
}

} // namespace BML::Api::Behavior

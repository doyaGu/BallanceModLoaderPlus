#ifndef BML_IMCWIRE_HPP
#define BML_IMCWIRE_HPP

#include "BML/Imc.h"
#include "BML/ImcTypes.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace BML::Imc::Wire {

// A tag packs the permanent field ID and its physical wire kind into one
// varuint: (field ID << 3) | wire kind. Fixed-width values need no length;
// length-delimited values carry one varuint length. Logical types live in the
// frozen ABI and generated codec rather than being repeated in every payload.
enum class Kind : std::uint8_t {
    VarUInt = 0,
    Fixed64 = 1,
    LengthDelimited = 2,
    Fixed32 = 5,
};

struct FieldView {
    std::uint32_t Id = 0;
    Kind WireKind = Kind::VarUInt;
    const std::uint8_t *Data = nullptr;
    std::size_t Size = 0;
};

namespace Detail {
inline void Store32(void *p, std::uint32_t value) noexcept {
    auto *out = static_cast<std::uint8_t *>(p);
    for (unsigned index = 0; index < 4; ++index)
        out[index] = static_cast<std::uint8_t>(value >> (index * 8));
}

inline void Store64(void *p, std::uint64_t value) noexcept {
    auto *out = static_cast<std::uint8_t *>(p);
    for (unsigned index = 0; index < 8; ++index)
        out[index] = static_cast<std::uint8_t>(value >> (index * 8));
}

inline std::uint32_t Load32(const void *p) noexcept {
    const auto *in = static_cast<const std::uint8_t *>(p);
    std::uint32_t value = 0;
    for (unsigned index = 0; index < 4; ++index)
        value |= static_cast<std::uint32_t>(in[index]) << (index * 8);
    return value;
}

inline std::uint64_t Load64(const void *p) noexcept {
    const auto *in = static_cast<const std::uint8_t *>(p);
    std::uint64_t value = 0;
    for (unsigned index = 0; index < 8; ++index)
        value |= static_cast<std::uint64_t>(in[index]) << (index * 8);
    return value;
}

inline std::size_t VarUInt32Size(std::uint32_t value) noexcept {
    std::size_t size = 1;
    while (value >= 0x80u) {
        value >>= 7;
        ++size;
    }
    return size;
}

inline std::size_t StoreVarUInt32(std::uint8_t *out, std::uint32_t value) noexcept {
    std::size_t size = 0;
    do {
        std::uint8_t byte = static_cast<std::uint8_t>(value & 0x7fu);
        value >>= 7;
        if (value) byte |= 0x80u;
        out[size++] = byte;
    } while (value);
    return size;
}

inline bool LoadVarUInt32(const std::uint8_t *data, std::size_t size,
                          std::size_t &cursor, std::uint32_t &out) noexcept {
    std::uint32_t value = 0;
    for (unsigned shift = 0; shift <= 28; shift += 7) {
        if (cursor >= size) return false;
        const std::uint8_t byte = data[cursor++];
        if (shift == 28 && (byte & 0xf0u)) return false;
        value |= static_cast<std::uint32_t>(byte & 0x7fu) << shift;
        if (!(byte & 0x80u)) {
            out = value;
            return true;
        }
    }
    return false;
}

inline bool SkipVarUInt64(const std::uint8_t *data, std::size_t size,
                          std::size_t &cursor) noexcept {
    for (unsigned index = 0; index < 10; ++index) {
        if (cursor >= size) return false;
        const std::uint8_t byte = data[cursor++];
        if (index == 9 && byte > 1) return false;
        if (!(byte & 0x80u)) return true;
    }
    return false;
}

inline bool Add(std::size_t &total, std::size_t amount) noexcept {
    const auto maximum = (std::numeric_limits<std::size_t>::max)();
    if (amount > maximum - total) return false;
    total += amount;
    return true;
}

inline bool MakeTag(std::uint32_t id, Kind kind, std::uint32_t &tag) noexcept {
    if (!id || id > (UINT32_MAX >> 3)) return false;
    tag = (id << 3) | static_cast<std::uint8_t>(kind);
    return true;
}

inline bool AddTagSize(std::size_t &total, std::uint32_t id, Kind kind) noexcept {
    std::uint32_t tag = 0;
    return MakeTag(id, kind, tag) && Add(total, VarUInt32Size(tag));
}
} // namespace Detail

inline bool AddBoolFieldSize(std::size_t &total, std::uint32_t id) noexcept {
    return Detail::AddTagSize(total, id, Kind::VarUInt) && Detail::Add(total, 1);
}

inline bool AddFixed32FieldSize(std::size_t &total, std::uint32_t id) noexcept {
    return Detail::AddTagSize(total, id, Kind::Fixed32) && Detail::Add(total, 4);
}

inline bool AddFixed64FieldSize(std::size_t &total, std::uint32_t id) noexcept {
    return Detail::AddTagSize(total, id, Kind::Fixed64) && Detail::Add(total, 8);
}

inline bool AddLengthDelimitedFieldSize(std::size_t &total, std::uint32_t id,
                                        std::size_t payload) noexcept {
    if (payload > UINT32_MAX || !Detail::AddTagSize(total, id, Kind::LengthDelimited))
        return false;
    return Detail::Add(
               total,
               Detail::VarUInt32Size(static_cast<std::uint32_t>(payload)))
        && Detail::Add(total, payload);
}

inline bool FixedArrayPayloadSize(std::size_t count, std::size_t elementSize,
                                  std::size_t &out) noexcept {
    if (elementSize && count > UINT32_MAX / elementSize) return false;
    out = count * elementSize;
    return out <= UINT32_MAX;
}

inline bool AddFixedArrayFieldSize(std::size_t &total, std::uint32_t id,
                                   std::size_t count, std::size_t elementSize) noexcept {
    std::size_t payload = 0;
    return FixedArrayPayloadSize(count, elementSize, payload)
        && AddLengthDelimitedFieldSize(total, id, payload);
}

inline bool StringArrayPayloadSize(const std::vector<std::string> &values,
                                   std::size_t &out) noexcept {
    out = 0;
    for (const auto &value : values) {
        if (value.size() > UINT32_MAX
                || !Detail::Add(
                    out,
                    Detail::VarUInt32Size(static_cast<std::uint32_t>(value.size())))
                || !Detail::Add(out, value.size()) || out > UINT32_MAX)
            return false;
    }
    return true;
}

inline bool AddStringArrayFieldSize(std::size_t &total, std::uint32_t id,
                                    const std::vector<std::string> &values) noexcept {
    std::size_t payload = 0;
    return StringArrayPayloadSize(values, payload)
        && AddLengthDelimitedFieldSize(total, id, payload);
}

class Writer {
public:
    Writer(void *data, std::size_t size) noexcept
        : m_Data(static_cast<std::uint8_t *>(data)), m_Size(size) {}

    [[nodiscard]] int Begin() noexcept {
        if (m_Started || (m_Size && !m_Data)) return BML_ERROR_INVALID_PARAMETER;
        m_Cursor = 0;
        m_Started = true;
        return BML_OK;
    }

    [[nodiscard]] int Finish() const noexcept {
        return m_Started && m_Cursor == m_Size
            ? BML_OK : BML_ERROR_MALFORMED_MESSAGE;
    }

    [[nodiscard]] int WriteBool(std::uint32_t id, bool value) noexcept {
        void *storage = nullptr;
        const int status = ReserveFixed(id, Kind::VarUInt, 1, &storage);
        if (status == BML_OK)
            *static_cast<std::uint8_t *>(storage) = value ? 1 : 0;
        return status;
    }

    [[nodiscard]] int WriteInt(std::uint32_t id, int value) noexcept {
        return WriteFixed32(id, static_cast<std::uint32_t>(value));
    }
    [[nodiscard]] int WriteFloat(std::uint32_t id, float value) noexcept {
        return WriteFixed32(id, std::bit_cast<std::uint32_t>(value));
    }
    [[nodiscard]] int WriteInt64(std::uint32_t id, std::int64_t value) noexcept {
        return WriteFixed64(id, std::bit_cast<std::uint64_t>(value));
    }
    [[nodiscard]] int WriteUInt64(std::uint32_t id, std::uint64_t value) noexcept {
        return WriteFixed64(id, value);
    }
    [[nodiscard]] int WriteDouble(std::uint32_t id, double value) noexcept {
        return WriteFixed64(id, std::bit_cast<std::uint64_t>(value));
    }
    [[nodiscard]] int WriteString(std::uint32_t id, std::string_view value) noexcept {
        return WriteLengthDelimited(id, value.data(), value.size());
    }
    [[nodiscard]] int WriteBytes(std::uint32_t id,
                   const std::vector<std::uint8_t> &value) noexcept {
        return WriteLengthDelimited(id, value.data(), value.size());
    }
    [[nodiscard]] int WriteObject(std::uint32_t id, const BML_ObjectRef &value) noexcept {
        std::uint8_t encoded[12];
        Detail::Store32(encoded, value.Domain);
        Detail::Store32(encoded + 4, value.Slot);
        Detail::Store32(encoded + 8, value.Generation);
        return WriteLengthDelimited(id, encoded, sizeof(encoded));
    }
    [[nodiscard]] int WriteVec2(std::uint32_t id, const BML_Vec2 &value) noexcept {
        std::uint8_t encoded[8];
        EncodeFloatObject(encoded, value);
        return WriteLengthDelimited(id, encoded, sizeof(encoded));
    }
    [[nodiscard]] int WriteVec3(std::uint32_t id, const BML_Vec3 &value) noexcept {
        std::uint8_t encoded[12];
        EncodeFloatObject(encoded, value);
        return WriteLengthDelimited(id, encoded, sizeof(encoded));
    }
    [[nodiscard]] int WriteMat4(std::uint32_t id, const BML_Mat4 &value) noexcept {
        std::uint8_t encoded[64];
        EncodeFloatObject(encoded, value);
        return WriteLengthDelimited(id, encoded, sizeof(encoded));
    }
    [[nodiscard]] int WriteBoolArray(std::uint32_t id,
                       const std::vector<bool> &values) noexcept {
        void *storage = nullptr;
        const int status = ReserveLengthDelimited(id, values.size(), &storage);
        if (status != BML_OK) return status;
        auto *out = static_cast<std::uint8_t *>(storage);
        for (std::size_t index = 0; index < values.size(); ++index)
            out[index] = values[index] ? 1 : 0;
        return BML_OK;
    }
    [[nodiscard]] int WriteIntArray(std::uint32_t id,
                      const std::vector<int> &values) noexcept {
        return WriteFixedArray(id, values, 4,
            [](std::uint8_t *out, int value) {
                Detail::Store32(out, static_cast<std::uint32_t>(value));
            });
    }
    [[nodiscard]] int WriteFloatArray(std::uint32_t id,
                        const std::vector<float> &values) noexcept {
        return WriteFixedArray(id, values, 4,
            [](std::uint8_t *out, float value) {
                Detail::Store32(out, std::bit_cast<std::uint32_t>(value));
            });
    }
    [[nodiscard]] int WriteInt64Array(std::uint32_t id,
                        const std::vector<std::int64_t> &values) noexcept {
        return WriteFixedArray(id, values, 8,
            [](std::uint8_t *out, std::int64_t value) {
                Detail::Store64(out, std::bit_cast<std::uint64_t>(value));
            });
    }
    [[nodiscard]] int WriteUInt64Array(std::uint32_t id,
                         const std::vector<std::uint64_t> &values) noexcept {
        return WriteFixedArray(id, values, 8,
            [](std::uint8_t *out, std::uint64_t value) {
                Detail::Store64(out, value);
            });
    }
    [[nodiscard]] int WriteDoubleArray(std::uint32_t id,
                         const std::vector<double> &values) noexcept {
        return WriteFixedArray(id, values, 8,
            [](std::uint8_t *out, double value) {
                Detail::Store64(out, std::bit_cast<std::uint64_t>(value));
            });
    }
    [[nodiscard]] int WriteStringArray(std::uint32_t id,
                         const std::vector<std::string> &values) noexcept {
        std::size_t payload = 0;
        if (!StringArrayPayloadSize(values, payload))
            return BML_ERROR_INVALID_PARAMETER;
        void *storage = nullptr;
        const int status = ReserveLengthDelimited(id, payload, &storage);
        if (status != BML_OK) return status;
        auto *out = static_cast<std::uint8_t *>(storage);
        for (const auto &value : values) {
            out += Detail::StoreVarUInt32(
                out, static_cast<std::uint32_t>(value.size()));
            if (!value.empty()) std::memcpy(out, value.data(), value.size());
            out += value.size();
        }
        return BML_OK;
    }
    [[nodiscard]] int WriteObjectArray(std::uint32_t id,
                         const std::vector<BML_ObjectRef> &values) noexcept {
        return WriteFixedArray(id, values, 12,
            [](std::uint8_t *out, const BML_ObjectRef &value) {
                Detail::Store32(out, value.Domain);
                Detail::Store32(out + 4, value.Slot);
                Detail::Store32(out + 8, value.Generation);
            });
    }
    [[nodiscard]] int WriteVec2Array(std::uint32_t id,
                       const std::vector<BML_Vec2> &values) noexcept {
        return WriteFloatObjectArray(id, values, 8);
    }
    [[nodiscard]] int WriteVec3Array(std::uint32_t id,
                       const std::vector<BML_Vec3> &values) noexcept {
        return WriteFloatObjectArray(id, values, 12);
    }
    [[nodiscard]] int WriteMat4Array(std::uint32_t id,
                       const std::vector<BML_Mat4> &values) noexcept {
        return WriteFloatObjectArray(id, values, 64);
    }

private:
    [[nodiscard]] int ReserveFixed(std::uint32_t id, Kind kind, std::size_t payload,
                     void **data) noexcept {
        if (!data || !m_Started) return BML_ERROR_INVALID_PARAMETER;
        std::uint32_t tag = 0;
        if (!Detail::MakeTag(id, kind, tag)) return BML_ERROR_INVALID_PARAMETER;
        const std::size_t header = Detail::VarUInt32Size(tag);
        if (m_Cursor > m_Size || header > m_Size - m_Cursor
                || payload > m_Size - m_Cursor - header)
            return BML_ERROR_MALFORMED_MESSAGE;
        auto *out = m_Data + m_Cursor;
        out += Detail::StoreVarUInt32(out, tag);
        *data = out;
        m_Cursor += header + payload;
        return BML_OK;
    }

    [[nodiscard]] int ReserveLengthDelimited(std::uint32_t id, std::size_t payload,
                               void **data) noexcept {
        if (!data || !m_Started || payload > UINT32_MAX)
            return BML_ERROR_INVALID_PARAMETER;
        std::uint32_t tag = 0;
        if (!Detail::MakeTag(id, Kind::LengthDelimited, tag))
            return BML_ERROR_INVALID_PARAMETER;
        const std::size_t header = Detail::VarUInt32Size(tag)
            + Detail::VarUInt32Size(static_cast<std::uint32_t>(payload));
        if (m_Cursor > m_Size || header > m_Size - m_Cursor
                || payload > m_Size - m_Cursor - header)
            return BML_ERROR_MALFORMED_MESSAGE;
        auto *out = m_Data + m_Cursor;
        out += Detail::StoreVarUInt32(out, tag);
        out += Detail::StoreVarUInt32(
            out, static_cast<std::uint32_t>(payload));
        *data = out;
        m_Cursor += header + payload;
        return BML_OK;
    }

    [[nodiscard]] int WriteLengthDelimited(std::uint32_t id, const void *data,
                             std::size_t size) noexcept {
        if (size && !data) return BML_ERROR_INVALID_PARAMETER;
        void *destination = nullptr;
        const int status = ReserveLengthDelimited(id, size, &destination);
        if (status == BML_OK && size) std::memcpy(destination, data, size);
        return status;
    }

    [[nodiscard]] int WriteFixed32(std::uint32_t id, std::uint32_t value) noexcept {
        void *storage = nullptr;
        const int status = ReserveFixed(id, Kind::Fixed32, 4, &storage);
        if (status == BML_OK) Detail::Store32(storage, value);
        return status;
    }

    [[nodiscard]] int WriteFixed64(std::uint32_t id, std::uint64_t value) noexcept {
        void *storage = nullptr;
        const int status = ReserveFixed(id, Kind::Fixed64, 8, &storage);
        if (status == BML_OK) Detail::Store64(storage, value);
        return status;
    }

    template <class Range, class Encoder>
    [[nodiscard]] int WriteFixedArray(std::uint32_t id, const Range &values,
                        std::size_t elementSize, Encoder encode) noexcept {
        std::size_t payload = 0;
        if (!FixedArrayPayloadSize(values.size(), elementSize, payload))
            return BML_ERROR_INVALID_PARAMETER;
        void *storage = nullptr;
        const int status = ReserveLengthDelimited(id, payload, &storage);
        if (status != BML_OK) return status;
        auto *out = static_cast<std::uint8_t *>(storage);
        for (std::size_t index = 0; index < values.size(); ++index)
            encode(out + index * elementSize, values[index]);
        return BML_OK;
    }

    template <class T>
    [[nodiscard]] int WriteFloatObjectArray(std::uint32_t id, const std::vector<T> &values,
                              std::size_t elementSize) noexcept {
        return WriteFixedArray(id, values, elementSize,
            [](std::uint8_t *out, const T &value) {
                EncodeFloatObject(out, value);
            });
    }

    static void StoreFloat(std::uint8_t *out, float value) noexcept {
        Detail::Store32(out, std::bit_cast<std::uint32_t>(value));
    }
    static void EncodeFloatObject(std::uint8_t *out,
                                  const BML_Vec2 &value) noexcept {
        StoreFloat(out, value.x); StoreFloat(out + 4, value.y);
    }
    static void EncodeFloatObject(std::uint8_t *out,
                                  const BML_Vec3 &value) noexcept {
        StoreFloat(out, value.x); StoreFloat(out + 4, value.y);
        StoreFloat(out + 8, value.z);
    }
    static void EncodeFloatObject(std::uint8_t *out,
                                  const BML_Mat4 &value) noexcept {
        const float elements[16] = {
            value.m00, value.m01, value.m02, value.m03,
            value.m10, value.m11, value.m12, value.m13,
            value.m20, value.m21, value.m22, value.m23,
            value.m30, value.m31, value.m32, value.m33,
        };
        for (std::size_t index = 0; index < 16; ++index)
            StoreFloat(out + index * 4, elements[index]);
    }

    std::uint8_t *m_Data = nullptr;
    std::size_t m_Size = 0;
    std::size_t m_Cursor = 0;
    bool m_Started = false;
};

class Reader {
public:
    Reader(const void *data, std::size_t size) noexcept
        : m_Data(static_cast<const std::uint8_t *>(data)), m_Size(size) {}

    [[nodiscard]] int Begin() noexcept {
        if (m_Started || (m_Size && !m_Data))
            return BML_ERROR_MALFORMED_MESSAGE;
        m_Cursor = 0;
        m_Started = true;
        return BML_OK;
    }

    [[nodiscard]] int Next(FieldView &out) noexcept {
        if (!m_Started) return BML_ERROR_MALFORMED_MESSAGE;
        if (m_Cursor == m_Size) return BML_ERROR_NOT_FOUND;
        std::uint32_t tag = 0;
        if (!Detail::LoadVarUInt32(m_Data, m_Size, m_Cursor, tag))
            return BML_ERROR_MALFORMED_MESSAGE;
        const std::uint32_t id = tag >> 3;
        if (!id) return BML_ERROR_MALFORMED_MESSAGE;
        const auto kind = static_cast<Kind>(tag & 0x7u);
        const std::size_t payloadStart = m_Cursor;
        std::size_t payloadSize = 0;
        switch (kind) {
        case Kind::VarUInt:
            if (!Detail::SkipVarUInt64(m_Data, m_Size, m_Cursor))
                return BML_ERROR_MALFORMED_MESSAGE;
            payloadSize = m_Cursor - payloadStart;
            break;
        case Kind::Fixed64:
            if (8 > m_Size - m_Cursor) return BML_ERROR_MALFORMED_MESSAGE;
            payloadSize = 8;
            m_Cursor += payloadSize;
            break;
        case Kind::LengthDelimited: {
            std::uint32_t length = 0;
            if (!Detail::LoadVarUInt32(m_Data, m_Size, m_Cursor, length)
                    || length > m_Size - m_Cursor)
                return BML_ERROR_MALFORMED_MESSAGE;
            payloadSize = length;
            out = {id, kind, m_Data + m_Cursor, payloadSize};
            m_Cursor += payloadSize;
            return BML_OK;
        }
        case Kind::Fixed32:
            if (4 > m_Size - m_Cursor) return BML_ERROR_MALFORMED_MESSAGE;
            payloadSize = 4;
            m_Cursor += payloadSize;
            break;
        default:
            return BML_ERROR_MALFORMED_MESSAGE;
        }
        out = {id, kind, m_Data + payloadStart, payloadSize};
        return BML_OK;
    }

    [[nodiscard]] int Finish() const noexcept {
        return m_Started && m_Cursor == m_Size
            ? BML_OK : BML_ERROR_MALFORMED_MESSAGE;
    }

    [[nodiscard]] static int ReadBool(const FieldView &field, bool &out) noexcept {
        if (field.WireKind != Kind::VarUInt) return BML_ERROR_TYPE_MISMATCH;
        if (field.Size != 1 || field.Data[0] > 1)
            return BML_ERROR_MALFORMED_MESSAGE;
        out = field.Data[0] != 0;
        return BML_OK;
    }
    [[nodiscard]] static int ReadInt(const FieldView &field, int &out) noexcept {
        if (field.WireKind != Kind::Fixed32) return BML_ERROR_TYPE_MISMATCH;
        out = static_cast<int>(Detail::Load32(field.Data));
        return BML_OK;
    }
    [[nodiscard]] static int ReadFloat(const FieldView &field, float &out) noexcept {
        if (field.WireKind != Kind::Fixed32) return BML_ERROR_TYPE_MISMATCH;
        out = std::bit_cast<float>(Detail::Load32(field.Data));
        return BML_OK;
    }
    [[nodiscard]] static int ReadInt64(const FieldView &field, std::int64_t &out) noexcept {
        if (field.WireKind != Kind::Fixed64) return BML_ERROR_TYPE_MISMATCH;
        out = std::bit_cast<std::int64_t>(Detail::Load64(field.Data));
        return BML_OK;
    }
    [[nodiscard]] static int ReadUInt64(const FieldView &field, std::uint64_t &out) noexcept {
        if (field.WireKind != Kind::Fixed64) return BML_ERROR_TYPE_MISMATCH;
        out = Detail::Load64(field.Data);
        return BML_OK;
    }
    [[nodiscard]] static int ReadDouble(const FieldView &field, double &out) noexcept {
        if (field.WireKind != Kind::Fixed64) return BML_ERROR_TYPE_MISMATCH;
        out = std::bit_cast<double>(Detail::Load64(field.Data));
        return BML_OK;
    }
    [[nodiscard]] static int ReadString(const FieldView &field, std::string &out) {
        if (field.WireKind != Kind::LengthDelimited)
            return BML_ERROR_TYPE_MISMATCH;
        out.assign(reinterpret_cast<const char *>(field.Data), field.Size);
        return BML_OK;
    }
    [[nodiscard]] static int ReadBytes(const FieldView &field,
                         std::vector<std::uint8_t> &out) {
        if (field.WireKind != Kind::LengthDelimited)
            return BML_ERROR_TYPE_MISMATCH;
        out.assign(field.Data, field.Data + field.Size);
        return BML_OK;
    }
    [[nodiscard]] static int ReadObject(const FieldView &field, BML_ObjectRef &out) noexcept {
        if (!IsLength(field, 12)) return LengthStatus(field);
        out = {Detail::Load32(field.Data), Detail::Load32(field.Data + 4),
               Detail::Load32(field.Data + 8)};
        return BML_OK;
    }
    [[nodiscard]] static int ReadVec2(const FieldView &field, BML_Vec2 &out) noexcept {
        if (!IsLength(field, 8)) return LengthStatus(field);
        out = {LoadFloat(field.Data), LoadFloat(field.Data + 4)};
        return BML_OK;
    }
    [[nodiscard]] static int ReadVec3(const FieldView &field, BML_Vec3 &out) noexcept {
        if (!IsLength(field, 12)) return LengthStatus(field);
        out = {LoadFloat(field.Data), LoadFloat(field.Data + 4),
               LoadFloat(field.Data + 8)};
        return BML_OK;
    }
    [[nodiscard]] static int ReadMat4(const FieldView &field, BML_Mat4 &out) noexcept {
        if (!IsLength(field, 64)) return LengthStatus(field);
        float elements[16];
        for (std::size_t index = 0; index < 16; ++index)
            elements[index] = LoadFloat(field.Data + index * 4);
        out = {
            elements[0], elements[1], elements[2], elements[3],
            elements[4], elements[5], elements[6], elements[7],
            elements[8], elements[9], elements[10], elements[11],
            elements[12], elements[13], elements[14], elements[15],
        };
        return BML_OK;
    }
    [[nodiscard]] static int ReadBoolArray(const FieldView &field,
                             std::vector<bool> &out) {
        if (field.WireKind != Kind::LengthDelimited)
            return BML_ERROR_TYPE_MISMATCH;
        std::vector<bool> decoded;
        decoded.reserve(field.Size);
        for (std::size_t index = 0; index < field.Size; ++index) {
            if (field.Data[index] > 1) return BML_ERROR_MALFORMED_MESSAGE;
            decoded.push_back(field.Data[index] != 0);
        }
        out = std::move(decoded);
        return BML_OK;
    }
    [[nodiscard]] static int ReadIntArray(const FieldView &field, std::vector<int> &out) {
        return ReadFixedArray(field, 4, out,
            [](const std::uint8_t *data) {
                return static_cast<int>(Detail::Load32(data));
            });
    }
    [[nodiscard]] static int ReadFloatArray(const FieldView &field,
                              std::vector<float> &out) {
        return ReadFixedArray(field, 4, out,
            [](const std::uint8_t *data) { return LoadFloat(data); });
    }
    [[nodiscard]] static int ReadInt64Array(const FieldView &field,
                              std::vector<std::int64_t> &out) {
        return ReadFixedArray(field, 8, out,
            [](const std::uint8_t *data) {
                return std::bit_cast<std::int64_t>(Detail::Load64(data));
            });
    }
    [[nodiscard]] static int ReadUInt64Array(const FieldView &field,
                               std::vector<std::uint64_t> &out) {
        return ReadFixedArray(field, 8, out,
            [](const std::uint8_t *data) { return Detail::Load64(data); });
    }
    [[nodiscard]] static int ReadDoubleArray(const FieldView &field,
                               std::vector<double> &out) {
        return ReadFixedArray(field, 8, out,
            [](const std::uint8_t *data) {
                return std::bit_cast<double>(Detail::Load64(data));
            });
    }
    [[nodiscard]] static int ReadStringArray(const FieldView &field,
                               std::vector<std::string> &out) {
        if (field.WireKind != Kind::LengthDelimited)
            return BML_ERROR_TYPE_MISMATCH;
        std::vector<std::string> decoded;
        std::size_t cursor = 0;
        while (cursor < field.Size) {
            std::uint32_t length = 0;
            if (!Detail::LoadVarUInt32(field.Data, field.Size, cursor, length)
                    || length > field.Size - cursor)
                return BML_ERROR_MALFORMED_MESSAGE;
            decoded.emplace_back(
                reinterpret_cast<const char *>(field.Data + cursor), length);
            cursor += length;
        }
        out = std::move(decoded);
        return BML_OK;
    }
    [[nodiscard]] static int ReadObjectArray(const FieldView &field,
                               std::vector<BML_ObjectRef> &out) {
        return ReadFixedArray(field, 12, out,
            [](const std::uint8_t *data) {
                return BML_ObjectRef{Detail::Load32(data),
                    Detail::Load32(data + 4), Detail::Load32(data + 8)};
            });
    }
    [[nodiscard]] static int ReadVec2Array(const FieldView &field,
                             std::vector<BML_Vec2> &out) {
        return ReadFixedArray(field, 8, out,
            [](const std::uint8_t *data) {
                return BML_Vec2{LoadFloat(data), LoadFloat(data + 4)};
            });
    }
    [[nodiscard]] static int ReadVec3Array(const FieldView &field,
                             std::vector<BML_Vec3> &out) {
        return ReadFixedArray(field, 12, out,
            [](const std::uint8_t *data) {
                return BML_Vec3{LoadFloat(data), LoadFloat(data + 4),
                    LoadFloat(data + 8)};
            });
    }
    [[nodiscard]] static int ReadMat4Array(const FieldView &field,
                             std::vector<BML_Mat4> &out) {
        return ReadFixedArray(field, 64, out,
            [](const std::uint8_t *data) {
                BML_Mat4 value{};
                float elements[16];
                for (std::size_t index = 0; index < 16; ++index)
                    elements[index] = LoadFloat(data + index * 4);
                value = {
                    elements[0], elements[1], elements[2], elements[3],
                    elements[4], elements[5], elements[6], elements[7],
                    elements[8], elements[9], elements[10], elements[11],
                    elements[12], elements[13], elements[14], elements[15],
                };
                return value;
            });
    }

private:
    static bool IsLength(const FieldView &field, std::size_t expected) noexcept {
        return field.WireKind == Kind::LengthDelimited && field.Size == expected;
    }
    [[nodiscard]] static int LengthStatus(const FieldView &field) noexcept {
        return field.WireKind == Kind::LengthDelimited
            ? BML_ERROR_MALFORMED_MESSAGE : BML_ERROR_TYPE_MISMATCH;
    }
    static float LoadFloat(const std::uint8_t *data) noexcept {
        return std::bit_cast<float>(Detail::Load32(data));
    }

    template <class T, class Decoder>
    [[nodiscard]] static int ReadFixedArray(const FieldView &field, std::size_t elementSize,
                              std::vector<T> &out, Decoder decode) {
        if (field.WireKind != Kind::LengthDelimited)
            return BML_ERROR_TYPE_MISMATCH;
        if (!elementSize || field.Size % elementSize)
            return BML_ERROR_MALFORMED_MESSAGE;
        std::vector<T> decoded;
        decoded.reserve(field.Size / elementSize);
        for (std::size_t offset = 0; offset < field.Size; offset += elementSize)
            decoded.push_back(decode(field.Data + offset));
        out = std::move(decoded);
        return BML_OK;
    }

    const std::uint8_t *m_Data = nullptr;
    std::size_t m_Size = 0;
    std::size_t m_Cursor = 0;
    bool m_Started = false;
};

} // namespace BML::Imc::Wire

#endif // BML_IMCWIRE_HPP

// How a payload is turned into bytes and back. An IMC message is a span of bytes and
// nothing else, so both sides need one agreed way of laying out the fields, and this is it:
// a Writer that puts them in, a Reader that takes them out, and the size helpers that say
// how many bytes it will come to.
//
// This is written for the code generator, not for a Mod. A generated *_imc.hpp calls these
// with the field numbers from the .imc file, and getting a number or an order wrong here
// produces a message the other side misreads, so a Mod encoding by hand is doing the
// generator's job. Read this to follow what the generated code does, or when adding a type
// the generator has to know how to carry.
//
// A field is a tag then a payload, the tag holding the field number and how long the
// payload is to be measured, which is what lets a reader step over a field it does not
// know: a newer sender's extra fields do not break an older reader. Field numbers are
// permanent once published. Renaming a field is nothing, renumbering or reusing a number is
// a different message.
//
// Sizing comes first and has to be exact. The Add*FieldSize helpers total up what the
// Writer will produce, the caller allocates that much, and Writer::Finish answers
// BML_ERROR_MALFORMED_MESSAGE unless exactly that many bytes were written, which catches a
// sizing function and a writing function that have drifted apart. Every helper answers
// false on overflow rather than wrapping.
//
// Reading is one pass forward: Begin, then Next until it answers BML_ERROR_NOT_FOUND, then
// Finish. A FieldView points into the message being read and copies nothing, so it lives
// only as long as those bytes do, which inside a handler means until the handler returns.
// A field read as the wrong type answers BML_ERROR_TYPE_MISMATCH and a truncated or
// otherwise unreadable one BML_ERROR_MALFORMED_MESSAGE, so a reader treats a bad message as
// data to reject rather than trusting what it was handed.
//
// Numbers go out least significant byte first whatever the host does, written and read a
// byte at a time, so nothing here depends on the alignment or the byte order of the machine
// it was compiled for.
//
// A generated payload does not spell any of that out field by field. It declares a table of
// FieldCodec rows, one Field<&Value::Member>(id) per field, and EncodedSize, Encode and
// Decode walk the table. Each field type has exactly one C++ type, so the member type alone
// decides which Writer and Reader member the field uses and the table names no wire kind.
#ifndef BML_IMCWIRE_HPP
#define BML_IMCWIRE_HPP

#include "BML/Imc.h"
#include "BML/Types.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
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

namespace Detail {
// One C++ type per .imc field type, so the member type alone says which Writer
// and Reader member the field uses. The generated table names no wire kind.
template <class T>
struct MemberPointer;

template <class Class, class Type>
struct MemberPointer<Type Class::*> {
    using Record = Class;
    using Value = Type;
};

template <class T>
struct VectorElement {
    using Type = void;
};

template <class T, class Allocator>
struct VectorElement<std::vector<T, Allocator>> {
    using Type = T;
};

// Bytes on the wire, not sizeof: a payload lays out its own members a byte at a
// time and must not follow the host's padding.
template <class T>
inline constexpr std::size_t WireFixedSize = 0;
template <> inline constexpr std::size_t WireFixedSize<bool> = 1;
template <> inline constexpr std::size_t WireFixedSize<int> = 4;
template <> inline constexpr std::size_t WireFixedSize<float> = 4;
template <> inline constexpr std::size_t WireFixedSize<std::int64_t> = 8;
template <> inline constexpr std::size_t WireFixedSize<std::uint64_t> = 8;
template <> inline constexpr std::size_t WireFixedSize<double> = 8;
template <> inline constexpr std::size_t WireFixedSize<BML_ObjectRef> = 12;
template <> inline constexpr std::size_t WireFixedSize<BML_Vec2> = 8;
template <> inline constexpr std::size_t WireFixedSize<BML_Vec3> = 12;
template <> inline constexpr std::size_t WireFixedSize<BML_Mat4> = 64;

template <class T>
inline bool AddFieldSize(std::size_t &total, std::uint32_t id, const T &value) noexcept {
    using Element = typename VectorElement<T>::Type;
    if constexpr (std::is_enum_v<T>) {
        return AddFieldSize(total, id, static_cast<std::underlying_type_t<T>>(value));
    } else if constexpr (std::is_same_v<T, bool>) {
        return AddBoolFieldSize(total, id);
    } else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, float>) {
        return AddFixed32FieldSize(total, id);
    } else if constexpr (std::is_same_v<T, std::int64_t> || std::is_same_v<T, std::uint64_t>
                         || std::is_same_v<T, double>) {
        return AddFixed64FieldSize(total, id);
    } else if constexpr (std::is_same_v<T, std::string>
                         || std::is_same_v<T, std::vector<std::uint8_t>>) {
        return AddLengthDelimitedFieldSize(total, id, value.size());
    } else if constexpr (std::is_same_v<T, BML_ObjectRef> || std::is_same_v<T, BML_Vec2>
                         || std::is_same_v<T, BML_Vec3> || std::is_same_v<T, BML_Mat4>) {
        return AddLengthDelimitedFieldSize(total, id, WireFixedSize<T>);
    } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
        return AddStringArrayFieldSize(total, id, value);
    } else {
        static_assert(WireFixedSize<Element> != 0, "unsupported IMC field type");
        return AddFixedArrayFieldSize(total, id, value.size(), WireFixedSize<Element>);
    }
}

template <class T>
inline int WriteField(Writer &writer, std::uint32_t id, const T &value) noexcept {
    if constexpr (std::is_enum_v<T>) {
        return WriteField(writer, id, static_cast<std::underlying_type_t<T>>(value));
    } else if constexpr (std::is_same_v<T, bool>) {
        return writer.WriteBool(id, value);
    } else if constexpr (std::is_same_v<T, int>) {
        return writer.WriteInt(id, value);
    } else if constexpr (std::is_same_v<T, float>) {
        return writer.WriteFloat(id, value);
    } else if constexpr (std::is_same_v<T, std::int64_t>) {
        return writer.WriteInt64(id, value);
    } else if constexpr (std::is_same_v<T, std::uint64_t>) {
        return writer.WriteUInt64(id, value);
    } else if constexpr (std::is_same_v<T, double>) {
        return writer.WriteDouble(id, value);
    } else if constexpr (std::is_same_v<T, std::string>) {
        return writer.WriteString(id, value);
    } else if constexpr (std::is_same_v<T, std::vector<std::uint8_t>>) {
        return writer.WriteBytes(id, value);
    } else if constexpr (std::is_same_v<T, BML_ObjectRef>) {
        return writer.WriteObject(id, value);
    } else if constexpr (std::is_same_v<T, BML_Vec2>) {
        return writer.WriteVec2(id, value);
    } else if constexpr (std::is_same_v<T, BML_Vec3>) {
        return writer.WriteVec3(id, value);
    } else if constexpr (std::is_same_v<T, BML_Mat4>) {
        return writer.WriteMat4(id, value);
    } else if constexpr (std::is_same_v<T, std::vector<bool>>) {
        return writer.WriteBoolArray(id, value);
    } else if constexpr (std::is_same_v<T, std::vector<int>>) {
        return writer.WriteIntArray(id, value);
    } else if constexpr (std::is_same_v<T, std::vector<float>>) {
        return writer.WriteFloatArray(id, value);
    } else if constexpr (std::is_same_v<T, std::vector<std::int64_t>>) {
        return writer.WriteInt64Array(id, value);
    } else if constexpr (std::is_same_v<T, std::vector<std::uint64_t>>) {
        return writer.WriteUInt64Array(id, value);
    } else if constexpr (std::is_same_v<T, std::vector<double>>) {
        return writer.WriteDoubleArray(id, value);
    } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
        return writer.WriteStringArray(id, value);
    } else if constexpr (std::is_same_v<T, std::vector<BML_ObjectRef>>) {
        return writer.WriteObjectArray(id, value);
    } else if constexpr (std::is_same_v<T, std::vector<BML_Vec2>>) {
        return writer.WriteVec2Array(id, value);
    } else if constexpr (std::is_same_v<T, std::vector<BML_Vec3>>) {
        return writer.WriteVec3Array(id, value);
    } else {
        static_assert(std::is_same_v<T, std::vector<BML_Mat4>>, "unsupported IMC field type");
        return writer.WriteMat4Array(id, value);
    }
}

template <class T>
inline int ReadField(const FieldView &field, T &out) {
    if constexpr (std::is_enum_v<T>) {
        std::underlying_type_t<T> raw{};
        const int status = ReadField(field, raw);
        if (status == BML_OK) out = static_cast<T>(raw);
        return status;
    } else if constexpr (std::is_same_v<T, bool>) {
        return Reader::ReadBool(field, out);
    } else if constexpr (std::is_same_v<T, int>) {
        return Reader::ReadInt(field, out);
    } else if constexpr (std::is_same_v<T, float>) {
        return Reader::ReadFloat(field, out);
    } else if constexpr (std::is_same_v<T, std::int64_t>) {
        return Reader::ReadInt64(field, out);
    } else if constexpr (std::is_same_v<T, std::uint64_t>) {
        return Reader::ReadUInt64(field, out);
    } else if constexpr (std::is_same_v<T, double>) {
        return Reader::ReadDouble(field, out);
    } else if constexpr (std::is_same_v<T, std::string>) {
        return Reader::ReadString(field, out);
    } else if constexpr (std::is_same_v<T, std::vector<std::uint8_t>>) {
        return Reader::ReadBytes(field, out);
    } else if constexpr (std::is_same_v<T, BML_ObjectRef>) {
        return Reader::ReadObject(field, out);
    } else if constexpr (std::is_same_v<T, BML_Vec2>) {
        return Reader::ReadVec2(field, out);
    } else if constexpr (std::is_same_v<T, BML_Vec3>) {
        return Reader::ReadVec3(field, out);
    } else if constexpr (std::is_same_v<T, BML_Mat4>) {
        return Reader::ReadMat4(field, out);
    } else if constexpr (std::is_same_v<T, std::vector<bool>>) {
        return Reader::ReadBoolArray(field, out);
    } else if constexpr (std::is_same_v<T, std::vector<int>>) {
        return Reader::ReadIntArray(field, out);
    } else if constexpr (std::is_same_v<T, std::vector<float>>) {
        return Reader::ReadFloatArray(field, out);
    } else if constexpr (std::is_same_v<T, std::vector<std::int64_t>>) {
        return Reader::ReadInt64Array(field, out);
    } else if constexpr (std::is_same_v<T, std::vector<std::uint64_t>>) {
        return Reader::ReadUInt64Array(field, out);
    } else if constexpr (std::is_same_v<T, std::vector<double>>) {
        return Reader::ReadDoubleArray(field, out);
    } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
        return Reader::ReadStringArray(field, out);
    } else if constexpr (std::is_same_v<T, std::vector<BML_ObjectRef>>) {
        return Reader::ReadObjectArray(field, out);
    } else if constexpr (std::is_same_v<T, std::vector<BML_Vec2>>) {
        return Reader::ReadVec2Array(field, out);
    } else if constexpr (std::is_same_v<T, std::vector<BML_Vec3>>) {
        return Reader::ReadVec3Array(field, out);
    } else {
        static_assert(std::is_same_v<T, std::vector<BML_Mat4>>, "unsupported IMC field type");
        return Reader::ReadMat4Array(field, out);
    }
}
} // namespace Detail

// One row per field of one payload record: the permanent field ID, whether the
// message must carry it, and the three operations that sizing, writing and
// reading need. An optional field also carries how its Has member is read and
// set, which is the only difference between a missing field and a present one.
template <class Record>
struct FieldCodec {
    std::uint32_t Id = 0;
    bool Required = true;
    bool (*AddSize)(std::size_t &total, std::uint32_t id, const Record &record) noexcept = nullptr;
    int (*Write)(Writer &writer, std::uint32_t id, const Record &record) noexcept = nullptr;
    int (*Read)(const FieldView &field, Record &record) = nullptr;
    bool (*Present)(const Record &record) noexcept = nullptr;
    void (*MarkPresent)(Record &record) noexcept = nullptr;
};

// Field<&Value::Member>(id) for a required field, Field<&Value::Member,
// &Value::HasMember>(id) for an optional one. Both members must belong to the
// same record, which is what keeps a table from naming another payload's field.
template <auto Member, auto Presence = nullptr>
[[nodiscard]] constexpr FieldCodec<typename Detail::MemberPointer<decltype(Member)>::Record>
Field(std::uint32_t id) noexcept {
    using Record = typename Detail::MemberPointer<decltype(Member)>::Record;
    constexpr bool optional = !std::is_same_v<decltype(Presence), std::nullptr_t>;
    FieldCodec<Record> codec{};
    codec.Id = id;
    codec.Required = !optional;
    codec.AddSize = [](std::size_t &total, std::uint32_t fieldId, const Record &record) noexcept {
        return Detail::AddFieldSize(total, fieldId, record.*Member);
    };
    codec.Write = [](Writer &writer, std::uint32_t fieldId, const Record &record) noexcept {
        return Detail::WriteField(writer, fieldId, record.*Member);
    };
    codec.Read = [](const FieldView &field, Record &record) {
        return Detail::ReadField(field, record.*Member);
    };
    if constexpr (optional) {
        static_assert(std::is_same_v<typename Detail::MemberPointer<decltype(Presence)>::Record, Record>,
                      "presence member belongs to another record");
        codec.Present = [](const Record &record) noexcept { return record.*Presence; };
        codec.MarkPresent = [](Record &record) noexcept { record.*Presence = true; };
    }
    return codec;
}

template <class Record>
inline std::size_t EncodedSize(const Record &record, const FieldCodec<Record> *fields,
                               std::size_t count) noexcept {
    std::size_t size = 0;
    for (std::size_t index = 0; index < count; ++index) {
        const FieldCodec<Record> &field = fields[index];
        if (field.Present && !field.Present(record)) continue;
        if (!field.AddSize(size, field.Id, record)) return 0;
    }
    return size;
}

template <class Record>
[[nodiscard]] inline int Encode(const Record &record, void *data, std::size_t size,
                                const FieldCodec<Record> *fields, std::size_t count) noexcept {
    if (size != EncodedSize(record, fields, count)) return BML_ERROR_INVALID_PARAMETER;
    Writer writer(data, size);
    int status = writer.Begin();
    for (std::size_t index = 0; status == BML_OK && index < count; ++index) {
        const FieldCodec<Record> &field = fields[index];
        if (field.Present && !field.Present(record)) continue;
        status = field.Write(writer, field.Id, record);
    }
    return status == BML_OK ? writer.Finish() : status;
}

// A field the table does not list is stepped over, a field it lists twice is a
// malformed message, and a required field that never arrived is malformed as
// well. The output is left untouched unless the whole message decodes.
template <class Record>
[[nodiscard]] inline int Decode(const BML_ImcMessage &message, Record &out,
                                const FieldCodec<Record> *fields, std::size_t count) {
    if (message.Size < sizeof(BML_ImcMessage) || (message.DataSize && !message.Data))
        return BML_ERROR_INVALID_PARAMETER;
    Reader reader(message.Data, message.DataSize);
    int status = reader.Begin();
    if (status != BML_OK) return status;
    Record decoded{};
    std::uint64_t seen = 0;
    std::uint64_t required = 0;
    for (std::size_t index = 0; index < count; ++index) {
        if (fields[index].Required) required |= UINT64_C(1) << index;
    }
    FieldView field;
    while ((status = reader.Next(field)) == BML_OK) {
        for (std::size_t index = 0; index < count; ++index) {
            const FieldCodec<Record> &codec = fields[index];
            if (codec.Id != field.Id) continue;
            const std::uint64_t bit = UINT64_C(1) << index;
            if (seen & bit) return BML_ERROR_MALFORMED_MESSAGE;
            status = codec.Read(field, decoded);
            if (status != BML_OK) return status;
            seen |= bit;
            if (codec.MarkPresent) codec.MarkPresent(decoded);
            break;
        }
    }
    if (status != BML_ERROR_NOT_FOUND) return status;
    status = reader.Finish();
    if (status != BML_OK) return status;
    if ((seen & required) != required) return BML_ERROR_MALFORMED_MESSAGE;
    out = std::move(decoded);
    return BML_OK;
}

template <class Record, std::size_t Count>
inline std::size_t EncodedSize(const Record &record,
                               const FieldCodec<Record> (&fields)[Count]) noexcept {
    return EncodedSize(record, fields, Count);
}

template <class Record, std::size_t Count>
[[nodiscard]] inline int Encode(const Record &record, void *data, std::size_t size,
                                const FieldCodec<Record> (&fields)[Count]) noexcept {
    return Encode(record, data, size, fields, Count);
}

template <class Record, std::size_t Count>
[[nodiscard]] inline int Decode(const BML_ImcMessage &message, Record &out,
                                const FieldCodec<Record> (&fields)[Count]) {
    return Decode(message, out, fields, Count);
}

} // namespace BML::Imc::Wire

#endif // BML_IMCWIRE_HPP

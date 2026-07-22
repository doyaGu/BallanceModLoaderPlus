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
#include <vector>

namespace BML::Imc::Wire {
inline constexpr std::uint32_t Magic = 0x32434d49u;
inline constexpr std::uint16_t Version = 1;
inline constexpr std::size_t HeaderSize = 24;
inline constexpr std::size_t FieldHeaderSize = 12;

enum class Type : std::uint16_t {
    Bool = 1, Int = 2, Float = 3, String = 4, Object = 5,
    Vec2 = 6, Vec3 = 7, Mat4 = 8, BoolArray = 9, IntArray = 10,
    FloatArray = 11, StringArray = 12, ObjectArray = 13,
    Vec2Array = 14, Vec3Array = 15, Mat4Array = 16,
    Int64 = 17, UInt64 = 18, Double = 19, Bytes = 20,
    Int64Array = 21, UInt64Array = 22, DoubleArray = 23,
};
struct FieldView {
    std::uint32_t Id = 0;
    Type ValueType = Type::Bool;
    std::uint16_t Flags = 0;
    const std::uint8_t *Data = nullptr;
    std::size_t Size = 0;
};

namespace Detail {
inline void Store16(void *p, std::uint16_t v) noexcept {
    auto *o = static_cast<std::uint8_t *>(p); o[0] = static_cast<std::uint8_t>(v); o[1] = static_cast<std::uint8_t>(v >> 8);
}
inline void Store32(void *p, std::uint32_t v) noexcept {
    auto *o = static_cast<std::uint8_t *>(p); for (unsigned i = 0; i < 4; ++i) o[i] = static_cast<std::uint8_t>(v >> (i * 8));
}
inline void Store64(void *p, std::uint64_t v) noexcept {
    auto *o = static_cast<std::uint8_t *>(p); for (unsigned i = 0; i < 8; ++i) o[i] = static_cast<std::uint8_t>(v >> (i * 8));
}
inline std::uint16_t Load16(const void *p) noexcept {
    const auto *i = static_cast<const std::uint8_t *>(p); return static_cast<std::uint16_t>(i[0]) | static_cast<std::uint16_t>(i[1]) << 8;
}
inline std::uint32_t Load32(const void *p) noexcept {
    const auto *i = static_cast<const std::uint8_t *>(p); std::uint32_t v = 0; for (unsigned n = 0; n < 4; ++n) v |= static_cast<std::uint32_t>(i[n]) << (n * 8); return v;
}
inline std::uint64_t Load64(const void *p) noexcept {
    const auto *i = static_cast<const std::uint8_t *>(p); std::uint64_t v = 0; for (unsigned n = 0; n < 8; ++n) v |= static_cast<std::uint64_t>(i[n]) << (n * 8); return v;
}
} // namespace Detail

inline bool AddFieldSize(std::size_t &total, std::size_t payload) noexcept {
    constexpr auto Max = (std::numeric_limits<std::size_t>::max)();
    if (payload > (std::numeric_limits<std::uint32_t>::max)() || payload > Max - FieldHeaderSize || total > Max - FieldHeaderSize - payload) return false;
    total += FieldHeaderSize + payload; return true;
}

inline bool FixedArrayPayloadSize(std::size_t count, std::size_t elementSize, std::size_t &payload) noexcept {
    constexpr auto MaxPayload = (std::numeric_limits<std::uint32_t>::max)();
    if (elementSize == 0 || count > (MaxPayload - 4u) / elementSize) return false;
    payload = 4u + count * elementSize;
    return true;
}

inline bool AddFixedArrayFieldSize(std::size_t &total, std::size_t count, std::size_t elementSize) noexcept {
    std::size_t payload = 0;
    return FixedArrayPayloadSize(count, elementSize, payload) && AddFieldSize(total, payload);
}

inline bool StringArrayPayloadSize(const std::vector<std::string> &values, std::size_t &payload) noexcept {
    constexpr auto MaxPayload = (std::numeric_limits<std::uint32_t>::max)();
    if (values.size() > MaxPayload) return false;
    std::size_t size = 4;
    for (const auto &value : values) {
        if (value.size() > MaxPayload || size > MaxPayload - 4u || value.size() > MaxPayload - size - 4u) return false;
        size += 4u + value.size();
    }
    payload = size;
    return true;
}

inline bool AddStringArrayFieldSize(std::size_t &total, const std::vector<std::string> &values) noexcept {
    std::size_t payload = 0;
    return StringArrayPayloadSize(values, payload) && AddFieldSize(total, payload);
}

class Writer {
public:
    Writer(void *data, std::size_t size) noexcept : m_Data(static_cast<std::uint8_t *>(data)), m_Size(size) {}
    int Begin(std::uint32_t schema, std::uint64_t hash, std::uint32_t fields, std::uint16_t flags = 0) noexcept {
        if (!m_Data || m_Size < HeaderSize || schema == 0 || hash == 0) return BML_ERROR_INVALID_PARAMETER;
        Detail::Store32(m_Data, Magic); Detail::Store16(m_Data + 4, Version); Detail::Store16(m_Data + 6, flags);
        Detail::Store32(m_Data + 8, schema); Detail::Store32(m_Data + 12, fields); Detail::Store64(m_Data + 16, hash);
        m_Cursor = HeaderSize; m_Fields = fields; m_Written = 0; m_Started = true; return BML_OK;
    }
    int Finish() const noexcept {
        return m_Started && m_Written == m_Fields && m_Cursor == m_Size ? BML_OK : BML_ERROR_MALFORMED_MESSAGE;
    }
    int ReserveRaw(std::uint32_t id, Type type, std::size_t size, void **data, std::uint16_t flags = 0) noexcept {
        if (!data) return BML_ERROR_INVALID_PARAMETER;
        if (!m_Started || id == 0 || m_Written >= m_Fields || size > UINT32_MAX || m_Cursor > m_Size || FieldHeaderSize + size > m_Size - m_Cursor) return BML_ERROR_MALFORMED_MESSAGE;
        auto *h = m_Data + m_Cursor; Detail::Store32(h, id); Detail::Store16(h + 4, static_cast<std::uint16_t>(type)); Detail::Store16(h + 6, flags); Detail::Store32(h + 8, static_cast<std::uint32_t>(size));
        *data = h + FieldHeaderSize; m_Cursor += FieldHeaderSize + size; ++m_Written; return BML_OK;
    }
    int WriteRaw(std::uint32_t id, Type type, const void *data, std::size_t size, std::uint16_t flags = 0) noexcept {
        if (size && !data) return BML_ERROR_MALFORMED_MESSAGE;
        void *destination = nullptr; const int status = ReserveRaw(id, type, size, &destination, flags);
        if (status == BML_OK && size) std::memcpy(destination, data, size); return status;
    }
    int WriteBool(std::uint32_t id, bool v) noexcept { const std::uint8_t b = v ? 1 : 0; return WriteRaw(id, Type::Bool, &b, 1); }
    int WriteInt(std::uint32_t id, int v) noexcept { std::uint8_t b[4]; Detail::Store32(b, static_cast<std::uint32_t>(v)); return WriteRaw(id, Type::Int, b, 4); }
    int WriteFloat(std::uint32_t id, float v) noexcept { std::uint8_t b[4]; Detail::Store32(b, std::bit_cast<std::uint32_t>(v)); return WriteRaw(id, Type::Float, b, 4); }
    int WriteInt64(std::uint32_t id, std::int64_t v) noexcept { std::uint8_t b[8]; Detail::Store64(b, std::bit_cast<std::uint64_t>(v)); return WriteRaw(id, Type::Int64, b, 8); }
    int WriteUInt64(std::uint32_t id, std::uint64_t v) noexcept { std::uint8_t b[8]; Detail::Store64(b, v); return WriteRaw(id, Type::UInt64, b, 8); }
    int WriteDouble(std::uint32_t id, double v) noexcept { std::uint8_t b[8]; Detail::Store64(b, std::bit_cast<std::uint64_t>(v)); return WriteRaw(id, Type::Double, b, 8); }
    int WriteString(std::uint32_t id, std::string_view v) noexcept { return WriteRaw(id, Type::String, v.data(), v.size()); }
    int WriteBytes(std::uint32_t id, const std::vector<std::uint8_t> &v) noexcept { return WriteRaw(id, Type::Bytes, v.data(), v.size()); }
    int WriteObject(std::uint32_t id, const BML_ObjectRef &v) noexcept {
        std::uint8_t b[12]; Detail::Store32(b, v.Domain); Detail::Store32(b + 4, v.Slot); Detail::Store32(b + 8, v.Generation); return WriteRaw(id, Type::Object, b, 12);
    }
    int WriteVec2(std::uint32_t id, const BML_Vec2 &v) noexcept { return WriteFloats(id, Type::Vec2, &v.x, 2); }
    int WriteVec3(std::uint32_t id, const BML_Vec3 &v) noexcept { return WriteFloats(id, Type::Vec3, &v.x, 3); }
    int WriteMat4(std::uint32_t id, const BML_Mat4 &v) noexcept { return WriteFloats(id, Type::Mat4, &v.m00, 16); }
    int WriteBoolArray(std::uint32_t id, const std::vector<bool> &values) noexcept {
        return WriteFixedArray(id, Type::BoolArray, values, 1, [](std::uint8_t *out, bool value) { out[0] = value ? 1 : 0; });
    }
    int WriteIntArray(std::uint32_t id, const std::vector<int> &values) noexcept {
        return WriteFixedArray(id, Type::IntArray, values, 4, [](std::uint8_t *out, int value) { Detail::Store32(out, static_cast<std::uint32_t>(value)); });
    }
    int WriteFloatArray(std::uint32_t id, const std::vector<float> &values) noexcept {
        return WriteFixedArray(id, Type::FloatArray, values, 4, [](std::uint8_t *out, float value) { Detail::Store32(out, std::bit_cast<std::uint32_t>(value)); });
    }
    int WriteInt64Array(std::uint32_t id, const std::vector<std::int64_t> &values) noexcept {
        return WriteFixedArray(id, Type::Int64Array, values, 8, [](std::uint8_t *out, std::int64_t value) { Detail::Store64(out, std::bit_cast<std::uint64_t>(value)); });
    }
    int WriteUInt64Array(std::uint32_t id, const std::vector<std::uint64_t> &values) noexcept {
        return WriteFixedArray(id, Type::UInt64Array, values, 8, [](std::uint8_t *out, std::uint64_t value) { Detail::Store64(out, value); });
    }
    int WriteDoubleArray(std::uint32_t id, const std::vector<double> &values) noexcept {
        return WriteFixedArray(id, Type::DoubleArray, values, 8, [](std::uint8_t *out, double value) { Detail::Store64(out, std::bit_cast<std::uint64_t>(value)); });
    }
    int WriteStringArray(std::uint32_t id, const std::vector<std::string> &values) noexcept {
        std::size_t payload = 0;
        if (!StringArrayPayloadSize(values, payload)) return BML_ERROR_INVALID_PARAMETER;
        void *storage = nullptr; const int status = ReserveRaw(id, Type::StringArray, payload, &storage);
        if (status != BML_OK) return status;
        auto *out = static_cast<std::uint8_t *>(storage); Detail::Store32(out, static_cast<std::uint32_t>(values.size())); out += 4;
        for (const auto &value : values) {
            Detail::Store32(out, static_cast<std::uint32_t>(value.size())); out += 4;
            if (!value.empty()) std::memcpy(out, value.data(), value.size()); out += value.size();
        }
        return BML_OK;
    }
    int WriteObjectArray(std::uint32_t id, const std::vector<BML_ObjectRef> &values) noexcept {
        return WriteFixedArray(id, Type::ObjectArray, values, 12, [](std::uint8_t *out, const BML_ObjectRef &value) {
            Detail::Store32(out, value.Domain); Detail::Store32(out + 4, value.Slot); Detail::Store32(out + 8, value.Generation);
        });
    }
    int WriteVec2Array(std::uint32_t id, const std::vector<BML_Vec2> &values) noexcept {
        return WriteFixedArray(id, Type::Vec2Array, values, 8, [](std::uint8_t *out, const BML_Vec2 &value) {
            Detail::Store32(out, std::bit_cast<std::uint32_t>(value.x)); Detail::Store32(out + 4, std::bit_cast<std::uint32_t>(value.y));
        });
    }
    int WriteVec3Array(std::uint32_t id, const std::vector<BML_Vec3> &values) noexcept {
        return WriteFixedArray(id, Type::Vec3Array, values, 12, [](std::uint8_t *out, const BML_Vec3 &value) {
            Detail::Store32(out, std::bit_cast<std::uint32_t>(value.x)); Detail::Store32(out + 4, std::bit_cast<std::uint32_t>(value.y)); Detail::Store32(out + 8, std::bit_cast<std::uint32_t>(value.z));
        });
    }
    int WriteMat4Array(std::uint32_t id, const std::vector<BML_Mat4> &values) noexcept {
        return WriteFixedArray(id, Type::Mat4Array, values, 64, [](std::uint8_t *out, const BML_Mat4 &value) {
            const float *elements = &value.m00;
            for (std::size_t i = 0; i < 16; ++i) Detail::Store32(out + i * 4, std::bit_cast<std::uint32_t>(elements[i]));
        });
    }
private:
    template <class Range, class Encoder>
    int WriteFixedArray(std::uint32_t id, Type type, const Range &values, std::size_t elementSize, Encoder encode) noexcept {
        std::size_t payload = 0;
        if (!FixedArrayPayloadSize(values.size(), elementSize, payload)) return BML_ERROR_INVALID_PARAMETER;
        void *storage = nullptr; const int status = ReserveRaw(id, type, payload, &storage);
        if (status != BML_OK) return status;
        auto *out = static_cast<std::uint8_t *>(storage); Detail::Store32(out, static_cast<std::uint32_t>(values.size())); out += 4;
        for (std::size_t i = 0; i < values.size(); ++i) encode(out + i * elementSize, values[i]);
        return BML_OK;
    }
    int WriteFloats(std::uint32_t id, Type type, const float *v, std::size_t count) noexcept {
        std::uint8_t b[64]; if (count > 16) return BML_ERROR_INVALID_PARAMETER;
        for (std::size_t i = 0; i < count; ++i) Detail::Store32(b + i * 4, std::bit_cast<std::uint32_t>(v[i])); return WriteRaw(id, type, b, count * 4);
    }
    std::uint8_t *m_Data = nullptr; std::size_t m_Size = 0, m_Cursor = 0;
    std::uint32_t m_Fields = 0, m_Written = 0; bool m_Started = false;
};

class Reader {
public:
    Reader(const void *data, std::size_t size) noexcept : m_Data(static_cast<const std::uint8_t *>(data)), m_Size(size) {}
    int Begin(std::uint32_t expectedSchema) noexcept {
        if (!m_Data || m_Size < HeaderSize || Detail::Load32(m_Data) != Magic || Detail::Load16(m_Data + 4) != Version) return BML_ERROR_MALFORMED_MESSAGE;
        m_Flags = Detail::Load16(m_Data + 6); m_Schema = Detail::Load32(m_Data + 8); m_Fields = Detail::Load32(m_Data + 12); m_Hash = Detail::Load64(m_Data + 16);
        if (expectedSchema && m_Schema != expectedSchema) return BML_ERROR_IMC_SCHEMA_MISMATCH;
        m_Cursor = HeaderSize; m_Read = 0; m_Started = true; return BML_OK;
    }
    int Next(FieldView &out) noexcept {
        if (!m_Started) return BML_ERROR_MALFORMED_MESSAGE; if (m_Read == m_Fields) return BML_ERROR_NOT_FOUND;
        if (m_Cursor > m_Size || FieldHeaderSize > m_Size - m_Cursor) return BML_ERROR_MALFORMED_MESSAGE;
        const auto *h = m_Data + m_Cursor; const std::uint32_t size = Detail::Load32(h + 8);
        if (size > m_Size - m_Cursor - FieldHeaderSize) return BML_ERROR_MALFORMED_MESSAGE;
        out = {Detail::Load32(h), static_cast<Type>(Detail::Load16(h + 4)), Detail::Load16(h + 6), h + FieldHeaderSize, size};
        m_Cursor += FieldHeaderSize + size; ++m_Read; return out.Id ? BML_OK : BML_ERROR_MALFORMED_MESSAGE;
    }
    int Finish() const noexcept { return m_Started && m_Read == m_Fields && m_Cursor == m_Size ? BML_OK : BML_ERROR_MALFORMED_MESSAGE; }
    std::uint32_t Schema() const noexcept { return m_Schema; }
    std::uint64_t DescriptorHash() const noexcept { return m_Hash; }
    std::uint16_t Flags() const noexcept { return m_Flags; }
    static int ReadBool(const FieldView &f, bool &out) noexcept {
        if (f.ValueType != Type::Bool || f.Size != 1 || f.Data[0] > 1) return BML_ERROR_TYPE_MISMATCH; out = f.Data[0] != 0; return BML_OK;
    }
    static int ReadInt(const FieldView &f, int &out) noexcept {
        if (f.ValueType != Type::Int || f.Size != 4) return BML_ERROR_TYPE_MISMATCH; out = static_cast<int>(Detail::Load32(f.Data)); return BML_OK;
    }
    static int ReadFloat(const FieldView &f, float &out) noexcept {
        if (f.ValueType != Type::Float || f.Size != 4) return BML_ERROR_TYPE_MISMATCH; out = std::bit_cast<float>(Detail::Load32(f.Data)); return BML_OK;
    }
    static int ReadInt64(const FieldView &f, std::int64_t &out) noexcept {
        if (f.ValueType != Type::Int64 || f.Size != 8) return BML_ERROR_TYPE_MISMATCH; out = std::bit_cast<std::int64_t>(Detail::Load64(f.Data)); return BML_OK;
    }
    static int ReadUInt64(const FieldView &f, std::uint64_t &out) noexcept {
        if (f.ValueType != Type::UInt64 || f.Size != 8) return BML_ERROR_TYPE_MISMATCH; out = Detail::Load64(f.Data); return BML_OK;
    }
    static int ReadDouble(const FieldView &f, double &out) noexcept {
        if (f.ValueType != Type::Double || f.Size != 8) return BML_ERROR_TYPE_MISMATCH; out = std::bit_cast<double>(Detail::Load64(f.Data)); return BML_OK;
    }
    static int ReadString(const FieldView &f, std::string &out) {
        if (f.ValueType != Type::String) return BML_ERROR_TYPE_MISMATCH;
        try { out.assign(reinterpret_cast<const char *>(f.Data), f.Size); }
        catch (...) { return BML_ERROR_OUT_OF_MEMORY; }
        return BML_OK;
    }
    static int ReadBytes(const FieldView &f, std::vector<std::uint8_t> &out) {
        if (f.ValueType != Type::Bytes) return BML_ERROR_TYPE_MISMATCH;
        std::vector<std::uint8_t> decoded;
        try { if (f.Size) decoded.assign(f.Data, f.Data + f.Size); }
        catch (...) { return BML_ERROR_OUT_OF_MEMORY; }
        out = std::move(decoded); return BML_OK;
    }
    static int ReadObject(const FieldView &f, BML_ObjectRef &out) noexcept {
        if (f.ValueType != Type::Object || f.Size != 12) return BML_ERROR_TYPE_MISMATCH; out = {Detail::Load32(f.Data), Detail::Load32(f.Data + 4), Detail::Load32(f.Data + 8)}; return BML_OK;
    }
    static int ReadVec2(const FieldView &f, BML_Vec2 &out) noexcept { return ReadFloats(f, Type::Vec2, &out.x, 2); }
    static int ReadVec3(const FieldView &f, BML_Vec3 &out) noexcept { return ReadFloats(f, Type::Vec3, &out.x, 3); }
    static int ReadMat4(const FieldView &f, BML_Mat4 &out) noexcept { return ReadFloats(f, Type::Mat4, &out.m00, 16); }
    static int ReadBoolArray(const FieldView &f, std::vector<bool> &out) {
        const std::uint8_t *data = nullptr; std::uint32_t count = 0;
        const int status = ReadArrayHeader(f, Type::BoolArray, 1, data, count);
        if (status != BML_OK) return status;
        for (std::uint32_t i = 0; i < count; ++i) if (data[i] > 1) return BML_ERROR_TYPE_MISMATCH;
        std::vector<bool> decoded;
        try { decoded.reserve(count); for (std::uint32_t i = 0; i < count; ++i) decoded.push_back(data[i] != 0); }
        catch (...) { return BML_ERROR_OUT_OF_MEMORY; }
        out = std::move(decoded); return BML_OK;
    }
    static int ReadIntArray(const FieldView &f, std::vector<int> &out) {
        return ReadFixedArray(f, Type::IntArray, 4, out, [](const std::uint8_t *data) { return static_cast<int>(Detail::Load32(data)); });
    }
    static int ReadFloatArray(const FieldView &f, std::vector<float> &out) {
        return ReadFixedArray(f, Type::FloatArray, 4, out, [](const std::uint8_t *data) { return std::bit_cast<float>(Detail::Load32(data)); });
    }
    static int ReadInt64Array(const FieldView &f, std::vector<std::int64_t> &out) {
        return ReadFixedArray(f, Type::Int64Array, 8, out, [](const std::uint8_t *data) { return std::bit_cast<std::int64_t>(Detail::Load64(data)); });
    }
    static int ReadUInt64Array(const FieldView &f, std::vector<std::uint64_t> &out) {
        return ReadFixedArray(f, Type::UInt64Array, 8, out, [](const std::uint8_t *data) { return Detail::Load64(data); });
    }
    static int ReadDoubleArray(const FieldView &f, std::vector<double> &out) {
        return ReadFixedArray(f, Type::DoubleArray, 8, out, [](const std::uint8_t *data) { return std::bit_cast<double>(Detail::Load64(data)); });
    }
    static int ReadStringArray(const FieldView &f, std::vector<std::string> &out) {
        if (f.ValueType != Type::StringArray) return BML_ERROR_TYPE_MISMATCH;
        if (f.Size < 4) return BML_ERROR_MALFORMED_MESSAGE;
        const std::uint32_t count = Detail::Load32(f.Data); const auto *cursor = f.Data + 4; std::size_t remaining = f.Size - 4;
        for (std::uint32_t i = 0; i < count; ++i) {
            if (remaining < 4) return BML_ERROR_MALFORMED_MESSAGE;
            const std::uint32_t length = Detail::Load32(cursor); cursor += 4; remaining -= 4;
            if (length > remaining) return BML_ERROR_MALFORMED_MESSAGE;
            cursor += length; remaining -= length;
        }
        if (remaining != 0) return BML_ERROR_MALFORMED_MESSAGE;
        std::vector<std::string> decoded;
        try {
            decoded.reserve(count); cursor = f.Data + 4;
            for (std::uint32_t i = 0; i < count; ++i) {
                const std::uint32_t length = Detail::Load32(cursor); cursor += 4;
                decoded.emplace_back(reinterpret_cast<const char *>(cursor), length); cursor += length;
            }
        } catch (...) { return BML_ERROR_OUT_OF_MEMORY; }
        out = std::move(decoded); return BML_OK;
    }
    static int ReadObjectArray(const FieldView &f, std::vector<BML_ObjectRef> &out) {
        return ReadFixedArray(f, Type::ObjectArray, 12, out, [](const std::uint8_t *data) {
            return BML_ObjectRef{Detail::Load32(data), Detail::Load32(data + 4), Detail::Load32(data + 8)};
        });
    }
    static int ReadVec2Array(const FieldView &f, std::vector<BML_Vec2> &out) {
        return ReadFixedArray(f, Type::Vec2Array, 8, out, [](const std::uint8_t *data) {
            return BML_Vec2{std::bit_cast<float>(Detail::Load32(data)), std::bit_cast<float>(Detail::Load32(data + 4))};
        });
    }
    static int ReadVec3Array(const FieldView &f, std::vector<BML_Vec3> &out) {
        return ReadFixedArray(f, Type::Vec3Array, 12, out, [](const std::uint8_t *data) {
            return BML_Vec3{std::bit_cast<float>(Detail::Load32(data)), std::bit_cast<float>(Detail::Load32(data + 4)), std::bit_cast<float>(Detail::Load32(data + 8))};
        });
    }
    static int ReadMat4Array(const FieldView &f, std::vector<BML_Mat4> &out) {
        return ReadFixedArray(f, Type::Mat4Array, 64, out, [](const std::uint8_t *data) {
            BML_Mat4 value{}; float *elements = &value.m00;
            for (std::size_t i = 0; i < 16; ++i) elements[i] = std::bit_cast<float>(Detail::Load32(data + i * 4));
            return value;
        });
    }
private:
    static int ReadArrayHeader(const FieldView &f, Type type, std::size_t elementSize, const std::uint8_t *&data, std::uint32_t &count) noexcept {
        if (f.ValueType != type) return BML_ERROR_TYPE_MISMATCH;
        if (f.Size < 4) return BML_ERROR_MALFORMED_MESSAGE;
        count = Detail::Load32(f.Data); std::size_t payload = 0;
        if (!FixedArrayPayloadSize(count, elementSize, payload) || payload != f.Size) return BML_ERROR_MALFORMED_MESSAGE;
        data = f.Data + 4; return BML_OK;
    }
    template <class T, class Decoder>
    static int ReadFixedArray(const FieldView &f, Type type, std::size_t elementSize, std::vector<T> &out, Decoder decode) {
        const std::uint8_t *data = nullptr; std::uint32_t count = 0;
        const int status = ReadArrayHeader(f, type, elementSize, data, count);
        if (status != BML_OK) return status;
        std::vector<T> decoded;
        try {
            decoded.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i) decoded.push_back(decode(data + static_cast<std::size_t>(i) * elementSize));
        } catch (...) { return BML_ERROR_OUT_OF_MEMORY; }
        out = std::move(decoded); return BML_OK;
    }
    static int ReadFloats(const FieldView &f, Type type, float *out, std::size_t count) noexcept {
        if (f.ValueType != type || f.Size != count * 4) return BML_ERROR_TYPE_MISMATCH;
        for (std::size_t i = 0; i < count; ++i) out[i] = std::bit_cast<float>(Detail::Load32(f.Data + i * 4)); return BML_OK;
    }
    const std::uint8_t *m_Data = nullptr; std::size_t m_Size = 0, m_Cursor = 0;
    std::uint32_t m_Schema = 0, m_Fields = 0, m_Read = 0; std::uint64_t m_Hash = 0;
    std::uint16_t m_Flags = 0; bool m_Started = false;
};

} // namespace BML::Imc::Wire
#endif // BML_IMCWIRE_HPP

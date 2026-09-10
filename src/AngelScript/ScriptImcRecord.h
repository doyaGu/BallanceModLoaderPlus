#ifndef BML_SCRIPTIMCRECORD_H
#define BML_SCRIPTIMCRECORD_H

#include <atomic>
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

#include "BML/Imc.h"
#include "BML/Types.h"

class CKObject;

namespace BML {

// Generated ASMod facades use this type as their wire boundary. It deliberately
// lives in BML::Detail on the script side; handwritten scripts should only see
// the record classes emitted from their .imc file.
class ScriptImcRecord final {
public:
    ScriptImcRecord() = default;

    void AddRef();
    void Release();

    int GetStatus() const { return m_Status; }
    bool Has(unsigned int id) const;

    void WriteBool(unsigned int id, bool value);
    void WriteInt(unsigned int id, int value);
    void WriteFloat(unsigned int id, float value);
    void WriteInt64(unsigned int id, std::int64_t value);
    void WriteUInt64(unsigned int id, std::uint64_t value);
    void WriteDouble(unsigned int id, double value);
    void WriteString(unsigned int id, const std::string &value);
    void WriteBytes(unsigned int id, const void *values);
    void WriteObject(unsigned int id, CKObject *value);
    void WriteVec2(unsigned int id, const BML_Vec2 &value);
    void WriteVec3(unsigned int id, const BML_Vec3 &value);
    void WriteMat4(unsigned int id, const BML_Mat4 &value);
    void WriteBoolArray(unsigned int id, const void *values);
    void WriteIntArray(unsigned int id, const void *values);
    void WriteFloatArray(unsigned int id, const void *values);
    void WriteInt64Array(unsigned int id, const void *values);
    void WriteUInt64Array(unsigned int id, const void *values);
    void WriteDoubleArray(unsigned int id, const void *values);
    void WriteStringArray(unsigned int id, const void *values);
    void WriteObjectArray(unsigned int id, const void *values);
    void WriteVec2Array(unsigned int id, const void *values);
    void WriteVec3Array(unsigned int id, const void *values);
    void WriteMat4Array(unsigned int id, const void *values);

    int ReadBool(unsigned int id, bool &value) const;
    int ReadInt(unsigned int id, int &value) const;
    int ReadFloat(unsigned int id, float &value) const;
    int ReadInt64(unsigned int id, std::int64_t &value) const;
    int ReadUInt64(unsigned int id, std::uint64_t &value) const;
    int ReadDouble(unsigned int id, double &value) const;
    int ReadString(unsigned int id, std::string &value) const;
    int ReadBytes(unsigned int id, void *values) const;
    int ReadObject(unsigned int id, CKObject *&value) const;
    int ReadVec2(unsigned int id, BML_Vec2 &value) const;
    int ReadVec3(unsigned int id, BML_Vec3 &value) const;
    int ReadMat4(unsigned int id, BML_Mat4 &value) const;
    int ReadBoolArray(unsigned int id, void *values) const;
    int ReadIntArray(unsigned int id, void *values) const;
    int ReadFloatArray(unsigned int id, void *values) const;
    int ReadInt64Array(unsigned int id, void *values) const;
    int ReadUInt64Array(unsigned int id, void *values) const;
    int ReadDoubleArray(unsigned int id, void *values) const;
    int ReadStringArray(unsigned int id, void *values) const;
    int ReadObjectArray(unsigned int id, void *values) const;
    int ReadVec2Array(unsigned int id, void *values) const;
    int ReadVec3Array(unsigned int id, void *values) const;
    int ReadMat4Array(unsigned int id, void *values) const;

    int Encode(std::vector<std::uint8_t> &bytes) const;
    static ScriptImcRecord *Decode(const BML_ImcMessage &message);

private:
    using Value = std::variant<
        bool, int, float, std::int64_t, std::uint64_t, double, std::string,
        std::vector<std::uint8_t>, BML_ObjectRef, BML_Vec2, BML_Vec3, BML_Mat4,
        std::vector<bool>, std::vector<int>, std::vector<float>,
        std::vector<std::int64_t>, std::vector<std::uint64_t>,
        std::vector<double>, std::vector<std::string>,
        std::vector<BML_ObjectRef>, std::vector<BML_Vec2>,
        std::vector<BML_Vec3>, std::vector<BML_Mat4>>;

    struct FieldSlice {
        unsigned int Kind = 0;
        std::size_t Offset = 0;
        std::size_t Size = 0;
    };

    template <class T>
    void Write(unsigned int id, T value);

    int Find(unsigned int id, FieldSlice &field) const;
    void SetError(int status);
    int Parse(const void *data, std::size_t size);

    std::atomic<int> m_RefCount{1};
    int m_Status = BML_OK;
    bool m_IsDecoded = false;
    std::map<unsigned int, Value> m_Values;
    std::vector<std::uint8_t> m_Bytes;
    std::map<unsigned int, FieldSlice> m_Fields;
};

ScriptImcRecord *CreateScriptImcRecord();

} // namespace BML

#endif

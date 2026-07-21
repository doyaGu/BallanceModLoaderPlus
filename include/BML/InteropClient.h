#ifndef BML_INTEROPCLIENT_H
#define BML_INTEROPCLIENT_H

#include "BML/InteropApi.h"

#include <string>
#include <utility>
#include <vector>

/* C++ is deliberately a convenience layer, not an ABI surface.  Every
 * class and function in this header is fully inline and owns only the fixed
 * C handles declared by InteropApi.h.  The loader exports no C++ type,
 * vtable, allocator, exception, RTTI, or C++ function for Interop. */
namespace BML {

using ObjectRef = BML_ObjectRef;
using Vec2 = BML_Vec2;
using Vec3 = BML_Vec3;
using Mat4 = BML_Mat4;

} // namespace BML

namespace BML::Interop {

using ObjectRef = BML_ObjectRef;
using RecordRef = BML_RecordRef;
using StreamRef = BML_StreamRef;
using CursorRef = BML_CursorRef;
using Vec2 = BML_Vec2;
using Vec3 = BML_Vec3;
using Mat4 = BML_Mat4;

inline constexpr bool IsNull(ObjectRef value) { return value.Domain == 0; }

inline int RequireApi(const BML_InteropApiDescriptor &api) {
    return BML_Interop_RequireApi(api.ApiId, api.Major, api.Hash);
}

class Record {
public:
    Record() = default;
    explicit Record(RecordRef ref) : m_Ref(ref) {}
    Record(const Record &) = delete;
    Record &operator=(const Record &) = delete;
    Record(Record &&other) noexcept : m_Ref(std::exchange(other.m_Ref, {})) {}
    Record &operator=(Record &&other) noexcept {
        if (this != &other) {
            Reset();
            m_Ref = std::exchange(other.m_Ref, {});
        }
        return *this;
    }
    ~Record() { Reset(); }

    bool Valid() const { return m_Ref.Value != 0; }
    RecordRef Handle() const { return m_Ref; }
    RecordRef ReleaseHandle() { return std::exchange(m_Ref, {}); }
    void Reset() {
        if (m_Ref.Value)
            BML_Interop_ReleaseRecord(std::exchange(m_Ref, {}));
    }

    int Schema(uint32_t &out) const { return BML_Interop_RecordSchema(m_Ref, &out); }
    int Sequence(uint64_t &out) const { return BML_Interop_RecordSequence(m_Ref, &out); }
    int Timestamp(uint64_t &out) const { return BML_Interop_RecordTimestamp(m_Ref, &out); }
    int GetBool(uint32_t field, bool &out) const {
        int value = 0;
        const int status = BML_Interop_RecordGetBool(m_Ref, field, &value);
        if (status == BML_OK) out = value != 0;
        return status;
    }
    int GetInt(uint32_t field, int &out) const { return BML_Interop_RecordGetInt(m_Ref, field, &out); }
    int GetFloat(uint32_t field, float &out) const { return BML_Interop_RecordGetFloat(m_Ref, field, &out); }
    int GetString(uint32_t field, std::string &out) const {
        size_t required = 0;
        int status = BML_Interop_RecordGetString(m_Ref, field, nullptr, 0, &required);
        if (status != BML_OK)
            return status;
        std::vector<char> storage(required ? required : 1);
        status = BML_Interop_RecordGetString(m_Ref, field, storage.data(), storage.size(), &required);
        if (status == BML_OK) out.assign(storage.data(), required > 0 ? required - 1 : 0);
        return status;
    }
    /* Avoid the Win32 GetObject macro in ordinary native mod translation
     * units. ObjectRef keeps the same ABI meaning as the script-side
     * Record::GetObject method. */
    int GetObjectRef(uint32_t field, ObjectRef &out) const {
        return BML_Interop_RecordGetObject(m_Ref, field, &out);
    }
    int GetVec2(uint32_t field, Vec2 &out) const { return BML_Interop_RecordGetVec2(m_Ref, field, &out); }
    int GetVec3(uint32_t field, Vec3 &out) const { return BML_Interop_RecordGetVec3(m_Ref, field, &out); }
    int GetMat4(uint32_t field, Mat4 &out) const { return BML_Interop_RecordGetMat4(m_Ref, field, &out); }
    int GetBoolArray(uint32_t field, std::vector<bool> &out) const {
        const void *data = nullptr;
        size_t count = 0;
        size_t elementSize = 0;
        int status = Borrow(field, BML_INTEROP_FIELD_BOOL_ARRAY, data, count, elementSize);
        if (status != BML_OK)
            return status;
        if (elementSize != sizeof(int) || (count > 0 && !data))
            return BML_ERROR_INTEROP_RECORD_INVALID;
        std::vector<bool> value;
        value.reserve(count);
        const int *items = static_cast<const int *>(data);
        for (size_t index = 0; index < count; ++index)
            value.push_back(items[index] != 0);
        out = std::move(value);
        return BML_OK;
    }
    int GetIntArray(uint32_t field, std::vector<int> &out) const {
        return CopyFixedArray(field, BML_INTEROP_FIELD_INT_ARRAY, out);
    }
    int GetFloatArray(uint32_t field, std::vector<float> &out) const {
        return CopyFixedArray(field, BML_INTEROP_FIELD_FLOAT_ARRAY, out);
    }
    int GetObjectArray(uint32_t field, std::vector<ObjectRef> &out) const {
        return CopyFixedArray(field, BML_INTEROP_FIELD_OBJECT_ARRAY, out);
    }
    int GetVec2Array(uint32_t field, std::vector<Vec2> &out) const {
        return CopyFixedArray(field, BML_INTEROP_FIELD_VEC2_ARRAY, out);
    }
    int GetVec3Array(uint32_t field, std::vector<Vec3> &out) const {
        return CopyFixedArray(field, BML_INTEROP_FIELD_VEC3_ARRAY, out);
    }
    int GetMat4Array(uint32_t field, std::vector<Mat4> &out) const {
        return CopyFixedArray(field, BML_INTEROP_FIELD_MAT4_ARRAY, out);
    }
    int Borrow(uint32_t field,
               BML_INTEROP_FIELD_TYPE expectedType,
               const void *&outData,
               size_t &outCount,
               size_t &outElementSize) const {
        return BML_Interop_RecordBorrowValue(m_Ref, field, expectedType,
                                              &outData, &outCount, &outElementSize);
    }
    int StringArrayCount(uint32_t field, size_t &outCount) const {
        return BML_Interop_RecordGetStringArrayCount(m_Ref, field, &outCount);
    }
    int GetStringArrayItem(uint32_t field, size_t item, std::string &out) const {
        size_t required = 0;
        int status = BML_Interop_RecordGetStringArrayItem(m_Ref, field, item,
                                                           nullptr, 0, &required);
        if (status != BML_OK)
            return status;
        std::vector<char> storage(required ? required : 1);
        status = BML_Interop_RecordGetStringArrayItem(m_Ref, field, item,
                                                       storage.data(), storage.size(), &required);
        if (status == BML_OK) out.assign(storage.data(), required > 0 ? required - 1 : 0);
        return status;
    }
    int GetStringArray(uint32_t field, std::vector<std::string> &out) const {
        size_t count = 0;
        int status = StringArrayCount(field, count);
        if (status != BML_OK)
            return status;
        std::vector<std::string> value;
        value.reserve(count);
        for (size_t index = 0; index < count; ++index) {
            std::string item;
            status = GetStringArrayItem(field, index, item);
            if (status != BML_OK)
                return status;
            value.push_back(std::move(item));
        }
        out = std::move(value);
        return BML_OK;
    }

private:
    template <typename T>
    int CopyFixedArray(uint32_t field, BML_INTEROP_FIELD_TYPE type, std::vector<T> &out) const {
        const void *data = nullptr;
        size_t count = 0;
        size_t elementSize = 0;
        const int status = Borrow(field, type, data, count, elementSize);
        if (status != BML_OK)
            return status;
        if (elementSize != sizeof(T) || (count > 0 && !data))
            return BML_ERROR_INTEROP_RECORD_INVALID;
        const T *items = static_cast<const T *>(data);
        std::vector<T> value;
        if (count > 0)
            value.assign(items, items + count);
        out = std::move(value);
        return BML_OK;
    }

    RecordRef m_Ref{};
};

class Stream {
public:
    Stream() = default;
    explicit Stream(StreamRef ref) : m_Ref(ref) {}
    Stream(const Stream &) = delete;
    Stream &operator=(const Stream &) = delete;
    Stream(Stream &&other) noexcept : m_Ref(std::exchange(other.m_Ref, {})) {}
    Stream &operator=(Stream &&other) noexcept {
        if (this != &other) {
            Close();
            m_Ref = std::exchange(other.m_Ref, {});
        }
        return *this;
    }
    ~Stream() { Close(); }
    bool Valid() const { return m_Ref.Value != 0; }
    int Poll(Record &out) const {
        BML_RecordRef ref{};
        const int status = BML_Interop_PollStream(m_Ref, &ref);
        if (status == BML_OK) {
            out.Reset();
            if (ref.Value)
                out = Record(ref);
        }
        return status;
    }
    int DroppedCount(int &out) const { return BML_Interop_GetDroppedStreamCount(m_Ref, &out); }
    int Close() {
        if (!m_Ref.Value) return BML_OK;
        const int status = BML_Interop_CloseStream(m_Ref);
        m_Ref = {};
        return status;
    }
private:
    StreamRef m_Ref{};
};

class Collection {
public:
    Collection() = default;
    explicit Collection(CursorRef ref) : m_Ref(ref) {}
    Collection(const Collection &) = delete;
    Collection &operator=(const Collection &) = delete;
    Collection(Collection &&other) noexcept : m_Ref(std::exchange(other.m_Ref, {})) {}
    Collection &operator=(Collection &&other) noexcept {
        if (this != &other) {
            Close();
            m_Ref = std::exchange(other.m_Ref, {});
        }
        return *this;
    }
    ~Collection() { Close(); }
    bool Valid() const { return m_Ref.Value != 0; }
    int Next(uint32_t limit, std::vector<Record> &out, bool &complete) const {
        if (limit == 0) return BML_ERROR_INVALID_PARAMETER;
        std::vector<BML_RecordRef> refs(limit);
        size_t count = 0;
        int isComplete = 0;
        const int status = BML_Interop_ReadCollectionPage(m_Ref, refs.data(), refs.size(), &count, &isComplete);
        if (status != BML_OK) return status;
        out.clear();
        out.reserve(count);
        for (size_t index = 0; index < count; ++index) out.emplace_back(refs[index]);
        complete = isComplete != 0;
        return BML_OK;
    }
    int Close() {
        if (!m_Ref.Value) return BML_OK;
        const int status = BML_Interop_CloseCollection(m_Ref);
        m_Ref = {};
        return status;
    }
private:
    CursorRef m_Ref{};
};

inline int ReadResource(const char *apiId, const char *endpoint, Record &out) {
    BML_RecordRef ref{};
    const int status = BML_Interop_ReadResource(apiId, endpoint, &ref);
    if (status == BML_OK) out = Record(ref);
    return status;
}

inline int ReadComponent(const char *apiId, const char *endpoint, ObjectRef object, Record &out) {
    BML_RecordRef ref{};
    const int status = BML_Interop_ReadComponent(apiId, endpoint, object, &ref);
    if (status == BML_OK) out = Record(ref);
    return status;
}

inline int OpenStream(const char *apiId, const char *endpoint, int capacity, Stream &out) {
    BML_StreamRef ref{};
    const int status = BML_Interop_OpenStream(apiId, endpoint, capacity, &ref);
    if (status == BML_OK) out = Stream(ref);
    return status;
}

inline int OpenCollection(const char *apiId, const char *endpoint, Collection &out) {
    BML_CursorRef ref{};
    const int status = BML_Interop_OpenCollection(apiId, endpoint, &ref);
    if (status == BML_OK) out = Collection(ref);
    return status;
}

} // namespace BML::Interop

namespace BML::Interop::Detail {

/* Private plumbing for generated, shallow facades.  It is header-only: no
 * C++ object or function is imported across a mod DLL boundary. */
class InputRecord final {
public:
    InputRecord() = default;
    InputRecord(const InputRecord &) = delete;
    InputRecord &operator=(const InputRecord &) = delete;
    InputRecord(InputRecord &&other) noexcept : m_Builder(std::exchange(other.m_Builder, nullptr)) {}
    InputRecord &operator=(InputRecord &&other) noexcept {
        if (this != &other) {
            Reset();
            m_Builder = std::exchange(other.m_Builder, nullptr);
        }
        return *this;
    }
    ~InputRecord() { Reset(); }

    int Create(const char *apiId, uint32_t schema) {
        Reset();
        return BML_Interop_CreateInputRecord(apiId, schema, &m_Builder);
    }
    void Reset() {
        if (m_Builder)
            BML_Interop_DestroyRecordBuilder(std::exchange(m_Builder, nullptr));
    }
    BML_InteropRecordBuilder *Get() const { return m_Builder; }

    int SetBool(uint32_t field, bool value) const {
        const int raw = value ? 1 : 0;
        return BML_Interop_RecordBuilder_SetValue(m_Builder, field, BML_INTEROP_FIELD_BOOL, &raw, 1);
    }
    int SetInt(uint32_t field, int value) const {
        return BML_Interop_RecordBuilder_SetValue(m_Builder, field, BML_INTEROP_FIELD_INT, &value, 1);
    }
    int SetFloat(uint32_t field, float value) const {
        return BML_Interop_RecordBuilder_SetValue(m_Builder, field, BML_INTEROP_FIELD_FLOAT, &value, 1);
    }
    int SetString(uint32_t field, const std::string &value) const {
        return BML_Interop_RecordBuilder_SetValue(m_Builder, field, BML_INTEROP_FIELD_STRING,
                                                  value.data(), value.size());
    }
    int SetObject(uint32_t field, ObjectRef value) const {
        return BML_Interop_RecordBuilder_SetValue(m_Builder, field, BML_INTEROP_FIELD_OBJECT, &value, 1);
    }
    int SetVec2(uint32_t field, Vec2 value) const {
        return BML_Interop_RecordBuilder_SetValue(m_Builder, field, BML_INTEROP_FIELD_VEC2, &value, 1);
    }
    int SetVec3(uint32_t field, Vec3 value) const {
        return BML_Interop_RecordBuilder_SetValue(m_Builder, field, BML_INTEROP_FIELD_VEC3, &value, 1);
    }
    int SetMat4(uint32_t field, Mat4 value) const {
        return BML_Interop_RecordBuilder_SetValue(m_Builder, field, BML_INTEROP_FIELD_MAT4, &value, 1);
    }
    int SetBoolArray(uint32_t field, const std::vector<bool> &values) const {
        std::vector<int> copy;
        copy.reserve(values.size());
        for (bool value : values)
            copy.push_back(value ? 1 : 0);
        return BML_Interop_RecordBuilder_SetValue(m_Builder,
                                                  field,
                                                  BML_INTEROP_FIELD_BOOL_ARRAY,
                                                  copy.empty() ? nullptr : copy.data(),
                                                  copy.size());
    }
    int SetIntArray(uint32_t field, const std::vector<int> &values) const {
        return SetFixedArray(field, BML_INTEROP_FIELD_INT_ARRAY, values);
    }
    int SetFloatArray(uint32_t field, const std::vector<float> &values) const {
        return SetFixedArray(field, BML_INTEROP_FIELD_FLOAT_ARRAY, values);
    }
    int SetObjectArray(uint32_t field, const std::vector<ObjectRef> &values) const {
        return SetFixedArray(field, BML_INTEROP_FIELD_OBJECT_ARRAY, values);
    }
    int SetVec2Array(uint32_t field, const std::vector<Vec2> &values) const {
        return SetFixedArray(field, BML_INTEROP_FIELD_VEC2_ARRAY, values);
    }
    int SetVec3Array(uint32_t field, const std::vector<Vec3> &values) const {
        return SetFixedArray(field, BML_INTEROP_FIELD_VEC3_ARRAY, values);
    }
    int SetMat4Array(uint32_t field, const std::vector<Mat4> &values) const {
        return SetFixedArray(field, BML_INTEROP_FIELD_MAT4_ARRAY, values);
    }
    int SetStringArray(uint32_t field, const std::vector<std::string> &values) const {
        std::vector<const char *> pointers;
        std::vector<size_t> sizes;
        pointers.reserve(values.size());
        sizes.reserve(values.size());
        for (const std::string &value : values) {
            pointers.push_back(value.data());
            sizes.push_back(value.size());
        }
        return BML_Interop_RecordBuilder_SetStringArray(m_Builder,
                                                        field,
                                                        pointers.empty() ? nullptr : pointers.data(),
                                                        sizes.empty() ? nullptr : sizes.data(),
                                                        values.size());
    }

private:
    template <typename T>
    int SetFixedArray(uint32_t field,
                      BML_INTEROP_FIELD_TYPE type,
                      const std::vector<T> &values) const {
        return BML_Interop_RecordBuilder_SetValue(m_Builder,
                                                  field,
                                                  type,
                                                  values.empty() ? nullptr : values.data(),
                                                  values.size());
    }

    BML_InteropRecordBuilder *m_Builder = nullptr;
};

inline int InvokeQuery(const char *apiId, const char *endpoint, const InputRecord &input, Record &out) {
    BML_RecordRef record{};
    const int status = BML_Interop_InvokeQuery(apiId, endpoint, input.Get(), &record);
    if (status == BML_OK)
        out = Record(record);
    return status;
}

inline int InvokeCommand(const char *apiId, const char *endpoint, const InputRecord &input) {
    BML_RecordRef record{};
    const int status = BML_Interop_InvokeCommand(apiId, endpoint, input.Get(), &record);
    if (record.Value)
        BML_Interop_ReleaseRecord(record);
    return status;
}

/* Commands have a response schema too.  Keep the existing discard-response
 * helper above for simple fire-and-forget callers, while generated typed
 * bindings use this overload to decode the immutable response snapshot. */
inline int InvokeCommand(const char *apiId,
                         const char *endpoint,
                         const InputRecord &input,
                         Record &out) {
    BML_RecordRef record{};
    const int status = BML_Interop_InvokeCommand(apiId, endpoint, input.Get(), &record);
    if (status == BML_OK)
        out = Record(record);
    else if (record.Value)
        BML_Interop_ReleaseRecord(record);
    return status;
}

inline int MakeEmptyInput(const char *apiId, uint32_t schema, InputRecord &out) {
    return out.Create(apiId, schema);
}

} // namespace BML::Interop::Detail

#endif // BML_INTEROPCLIENT_H
